/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwProfileLog.h"

//Qt includes
#include <QByteArray>
#include <QDebug>
#include <QFile>

#ifdef Q_OS_MACOS
#include <mach/mach.h>
#endif

Q_LOGGING_CATEGORY(lcProfileRender, "cw.profile.render", QtWarningMsg)
Q_LOGGING_CATEGORY(lcProfilePick, "cw.profile.pick", QtWarningMsg)
Q_LOGGING_CATEGORY(lcProfileLoad, "cw.profile.load", QtWarningMsg)

void cw::profile::write(const QLoggingCategory& category, const QString& line)
{
    if (!category.isDebugEnabled()) {
        return;
    }

    QMessageLogger().debug(category).noquote() << line;
}

qint64 cw::profile::peakResidentBytes()
{
#if defined(Q_OS_MACOS)
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return 0;
    }
    return qint64(info.resident_size_max);
#elif defined(Q_OS_LINUX)
    //VmHWM is the high water mark, in kilobytes
    constexpr qint64 kBytesPerKilobyte = 1024;
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    while (!status.atEnd()) {
        const QByteArray line = status.readLine();
        if (!line.startsWith("VmHWM:")) {
            continue;
        }

        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() >= 2) {
            return fields.at(1).toLongLong() * kBytesPerKilobyte;
        }
    }
    return 0;
#else
    return 0;
#endif
}
