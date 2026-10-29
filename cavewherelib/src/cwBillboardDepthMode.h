/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBILLBOARDDEPTHMODE_H
#define CWBILLBOARDDEPTHMODE_H

//Our includes
#include "CaveWhereLibExport.h"

class QMatrix4x4;

// How a billboard layer meets the scene's depth buffer. The default, depth test
// on and no x-ray, is the single depth-tested pass station labels draw with.
// Marker layers that follow the marker knobs in cwRenderingSettings take theirs
// from them: with x-ray on, the part of a marker hidden behind geometry draws as
// a ghost at xRayOpacity, and with the depth test off every marker draws over
// everything.
struct cwBillboardDepthMode {
    bool depthTest = true;
    bool xRay = false;
    float xRayOpacity = 1.0f;

    // The x-ray pass draws the part geometry hides.
    bool drawsHiddenPass() const { return depthTest && xRay; }

    // The depth test hides a marker behind geometry outright, so it can be
    // neither tapped nor claim declutter space.
    bool hidesOccluded() const { return depthTest && !xRay; }

    bool operator==(const cwBillboardDepthMode&) const = default;
};

namespace cw::billboard {

constexpr bool kDefaultMarkerDepthTest = true;
constexpr bool kDefaultMarkerXRay = true;
constexpr int kDefaultXRayOpacityPercent = 30;
constexpr int kMinXRayOpacityPercent = 5;
constexpr int kMaxXRayOpacityPercent = 100;
constexpr int kDefaultXRayRampStartTiltDegrees = 60;
constexpr int kDefaultXRayRampEndTiltDegrees = 90;
constexpr int kMinXRayRampTiltDegrees = 0;
constexpr int kMaxXRayRampTiltDegrees = 90;
constexpr int kDefaultXRayOverheadOpacityPercent = 100;

// The depth mode the marker knobs describe, with the opacity as a 0-1 factor.
CAVEWHERE_LIB_EXPORT cwBillboardDepthMode markerDepthMode(bool depthTest, bool xRay,
                                                          int xRayOpacityPercent);

// The marker knobs' defaults as a depth mode, for a marker view with no
// cwRenderingSettings to read.
CAVEWHERE_LIB_EXPORT cwBillboardDepthMode defaultMarkerDepthMode();

// The camera's angle below the horizon in degrees, from @a viewMatrix: 90 looks
// straight down (the identity rotation, the turntable's plan view), 0 looks
// level and -90 looks straight up.
CAVEWHERE_LIB_EXPORT double cameraTiltDegrees(const QMatrix4x4& viewMatrix);

// The x-ray ghost's opacity as a 0-1 factor at @a tiltDegrees: the base opacity
// up to rampStartTiltDegrees, the overhead opacity from rampEndTiltDegrees on,
// and a linear blend between. When the start is at or past the end, it steps
// from base to overhead at the end.
CAVEWHERE_LIB_EXPORT float rampedXRayOpacity(double tiltDegrees,
                                             int baseOpacityPercent,
                                             int overheadOpacityPercent,
                                             int rampStartTiltDegrees,
                                             int rampEndTiltDegrees);

} // namespace cw::billboard

#endif // CWBILLBOARDDEPTHMODE_H
