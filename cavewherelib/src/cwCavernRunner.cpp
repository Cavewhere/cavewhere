/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCavernRunner.h"
#include "cwLinePlotErrorCodes.h"

// Survex library
#include "cavern_lib.h"

#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <array>

namespace {

QString sidecarPath(const QString& output3dPath, const QString& extension)
{
    const QFileInfo info(output3dPath);
    return info.absolutePath() + QStringLiteral("/") + info.completeBaseName()
           + QStringLiteral(".") + extension;
}

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream stream(&file);
    return stream.readAll();
}

} // namespace

Monad::Result<cwCavernRunner::Result>
cwCavernRunner::run(const QString& svxPath, const QString& output3dPath)
{
    // cavern_run() uses global state; serialise all callers.
    static QMutex cavernMutex;
    QMutexLocker locker(&cavernMutex);

    char arg0[] = "cavern";
    char logArg[] = "--log";
    char quietArg[] = "--quiet";
    QByteArray outputArg = QStringLiteral("--output=%1").arg(output3dPath).toUtf8();
    QByteArray inputArg = svxPath.toUtf8();

    // Trailing nullptr matches the C standard argv convention; argc excludes it.
    std::array<char*, 7> argv = {
        arg0,
        logArg,
        quietArg,
        quietArg,
        outputArg.data(),
        inputArg.data(),
        nullptr,
    };

    const int returnCode = cavern_run(argv.size() - 1, argv.data());

    const QString logText = readTextFile(sidecarPath(output3dPath, QStringLiteral("log")));
    const QString loopClosureStats =
        readTextFile(sidecarPath(output3dPath, QStringLiteral("err")));

    if (returnCode != 0) {
        QString message = logText;
        if (message.isEmpty()) {
            message = QStringLiteral("cavern exited with code %1 (no log captured)")
                          .arg(returnCode);
        }
        return Monad::Result<Result>(message,
                                     static_cast<int>(LinePlotErrorCode::CavernFailed));
    }

    Result result;
    result.output3dPath = output3dPath;
    result.logText = logText;
    result.loopClosureStats = loopClosureStats;
    result.exitCode = returnCode;
    return Monad::Result<Result>(result);
}
