/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBILLBOARDGEOMETRY_H
#define CWBILLBOARDGEOMETRY_H

// Placement math shared by every billboard backend: the QQuickItem layer
// (cwRenderBillboards, leads) and the text layer (cwRenderTextBillboards,
// station labels). Labels and leads sit in the same 3D space, so both build
// their model matrices, sizes and draw order from these helpers and agree on
// where a billboard lands. Pure (no QRhi), so it's unit-tested directly.

//Our includes
#include "CaveWhereLibExport.h"

//Qt includes
#include <QMatrix4x4>
#include <QPointF>
#include <QSize>
#include <QSizeF>
#include <QVector>
#include <QVector2D>
#include <QVector3D>

//Std includes
#include <algorithm>

class cwCamera;
class cwGeometryItersecter;

enum class cwBillboardSizeMode {
    WorldSized,      // scales with the 3D scene (a fixed world size)
    ScreenConstant,  // fixed pixel size on screen regardless of distance
};

// The camera's line of sight, resolved once per frame so biasing each billboard
// costs no matrix work.
//
// Which line it is depends on the projection: under perspective it runs from the
// billboard to the eye point, but under a parallel projection every billboard
// shares one direction — the view axis — and the eye point is an arbitrary point
// on it (the turntable parks it ~50 m from the pivot at any cave size). Offsetting
// orthographic labels toward that point is what made them drown in the scraps,
// issue #645. Either way the offset is screen-invariant.
class CAVEWHERE_LIB_EXPORT cwBillboardSightLine
{
public:
    cwBillboardSightLine(const QMatrix4x4& view, const QMatrix4x4& projection);

    // Shifts @a worldPosition toward the viewer by @a depthBias world units, so
    // nearby geometry (a scrap surface the station sits on, the centerline) doesn't
    // draw over the billboard's content.
    QVector3D biasedPosition(const QVector3D& worldPosition, float depthBias) const;

private:
    QVector3D m_towardViewer;  // orthographic: the direction every billboard shares
    QVector3D m_eye;           // perspective: the point every billboard aims at
    bool m_orthographic;
};

namespace cw::billboard {

// World units per content pixel along the camera's right (x) and up (y) axes for
// a billboard at @a worldPosition. WorldSized is a fixed scale; ScreenConstant
// inverts the projection so one content pixel covers one logical screen pixel at
// that depth.
CAVEWHERE_LIB_EXPORT QVector2D worldPerPixel(const QVector3D& worldPosition,
                                             cwBillboardSizeMode sizeMode,
                                             const QMatrix4x4& projection,
                                             const QMatrix4x4& viewProjection,
                                             const QSizeF& logicalViewport);

// The billboard's model matrix: a camera-facing quad of @a contentSize content
// pixels centered on @a worldPosition, pulled toward the viewer by @a depthBias
// along @a sightLine, then shifted on screen by @a pixelOffset (content pixels,
// y-up). Content pixel space is y-down.
CAVEWHERE_LIB_EXPORT QMatrix4x4 buildModelMatrix(const QVector3D& worldPosition,
                                                 const QSizeF& contentSize,
                                                 cwBillboardSizeMode sizeMode,
                                                 float depthBias,
                                                 const QMatrix4x4& view,
                                                 const QMatrix4x4& projection,
                                                 const QMatrix4x4& viewProjection,
                                                 const QSizeF& logicalViewport,
                                                 const cwBillboardSightLine& sightLine,
                                                 const QPointF& pixelOffset = QPointF());

// The camera state every billboard in one gather shares, resolved once per frame
// (building the sight line inverts a matrix under perspective) so placing each
// billboard costs only its own model matrix. Both backends construct it from the
// render job's matrices, target pixel size and device pixel ratio.
class CAVEWHERE_LIB_EXPORT FrameCamera
{
public:
    FrameCamera(const QMatrix4x4& view,
                const QMatrix4x4& projection,
                const QMatrix4x4& viewProjection,
                const QSize& pixelSize,
                float devicePixelRatio);

    const QMatrix4x4& viewProjection() const { return m_viewProjection; }

    // buildModelMatrix with this frame's camera and viewport.
    QMatrix4x4 modelMatrix(const QVector3D& worldPosition,
                           const QSizeF& contentSize,
                           cwBillboardSizeMode sizeMode,
                           float depthBias,
                           const QPointF& pixelOffset = QPointF()) const;

private:
    QMatrix4x4 m_view;
    QMatrix4x4 m_projection;
    QMatrix4x4 m_viewProjection;
    QSizeF m_logicalViewport;
    cwBillboardSightLine m_sightLine;
};

// True when no drawn pixel of the camera-facing sprite at @a worldPosition lies
// under @a tapViewportPosition (a Qt-viewport pixel, top-left origin): the tap
// falls outside the sprite's quad of @a spriteSize logical pixels placed by
// @a anchor, or solid geometry in @a intersecter is nearer than the sprite's
// plane, less @a depthBias world units, at that pixel. @a anchor is normalized
// content space, y-down, like cwRenderSpriteBillboards::anchor. An empty
// @a spriteSize tests the plane only. False without a camera or intersecter.
CAVEWHERE_LIB_EXPORT bool isOccluded(const cwGeometryItersecter* intersecter,
                                     const cwCamera* camera,
                                     const QVector3D& worldPosition,
                                     const QPointF& tapViewportPosition,
                                     const QSizeF& spriteSize,
                                     const QPointF& anchor,
                                     float depthBias);

// The NDC depth of @a worldPosition through @a viewProjection, the key the
// back-to-front sort orders by. Larger is farther from the eye.
CAVEWHERE_LIB_EXPORT float sortDepth(const QVector3D& worldPosition,
                                     const QMatrix4x4& viewProjection);

// Sorts render slots back-to-front (farthest from the eye first). Every quad is a
// parallel, camera-facing plane, so center-depth order equals per-pixel order
// everywhere on the quad. Fills each slot's sortDepth scratch once so the
// comparator only compares keys, and breaks depth ties by ascending id so the order
// is total and deterministic. In-place, with no heap allocation.
//
// Slot needs `worldPosition` (QVector3D), `sortDepth` (float) and an `id` with
// operator<.
template <typename Slot>
void sortBackToFront(QVector<Slot>& renderSlots, const QMatrix4x4& viewProjection)
{
    if (renderSlots.size() < 2) {
        return;
    }

    for (Slot& slot : renderSlots) {
        slot.sortDepth = sortDepth(slot.worldPosition, viewProjection);
    }

    std::sort(renderSlots.begin(), renderSlots.end(),
              [](const Slot& a, const Slot& b) {
        if (a.sortDepth != b.sortDepth) {
            return a.sortDepth > b.sortDepth;  // farthest first (back-to-front)
        }
        return a.id < b.id;
    });
}

} // namespace cw::billboard

#endif // CWBILLBOARDGEOMETRY_H
