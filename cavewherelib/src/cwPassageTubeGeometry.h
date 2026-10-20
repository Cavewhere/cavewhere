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
#include <array>
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
constexpr float kMinInPlaneLength = 1e-6f;

inline QVector3D binDirection(const cwPassageTubeBuilder::Ring& ring, int bin)
{
    const double theta = ring.binAngles.at(bin);
    return ring.frame.right * static_cast<float>(std::cos(theta)) + ring.frame.up * static_cast<float>(std::sin(theta));
}

// Where the ring's bin lies: radii[bin] along its direction in the ring's
// plane, offsetsAlong[bin] off it.
inline QVector3D ringPoint(const cwPassageTubeBuilder::Ring& ring, int bin)
{
    const double along = ring.offsetsAlong.isEmpty() ? 0.0 : ring.offsetsAlong.at(bin);
    return ring.center + binDirection(ring, bin) * static_cast<float>(ring.radii.at(bin))
           + ring.frame.tangent * static_cast<float>(along);
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

// Makes vertices the ring's, around its center: its frame faces along
// tangent with right toward vertex 0, or toward rightHint where vertex 0
// lies on the tangent, and radii, binAngles and offsetsAlong place each
// vertex. A vertex whose angle would fall behind the previous one's takes
// that angle, so the angles keep rising.
inline void setRingVertices(cwPassageTubeBuilder::Ring& ring,
                            const QVector<QVector3D>& vertices,
                            const QVector3D& tangent,
                            const QVector3D& rightHint,
                            double inclinationWeight)
{
    const QVector3D unitTangent = tangent.normalized();
    const auto inPlane = [&](const QVector3D& v) {
        return v - unitTangent * QVector3D::dotProduct(v, unitTangent);
    };
    QVector3D right = inPlane(vertices.first() - ring.center);
    if (right.length() < kMinInPlaneLength) {
        right = inPlane(rightHint);
    }
    right.normalize();
    const QVector3D up = QVector3D::crossProduct(right, unitTangent);
    ring.frame = cwPassageTubeBuilder::Frame {unitTangent, up, right, inclinationWeight};

    const int resolution = vertices.size();
    ring.vertices = vertices;
    ring.radii.resize(resolution);
    ring.binAngles.resize(resolution);
    ring.offsetsAlong.resize(resolution);
    double previousAngle = 0.0;
    for (int bin = 0; bin < resolution; ++bin) {
        const QVector3D offset = vertices.at(bin) - ring.center;
        const double x = QVector3D::dotProduct(offset, right);
        const double y = QVector3D::dotProduct(offset, up);
        const double angle = bin == 0 ? 0.0 : std::atan2(y, x);
        const double wrapped = angle < 0.0 ? angle + kTwoPi : angle;
        previousAngle = std::max(previousAngle, wrapped);
        ring.binAngles[bin] = previousAngle;
        ring.radii[bin] = std::hypot(x, y);
        ring.offsetsAlong[bin] = QVector3D::dotProduct(offset, unitTangent);
    }
}

// Places the ring's vertices where its radii, angles and offsets put them.
// A vertex that would land behind the same bin's vertex of the previous ring,
// along that ring's tangent, folds the tube inside out where a junction grew
// a column; it slides along its own ray until it lies level with that vertex.
// Two rings at one station, the sections its legs read there, keep their
// vertices where the sections put them.
inline void placeRingVertices(cwPassageTubeBuilder::Ring& ring, const cwPassageTubeBuilder::Ring* previous)
{
    const int resolution = ring.radii.size();
    ring.vertices.resize(resolution);
    const bool isApartFromPrevious = previous != nullptr && previous->center.distanceToPoint(ring.center) >= kMinRingSpacingMeters;
    for (int bin = 0; bin < resolution; ++bin) {
        QVector3D vertex = ringPoint(ring, bin);
        if (isApartFromPrevious && bin < previous->vertices.size()) {
            const QVector3D& planeNormal = previous->frame.tangent;
            const QVector3D& previousVertex = previous->vertices.at(bin);
            if (QVector3D::dotProduct(vertex - previousVertex, planeNormal) < 0.0f) {
                const QVector3D direction = binDirection(ring, bin);
                const float approach = QVector3D::dotProduct(direction, planeNormal);
                const QVector3D offAxis = vertex - direction * static_cast<float>(ring.radii.at(bin));
                const double toPlane = approach < 0.0f
                                           ? QVector3D::dotProduct(previousVertex - offAxis, planeNormal) / approach
                                           : 0.0;
                vertex = offAxis + direction * static_cast<float>(std::max(toPlane, cwPassageTubeBuilder::kMinRadiusMeters));
            }
        }
        ring.vertices[bin] = vertex;
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

constexpr float kMinPlanFacingLength = 0.01f;
constexpr QVector3D kFallbackFacing(0.0f, 1.0f, 0.0f);

// The level direction a quarter turn clockwise of direction, seen from above.
inline QVector3D clockwiseOf(const QVector3D& direction)
{
    return QVector3D(direction.y(), -direction.x(), 0.0f).normalized();
}

// The plan part of the leg's unit direction from → to.
inline QVector3D planDirectionOf(const cwPassageTubeBuilder::Leg& leg)
{
    const QVector3D along = (leg.toPosition - leg.fromPosition).normalized();
    return QVector3D(along.x(), along.y(), 0.0f);
}

// The plan direction each station of the chain faces, in walk order, one per
// station: the bisector of the plan directions of the unit legs either side,
// square to the arriving leg where the two double back, or the one leg's plan
// direction at a chain end. A loop's first and last stations are one
// station. A station whose legs run plumb takes the facing of the nearest
// station along the chain that has one, or north.
inline QVector<QVector3D> chainFacings(const QVector<cwPassageTubeBuilder::Leg>& legs,
                                       const cwPassageTubeBuilder::Chain& chain)
{
    const int legCount = static_cast<int>(chain.legs.size());
    const bool isLoop = chain.startCap == cwPassageTubeBuilder::EndCap::None;
    QVector<QVector3D> directions;
    directions.reserve(legCount);
    for (const cwPassageTubeBuilder::ChainLeg& chainLeg : chain.legs) {
        const QVector3D plan = planDirectionOf(legs.at(chainLeg.legIndex));
        directions.append(chainLeg.reversed ? -plan : plan);
    }

    QVector<std::optional<QVector3D>> planFacings(legCount + 1);
    for (int s = 0; s <= legCount; ++s) {
        const bool hasIn = s > 0 || isLoop;
        const bool hasOut = s < legCount || isLoop;
        const QVector3D in = hasIn ? directions.at(s > 0 ? s - 1 : legCount - 1) : QVector3D();
        const QVector3D out = hasOut ? directions.at(s < legCount ? s : 0) : QVector3D();
        const QVector3D sum = in + out;
        if (sum.length() >= kMinPlanFacingLength) {
            planFacings[s] = sum.normalized();
        } else if (hasIn && hasOut && in.length() >= kMinPlanFacingLength) {
            planFacings[s] = clockwiseOf(in);
        }
    }

    QVector<QVector3D> facings;
    facings.reserve(legCount + 1);
    for (int s = 0; s <= legCount; ++s) {
        QVector3D facing = kFallbackFacing;
        for (int offset = 0; offset <= legCount; ++offset) {
            const int before = s - offset;
            const int after = s + offset;
            if (before >= 0 && planFacings.at(before)) {
                facing = *planFacings.at(before);
                break;
            }
            if (after <= legCount && planFacings.at(after)) {
                facing = *planFacings.at(after);
                break;
            }
        }
        facings.append(facing);
    }
    return facings;
}

// The facing at the from and to station of the chain's leg i, given the
// chain's stationFacings: turned around where the chain walks it backward.
inline std::array<QVector3D, 2> chainLegFacings(const cwPassageTubeBuilder::Chain& chain,
                                                const QVector<QVector3D>& stationFacings,
                                                int i)
{
    const QVector3D& start = stationFacings.at(i);
    const QVector3D& end = stationFacings.at(i + 1);
    if (chain.legs.at(i).reversed) {
        return {-end, -start};
    }
    return {start, end};
}

// The facing at each leg's from and to station as its own chunk walks it,
// indexed by leg: chainFacings of the chain that holds it, turned around
// where the chain walks the leg backward. A leg on no chain faces along
// itself in plan, or north when it runs plumb.
inline QVector<std::array<QVector3D, 2>> legEndFacings(const QVector<cwPassageTubeBuilder::Leg>& legs,
                                                        const QVector<cwPassageTubeBuilder::Chain>& chains)
{
    QVector<std::array<QVector3D, 2>> facings;
    facings.reserve(legs.size());
    for (const cwPassageTubeBuilder::Leg& leg : legs) {
        const QVector3D plan = planDirectionOf(leg);
        const QVector3D facing = plan.length() >= kMinPlanFacingLength ? plan.normalized() : kFallbackFacing;
        facings.append({facing, facing});
    }
    for (const cwPassageTubeBuilder::Chain& chain : chains) {
        const QVector<QVector3D> stationFacings = chainFacings(legs, chain);
        for (int i = 0; i < chain.legs.size(); ++i) {
            facings[chain.legs.at(i).legIndex] = chainLegFacings(chain, stationFacings, i);
        }
    }
    return facings;
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
// along the record's right (its horizontal facing turned clockwise, turned
// around with the walls it reads when the record faces against the leg) or
// against it for the left.
inline SideWallPoints sideWallPoints(const cwStationLrud& lrud,
                                     const QVector3D& facing,
                                     double stationFraction,
                                     const QVector3D& towardFar,
                                     const QVector3D& squareRight)
{
    const QVector3D facingRight = clockwiseOf(facing);
    const bool facesAgainst = QVector3D::dotProduct(facingRight, squareRight) < 0.0f;
    const QVector3D recordRight = facesAgainst ? -facingRight : facingRight;
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
