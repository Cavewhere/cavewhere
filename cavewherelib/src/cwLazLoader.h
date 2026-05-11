/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZLOADER_H
#define CWLAZLOADER_H

//Qt includes
#include <QFuture>
#include <QString>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGeometry.h"
#include "cwGeoPoint.h"

/**
 * Result of a single LAZ load. Geometry holds Type::Points with one
 * Position(Vec3) attribute, in worldOrigin-relative coordinates.
 */
struct CAVEWHERE_LIB_EXPORT cwLazLoadResult
{
    cwGeometry geometry;
    QVector3D bboxMin;
    QVector3D bboxMax;
    QString sourceCS; // CS actually used during load (override > LAZ-embedded > "")
};

/**
 * LAZ/LAS file loader.
 *
 * Wraps LAStools' LASlib reader. Single static load() returns a QFuture so the
 * caller can wrap it in a cwFuture and hand it to cwFutureManagerToken::addJob
 * for global progress tracking.
 *
 * Threading: each load() spins a worker on cwConcurrent's thread pool. The
 * worker constructs its own LASreader and cwCoordinateTransform, neither of
 * which is thread-safe across instances — never share the future's worker
 * state with the caller.
 *
 * Cancellation: honored via QPromise::isCanceled() between point chunks.
 */
class CAVEWHERE_LIB_EXPORT cwLazLoader
{
public:
    struct Request {
        QString path;              //!< absolute filesystem path to a .laz / .las file
        QString sourceCSOverride;  //!< empty → use LAZ-embedded CS (or identity)
        QString globalCS;          //!< destination CS for the output points
        cwGeoPoint worldOrigin;    //!< subtracted before narrowing to QVector3D
        qsizetype maxPoints = -1;  //!< stop after this many; -1 means all
    };

    static QFuture<cwLazLoadResult> load(const Request& request);

    /**
     * Header-only probe. Opens the LAZ, reads the embedded CS (OGC WKT) and
     * raw bounding box, then closes — no point iteration, microseconds. Used
     * to auto-adopt the project CS + worldOrigin on the first add to an
     * otherwise empty project.
     */
    struct ProbeResult {
        bool valid = false;        //!< false if the file could not be opened
        QString sourceCS;          //!< empty for older GeoTIFF-only LAZs
        cwGeoPoint bboxMin;        //!< raw LAZ source-CS coordinates
        cwGeoPoint bboxMax;        //!< raw LAZ source-CS coordinates
        cwGeoPoint bboxCenter;     //!< midpoint of bbox in source CS
    };

    static ProbeResult probeHeader(const QString& path);
};

#endif // CWLAZLOADER_H
