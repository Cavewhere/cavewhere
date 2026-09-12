/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGEOMETRYBOUNDS_H
#define CWGEOMETRYBOUNDS_H

//Std includes
#include <optional>

//QMath3d includes
#include <QBox3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGeometry.h"

/**
 * Bounding-box math over cwGeometry. Pure and safe to call from any thread, so
 * a producer can measure its mesh on the worker that built it instead of the
 * render thread walking every vertex again at the sync barrier.
 */
namespace cw::geometry {

    /**
     * The union of every position in @a geometry, in the geometry's own space.
     * Returns nullopt when the geometry carries no Position attribute or no
     * vertices — a caller with no box draws rather than risking a wrong cull.
     */
    CAVEWHERE_LIB_EXPORT std::optional<QBox3D> positionBounds(const cwGeometry& geometry);
}

#endif // CWGEOMETRYBOUNDS_H
