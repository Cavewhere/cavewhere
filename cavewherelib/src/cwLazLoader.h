/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLAZLOADER_H
#define CWLAZLOADER_H

//Qt includes
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGeoPoint.h"

class LASheader;

/**
 * Header-level reader for LAZ/LAS files, wrapping LAStools' LASlib.
 *
 * Points reach the renderer through cwPointOctreeBuilder; this class answers
 * only the header-level questions — where the file sits and what CRS it is in.
 * Both calls read the header alone and return synchronously.
 */
class CAVEWHERE_LIB_EXPORT cwLazLoader
{
public:
    /**
     * Header-only probe. Opens the LAZ, reads the embedded CS and
     * raw bounding box, then closes — no point iteration, microseconds.
     *
     * This is how a project whose only georeferenced input is a point cloud
     * gets a frame at all: the octree builder transforms points into the
     * project's frame, so the frame has to exist before the build can run, but
     * the frame is derived from the cloud's own coordinates. The probe breaks
     * the cycle by reading the position out of the header without loading
     * anything. cwLazLayer runs one per layer off the GUI thread, including
     * for layers that are disabled and will never build an octree.
     */
    struct ProbeResult {
        bool valid = false;        //!< false if the file could not be opened
        QString sourceCS;          //!< empty when the LAZ names no CRS
        cwGeoPoint bboxMin;        //!< raw LAZ source-CS coordinates
        cwGeoPoint bboxMax;        //!< raw LAZ source-CS coordinates
    };

    static ProbeResult probeHeader(const QString& path);

    /**
     * Resolves the source CRS for a LAZ file: explicit @a override wins;
     * otherwise the LAZ's embedded OGC WKT VLR (if present); otherwise the
     * GeoTIFF GeoKeys as "EPSG:<code>"; otherwise empty (identity).
     */
    static QString resolveSourceCS(const QString& override, const LASheader& header);
};

#endif // CWLAZLOADER_H
