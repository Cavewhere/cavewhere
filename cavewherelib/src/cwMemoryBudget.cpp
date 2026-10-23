//Our includes
#include "cwMemoryBudget.h"
#include "cwRenderBudgets.h"

//Qt includes
#include <QCoreApplication>
#include <QFile>

//Std includes
#include <algorithm>

#if defined(Q_OS_APPLE)
#include <sys/sysctl.h>
#endif
#if defined(Q_OS_IOS)
#include <os/proc.h>
#endif
#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif
#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
    using cw::budgets::kBytesPerMegabyte;
    constexpr qint64 kBytesPerKiB = 1024;

    // Graphics (swapchain, GPU textures) and Qt's own heap live outside the ledger.
    constexpr qint64 kGraphicsReserveBytes = 512 * kBytesPerMegabyte;

    // The automatic cap never drops below this, so a tight device still admits work.
    constexpr qint64 kMinimumCapBytes = 64 * kBytesPerMegabyte;

#if defined(Q_OS_MACOS)
    // macOS reports no per-process limit and swap absorbs overruns; the cap keeps the app fast.
    constexpr double kDesktopBudgetFraction = 0.5;
#endif

#if defined(Q_OS_APPLE)
    qint64 physicalMemoryBytes()
    {
        quint64 memSize = 0;
        size_t length = sizeof(memSize);
        if(sysctlbyname("hw.memsize", &memSize, &length, nullptr, 0) == 0) {
            return static_cast<qint64>(memSize);
        }
        return 0;
    }
#endif

    struct MemInfo {
        qint64 totalBytes = 0;
        qint64 availableBytes = 0;
    };

#if defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
    MemInfo readProcMemInfo()
    {
        MemInfo info;
        QFile file(QStringLiteral("/proc/meminfo"));
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return info;
        }

        // Lines read "MemTotal:       16318412 kB".
        const auto kiBValue = [](const QByteArray& line) {
            const QList<QByteArray> fields = line.simplified().split(' ');
            return fields.size() >= 2 ? fields.at(1).toLongLong() * kBytesPerKiB : qint64(0);
        };

        while(!file.atEnd()) {
            const QByteArray line = file.readLine();
            if(line.startsWith("MemTotal:")) {
                info.totalBytes = kiBValue(line);
            } else if(line.startsWith("MemAvailable:")) {
                info.availableBytes = kiBValue(line);
            }
        }
        return info;
    }
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // cgroup v2 memory.max holds a byte count, or "max" when the group is unlimited.
    qint64 cgroupLimitBytes()
    {
        QFile file(QStringLiteral("/sys/fs/cgroup/memory.max"));
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return 0;
        }
        bool isNumber = false;
        const qint64 limit = file.readAll().trimmed().toLongLong(&isNumber);
        return isNumber ? limit : 0;
    }

    qint64 cappedByCgroup(qint64 bytes)
    {
        const qint64 limit = cgroupLimitBytes();
        return limit > 0 ? std::min(bytes, limit) : bytes;
    }
#endif

#if defined(Q_OS_ANDROID)
    // availMem − threshold is what the low-memory killer leaves the foreground app.
    MemInfo readActivityManagerMemInfo()
    {
        MemInfo info;
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if(!context.isValid()) {
            return info;
        }
        QJniObject activityManager = context.callObjectMethod(
            "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;",
            QJniObject::fromString(QStringLiteral("activity")).object<jstring>());
        if(!activityManager.isValid()) {
            return info;
        }
        QJniObject memoryInfo("android/app/ActivityManager$MemoryInfo");
        activityManager.callMethod<void>("getMemoryInfo",
                                         "(Landroid/app/ActivityManager$MemoryInfo;)V",
                                         memoryInfo.object());
        const qint64 availMem = memoryInfo.getField<jlong>("availMem");
        const qint64 threshold = memoryInfo.getField<jlong>("threshold");
        info.totalBytes = memoryInfo.getField<jlong>("totalMem");
        info.availableBytes = std::max(availMem - threshold, qint64(0));
        return info;
    }

    MemInfo readAndroidMemInfo()
    {
        const MemInfo activityInfo = readActivityManagerMemInfo();
        if(activityInfo.totalBytes > 0) {
            return activityInfo;
        }
        return readProcMemInfo();
    }
