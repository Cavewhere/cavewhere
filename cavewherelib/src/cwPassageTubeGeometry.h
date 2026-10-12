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
#include <cmath>
#include <numbers>

// Ring and chain helpers shared by cwPassageTubeBuilder and
// cwPassageTubeJunction.
namespace cwPassageTubeGeometry {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kMinRingSpacingMeters = 0.05;
constexpr float kMinAxisLengthSquared = 0.25f;

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
