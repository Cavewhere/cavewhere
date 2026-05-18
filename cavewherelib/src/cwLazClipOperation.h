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

//Monad
#include "Monad/Result.h"

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGeoPoint.h"
#include "cwGeometry.h"

/**
 * Polygon-clips a set of already-loaded point-cloud geometries into a
 * single output .laz.
 *
 * Sources are @c cwGeometry snapshots (implicitly shared — copy is a
 * header bump). Each is expected to carry a Vec3 Position attribute in
 * worldOrigin-relative project coordinates, which is what
 * cwLazLoader produces. Each vertex is tested against @c polygonLocalXY
 * (same worldOrigin-relative XY space) via Qt::OddEvenFill.
 *
 * - Mode::Keep retains points inside the polygon.
 * - Mode::Remove retains points outside.
 *
 * Output is LAS 1.2 point format 0 (XYZ only). Vertices are converted
 * back to absolute coords by adding @c worldOrigin before encoding, so
 * the output stays geographic. The output's OGC WKT VLR is set to
 * @c outputWktCS so cwLazLoader round-trips the result as project-CS
 * data.
 *
 * Threading: one worker on cwConcurrent's thread pool. LASwriter is
 * not thread-safe; all I/O stays on that one worker. cwGeometry is
 * implicitly shared and read-only on the worker, so concurrent main-
 * thread mutation of the original layer's geometry detaches into a new
 * buffer and leaves the worker's view untouched. Cancellation is
 * honored between point chunks via QPromise::isCanceled().
 *
 * Result is a @c Monad::Result<SuccessValue> — on success carries
 * @c {pointsWritten, outputPath}; on failure carries an @c ErrorCode
 * (test-stable) plus a human-readable message.
 */
class CAVEWHERE_LIB_EXPORT cwLazClipOperation
{
public:
    enum class Mode : int {
        Keep,
        Remove,
    };

    // Stable error codes for callers that want to react programmatically
    // (e.g. tests asserting on the kind of failure without depending on
    // exact wording of the user-facing message).
    enum ErrorCode : int {
        NoSources = Monad::ResultBase::CustomError,
        BadPolygon,
        EmptyOutputPath,
        NonFiniteInput,
        MissingPosition,
        WriterOpenFailed,
        WritePointFailed,
        FinalizeFailed,
        Cancelled,
    };

    struct SuccessValue {
        qint64 pointsWritten = 0;
        QString outputPath;
    };

    using Result = Monad::Result<SuccessValue>;

    struct Request {
        QList<cwGeometry> sources;
        QPolygonF polygonLocalXY;
        cwGeoPoint worldOrigin;
        // OGC WKT written into the output's VLR. Caller's choice; an empty
        // string skips the VLR. cwLazLoader uses this to re-anchor the
        // loaded result in the project's CS on the round-trip.
        QString outputWktCS;
        Mode mode = Mode::Keep;
        QString outputPath;
    };

    static QFuture<Result> run(const Request& request);
};

#endif // CWLAZCLIPOPERATION_H
