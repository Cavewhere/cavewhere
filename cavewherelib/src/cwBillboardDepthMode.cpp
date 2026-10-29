/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBillboardDepthMode.h"
#include "cwBillboardDepthPass.h"

//Qt includes
#include <QMatrix4x4>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>

namespace {
constexpr float kPercentScale = 100.0f;
}

namespace cw::billboard {

cwBillboardDepthMode markerDepthMode(bool depthTest, bool xRay, int xRayOpacityPercent)
{
    return cwBillboardDepthMode{depthTest, xRay, float(xRayOpacityPercent) / kPercentScale};
}

cwBillboardDepthMode defaultMarkerDepthMode()
{
    return markerDepthMode(kDefaultMarkerDepthTest, kDefaultMarkerXRay, kDefaultXRayOpacityPercent);
}

double cameraTiltDegrees(const QMatrix4x4& viewMatrix)
{
    // The view's forward axis in world space is the negated third row of its
    // rotation, so the row's z is the sine of the angle below the horizon.
    const QVector3D forwardRow = viewMatrix.row(2).toVector3D().normalized();
    const double sine = std::clamp(double(forwardRow.z()), -1.0, 1.0);
    return qRadiansToDegrees(std::asin(sine));
}

float rampedXRayOpacity(double tiltDegrees,
                        int baseOpacityPercent,
                        int overheadOpacityPercent,
                        int rampStartTiltDegrees,
                        int rampEndTiltDegrees)
{
    if (tiltDegrees >= rampEndTiltDegrees) {
        return float(overheadOpacityPercent) / kPercentScale;
    }
    if (tiltDegrees <= rampStartTiltDegrees) {
        return float(baseOpacityPercent) / kPercentScale;
    }
    const double t = (tiltDegrees - rampStartTiltDegrees) / double(rampEndTiltDegrees - rampStartTiltDegrees);
    const double percent = baseOpacityPercent + t * (overheadOpacityPercent - baseOpacityPercent);
    return float(percent) / kPercentScale;
}

QVector<DepthPass> depthPasses(const cwBillboardDepthMode& mode)
{
    if (mode.depthTest) {
        const DepthPass visible{true, true, QRhiGraphicsPipeline::Less, 1.0f};
        if (mode.xRay) {
            const DepthPass hidden{true, false, QRhiGraphicsPipeline::Greater, mode.xRayOpacity};
            return {hidden, visible};
        }
        return {visible};
    }
    return {DepthPass{false, false, QRhiGraphicsPipeline::Less, 1.0f}};
}

} // namespace cw::billboard
