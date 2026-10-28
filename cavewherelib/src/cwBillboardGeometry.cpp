/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBillboardGeometry.h"
#include "cwCamera.h"
#include "cwGeometryItersecter.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"

//Qt includes
#include <QRay3D>
#include <QRectF>
#include <QVector4D>

namespace {

// World units per content pixel for WorldSized billboards: the quad scales with
// the 3D scene, like a QtQuick3D Item2D under a Node.
constexpr float kWorldPerPixel = 0.02f;

// Floors guarding the screen-constant size division: clamp the perspective w and
// the projection scales away from zero so a billboard crossing the eye plane (or
// a degenerate projection) can't produce an infinite scale.
constexpr float kMinClipW = 1e-4f;
constexpr float kMinProjectionScale = 1e-6f;

// Element (3, 2) — the one that divides by view-space depth — is 0 for an
// orthographic projection and -1 for a perspective one. cwProjectionTransition
// scrubs between the two with an element-wise lerp, so this midpoint classifies a
// blended frame by whichever it is closer to. Same test cwEDLEffect uses on the
// same render thread.
constexpr float kPerspectiveDepthDivide = -0.5f;

// True when @a projection is orthographic. The clip-space correction
// cwRhiFrameRenderer premultiplies leaves the bottom row alone, so this holds for
// the corrected matrices the render data carries.
bool isOrthographic(const QMatrix4x4& projection)
{
    return projection(3, 2) >= kPerspectiveDepthDivide;
}

} // namespace

cwBillboardSightLine::cwBillboardSightLine(const QMatrix4x4& view, const QMatrix4x4& projection) :
    // The camera's +Z axis in world space (the view matrix's third row): the
    // direction the eye looks *from*.
    m_towardViewer(view(2, 0), view(2, 1), view(2, 2)),
    m_orthographic(isOrthographic(projection))
{
    if (!m_orthographic) {
        m_eye = view.inverted().column(3).toVector3D();
    }
}

QVector3D cwBillboardSightLine::biasedPosition(const QVector3D& worldPosition,
                                               float depthBias) const
{
    if (depthBias == 0.0f) {
        return worldPosition;
    }

    if (m_orthographic) {
        return worldPosition + m_towardViewer * depthBias;
    }

    const QVector3D toEye = m_eye - worldPosition;
    if (toEye.isNull()) {
        return worldPosition;
    }
    return worldPosition + toEye.normalized() * depthBias;
}

