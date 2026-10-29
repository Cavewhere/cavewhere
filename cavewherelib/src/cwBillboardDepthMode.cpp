/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBillboardDepthMode.h"
#include "cwBillboardDepthPass.h"

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
