/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEGEOMETRY_H
#define CWPASSAGETUBEGEOMETRY_H

//Our includes
#include "cwPassageTubeBuilder.h"

//Qt includes
#include <QString>
#include <QVector>
#include <QVector3D>

//Std includes
#include <algorithm>
#include <cmath>
#include <numbers>

// Ring and chain helpers shared by cwPassageTubeBuilder and
// cwPassageTubeJunction.
namespace cwPassageTubeGeometry {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kMinRingSpacingMeters = 0.05;
constexpr float kMinAxisLengthSquared = 0.25f;
constexpr int kMinLoopRings = 3;
// The most a sharp bend's elbow turns from one ring to the next.
constexpr double kElbowStepDegrees = 30.0;

inline QVector3D binDirection(const cwPassageTubeBuilder::Ring& ring, int bin)
{
    const double theta = ring.binAngles.at(bin);
    return ring.frame.right * static_cast<float>(std::cos(theta)) + ring.frame.up * static_cast<float>(std::sin(theta));
}

inline bool isFinite(const QVector3D& v)
{
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

// A ring whose frame could not be built tears the chain.
inline bool hasValidFrame(const cwPassageTubeBuilder::Ring& ring)
{
    return isFinite(ring.center)
           && isFinite(ring.frame.tangent)
           && isFinite(ring.frame.up)
           && isFinite(ring.frame.right)
           && ring.frame.right.lengthSquared() > kMinAxisLengthSquared;
}

// Places the ring's vertices. A vertex that would land behind the previous
// ring's plane, folding the tube inside out on the inside of a bend, slides
// along its own ray onto that plane.
inline void placeRingVertices(cwPassageTubeBuilder::Ring& ring, const cwPassageTubeBuilder::Ring* previous)
{
    const int resolution = ring.radii.size();
    ring.vertices.resize(resolution);
    for (int bin = 0; bin < resolution; ++bin) {
        const QVector3D direction = binDirection(ring, bin);
        double radius = ring.radii.at(bin);
        if (previous != nullptr) {
            const QVector3D& planeNormal = previous->frame.tangent;
            const QVector3D vertex = ring.center + direction * static_cast<float>(radius);
            if (QVector3D::dotProduct(vertex - previous->center, planeNormal) < 0.0f) {
                const float approach = QVector3D::dotProduct(direction, planeNormal);
                const double toPlane = approach < 0.0f
                                           ? QVector3D::dotProduct(previous->center - ring.center, planeNormal) / approach
                                           : 0.0;
                radius = std::max(toPlane, cwPassageTubeBuilder::kMinRadiusMeters);
            }
        }
        ring.vertices[bin] = ring.center + direction * static_cast<float>(radius);
    }
}

// Places every ring's vertices. A loop's last ring repeats its first, so the
// first ring folds against the ring before that one.
inline void placeChainVertices(cwPassageTubeBuilder::Chain& chain)
{
    const bool isLoop = chain.startCap == cwPassageTubeBuilder::EndCap::None && chain.rings.size() >= kMinLoopRings;
    const cwPassageTubeBuilder::Ring* previous = nullptr;
    if (isLoop) {
        const cwPassageTubeBuilder::Ring& closing = chain.rings.at(chain.rings.size() - 2);
        if (hasValidFrame(closing)) {
            previous = &closing;
        }
    }
    for (cwPassageTubeBuilder::Ring& ring : chain.rings) {
        if (hasValidFrame(ring)) {
            placeRingVertices(ring, previous);
            previous = &ring;
        } else {
            ring.vertices.clear();
        }
    }
}

inline const QString& chainStartStation(const QVector<cwPassageTubeBuilder::Leg>& legs,
                                        const cwPassageTubeBuilder::ChainLeg& chainLeg)
{
    const cwPassageTubeBuilder::Leg& leg = legs.at(chainLeg.legIndex);
    return chainLeg.reversed ? leg.to : leg.from;
}

inline const QString& chainEndStation(const QVector<cwPassageTubeBuilder::Leg>& legs,
                                      const cwPassageTubeBuilder::ChainLeg& chainLeg)
{
    const cwPassageTubeBuilder::Leg& leg = legs.at(chainLeg.legIndex);
    return chainLeg.reversed ? leg.from : leg.to;
}

} // namespace cwPassageTubeGeometry

#endif // CWPASSAGETUBEGEOMETRY_H
