#include "cwDebug.h"

#if defined(QT_DEBUG) && defined(Q_OS_MAC) && !defined(Q_OS_IOS)
#include <QCoreApplication>
#include <QProcess>
#include <QStringList>

#include <QtGlobal>

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <cstdlib>
#endif

void cwDebug::printStackTrace(int maxFrames)
{
#if defined(QT_DEBUG) && defined(Q_OS_MAC) && !defined(Q_OS_IOS)
    auto demangle = [](const char* symbol) -> QString {
        if(symbol == nullptr) {
            return QStringLiteral("<unknown>");
        }

        int status = 0;
        size_t length = 0;
        char* demangled = abi::__cxa_demangle(symbol, nullptr, &length, &status);
        if(status == 0 && demangled != nullptr) {
            QString result = QString::fromLatin1(demangled);
            std::free(demangled);
            return result;
        }

        return QString::fromLatin1(symbol);
    };

    auto resolveWithAtos = [](void* const* addresses, int frameCount) -> QStringList {
        QStringList args;
        args << "-p" << QString::number(QCoreApplication::applicationPid());
        for(int i = 0; i < frameCount; ++i) {
            args << QString::number(reinterpret_cast<quintptr>(addresses[i]), 16);
        }

        QProcess atos;
        atos.start(QStringLiteral("atos"), args);
        if(!atos.waitForFinished(500)) {
            return {};
        }

        const QStringList lines = QString::fromUtf8(atos.readAllStandardOutput())
                .split('\n', Qt::SkipEmptyParts);
        return lines;
    };

    const int clampedFrames = qBound(1, maxFrames, 256);
    void* addresses[256];
    int frameCount = backtrace(addresses, clampedFrames);

    if(frameCount <= 0) {
        qWarning() << "cwDebug::printStackTrace: No stack frames available";
        return;
    }

    char** symbols = backtrace_symbols(addresses, frameCount);
    if(symbols == nullptr) {
        qWarning() << "cwDebug::printStackTrace: backtrace_symbols failed";
        return;
    }

    const QStringList atosLines = resolveWithAtos(addresses, frameCount);

    qDebug().noquote() << "Stack trace (" << frameCount << " frames):";
    for(int i = 0; i < frameCount; ++i) {
        Dl_info info {};
        QString library;
        QString symbolName;
        quintptr offset = 0;

        if(dladdr(addresses[i], &info) != 0) {
            if(info.dli_fname != nullptr) {
                library = QString::fromLatin1(info.dli_fname);
            }
            if(info.dli_sname != nullptr) {
                symbolName = demangle(info.dli_sname);
                offset = reinterpret_cast<quintptr>(addresses[i]) -
                         reinterpret_cast<quintptr>(info.dli_saddr);
            }
        }

        const QString atosLine = i < atosLines.size() ? atosLines.at(i) : QString();
        QDebug dbg = qDebug().noquote().nospace();
        dbg << "#" << qSetFieldWidth(2) << qSetPadChar('0') << i
            << qSetFieldWidth(0) << " "
            << (library.isEmpty() ? QStringLiteral("<unknown>") : library) << " "
            << (symbolName.isEmpty() ? QString::fromLatin1(symbols[i]) : symbolName)
            << " +0x" << QString::number(offset, 16)
            << " [" << addresses[i] << "]";

        if(!atosLine.isEmpty()) {
            dbg << " (" << atosLine << ")";
        }
    }

    std::free(symbols);
#else
    Q_UNUSED(maxFrames);
#endif
}
