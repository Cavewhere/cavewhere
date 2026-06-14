#ifndef PROJECTIONSCALETESTHELPERS_H
#define PROJECTIONSCALETESTHELPERS_H

//Std
#include <cmath>

// Visible-half-height conversions: ortho shows (viewportHeight/2) * zoomScale,
// perspective shows distance * tan(fov/2). These invert
// cwBaseTurnTableInteraction::reconcileZoomForProjection, so tests can predict
// the matched channel after a projection swap. Shared by
// test_cwBaseTurnTableInteraction (the reconcile math) and
// test_cwProjectionTransition (the transition that drives it).
inline double matchedPerspectiveDistance(double zoomScale, double fovYDegrees, double viewportHeight)
{
    const double tanHalfFov = std::tan(0.5 * fovYDegrees * M_PI / 180.0);
    return (viewportHeight / 2.0) * zoomScale / tanHalfFov;
}

inline double matchedOrthoZoom(double distance, double fovYDegrees, double viewportHeight)
{
    const double tanHalfFov = std::tan(0.5 * fovYDegrees * M_PI / 180.0);
    return distance * tanHalfFov / (viewportHeight / 2.0);
}

#endif // PROJECTIONSCALETESTHELPERS_H