#endif

#if defined(Q_OS_WIN)
    MemInfo readWindowsMemInfo()
    {
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        if(GlobalMemoryStatusEx(&status)) {
            return {static_cast<qint64>(status.ullTotalPhys), static_cast<qint64>(status.ullAvailPhys)};
        }
        return {};
    }
#endif
}

cwMemoryBudget* cwMemoryBudget::Budget = nullptr;

cwMemoryBudget::cwMemoryBudget(QObject* parent) :
    QObject(parent)
{
}

qint64 cwMemoryBudget::capBytes() const
{
    const qint64 overrideBytes = m_capOverrideBytes.load();
    if(overrideBytes > 0) {
        return overrideBytes;
    }
    return automaticCapBytes();
}

qint64 cwMemoryBudget::automaticCapBytes() const
{
    const qint64 cap = std::max(availableBytes() - kGraphicsReserveBytes, kMinimumCapBytes);
    const qint64 total = totalBytes();
    return total > 0 ? std::min(cap, total) : cap;
}

qint64 cwMemoryBudget::inFlightBytes() const
{
    return m_inFlightBytes.load();
}

void cwMemoryBudget::setCapOverrideBytes(qint64 bytes)
{
    m_capOverrideBytes.store(std::max(bytes, qint64(0)));
}

qint64 cwMemoryBudget::capOverrideBytes() const
{
    return m_capOverrideBytes.load();
}

void cwMemoryBudget::setPaused(bool paused)
{
    m_paused.store(paused);
}

bool cwMemoryBudget::isPaused() const
{
    return m_paused.load();
}

bool cwMemoryBudget::tryCharge(qint64 bytes)
{
    if(m_paused.load()) {
        return false;
    }

    const qint64 cap = capBytes();
    qint64 inFlight = m_inFlightBytes.load();
    do {
        if(inFlight + bytes > cap) {
            return false;
        }
    } while(!m_inFlightBytes.compare_exchange_weak(inFlight, inFlight + bytes));
    return true;
}

void cwMemoryBudget::release(qint64 bytes)
{
    m_inFlightBytes.fetch_sub(bytes);
}

qint64 cwMemoryBudget::availableBytes()
{
#if defined(Q_OS_IOS)
    return static_cast<qint64>(os_proc_available_memory());
#elif defined(Q_OS_MACOS)
    return static_cast<qint64>(physicalMemoryBytes() * kDesktopBudgetFraction);
#elif defined(Q_OS_ANDROID)
    return readAndroidMemInfo().availableBytes;
#elif defined(Q_OS_LINUX)
    return cappedByCgroup(readProcMemInfo().availableBytes);
#elif defined(Q_OS_WIN)
    return readWindowsMemInfo().availableBytes;
#else
    return 0;
#endif
}

qint64 cwMemoryBudget::totalBytes()
{
#if defined(Q_OS_APPLE)
    return physicalMemoryBytes();
#elif defined(Q_OS_ANDROID)
    return readAndroidMemInfo().totalBytes;
#elif defined(Q_OS_LINUX)
    return cappedByCgroup(readProcMemInfo().totalBytes);
#elif defined(Q_OS_WIN)
    return readWindowsMemInfo().totalBytes;
#else
    return 0;
#endif
}

cwMemoryBudget* cwMemoryBudget::instance()
{
    return Budget;
}

void cwMemoryBudget::initialize()
{
    if(Budget == nullptr) {
        Budget = new cwMemoryBudget(QCoreApplication::instance());
    }
}
