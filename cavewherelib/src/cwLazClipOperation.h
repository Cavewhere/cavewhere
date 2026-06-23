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

/// One on-disk LAZ/LAS source to clip. The worker reopens the file and
/// streams it point-by-point — a multi-GB cloud never lands in memory in
/// full, and every source attribute (intensity, RGB, GPS time, returns,
/// classification, …) is available to carry into the output.
struct CAVEWHERE_LIB_EXPORT cwLazClipSource {
    QString sourcePath;
    QString sourceCSOverride; //!< empty → use the LAZ-embedded CS (or identity)
};

/**
 * Polygon-clips a set of on-disk LAZ point clouds into one output .laz.
 *
 * Each source is reopened via LASreader and streamed: a point is reprojected
 * from its source CS to @c outputWktCS, shifted to worldOrigin-relative
 * coords, then projected through @c viewMatrix to eye space for a 2D
 * containment test in eye XY — pan-invariant (translation cancels) and
 * ortho-zoom-invariant (projection matrix not consulted). Streaming, not an
 * in-memory snapshot, keeps a huge cloud off the heap and preserves the full
 * point record.
 *
 * Kept points pass through with all standard LAS attributes intact: the
 * output point format is the richest among the sources and every source point
 * upconverts into it (LASpoint::operator= bridges legacy↔extended), so no
 * intensity/RGB/GPS-time/return/classification data is lost. Only the XYZ is
 * re-encoded (reprojected + re-anchored on worldOrigin). Custom extra-byte
 * attributes survive only when the sources share one schema.
 *
 * Threading: one worker on cwConcurrent's pool. LASreader, LASwriter and
 * cwCoordinateTransform are each non-thread-safe, so all I/O stays on that
 * worker. Cancellation is checked between point chunks.
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
        SourceOpenFailed,
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
        QList<cwLazClipSource> sources;
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
