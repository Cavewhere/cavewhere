/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZCLIPOPERATION_H
#define CWLAZCLIPOPERATION_H

//Qt includes
#include <QFuture>
#include <QList>
#include <QPolygonF>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGeoPoint.h"

/**
 * One source LAZ feeding cwLazClipOperation.
 *
 * @c sourceCSOverride mirrors cwLazLoader::Request::sourceCSOverride: if
 * empty, the LAZ-embedded OGC WKT (when present) drives reprojection;
 * otherwise this explicit CS takes precedence. Files with neither an
 * embedded CS nor an override skip reprojection, matching the loader's
 * identity-short-circuit.
 */
struct CAVEWHERE_LIB_EXPORT cwLazClipSource {
    QString sourcePath;
    QString sourceCSOverride;
};

/**
 * Polygon-clips a set of LAZ source files into a single output .laz.
 *
 * Each source is reopened independently via LASlib::LASreader, reprojected
 * to @c globalCS, and each point is tested against @c polygonLocalXY (the
 * same worldOrigin-relative XY space cwLazLoader produces — i.e. project
 * coords with worldOrigin subtracted).
 *
 * - Mode::Keep retains points inside the polygon (Qt::OddEvenFill).
 * - Mode::Remove retains points outside.
 *
 * Output is LAS 1.2 point format 0 (XYZ only). Source intensity, RGB,
 * classification, GPS time etc. are dropped. The output's OGC WKT VLR is
 * set to @c globalCS so cwLazLoader round-trips the result as project-CS
 * data.
 *
 * Threading: one worker on cwConcurrent's thread pool. LASreader,
 * LASwriter and cwCoordinateTransform are each non-thread-safe; all I/O
 * stays on that one worker. Cancellation is honored between point chunks
 * via QPromise::isCanceled().
 */
class CAVEWHERE_LIB_EXPORT cwLazClipOperation
{
public:
    enum class Mode {
        Keep,
        Remove,
    };

    struct Request {
        QList<cwLazClipSource> sources;
        QPolygonF polygonLocalXY;
        cwGeoPoint worldOrigin;
        QString globalCS;
        Mode mode = Mode::Keep;
        QString outputPath;
    };

    struct Result {
        bool success = false;
        QString errorMessage;
        qint64 pointsWritten = 0;
        QString outputPath;
    };

    static QFuture<Result> run(const Request& request);
};

#endif // CWLAZCLIPOPERATION_H