namespace cw::billboard {

QVector2D worldPerPixel(const QVector3D& worldPosition,
                        cwBillboardSizeMode sizeMode,
                        const QMatrix4x4& projection,
                        const QMatrix4x4& viewProjection,
                        const QSizeF& logicalViewport)
{
    if (sizeMode == cwBillboardSizeMode::WorldSized) {
        return QVector2D(kWorldPerPixel, kWorldPerPixel);
    }

    // Invert PointCloud.vert's world-radius -> pixels projection so one content
    // pixel maps to one logical screen pixel at this depth. Callers pass the
    // unbiased position so the size doesn't drift with depthBias.
    const QVector4D clip = viewProjection * QVector4D(worldPosition, 1.0f);
    const float w = qMax(qAbs(clip.w()), kMinClipW);
    const float projX = qMax(qAbs(projection(0, 0)), kMinProjectionScale);
    const float projY = qMax(qAbs(projection(1, 1)), kMinProjectionScale);
    return QVector2D(2.0f * w / (projX * float(logicalViewport.width())),
                     2.0f * w / (projY * float(logicalViewport.height())));
}

// Columns 0/1/2 are the camera basis (from the view matrix rows) so the quad
// faces the camera; column 1 is negated so the content's y-down pixel space maps
// to world-down. Column 3 places it at the world position, pulled toward the
// viewer by the depth bias so a billboard sitting on a wall isn't self-occluded.
// The trailing translate centers the content on that position, shifted by the
// y-up pixel offset in the y-down content space.
QMatrix4x4 buildModelMatrix(const QVector3D& worldPosition,
                            const QSizeF& contentSize,
                            cwBillboardSizeMode sizeMode,
                            float depthBias,
                            const QMatrix4x4& view,
                            const QMatrix4x4& projection,
                            const QMatrix4x4& viewProjection,
                            const QSizeF& logicalViewport,
                            const cwBillboardSightLine& sightLine,
                            const QPointF& pixelOffset)
{
    const QVector3D right(view(0, 0), view(0, 1), view(0, 2));
    const QVector3D up(view(1, 0), view(1, 1), view(1, 2));
    const QVector3D forward(view(2, 0), view(2, 1), view(2, 2));

    const QVector3D position = sightLine.biasedPosition(worldPosition, depthBias);
    const QVector2D scale = worldPerPixel(worldPosition, sizeMode, projection,
                                          viewProjection, logicalViewport);

    QMatrix4x4 model;
    model.setColumn(0, QVector4D(right * scale.x(), 0.0f));
    model.setColumn(1, QVector4D(-up * scale.y(), 0.0f));
    model.setColumn(2, QVector4D(forward, 0.0f));
    model.setColumn(3, QVector4D(position, 1.0f));
    model.translate(float(pixelOffset.x() - contentSize.width() / 2.0),
                    float(-pixelOffset.y() - contentSize.height() / 2.0),
                    0.0f);
    return model;
}

FrameCamera::FrameCamera(const QMatrix4x4& view,
                         const QMatrix4x4& projection,
                         const QMatrix4x4& viewProjection,
                         const QSize& pixelSize,
                         float devicePixelRatio) :
    m_view(view),
    m_projection(projection),
    m_viewProjection(viewProjection),
    m_logicalViewport(pixelSize.width() / devicePixelRatio,
                      pixelSize.height() / devicePixelRatio),
    m_sightLine(view, projection)
{
}

QMatrix4x4 FrameCamera::modelMatrix(const QVector3D& worldPosition,
                                    const QSizeF& contentSize,
                                    cwBillboardSizeMode sizeMode,
                                    float depthBias,
                                    const QPointF& pixelOffset) const
{
    return buildModelMatrix(worldPosition, contentSize, sizeMode, depthBias,
                            m_view, m_projection, m_viewProjection,
                            m_logicalViewport, m_sightLine, pixelOffset);
}

bool isOccluded(const cwGeometryItersecter* intersecter,
                const cwCamera* camera,
                const QVector3D& worldPosition,
                const QPointF& tapViewportPosition,
                const QSizeF& spriteSize,
                const QPointF& anchor,
                float depthBias)
{
    if(intersecter == nullptr || camera == nullptr) {
        return false;
    }

    if(!spriteSize.isEmpty()) {
        const QPointF anchorOffset(anchor.x() * spriteSize.width(),
                                   anchor.y() * spriteSize.height());
        const QRectF quad(camera->project(worldPosition) - anchorOffset, spriteSize);
        if(!quad.contains(tapViewportPosition)) {
            return true;
        }
    }

    const QRay3D ray = camera->rayFromQtViewport(tapViewportPosition);

    // The sprite is a camera-facing quad: every pixel of it, wherever the anchor
    // places it, lies on the plane through worldPosition whose normal is the
    // camera's view direction (third row of the view matrix). A single
    // eye->center ray can't match the per-pixel silhouette the depth buffer clips
    // against, so the depth is taken where the tap ray crosses that plane.
    const QMatrix4x4 view = camera->viewMatrix();
    const QVector3D planeNormal(view(2, 0), view(2, 1), view(2, 2));
    const double denom = QVector3D::dotProduct(ray.direction(), planeNormal);
    if(qFuzzyIsNull(denom)) {
        return false;
    }
    // ray.direction() is unit length (cwCamera::frustumRay normalizes it), so
    // this parameter is a world distance comparable to cwRayHit::tWorld(). The
    // depthBias slack mirrors the line-of-sight shift the billboard renders with
    // (cwBillboardSightLine).
    const double quadDistance =
        QVector3D::dotProduct(worldPosition - ray.origin(), planeNormal) / denom;
    if(quadDistance <= 0.0) {
        return false;
    }

    // Only solid geometry hides a sprite; one behind the centerline still shows.
    const cwRayHit hit = intersecter->intersectsDetailed(ray, cwPickQuery{.kinds = cwPickQuery::Solid});
    if(!hit.hit()) {
        return false;
    }

    return hit.tWorld() < quadDistance - depthBias;
}

float sortDepth(const QVector3D& worldPosition, const QMatrix4x4& viewProjection)
{
    // The clamped |w| matches worldPerPixel so a billboard near the eye plane
    // can't produce a degenerate key.
    const QVector4D clip = viewProjection * QVector4D(worldPosition, 1.0f);
    return clip.z() / qMax(qAbs(clip.w()), kMinClipW);
}

} // namespace cw::billboard
