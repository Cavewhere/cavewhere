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
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

// Ring, chain and wall helpers shared by cwPassageTubeBuilder,
// cwPassageTubeJunction and cwPassageTubeMeshCheck.
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

// A side reading as a point on the wall, by its fraction along a leg and its
// half-width square to the leg.
struct WallPoint {
    double fraction = 0.0;
    double halfWidth = 0.0;
    double reading = 0.0;
};

struct SideWallPoints {
    std::optional<WallPoint> left;
    std::optional<WallPoint> right;
};

// A record's side readings as points on the wall: the station, at
// stationFraction along a leg one unit of towardFar long, plus each reading
// along the record's right (its horizontal facing turned clockwise) or
// against it for the left. A record facing more than kMaxFacingDegrees off
// the leg, such as a hairpin's chord, would put its points near the
// centerline, so it reads along squareRight, the leg's own right, instead.
inline SideWallPoints sideWallPoints(const cwStationLrud& lrud,
                                     const QVector3D& facing,
                                     double stationFraction,
                                     const QVector3D& towardFar,
                                     const QVector3D& squareRight)
{
    const QVector3D facingRight = QVector3D(facing.y(), -facing.x(), 0.0f).normalized();
    const double component = QVector3D::dotProduct(facingRight, squareRight);
    const bool facesAgainst = component < 0.0;
    const bool readsAlongFacing = std::abs(component) >= std::cos(qDegreesToRadians(cwPassageTubeBuilder::kMaxFacingDegrees));
    const QVector3D recordRight = readsAlongFacing ? facingRight * (facesAgainst ? -1.0f : 1.0f) : squareRight;
    const auto point = [&](const std::optional<double>& reading, float side) -> std::optional<WallPoint> {
        if (!reading) {
            return std::nullopt;
        }
        const QVector3D offset = recordRight * (side * static_cast<float>(*reading));
        return WallPoint {stationFraction + QVector3D::dotProduct(offset, towardFar),
                          QVector3D::dotProduct(offset, squareRight) * side,
                          *reading};
    };
    return {point(facesAgainst ? lrud.right : lrud.left, -1.0f), point(facesAgainst ? lrud.left : lrud.right, 1.0f)};
}

// One side's wall along a leg: the straight line between its two stations'
// reading points, as the half-width square to the leg at each point's
// fraction along it. It runs on across the whole leg and holds beyond the
// leg and the points.
struct SideWall {
    double startFraction = 0.0;
    double startHalfWidth = 0.0;
    double endFraction = 1.0;
    double endHalfWidth = 0.0;

    double at(double fraction) const
    {
        const double clamped = std::clamp(fraction, std::min(startFraction, 0.0), std::max(endFraction, 1.0));
        const double t = (clamped - startFraction) / (endFraction - startFraction);
        return std::max(cwPassageTubeBuilder::kMinRadiusMeters, startHalfWidth + (endHalfWidth - startHalfWidth) * t);
    }
};

// The wall through a leg's two reading points on one side, or between the
// readings at face value at the stations when the points cross over along
// the leg.
inline std::optional<SideWall> sideWallBetween(const std::optional<WallPoint>& start,
                                               const std::optional<WallPoint>& end,
                                               double legLength)
{
    if (!start || !end) {
        return std::nullopt;
    }
    if ((end->fraction - start->fraction) * legLength < kMinRingSpacingMeters) {
        return SideWall {0.0, start->reading, 1.0, end->reading};
    }
    return SideWall {start->fraction, start->halfWidth, end->fraction, end->halfWidth};
}

} // namespace cwPassageTubeGeometry

#endif // CWPASSAGETUBEGEOMETRY_H
