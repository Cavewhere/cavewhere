/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDEBUG_H
#define CWDEBUG_H

#define LOCATION __FILE__ << __LINE__
#define LOCATION_STR QString("%1 %2").arg(__FILE__).arg(__LINE__).toLocal8Bit()

//Qt includes
#include <QQmlComponent>
#include <QDebug>
#include <QtGlobal>

#if defined(QT_DEBUG) && defined(Q_OS_MAC)
#include <execinfo.h>
#include <cstdlib>
#endif

class cwDebug {
public:
    /**
     * @brief printErrors
     * @param component
     *
     * Afte the compontent has been create. This will print out any error to the commandline
     * of the component. If the component has no errors, this does nothing.
     */
    static void printErrors(QQmlComponent* component) {
        if(!component->errors().isEmpty()) {
            qDebug() << component->errorString();
        }
    }

    /**
     * @brief printStackTrace
     *
     * Prints the current stack trace to the debug output. Only implemented for
     * macOS debug builds; on other platforms this is a no-op.
     */
    static void printStackTrace(int maxFrames = 64) {
#if defined(QT_DEBUG) && defined(Q_OS_MAC)
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

        qDebug().noquote() << "Stack trace (" << frameCount << "frames):";
        for(int i = 0; i < frameCount; ++i) {
            qDebug().noquote() << symbols[i];
        }

        std::free(symbols);
#else
        Q_UNUSED(maxFrames);
#endif
    }
};

#endif // CWDEBUG_H
