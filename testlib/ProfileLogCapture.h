/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef PROFILELOGCAPTURE_H
#define PROFILELOGCAPTURE_H

//Qt includes
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QtGlobal>

//Std includes
#include <cstdio>
#include <utility>

// Turns logging rules on for as long as it lives and keeps every message that
// arrives while they are, so a test can read what the profiling harness wrote.
// One at a time: the message handler Qt installs is process wide, and so are
// the filter rules.
class ProfileLogCapture
{
public:
    explicit ProfileLogCapture(const QString& filterRules)
    {
        {
            const QMutexLocker locker(&mutex());
            lines().clear();
        }

        previousHandler() = qInstallMessageHandler(&ProfileLogCapture::keep);
        QLoggingCategory::setFilterRules(filterRules);
    }

    ~ProfileLogCapture()
    {
        QLoggingCategory::setFilterRules(QString());
        qInstallMessageHandler(previousHandler());
        previousHandler() = nullptr;
    }

    ProfileLogCapture(const ProfileLogCapture&) = delete;
    ProfileLogCapture& operator=(const ProfileLogCapture&) = delete;

    //! Everything kept so far whose message begins with @a prefix.
    static QStringList linesStartingWith(const QString& prefix)
    {
        const QMutexLocker locker(&mutex());

        QStringList matched;
        for (const QString& line : std::as_const(lines())) {
            if (line.startsWith(prefix)) {
                matched.append(line);
            }
        }
        return matched;
    }

private:
    // The profiling categories write from the render thread and, for
    // cw.profile.load, from a pooled worker, so the kept lines are locked.
    static void keep(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        {
            const QMutexLocker locker(&mutex());
            lines().append(message);
        }

        // Anything the test is not looking at still belongs on the terminal.
        if (previousHandler() != nullptr) {
            previousHandler()(type, context, message);
        } else {
            const QByteArray line = qFormatLogMessage(type, context, message).toLocal8Bit();
            fputs(line.constData(), stderr);
            fputc('\n', stderr);
        }
    }

    static QStringList& lines()
    {
        static QStringList kept;
        return kept;
    }

    static QMutex& mutex()
    {
        static QMutex guard;
        return guard;
    }

    static QtMessageHandler& previousHandler()
    {
        static QtMessageHandler handler = nullptr;
        return handler;
    }
};

#endif // PROFILELOGCAPTURE_H
