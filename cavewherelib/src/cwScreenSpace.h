#ifndef CWSCREENSPACE_H
#define CWSCREENSPACE_H

// Qt includes
#include <QMatrix4x4>

// QMath3d includes
#include <QBox3D>

// Our includes
#include "CaveWhereLibExport.h"

/**
 * The projected-pixel-size primitive shared by texture mip selection and point
 * cloud node selection: a world length of L meters sitting at clip depth w
 * covers L · |P(1, 1)| · viewportHeightPx / (2 · w) pixels.
 *
 * Orthographic cameras have w = 1 everywhere by construction — the clip
 * corrected matrices from cwRhiFrameRenderer::clipSpaceCorrectedCamera() keep
 * row 3 of an orthographic projection as (0, 0, 0, 1) — so one formula serves
 * both projections. Every function here is pure and safe to call from any
 * thread.
 */
namespace cw::sse {

    //Keeps pixelsPerMeter() finite for geometry that straddles or sits behind the eye
    constexpr double kMinimumClipW = 1e-4;

    /**
     * The smallest positive clip-space w over the box's 8 corners, floored at
     * kMinimumClipW. A box entirely behind the eye returns kMinimumClipW.
     */
    CAVEWHERE_LIB_EXPORT double nearestClipW(const QBox3D& worldBounds,
                                             const QMatrix4x4& viewProjection);

    //How many pixels one meter covers at clip depth clipW
    CAVEWHERE_LIB_EXPORT double pixelsPerMeter(double absP11,
                                               int viewportHeightPx,
                                               double clipW);

    //How many pixels worldLength meters cover at the nearest corner of worldBounds
    CAVEWHERE_LIB_EXPORT double projectedPixels(double worldLength,
                                                const QBox3D& worldBounds,
                                                const QMatrix4x4& viewProjection,
                                                double absP11,
                                                int viewportHeightPx);
}

#endif // CWSCREENSPACE_H
