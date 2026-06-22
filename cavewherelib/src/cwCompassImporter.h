/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOMPASSIMPORTER_H
#define CWCOMPASSIMPORTER_H

//Our includes
#include "cwCave.h"
#include "cwGlobals.h"

//Qt include
#include <QFuture>
#include <QStringList>


/**
 * @brief The cwCompassImporter class
 *
 * This allows cavewhere to import a compass dat file. The import runs on a
 * background thread through cwConcurrent::run() and reports determinate
 * progress and cancellation through the QPromise it is given. Warnings and
 * errors are collected into Result::messages instead of being emitted, so the
 * worker has no thread affinity or signal/slot surface.
 */
class CAVEWHERE_LIB_EXPORT cwCompassImporter
{
public:
    struct Result {
        QList<cwCaveData> caves;
        QStringList messages; //!< Warnings/errors collected during the parse
    };

    static QFuture<Result> run(QStringList compassDataFiles);

    struct Worker;
};

#endif // CWCOMPASSIMPORTER_H
