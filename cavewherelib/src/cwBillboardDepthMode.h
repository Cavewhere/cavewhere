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

// The depth mode the marker knobs describe, with the opacity as a 0-1 factor.
CAVEWHERE_LIB_EXPORT cwBillboardDepthMode markerDepthMode(bool depthTest, bool xRay,
                                                          int xRayOpacityPercent);

// The marker knobs' defaults as a depth mode, for a marker view with no
// cwRenderingSettings to read.
CAVEWHERE_LIB_EXPORT cwBillboardDepthMode defaultMarkerDepthMode();

} // namespace cw::billboard

#endif // CWBILLBOARDDEPTHMODE_H
