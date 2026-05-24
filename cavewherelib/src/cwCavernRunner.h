/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAVERNRUNNER_H
#define CWCAVERNRUNNER_H

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"

#include <QString>

/**
 * \brief Runs survex's cavern in-process and writes the .3d output.
 *
 * Cavern is invoked with --log so its diagnostic output lands in a sidecar
 * file derived from the output path (same directory, same complete base name,
 * .log extension). The log is read back into Result::logText so callers can
 * surface it (survex defines STDERR=stdout, so --log captures both success-
 * time info messages and failure-time error text).
 *
 * Netskel additionally writes an .err sidecar containing the loop-closure
 * report (traverse / length / bearing / gradient error statistics) whenever
 * the cave has loops. Its contents are read into Result::loopClosureStats so
 * callers can surface them too. Neither sidecar file is deleted here — the
 * caller owns the working directory (typically a QTemporaryDir) and cleans
 * up on scope exit.
 *
 * On non-zero exit the returned Monad::Result carries the log text as its
 * error message and SolveErrorCode::CavernFailed as its code, so the caller
 * can route the diagnostic to a UI surface without rerunning cavern.
 *
 * cavernlib uses global state; run() serializes concurrent callers via an
 * internal QMutex. cavernlib crashes terminate the process — only
 * cleanly-detected errors come back as a Result error.
 */
class CAVEWHERE_LIB_EXPORT cwCavernRunner
{
public:
    cwCavernRunner() = delete;

    struct Result {
        QString output3dPath;
        QString logText;            // <basename>.log — info + error messages
        QString loopClosureStats;   // <basename>.err — netskel loop-closure report
        int exitCode = 0;
    };

    static Monad::Result<Result> run(const QString& svxPath,
                                     const QString& output3dPath);
};

#endif // CWCAVERNRUNNER_H
