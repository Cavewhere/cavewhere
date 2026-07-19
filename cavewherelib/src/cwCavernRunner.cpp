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

// Survex's per-run warning counter (message.c, declared in message.h).
// cavern_run resets it via msg_reset_counters(), so reading it right
// after the run — still under the serialising mutex — yields this run's
// count. Hand-declared because survex's message.h can't be included from
// C++/Qt code (requires config.h first and #defines error(...), which
// clobbers Qt headers); exporting counters via cavern_lib.h would fix
// this properly but needs a survex submodule change.
extern "C" int msg_warnings;

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

    // Single --quiet suppresses progress chatter on stdout but still writes
    // info-level messages (implicit-fix notices, etc.) to the .log file —
    // which CavernOutputPage surfaces to the user. Double --quiet additionally
    // empties the .log file (verified against our vendored cavernlib), so we
    // intentionally pass only one.
    // Trailing nullptr matches the C standard argv convention; argc excludes it.
    std::array<char*, 6> argv = {
        arg0,
        logArg,
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
    result.warningCount = msg_warnings;
    return Monad::Result<Result>(result);
}
