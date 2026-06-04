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
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>

//Monad
#include "Monad/Result.h"

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGeoPoint.h"
#include "cwGeometry.h"

/**
 * Polygon-clips a set of cwGeometry point clouds into one output .laz.
 *
 * Polygon and geometry live in worldOrigin-relative coords. The worker
 * projects both through @c viewMatrix to eye space and runs a 2D
 * containment test in eye XY — pan-invariant (translation cancels) and
 * ortho-zoom-invariant (projection matrix not consulted).
 *
 * Sources are implicitly-shared snapshots; main-thread mutation after
 * commit detaches and leaves the worker's view intact. LASwriter is
 * not thread-safe, so all I/O stays on the single worker. Cancellation
 * is checked between point chunks.
 */
class CAVEWHERE_LIB_EXPORT cwLazClipOperation
{
public:
    enum class Mode : int {
        Keep,
        Remove,
    };

    // Stable codes so tests can assert on failure kind without tying to
    // the user-facing message wording.
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
        // worldOrigin-relative; same frame as the LAZ geometry.
        QList<QVector3D> polygonWorldXYZ;
        // Frozen camera view at commit time. Worker projects both polygon
        // vertices and each LAZ point through this matrix for the 2D
        // eye-XY containment test.
        QMatrix4x4 viewMatrix;
        cwGeoPoint worldOrigin;
        // Written into the output's OGC WKT VLR; empty string skips it.
        QString outputWktCS;
        Mode mode = Mode::Keep;
        QString outputPath;
    };

    static QFuture<Result> run(const Request& request);
};

#endif // CWLAZCLIPOPERATION_H
