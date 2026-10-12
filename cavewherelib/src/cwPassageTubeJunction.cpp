/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeJunction.h"
#include "cwPassageTubeGeometry.h"

//Qt includes
#include <QHash>
#include <QSet>
#include <QtMath>

//Std includes
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>

namespace {

using namespace cwPassageTubeGeometry;

using Builder = cwPassageTubeBuilder;
using Ring = Builder::Ring;
using Chain = Builder::Chain;
using ChainLeg = Builder::ChainLeg;
using Leg = Builder::Leg;
using Options = Builder::Options;
using EndCap = Builder::EndCap;
using JunctionWindow = Builder::JunctionWindow;
using WindowMouth = Builder::WindowMouth;
using ZipCorner = Builder::ZipCorner;
using MouthPoint = Builder::MouthPoint;
using Warning = cwPassageTubeMesh::Warning;
using RingTube = cwPassageTubeJunction::RingTube;

constexpr double kMinJunctionDegrees = 10.0;
const double kMinJunctionSine = std::sin(qDegreesToRadians(kMinJunctionDegrees));
constexpr double kJunctionReachMargin = 0.5;
constexpr double kUnionReachFactor = 2.0;
constexpr double kJunctionRingsPerRadius = 2.0;
// At most this many rings across a junction's reach, which keeps a thin,
// tall arm from slicing the through into thousands of rings.
constexpr double kMaxJunctionRings = 48.0;
constexpr double kGrowStep = 0.05;
constexpr double kCoarseGrowStep = 4.0 * kGrowStep;
constexpr int kGrowBisections = 12;
// How far past a mouth point, as a multiple of its distance from the
// through's axis, the search for the through wall reaches.
constexpr double kWallSearchFactor = 3.0;
constexpr double kJunctionTolerance = 0.02;
constexpr double kParallelRingDegrees = 1.0;
const float kParallelRingCosine = static_cast<float>(std::cos(qDegreesToRadians(kParallelRingDegrees)));
// Elbow rings turning at most this much from one to the next take rings
// between them.
constexpr double kDensifiableRingDegrees = kElbowStepDegrees + 1.0;
const float kDensifiableRingCosine = static_cast<float>(std::cos(qDegreesToRadians(kDensifiableRingDegrees)));
// How far, in quads, the window reaches past the mouth, so its edge stays
// clear of the mouth and the zip band keeps a width.
constexpr double kWindowMarginQuads = 0.25;
// Zip triangles that fold over or are slivers cost this much more than any
// bridging edge, so the band takes them only when it has no other way round.
constexpr double kFoldedZipPenalty = 1e6;
constexpr double kSliverZipPenalty = 1e3;
constexpr double kMaxZipAspectRatio = 20.0;
// A sliver past the mesh check's aspect-ratio limit costs more again, so a
// band that must take slivers takes the least thin ones.
constexpr double kThinSliverZipPenalty = 1e5;
constexpr double kMaxThinZipAspectRatio = 50.0;
constexpr float kDegenerateLengthSquared = 1e-12f;
constexpr double kParallelEpsilon = 1e-12;
// A point this close to a through wall counts as on it, so a column that
// starts on the wall can grow past it.
constexpr double kOnWallMeters = 1e-5;
constexpr double kMaxCarryDegrees = 45.0;
// A mouth vertex this close to a window boundary vertex, or to the mouth
// vertex before it, and within kWeldQuads of it on the through's grid, is
// welded to it.
constexpr double kWeldMeters = 0.03;
constexpr double kWeldQuads = 1.0;
// Columns between an arm's bins are cut until neighboring mouth points lie
// within this many through ring spacings of each other, halving each gap at
// most kMaxMouthDepth times.
constexpr double kMouthStepFactor = 1.5;
constexpr int kMaxMouthDepth = 4;
// Two mouths of one window are each unwrapped on their own, so one may lie
// up to this many turns of the through's columns from the other.
constexpr int kMaxMouthWraps = 2;
// A zip loop keeps at least a triangle's corners.
constexpr int kMinLoopSize = 3;
// How far into the through a fan closing an unzipped mouth reaches, as a
// fraction of the arm's largest radius.
constexpr double kFanDepthFactor = 0.5;
// Lets a segment that ends on, or passes through, a vertex or edge of the
// through's triangles hit them despite rounding.
constexpr double kBarycentricEpsilon = 1e-6;
const double kMinCarryCosine = std::cos(qDegreesToRadians(kMaxCarryDegrees));

using StationKey = QPair<QUuid, QString>;
using Vertex = QPair<int, int>;

QVector3D unpulledVertex(const Ring& ring, int bin)
{
    return ring.center + binDirection(ring, bin) * static_cast<float>(ring.radii.at(bin));
}

double largestRadius(const Ring& ring)
{
    return ring.radii.isEmpty() ? 0.0 : *std::max_element(ring.radii.cbegin(), ring.radii.cend());
}

double wrappedAngle(double angle)
{
    const double wrapped = std::fmod(angle, kTwoPi);
    return wrapped < 0.0 ? wrapped + kTwoPi : wrapped;
}

// The angle + 2πn closest to reference.
double unwrapped(double angle, double reference)
{
    return angle + kTwoPi * std::round((reference - angle) / kTwoPi);
}

// Distance from point to the segment from a to b.
double distanceToSegment(const QVector3D& point, const QVector3D& a, const QVector3D& b)
{
    const QVector3D axis = b - a;
    const float lengthSquared = axis.lengthSquared();
    if (lengthSquared < kDegenerateLengthSquared) {
        return point.distanceToPoint(a);
    }
    const float t = std::clamp(QVector3D::dotProduct(point - a, axis) / lengthSquared, 0.0f, 1.0f);
    return point.distanceToPoint(a + axis * t);
}

// The closest distance between the segments p0–p1 and q0–q1 (Ericson,
// Real-Time Collision Detection, 5.1.9).
double segmentDistance(const QVector3D& p0, const QVector3D& p1, const QVector3D& q0, const QVector3D& q1)
{
    const QVector3D d1 = p1 - p0;
    const QVector3D d2 = q1 - q0;
    const QVector3D r = p0 - q0;
    const double a = QVector3D::dotProduct(d1, d1);
    const double e = QVector3D::dotProduct(d2, d2);
    const double f = QVector3D::dotProduct(d2, r);
    if (a < kDegenerateLengthSquared && e < kDegenerateLengthSquared) {
        return p0.distanceToPoint(q0);
    }
    if (a < kDegenerateLengthSquared) {
        return distanceToSegment(p0, q0, q1);
    }
    if (e < kDegenerateLengthSquared) {
        return distanceToSegment(q0, p0, p1);
    }
    const double c = QVector3D::dotProduct(d1, r);
    const double b = QVector3D::dotProduct(d1, d2);
    const double denominator = a * e - b * b;
    double s = denominator > kParallelEpsilon ? std::clamp((b * f - c * e) / denominator, 0.0, 1.0) : 0.0;
    double t = (b * s + f) / e;
    if (t < 0.0) {
        t = 0.0;
        s = std::clamp(-c / a, 0.0, 1.0);
    } else if (t > 1.0) {
        t = 1.0;
        s = std::clamp((b - c) / a, 0.0, 1.0);
    }
    return (p0 + d1 * static_cast<float>(s)).distanceToPoint(q0 + d2 * static_cast<float>(t));
}

// Where a point lies along a run of rings: between ring segment and
// segment + 1, fraction of the way, or past the last ring along an
// extension. segment is -1 when the point lies past either end.
struct Slab {
    int segment = -1;
    double fraction = 0.0;
};

// The slab between two consecutive rings holding the point, nearest the
// chain's axis among those that hold it. Each ring's tangent is oriented
// along the run, so a slab keeps its orientation through a bend.
Slab slabOf(const QVector<Ring>& rings, const QVector3D& point, int first, int last, const QVector3D& extendTangent)
{
    Slab best;
    double bestDistance = std::numeric_limits<double>::max();
    for (int i = first; i < last; ++i) {
        const Ring& a = rings.at(i);
        const Ring& b = rings.at(i + 1);
        const QVector3D axis = b.center - a.center;
        if (axis.lengthSquared() < kDegenerateLengthSquared) {
            continue;
        }
        const float signA = QVector3D::dotProduct(a.frame.tangent, axis) >= 0.0f ? 1.0f : -1.0f;
        const float signB = QVector3D::dotProduct(b.frame.tangent, axis) >= 0.0f ? 1.0f : -1.0f;
        const double alongA = signA * QVector3D::dotProduct(point - a.center, a.frame.tangent);
        const double alongB = signB * QVector3D::dotProduct(point - b.center, b.frame.tangent);
        if (alongA < 0.0 || alongB > 0.0) {
            continue;
        }
        const double distance = distanceToSegment(point, a.center, b.center);
        if (distance < bestDistance) {
            bestDistance = distance;
            const double span = alongA - alongB;
            best = Slab {i, span > 0.0 ? alongA / span : 0.0};
        }
    }

    if (!extendTangent.isNull() && last >= first) {
        const Ring& end = rings.at(last);
        const double along = QVector3D::dotProduct(point - end.center, extendTangent);
        if (along >= 0.0) {
            const QVector3D onRay = end.center + extendTangent * static_cast<float>(along);
            if (point.distanceToPoint(onRay) < bestDistance) {
                best = Slab {last, 0.0};
            }
        }
    }
    return best;
}

// The bins either side of a polar angle: low is the last bin at or before
// it, high the next one around the ring, and fraction how far the angle lies
// between their angles.
struct BinSpan {
    int low = 0;
    int high = 0;
    double fraction = 0.0;
};

BinSpan binSpan(const Ring& ring, double theta)
{
    const QVector<double>& angles = ring.binAngles;
    const int resolution = angles.size();
    const double angle = wrappedAngle(theta);
    const int low = static_cast<int>(std::upper_bound(angles.cbegin(), angles.cend(), angle) - angles.cbegin()) - 1;
    const int high = (low + 1) % resolution;
    const double highAngle = high == 0 ? kTwoPi : angles.at(high);
    const double gap = highAngle - angles.at(low);
    return BinSpan {low, high, gap > 0.0 ? (angle - angles.at(low)) / gap : 0.0};
}

QPointF localVertex(const Ring& ring, int bin)
{
    const double radius = ring.radii.at(bin);
    return QPointF(radius * std::cos(ring.binAngles.at(bin)), radius * std::sin(ring.binAngles.at(bin)));
}

// The ring's radius toward theta: where the ray meets the chord between the
// bins either side, as the mesh draws the wall there.
double polarRadius(const Ring& ring, double theta)
{
    const BinSpan span = binSpan(ring, theta);
    const QPointF low = localVertex(ring, span.low);
    const QPointF high = localVertex(ring, span.high);
    const QPointF chord = high - low;
    const QPointF direction(std::cos(theta), std::sin(theta));
    const double denominator = direction.x() * chord.y() - direction.y() * chord.x();
    const double numerator = low.x() * high.y() - low.y() * high.x();
    if (std::abs(denominator) < kParallelEpsilon || numerator / denominator <= 0.0) {
        return ring.radii.at(span.low) * (1.0 - span.fraction) + ring.radii.at(span.high) * span.fraction;
    }
    return numerator / denominator;
}

QPointF localOf(const Ring& ring, const QVector3D& point)
{
    const QVector3D offset = point - ring.center;
    return QPointF(QVector3D::dotProduct(offset, ring.frame.right), QVector3D::dotProduct(offset, ring.frame.up));
}

QVector3D stationPosition(const QVector<Leg>& legs, const ChainLeg& chainLeg, bool atEnd)
{
    const Leg& leg = legs.at(chainLeg.legIndex);
    return atEnd != chainLeg.reversed ? leg.toPosition : leg.fromPosition;
}

StationKey stationKey(const QVector<Leg>& legs, const ChainLeg& chainLeg, const QString& station)
{
    return {legs.at(chainLeg.legIndex).caveId, cwStation::canonicalKey(station)};
}

EndCap& capAt(Chain& chain, bool isStart)
{
    return isStart ? chain.startCap : chain.endCap;
}

// The far end of an arm that is not its mouth.
struct ArmEnd {
    int chain = -1;
    bool isStart = false;
};

struct Junction {
    int through = -1;
    QString station;
    QVector3D position;
    QVector<ArmEnd> arms;
};

QVector<Junction> findJunctions(const QVector<Leg>& legs, QVector<Chain>& chains)
{
    QHash<StationKey, int> throughByStation;
    for (int c = 0; c < chains.size(); ++c) {
        const QVector<ChainLeg>& chainLegs = chains.at(c).legs;
        for (int i = 1; i < chainLegs.size(); ++i) {
            const StationKey key = stationKey(legs, chainLegs.at(i), chainStartStation(legs, chainLegs.at(i)));
            if (!throughByStation.contains(key)) {
                throughByStation.insert(key, c);
            }
        }
    }

    QVector<Junction> junctions;
    QHash<StationKey, int> junctionByStation;
    const auto addArm = [&](int c, bool isStart) {
        Chain& chain = chains[c];
        const ChainLeg& chainLeg = isStart ? chain.legs.first() : chain.legs.last();
        const QString& station = isStart ? chainStartStation(legs, chainLeg) : chainEndStation(legs, chainLeg);
        const StationKey key = stationKey(legs, chainLeg, station);
        const int through = throughByStation.value(key, -1);
        if (through < 0 || chain.rings.size() < 2) {
            capAt(chain, isStart) = EndCap::Fan;
            return;
        }
        auto it = junctionByStation.constFind(key);
        if (it == junctionByStation.constEnd()) {
            it = junctionByStation.insert(key, junctions.size());
            junctions.append(Junction {through, station, stationPosition(legs, chainLeg, !isStart), {}});
        }
        junctions[it.value()].arms.append(ArmEnd {c, isStart});
    };

    for (int c = 0; c < chains.size(); ++c) {
        if (chains.at(c).startCap == EndCap::Mouth) {
            addArm(c, true);
        }
        if (chains.at(c).endCap == EndCap::Mouth) {
            addArm(c, false);
        }
    }
    return junctions;
}

// The arm's rings ordered from its far end to the junction.
QVector<Ring> ringsTowardJunction(const Chain& chain, bool isStart)
{
    QVector<Ring> rings = chain.rings;
    if (isStart) {
        std::reverse(rings.begin(), rings.end());
    }
    return rings;
}

const Ring& junctionRing(const Chain& chain, bool isStart)
{
    return isStart ? chain.rings.first() : chain.rings.last();
}

// The arm's unit tangent arriving at the junction.
QVector3D arrivingTangent(const Chain& chain, bool isStart)
{
    const QVector3D tangent = junctionRing(chain, isStart).frame.tangent;
    return isStart ? -tangent : tangent;
}

QVector<double> distancesAlong(const QVector<Ring>& rings)
{
    QVector<double> distances;
    distances.reserve(rings.size());
    double distance = 0.0;
    for (int i = 0; i < rings.size(); ++i) {
        if (i > 0) {
            distance += rings.at(i).center.distanceToPoint(rings.at(i - 1).center);
        }
        distances.append(distance);
    }
    return distances;
}

// The through ring at the junction: the nearest of its inner rings, since a
// chain that loops back to end at the junction it passes has an end ring
// there too.
int junctionRingIndex(const QVector<Ring>& rings, const QVector3D& junction)
{
    const bool hasInnerRings = rings.size() >= 3;
    const int first = hasInnerRings ? 1 : 0;
    const int last = hasInnerRings ? rings.size() - 2 : rings.size() - 1;
    int nearest = first;
    for (int i = first + 1; i <= last; ++i) {
        if (rings.at(i).center.distanceToPoint(junction) < rings.at(nearest).center.distanceToPoint(junction)) {
            nearest = i;
        }
    }
    return nearest;
}

// How a junction's through chain sits around it.
struct ThroughSpan {
    QVector<double> along;
    int junctionRing = 0;
    // Rings within reach of the junction, plus one on each side.
    int first = 0;
    int last = 0;

    ThroughSpan(const QVector<Ring>& rings, const QVector3D& junction, double reach) :
        along(distancesAlong(rings)),
        junctionRing(junctionRingIndex(rings, junction))
    {
        const double center = along.at(junctionRing);
        first = junctionRing;
        while (first > 0 && along.at(first) > center - reach) {
            --first;
        }
        last = junctionRing;
        while (last < rings.size() - 1 && along.at(last) < center + reach) {
            ++last;
        }
    }

    bool isNear(int ring, double reach) const
    {
        return std::abs(along.at(ring) - along.at(junctionRing)) <= reach;
    }
};

// The unit vector fraction of the way from unit a to unit b along the arc
// between them.
QVector3D slerp(const QVector3D& a, const QVector3D& b, double fraction)
{
    const double angle = std::acos(std::clamp(static_cast<double>(QVector3D::dotProduct(a, b)), -1.0, 1.0));
    const double sine = std::sin(angle);
    if (sine < kParallelEpsilon) {
        return (a + (b - a) * static_cast<float>(fraction)).normalized();
    }
    const float weightA = static_cast<float>(std::sin((1.0 - fraction) * angle) / sine);
    const float weightB = static_cast<float>(std::sin(fraction * angle) / sine);
    return (a * weightA + b * weightB).normalized();
}

// A ring fraction of the way along the chord from a to b, facing between
// their tangents, with radii and up blended between the two.
Ring lerpRing(const Ring& a, const Ring& b, double fraction)
{
    const float f = static_cast<float>(fraction);
    Ring ring;
    ring.center = a.center + (b.center - a.center) * f;
    ring.radii.reserve(a.radii.size());
    ring.binAngles.reserve(a.radii.size());
    for (int bin = 0; bin < a.radii.size(); ++bin) {
        ring.radii.append(a.radii.at(bin) + (b.radii.at(bin) - a.radii.at(bin)) * fraction);
        ring.binAngles.append(a.binAngles.at(bin) + (b.binAngles.at(bin) - a.binAngles.at(bin)) * fraction);
    }
    ring.measured = QVector<bool>(a.radii.size(), false);
    ring.chainLeg = b.chainLeg;
    ring.isElbow = a.isElbow || b.isElbow;

    const QVector3D tangent = slerp(a.frame.tangent, b.frame.tangent, fraction);
    QVector3D up = a.frame.up * (1.0f - f) + b.frame.up * f;
    up -= tangent * QVector3D::dotProduct(up, tangent);
    if (up.lengthSquared() < kDegenerateLengthSquared) {
        up = a.frame.up - tangent * QVector3D::dotProduct(a.frame.up, tangent);
    }
    up.normalize();
    const double weight = a.frame.inclinationWeight + (b.frame.inclinationWeight - a.frame.inclinationWeight) * fraction;
    ring.frame = Builder::Frame {tangent, up, QVector3D::crossProduct(tangent, up), weight};
    return ring;
}

// Whether every wall vertex of ring lies in front of previous's plane, so
// placing the mesh vertices never pulls one back onto it.
bool isInFrontOf(const Ring& ring, const Ring& previous)
{
    for (int bin = 0; bin < ring.radii.size(); ++bin) {
        if (QVector3D::dotProduct(unpulledVertex(ring, bin) - previous.center, previous.frame.tangent) < kMinRingSpacingMeters) {
            return false;
        }
    }
    return true;
}

// Whether rings can be inserted between two rings: they face the same way,
// as along a straight leg, or turn by an elbow's step. Between a mitered
// corner ring and its inset rings they would crease its inner wall.
bool isDensifiable(const Ring& a, const Ring& b)
{
    const float limit = (a.isElbow || b.isElbow) ? kDensifiableRingCosine : kParallelRingCosine;
    return QVector3D::dotProduct(a.frame.tangent, b.frame.tangent) >= limit;
}

// Inserts rings between the through rings around the junction so no two
// within reach lie farther apart than spacing, on the chord between them and
// facing between their tangents. A ring that would fold against its
// neighbors, as one near an elbow's inner wall can, is left out.
void densify(Chain& through, const QVector3D& junction, double reach, double spacing)
{
    const ThroughSpan span(through.rings, junction, reach);
    const double center = span.along.at(span.junctionRing);
    QVector<Ring> rings;
    rings.reserve(through.rings.size());
    for (int i = 0; i < through.rings.size(); ++i) {
        const Ring& ring = through.rings.at(i);
        rings.append(ring);
        if (i + 1 == through.rings.size()) {
            continue;
        }
        const Ring& next = through.rings.at(i + 1);
        const double start = span.along.at(i);
        const double end = span.along.at(i + 1);
        const bool overlapsReach = end >= center - reach && start <= center + reach;
        if (!overlapsReach || !hasValidFrame(ring) || !hasValidFrame(next) || !isDensifiable(ring, next)) {
            continue;
        }
        const double gap = end - start;
        const int intervals = std::min(static_cast<int>(std::ceil(gap / spacing)),
                                       static_cast<int>(std::floor(gap / kMinRingSpacingMeters)));
        for (int j = 1; j < intervals; ++j) {
            const Ring inserted = lerpRing(ring, next, static_cast<double>(j) / intervals);
            if (isInFrontOf(inserted, rings.last()) && isInFrontOf(next, inserted)) {
                rings.append(inserted);
            }
        }
    }
    through.rings = rings;
}

// A through ring's polygon seen along an arm's across direction: how far it
// reaches along that direction, and how wide it is across it.
struct RingExtent {
    QPointF across;
    QPointF width;
    double lowAcross = 0.0;
    double highAcross = 0.0;
    double lowWidth = 0.0;
    double highWidth = 0.0;
    bool isValid = false;
};

RingExtent ringExtent(const Ring& ring, const QVector3D& normal)
{
    RingExtent extent;
    QPointF across(QVector3D::dotProduct(normal, ring.frame.right), QVector3D::dotProduct(normal, ring.frame.up));
    const double acrossLength = std::hypot(across.x(), across.y());
    if (normal.isNull() || acrossLength < kParallelEpsilon) {
        return extent;
    }
    extent.across = across / acrossLength;
    extent.width = QPointF(-extent.across.y(), extent.across.x());
    extent.lowAcross = std::numeric_limits<double>::max();
    extent.highAcross = std::numeric_limits<double>::lowest();
    extent.lowWidth = std::numeric_limits<double>::max();
    extent.highWidth = std::numeric_limits<double>::lowest();
    for (int bin = 0; bin < ring.radii.size(); ++bin) {
        const QPointF vertex = localVertex(ring, bin);
        const double a = QPointF::dotProduct(vertex, extent.across);
        const double w = QPointF::dotProduct(vertex, extent.width);
        extent.lowAcross = std::min(extent.lowAcross, a);
        extent.highAcross = std::max(extent.highAcross, a);
        extent.lowWidth = std::min(extent.lowWidth, w);
        extent.highWidth = std::max(extent.highWidth, w);
    }
    extent.isValid = true;
    return extent;
}

// One arm's section carried across the through tube: inside the arm's tube
// and, along the arm's across direction t_through × t_arm, between the
// through's walls.
struct Fill {
    RingTube arm;
    const RingTube* through = nullptr;
    // One per through ring; all invalid for an arm nearly parallel to the
    // through.
    QVector<RingExtent> extents;
    // The unit across direction, null for an arm nearly parallel to the
    // through.
    QVector3D across;

    // A column carries the fill when it points within kMaxCarryDegrees of
    // the across direction in its ring's plane. A column running nearly
    // along a floor or ceiling would otherwise skim through the slab of a
    // fill below or above it all the way to the far wall.
    bool isCarriedBy(const Ring& ring, const QVector3D& direction) const
    {
        if (across.isNull()) {
            return true;
        }
        const QVector3D inPlane = across - ring.frame.tangent * QVector3D::dotProduct(across, ring.frame.tangent);
        return std::abs(QVector3D::dotProduct(direction, inPlane)) >= kMinCarryCosine * inPlane.length();
    }

    // Inside the through ring's polygon, or past its extent along the across
    // direction within its width: the part of an arm that reaches above a
    // ceiling, below a floor or beyond a side wall is carried to the far
    // wall, while the part within the through's extent keeps its curved wall.
    bool extruded(const QVector3D& point) const
    {
        const int index = through->nearestRing(point);
        if (index < 0) {
            return false;
        }
        if (through->inside(point, kOnWallMeters)) {
            return true;
        }
        const QPointF local = through->localCoordinates(point, index);
        const RingExtent& extent = extents.at(index);
        if (!extent.isValid) {
            return false;
        }
        const double pointAcross = QPointF::dotProduct(local, extent.across);
        const double pointWidth = QPointF::dotProduct(local, extent.width);
        const bool isPastExtent = pointAcross < extent.lowAcross || pointAcross > extent.highAcross;
        return isPastExtent && pointWidth >= extent.lowWidth && pointWidth <= extent.highWidth;
    }

    bool inside(const QVector3D& point, double tolerance) const
    {
        return arm.inside(point, tolerance) && extruded(point);
    }
};

// An arm's rings toward the junction. An arm that is also the junction's
// through chain starts past the ring where it passes the junction, at
// firstRing of its rings toward the junction.
struct ArmGeometry {
    ArmEnd end;
    QVector<Ring> rings;
    QVector3D tangent;
    int firstRing = 0;
};

int armFirstRing(const Chain& arm, const ArmEnd& end, const Junction& junction)
{
    if (end.chain != junction.through) {
        return 0;
    }
    const int passing = junctionRingIndex(arm.rings, junction.position);
    return (end.isStart ? arm.rings.size() - 1 - passing : passing) + 1;
}

// A junction's solids before growth: the through rings around it and each
// arm's rings toward it, which the fills and the union read.
struct JunctionSolids {
    QVector<Ring> throughRings;
    // The index in the through chain of throughRings' first ring.
    int throughFirst = 0;
    // The same rings after growth, which the arms are cut against.
    QVector<Ring> grownThroughRings;
    QVector<ArmGeometry> arms;
    QVector3D throughTangent;
    QVector3D position;
    double reach = 0.0;
    // How far along the through throughRings reach from the junction.
    double unionReach = 0.0;
    double armRadius = 0.0;
    // Every point of the union lies within bound of the junction.
    double bound = 0.0;

    QVector<Fill> fills(const RingTube& through) const
    {
        QVector<Fill> result;
        result.reserve(arms.size());
        for (const ArmGeometry& arm : arms) {
            const QVector3D normal = QVector3D::crossProduct(throughTangent, arm.tangent);
            const QVector3D across = normal.length() >= kMinJunctionSine ? normal.normalized() : QVector3D();
            QVector<RingExtent> extents;
            extents.reserve(throughRings.size());
            for (const Ring& ring : throughRings) {
                extents.append(ringExtent(ring, across));
            }
            result.append(Fill {RingTube {&arm.rings, arm.tangent}, &through, extents, across});
        }
        return result;
    }

    bool isNear(const QVector3D& point) const
    {
        return point.distanceToPoint(position) <= bound;
    }

    // Whether the segment from start to end passes within an arm's radius of
    // its axis, the only place a fill can be.
    bool mayCrossFill(const QVector3D& start, const QVector3D& end) const
    {
        return std::any_of(arms.cbegin(), arms.cend(), [&](const ArmGeometry& arm) {
            const QVector<Ring>& rings = arm.rings;
            const double radius = largestRadius(rings.last()) + kJunctionReachMargin;
            for (int i = 1; i < rings.size(); ++i) {
                if (segmentDistance(start, end, rings.at(i - 1).center, rings.at(i).center)
                    <= std::max(largestRadius(rings.at(i - 1)), largestRadius(rings.at(i))) + kJunctionReachMargin) {
                    return true;
                }
            }
            const QVector3D extensionEnd = rings.last().center + arm.tangent * static_cast<float>(bound);
            return segmentDistance(start, end, rings.last().center, extensionEnd) <= radius;
        });
    }
};

QVector<ArmGeometry> armGeometries(const QVector<Chain>& chains, const Junction& junction)
{
    QVector<ArmGeometry> arms;
    for (const ArmEnd& end : junction.arms) {
        const Chain& chain = chains.at(end.chain);
        const int firstRing = armFirstRing(chain, end, junction);
        arms.append(ArmGeometry {end, ringsTowardJunction(chain, end.isStart).mid(firstRing), arrivingTangent(chain, end.isStart), firstRing});
    }
    return arms;
}

bool hasValidJunction(const QVector<Chain>& chains, const Junction& junction)
{
    const Chain& through = chains.at(junction.through);
    if (through.rings.size() < 2) {
        return false;
    }
    const Ring& throughRing = through.rings.at(junctionRingIndex(through.rings, junction.position));
    if (!hasValidFrame(throughRing)) {
        return false;
    }
    return std::all_of(junction.arms.cbegin(), junction.arms.cend(), [&](const ArmEnd& end) {
        return hasValidFrame(junctionRing(chains.at(end.chain), end.isStart));
    });
}

double junctionReach(const QVector3D& throughTangent, const QVector<ArmGeometry>& arms)
{
    double reach = kJunctionReachMargin;
    for (const ArmGeometry& arm : arms) {
        const double sine = QVector3D::crossProduct(throughTangent, arm.tangent).length();
        reach += largestRadius(arm.rings.last()) / std::max(sine, kMinJunctionSine);
    }
    return reach;
}

// Rings close enough to follow every arm's outline where it crosses the
// through and the through's own section: kJunctionRingsPerRadius rings over
// half the arm's width along the through, and over the through's mean radius.
double junctionSpacing(const Options& options, const Ring& throughRing, const QVector<ArmGeometry>& arms)
{
    const QVector3D& throughTangent = throughRing.frame.tangent;
    const double meanThroughRadius = std::accumulate(throughRing.radii.cbegin(), throughRing.radii.cend(), 0.0) / throughRing.radii.size();
    double spacing = std::min(options.sampleSpacingMeters, meanThroughRadius / kJunctionRingsPerRadius);
    for (const ArmGeometry& arm : arms) {
        const Ring& ring = arm.rings.last();
        double low = std::numeric_limits<double>::max();
        double high = std::numeric_limits<double>::lowest();
        for (int bin = 0; bin < ring.radii.size(); ++bin) {
            const double along = QVector3D::dotProduct(unpulledVertex(ring, bin) - ring.center, throughTangent);
            low = std::min(low, along);
            high = std::max(high, along);
        }
        spacing = std::min(spacing, (high - low) / 2.0 / kJunctionRingsPerRadius);
    }
    return std::max(spacing, kMinRingSpacingMeters);
}

// How far along the through the union reads. It reaches past the growth: a
// grazing arm's far wall clears the through's wall only where the arm's axis
// lies the two radii off the through's axis.
double unionReach(const Ring& throughRing, const QVector<ArmGeometry>& arms, double reach)
{
    const double throughRadius = largestRadius(throughRing);
    double result = kUnionReachFactor * reach;
    for (const ArmGeometry& arm : arms) {
        const double sine = QVector3D::crossProduct(throughRing.frame.tangent, arm.tangent).length();
        const double farWall = (throughRadius + largestRadius(arm.rings.last())) / std::max(sine, kMinJunctionSine);
        result = std::max(result, farWall + kJunctionReachMargin);
    }
    return result;
}

QVector<Ring> unionThroughRings(const QVector<Ring>& rings, const QVector3D& junction, double unionReach)
{
    const ThroughSpan span(rings, junction, unionReach);
    return rings.mid(span.first, span.last - span.first + 1);
}

JunctionSolids junctionSolids(const QVector<Chain>& chains, const Junction& junction)
{
    JunctionSolids solids;
    const Chain& through = chains.at(junction.through);
    solids.arms = armGeometries(chains, junction);
    solids.position = junction.position;
    const Ring& throughRing = through.rings.at(junctionRingIndex(through.rings, junction.position));
    solids.throughTangent = throughRing.frame.tangent;
    solids.reach = junctionReach(solids.throughTangent, solids.arms);
    solids.unionReach = unionReach(throughRing, solids.arms, solids.reach);

    solids.throughRings = unionThroughRings(through.rings, junction.position, solids.unionReach);
    solids.throughFirst = ThroughSpan(through.rings, junction.position, solids.unionReach).first;

    double throughRadius = 0.0;
    for (const Ring& ring : std::as_const(solids.throughRings)) {
        throughRadius = std::max(throughRadius, largestRadius(ring));
        solids.bound = std::max(solids.bound, static_cast<double>(ring.center.distanceToPoint(junction.position)));
    }
    for (const ArmGeometry& arm : std::as_const(solids.arms)) {
        solids.armRadius = std::max(solids.armRadius, largestRadius(arm.rings.last()));
    }
    solids.bound += throughRadius + solids.armRadius + kJunctionReachMargin;

    // Only an arm's rings near the junction can hold a point of the union.
    for (ArmGeometry& arm : solids.arms) {
        int first = arm.rings.size() - 1;
        while (first > 0 && arm.rings.at(first).center.distanceToPoint(junction.position) <= solids.bound) {
            --first;
        }
        arm.rings.remove(0, first);
    }
    return solids;
}

// How far ring's wall can reach along direction and stay between its
// neighbors' planes, so a grown column never folds behind the ring before it
// or past the ring after it.
double unfoldedRadius(const QVector<Ring>& rings, int ring, const QVector3D& direction)
{
    double limit = Builder::kMaxRadiusMeters;
    const QVector3D& center = rings.at(ring).center;
    const auto clampTo = [&](const Ring& neighbor, float side) {
        const float approach = side * QVector3D::dotProduct(direction, neighbor.frame.tangent);
        if (approach > 0.0f && hasValidFrame(neighbor)) {
            const double gap = side * QVector3D::dotProduct(neighbor.center - center, neighbor.frame.tangent);
            limit = std::min(limit, (gap - kMinRingSpacingMeters) / approach);
        }
    };
    if (ring > 0) {
        clampTo(rings.at(ring - 1), -1.0f);
    }
    if (ring + 1 < rings.size()) {
        clampTo(rings.at(ring + 1), 1.0f);
    }
    return limit;
}

// Each through column near the junction reaches the far side of the fills
// its ray crosses, so a floor whose ray first runs under the through before
// meeting an arm's section still drops to that section's floor.
void growThrough(Chain& through, const JunctionSolids& solids)
{
    const RingTube throughTube {&solids.throughRings, {}};
    const QVector<Fill> fills = solids.fills(throughTube);
    const ThroughSpan span(through.rings, solids.position, solids.reach);

    for (int i = span.first; i <= span.last; ++i) {
        Ring& ring = through.rings[i];
        if (!span.isNear(i, solids.reach) || !hasValidFrame(ring)) {
            continue;
        }
        // A fill lies within the through's walls across the arm, and within
        // the arm's walls along it.
        const double limit = std::min(Builder::kMaxRadiusMeters,
                                      largestRadius(ring) + solids.armRadius + kJunctionReachMargin);
        const int resolution = ring.radii.size();
        for (int bin = 0; bin < resolution; ++bin) {
            const QVector3D direction = binDirection(ring, bin);
            const auto inFill = [&](const QVector3D& point) {
                return solids.isNear(point) && std::any_of(fills.cbegin(), fills.cend(), [&](const Fill& fill) {
                    return fill.isCarriedBy(ring, direction) && fill.inside(point, 0.0);
                });
            };
            const double own = ring.radii.at(bin);
            const auto pointAt = [&](double radius) { return ring.center + direction * static_cast<float>(radius); };
            if (!solids.mayCrossFill(pointAt(own), pointAt(limit))) {
                continue;
            }
            // Coarse steps find the last fill the ray crosses before it
            // reaches another part of the through, past a bend, which
            // already holds that space; fine steps find the fill's far side.
            const auto isInOtherThroughPart = [&](const QVector3D& point) {
                const int nearest = throughTube.nearestRing(point);
                return nearest >= 0 && std::abs(nearest + solids.throughFirst - i) > 1 && throughTube.inside(point);
            };
            std::optional<double> farthest;
            for (double radius = own; radius <= limit; radius += kCoarseGrowStep) {
                const QVector3D point = pointAt(radius);
                if (isInOtherThroughPart(point)) {
                    break;
                }
                if (inFill(point)) {
                    farthest = radius;
                }
            }
            if (!farthest) {
                continue;
            }
            double inside = *farthest;
            while (inside + kGrowStep <= *farthest + kCoarseGrowStep && inFill(pointAt(inside + kGrowStep))) {
                inside += kGrowStep;
            }
            double outside = std::min(inside + kGrowStep, Builder::kMaxRadiusMeters);
            for (int step = 0; step < kGrowBisections; ++step) {
                const double middle = (inside + outside) / 2.0;
                (inFill(pointAt(middle)) ? inside : outside) = middle;
            }
            inside = std::min(inside, unfoldedRadius(through.rings, i, direction));
            if (inside > own) {
                ring.radii[bin] = inside;
                ring.measured[bin] = false;
                ring.isGrown = true;
            }
        }
    }
}

// A through side triangle split from its quad as the mesh writer splits it,
// with each corner's place on the through's (ring, column) grid and the
// outward normal, unnormalized.
struct SurfaceTriangle {
    std::array<QVector3D, 3> points;
    std::array<QPointF, 3> parameters;
    QVector3D normal;
    QVector3D low;
    QVector3D high;

    bool overlapsBox(const QVector3D& boxLow, const QVector3D& boxHigh) const
    {
        return low.x() <= boxHigh.x() && high.x() >= boxLow.x()
               && low.y() <= boxHigh.y() && high.y() >= boxLow.y()
               && low.z() <= boxHigh.z() && high.z() >= boxLow.z();
    }

    // The point and its (ring, column) at barycentric weights u and v on the
    // second and third corners.
    QVector3D pointAt(double u, double v) const
    {
        return points[0] + (points[1] - points[0]) * static_cast<float>(u) + (points[2] - points[0]) * static_cast<float>(v);
    }

    QPointF parameterAt(double u, double v) const
    {
        return parameters[0] + (parameters[1] - parameters[0]) * u + (parameters[2] - parameters[0]) * v;
    }
};

// A point on the through surface and its (ring, column) on the through's
// grid.
struct SurfacePoint {
    QVector3D position;
    QPointF parameter;
};

// Where a segment crosses the through surface: the point, its (ring, column)
// on the through's grid, and how far along the segment it lies, 0 to 1.
struct SurfaceHit {
    QVector3D position;
    QPointF parameter;
    double along = 0.0;
};

// The side triangles between consecutive rings first to last of the
// through, with its vertices placed as the mesh places them. Each quad
// splits along its shorter diagonal, as cwPassageTubeBuilder's mesh writer
// splits it.
QVector<SurfaceTriangle> surfaceTriangles(Chain through, int first, int last)
{
    placeChainVertices(through);
    const QVector<Ring>& rings = through.rings;
    QVector<SurfaceTriangle> triangles;
    const auto add = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c,
                         const QPointF& pa, const QPointF& pb, const QPointF& pc) {
        const QVector3D low(std::min({a.x(), b.x(), c.x()}), std::min({a.y(), b.y(), c.y()}), std::min({a.z(), b.z(), c.z()}));
        const QVector3D high(std::max({a.x(), b.x(), c.x()}), std::max({a.y(), b.y(), c.y()}), std::max({a.z(), b.z(), c.z()}));
        triangles.append(SurfaceTriangle {{a, b, c}, {pa, pb, pc}, QVector3D::crossProduct(b - a, c - a), low, high});
    };
    for (int i = first; i < last; ++i) {
        const Ring& ring = rings.at(i);
        const Ring& next = rings.at(i + 1);
        if (!hasValidFrame(ring) || !hasValidFrame(next)) {
            continue;
        }
        const int resolution = ring.radii.size();
        const double ringParameter = i;
        for (int k = 0; k < resolution; ++k) {
            const int nextColumn = (k + 1) % resolution;
            const QVector3D& a0 = ring.vertices.at(k);
            const QVector3D& a1 = ring.vertices.at(nextColumn);
            const QVector3D& b0 = next.vertices.at(k);
            const QVector3D& b1 = next.vertices.at(nextColumn);
            const QPointF pa0(ringParameter, k);
            const QPointF pa1(ringParameter, k + 1);
            const QPointF pb0(ringParameter + 1, k);
            const QPointF pb1(ringParameter + 1, k + 1);
            if ((b1 - a0).lengthSquared() <= (b0 - a1).lengthSquared()) {
                add(a0, b0, b1, pa0, pb0, pb1);
                add(a0, b1, a1, pa0, pb1, pa1);
            } else {
                add(a0, b0, a1, pa0, pb0, pa1);
                add(b0, b1, a1, pb0, pb1, pa1);
            }
        }
    }
    return triangles;
}

// Möller–Trumbore, as cwPassageTubeMeshCheck's rayCrosses: where the segment
// from start to end crosses the triangle. The hit is placed by its
// barycentric coordinates, so it lies on the triangle.
std::optional<SurfaceHit> segmentCrossing(const SurfaceTriangle& triangle, const QVector3D& start, const QVector3D& end)
{
    const QVector3D direction = end - start;
    const QVector3D edge1 = triangle.points[1] - triangle.points[0];
    const QVector3D edge2 = triangle.points[2] - triangle.points[0];
    const QVector3D p = QVector3D::crossProduct(direction, edge2);
    const double determinant = QVector3D::dotProduct(edge1, p);
    if (std::abs(determinant) < kParallelEpsilon) {
        return std::nullopt;
    }
    const double inverse = 1.0 / determinant;
    const QVector3D s = start - triangle.points[0];
    const double u = QVector3D::dotProduct(s, p) * inverse;
    if (u < -kBarycentricEpsilon || u > 1.0 + kBarycentricEpsilon) {
        return std::nullopt;
    }
    const QVector3D q = QVector3D::crossProduct(s, edge1);
    const double v = QVector3D::dotProduct(direction, q) * inverse;
    if (v < -kBarycentricEpsilon || u + v > 1.0 + kBarycentricEpsilon) {
        return std::nullopt;
    }
    const double along = QVector3D::dotProduct(edge2, q) * inverse;
    if (along < -kBarycentricEpsilon || along > 1.0 + kBarycentricEpsilon) {
        return std::nullopt;
    }
    return SurfaceHit {triangle.pointAt(u, v), triangle.parameterAt(u, v), along};
}

enum class Crossing {
    Entering,
    Any
};

// The crossing of the through surface nearest start along the segment to
// end; an entering crossing meets a triangle from outside.
std::optional<SurfaceHit> nearestCrossing(const QVector<SurfaceTriangle>& triangles,
                                          const QVector3D& start,
                                          const QVector3D& end,
                                          Crossing crossing)
{
    const QVector3D low(std::min(start.x(), end.x()), std::min(start.y(), end.y()), std::min(start.z(), end.z()));
    const QVector3D high(std::max(start.x(), end.x()), std::max(start.y(), end.y()), std::max(start.z(), end.z()));
    const QVector3D direction = end - start;
    std::optional<SurfaceHit> nearest;
    for (const SurfaceTriangle& triangle : triangles) {
        if (!triangle.overlapsBox(low, high)) {
            continue;
        }
        if (crossing == Crossing::Entering && QVector3D::dotProduct(triangle.normal, direction) >= 0.0f) {
            continue;
        }
        const std::optional<SurfaceHit> hit = segmentCrossing(triangle, start, end);
        if (hit && (!nearest || hit->along < nearest->along)) {
            nearest = hit;
        }
    }
    return nearest;
}

// The point of the triangle closest to point, by its barycentric weights on
// the triangle's second and third corners (Ericson, Real-Time Collision
// Detection 5.1.5).
QPointF closestWeights(const SurfaceTriangle& triangle, const QVector3D& point)
{
    const QVector3D& a = triangle.points[0];
    const QVector3D& b = triangle.points[1];
    const QVector3D& c = triangle.points[2];
    const QVector3D ab = b - a;
    const QVector3D ac = c - a;
    const QVector3D ap = point - a;
    const double d1 = QVector3D::dotProduct(ab, ap);
    const double d2 = QVector3D::dotProduct(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) {
        return QPointF(0.0, 0.0);
    }
    const QVector3D bp = point - b;
    const double d3 = QVector3D::dotProduct(ab, bp);
    const double d4 = QVector3D::dotProduct(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) {
        return QPointF(1.0, 0.0);
    }
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        return QPointF(d1 / (d1 - d3), 0.0);
    }
    const QVector3D cp = point - c;
    const double d5 = QVector3D::dotProduct(ab, cp);
    const double d6 = QVector3D::dotProduct(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) {
        return QPointF(0.0, 1.0);
    }
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        return QPointF(0.0, d2 / (d2 - d6));
    }
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return QPointF(1.0 - w, w);
    }
    const double denominator = 1.0 / (va + vb + vc);
    return QPointF(vb * denominator, vc * denominator);
}

// The point of the through surface nearest point, when one lies within
// reach of it.
std::optional<SurfacePoint> nearestOnSurface(const QVector<SurfaceTriangle>& triangles, const QVector3D& point, double reach)
{
    const QVector3D margin(reach, reach, reach);
    const QVector3D low = point - margin;
    const QVector3D high = point + margin;
    std::optional<SurfacePoint> nearest;
    double nearestDistance = reach;
    for (const SurfaceTriangle& triangle : triangles) {
        if (!triangle.overlapsBox(low, high)) {
            continue;
        }
        const QPointF weights = closestWeights(triangle, point);
        const QVector3D onTriangle = triangle.pointAt(weights.x(), weights.y());
        const double distance = onTriangle.distanceToPoint(point);
        if (distance <= nearestDistance) {
            nearestDistance = distance;
            nearest = SurfacePoint {onTriangle, triangle.parameterAt(weights.x(), weights.y())};
        }
    }
    return nearest;
}

// The arm cut where it enters the grown through: its mouth on the through's
// triangles, with each point's (ring, column) on the through's grid, unset
// for a point off the through, and how many of its rings, counted from the
// far end, stay entirely outside.
struct Trim {
    QVector<MouthPoint> mouth;
    QVector<std::optional<QPointF>> parameters;
    int keptRings = 0;
};

// Where one column of an arm enters the through: the mouth point, its place
// on the through's grid, and the last polyline vertex before it that lies
// outside, -1 when the column starts inside.
struct ColumnEntry {
    QVector3D position;
    std::optional<QPointF> parameter;
    int lastOutside = -1;
};

// An arm that is also the through chain starts on the through, so it is cut
// where it comes back into the through rather than where it starts there.
// Each whole column is cut first; then the columns between two whole columns
// are cut from the kept end ring on, halving the gap between neighboring
// mouth points until none is farther apart than kMouthStepFactor times the
// through's ring spacing at the junction. A gap whose middle point lies on
// the straight edge across it already follows the through and stays whole;
// one whose middle point lies farther from either end than the ends lie
// from each other, where the column grazes the through and enters it on one
// side or the other, stays whole too, as splitting it would zigzag.
Trim trimArm(const QVector<Ring>& rings,
             const QVector3D& tangent,
             const JunctionSolids& solids,
             const QVector<SurfaceTriangle>& throughTriangles,
             bool startsOnThrough,
             double throughSpacing)
{
    const RingTube grownTube {&solids.grownThroughRings, {}};
    const auto inGrownThrough = [&](const QVector3D& point) {
        return solids.isNear(point) && grownTube.inside(point, kJunctionTolerance);
    };
    // A column of the through itself runs on its own surface until it
    // leaves the through.
    const auto isOutsideThrough = [&](const QVector3D& point) {
        return !inGrownThrough(point) && !nearestOnSurface(throughTriangles, point, kJunctionTolerance);
    };
    // Where the segment first enters the through, or first comes within
    // kJunctionTolerance of its surface, as a column running along a wall
    // the growth took to the arm's own wall does, snapped onto the surface.
    const auto entry = [&](const QVector3D& start, const QVector3D& end) -> std::optional<SurfaceHit> {
        std::optional<SurfaceHit> hit = nearestCrossing(throughTriangles, start, end, Crossing::Entering);
        if (hit && !solids.isNear(hit->position)) {
            hit.reset();
        }
        const int steps = std::max(1, static_cast<int>(std::ceil(start.distanceToPoint(end) / kGrowStep)));
        for (int step = 1; step <= steps; ++step) {
            const double along = static_cast<double>(step) / steps;
            if (hit && hit->along <= along) {
                break;
            }
            const QVector3D sample = start + (end - start) * static_cast<float>(along);
            if (!solids.isNear(sample)) {
                continue;
            }
            const std::optional<SurfacePoint> grazing = nearestOnSurface(throughTriangles, sample, kJunctionTolerance);
            if (grazing) {
                return SurfaceHit {grazing->position, grazing->parameter, along};
            }
        }
        return hit;
    };

    // Where the ray from the through's axis out through the point, along its
    // ring's radial direction, crosses the wall.
    const auto ontoThroughWall = [&](const QVector3D& point) -> std::optional<SurfaceHit> {
        const int index = grownTube.nearestRing(point);
        if (index < 0) {
            return std::nullopt;
        }
        const Ring& ring = solids.grownThroughRings.at(index);
        const QVector3D axisPoint = ring.center + ring.frame.tangent * QVector3D::dotProduct(point - ring.center, ring.frame.tangent);
        const QVector3D radial = point - axisPoint;
        if (radial.length() < kMinRingSpacingMeters) {
            return std::nullopt;
        }
        return nearestCrossing(throughTriangles, axisPoint, axisPoint + radial * static_cast<float>(kWallSearchFactor), Crossing::Any);
    };

    // The point between the polyline's rings, short of its tangent
    // extension, that passes closest to the through's wall, sampled every
    // kGrowStep; its last ring vertex when none lies by the through.
    const auto closestToWall = [&](const QVector<QVector3D>& polyline) {
        QVector3D closest = polyline.at(polyline.size() - 2);
        double nearest = std::numeric_limits<double>::max();
        for (int index = 1; index < polyline.size() - 1; ++index) {
            const QVector3D& start = polyline.at(index - 1);
            const QVector3D& end = polyline.at(index);
            const int steps = std::max(1, static_cast<int>(std::ceil(start.distanceToPoint(end) / kGrowStep)));
            for (int step = 0; step <= steps; ++step) {
                const QVector3D sample = start + (end - start) * static_cast<float>(static_cast<double>(step) / steps);
                if (!solids.isNear(sample)) {
                    continue;
                }
                const std::optional<double> distance = grownTube.wallDistance(sample);
                if (distance && *distance < nearest) {
                    nearest = *distance;
                    closest = sample;
                }
            }
        }
        return closest;
    };

    // The polyline ends in the tangent extension past its last ring vertex.
    // Rings up to lastOutside keep the column outside the through. A column
    // that misses the through ends on its wall below the point where its
    // rings pass closest to it.
    const auto findEntry = [&](const QVector<QVector3D>& polyline) {
        const int lastRing = polyline.size() - 2;
        bool hasLeftThrough = startsOnThrough ? isOutsideThrough(polyline.first()) : !inGrownThrough(polyline.first());
        if (!hasLeftThrough && !startsOnThrough) {
            return ColumnEntry {polyline.first(), std::nullopt, -1};
        }
        for (int index = 1; index < polyline.size(); ++index) {
            if (hasLeftThrough) {
                const std::optional<SurfaceHit> hit = entry(polyline.at(index - 1), polyline.at(index));
                if (hit) {
                    return ColumnEntry {hit->position, hit->parameter, std::min(index - 1, lastRing)};
                }
            }
            hasLeftThrough = hasLeftThrough || isOutsideThrough(polyline.at(index));
        }
        const QVector3D passing = closestToWall(polyline);
        const std::optional<SurfaceHit> onWall = ontoThroughWall(passing);
        return ColumnEntry {onWall ? onWall->position : passing,
                            onWall ? std::optional<QPointF>(onWall->parameter) : std::nullopt,
                            lastRing - 1};
    };
    const auto withExtension = [&](QVector<QVector3D> polyline) {
        polyline.append(polyline.last() + tangent * static_cast<float>(solids.reach));
        return polyline;
    };

    const int count = rings.size();
    const int resolution = rings.last().radii.size();
    QVector<ColumnEntry> wholeColumns;
    wholeColumns.reserve(resolution);
    int keptRings = count;
    for (int bin = 0; bin < resolution; ++bin) {
        QVector<QVector3D> polyline;
        polyline.reserve(count + 1);
        for (const Ring& ring : rings) {
            polyline.append(unpulledVertex(ring, bin));
        }
        wholeColumns.append(findEntry(withExtension(polyline)));
        keptRings = std::min(keptRings, wholeColumns.last().lastOutside + 1);
    }

    // A kept ring right at the mouth would leave a strip of slivers.
    const auto isAtMouth = [&](int ring) {
        for (int bin = 0; bin < resolution; ++bin) {
            if (unpulledVertex(rings.at(ring), bin).distanceToPoint(wholeColumns.at(bin).position) < kMinRingSpacingMeters) {
                return true;
            }
        }
        return false;
    };
    while (keptRings > 1 && isAtMouth(keptRings - 1)) {
        --keptRings;
    }

    Trim trim;
    trim.keptRings = keptRings;
    if (keptRings <= 0) {
        return trim;
    }

    // The column fraction of the way from bin to the next, from the kept end
    // ring on; unset when its start lies inside the through or its mouth
    // lies off it.
    const auto betweenColumns = [&](int bin, double fraction) -> std::optional<ColumnEntry> {
        const int next = (bin + 1) % resolution;
        const float f = static_cast<float>(fraction);
        QVector<QVector3D> polyline;
        polyline.reserve(count - keptRings + 2);
        for (int i = keptRings - 1; i < count; ++i) {
            const QVector3D low = unpulledVertex(rings.at(i), bin);
            polyline.append(low + (unpulledVertex(rings.at(i), next) - low) * f);
        }
        const ColumnEntry column = findEntry(withExtension(polyline));
        if (column.lastOutside < 0 || !column.parameter) {
            return std::nullopt;
        }
        return column;
    };
    const double maxStep = kMouthStepFactor * throughSpacing;
    const auto append = [&](const ColumnEntry& column, double at) {
        trim.mouth.append(MouthPoint {column.position, at});
        trim.parameters.append(column.parameter);
    };
    const auto refine = [&](const auto& self, int bin, double low, const QVector3D& lowPoint,
                            double high, const QVector3D& highPoint, int depth) -> void {
        const double gap = lowPoint.distanceToPoint(highPoint);
        if (depth >= kMaxMouthDepth || gap <= maxStep) {
            return;
        }
        const double middle = (low + high) / 2.0;
        const std::optional<ColumnEntry> column = betweenColumns(bin, middle);
        if (!column || distanceToSegment(column->position, lowPoint, highPoint) <= kJunctionTolerance
            || column->position.distanceToPoint(lowPoint) >= gap || column->position.distanceToPoint(highPoint) >= gap) {
            return;
        }
        self(self, bin, low, lowPoint, middle, column->position, depth + 1);
        append(*column, bin + middle);
        self(self, bin, middle, column->position, high, highPoint, depth + 1);
    };
    for (int bin = 0; bin < resolution; ++bin) {
        append(wholeColumns.at(bin), bin);
        refine(refine, bin, 0.0, wholeColumns.at(bin).position, 1.0, wholeColumns.at((bin + 1) % resolution).position, 0);
    }
    return trim;
}

Warning armWarning(const QVector<Leg>& legs, const Chain& chain, bool isStart, Warning::Type type, const QString& message)
{
    const ChainLeg& chainLeg = isStart ? chain.legs.last() : chain.legs.first();
    const QString& station = isStart ? chainEndStation(legs, chainLeg) : chainStartStation(legs, chainLeg);
    return Warning {type, legs.at(chainLeg.legIndex).tripId, station, message.arg(station)};
}

// Leaves out an arm that lies inside the passages it joins.
void dropArm(const QVector<Leg>& legs, Chain& arm, bool isStart, const QString& message, QVector<Warning>& warnings)
{
    warnings.append(armWarning(legs, arm, isStart, Warning::ArmInsideThrough, message));
    arm.rings.clear();
    arm.startMouth.clear();
    arm.endMouth.clear();
}

QUuid junctionTrip(const QVector<Leg>& legs, const Chain& arm, bool isStart)
{
    const ChainLeg& chainLeg = arm.legs.at(junctionRing(arm, isStart).chainLeg);
    return legs.at(chainLeg.legIndex).tripId;
}

// Leaves out an arm none of whose rings stays outside the through: it lies
// inside the through and is dropped, or, when it is the through itself,
// keeps a fan.
void leaveOutArm(const QVector<Leg>& legs, Chain& arm, bool isStart, int firstRing, QVector<Warning>& warnings)
{
    if (firstRing > 0) {
        capAt(arm, isStart) = EndCap::Fan;
    } else {
        dropArm(legs, arm, isStart,
                QStringLiteral("The passage from station %1 lies inside the passage it joins; the tube leaves it out."), warnings);
    }
}

// Cuts the arm back to the rings the trim keeps and gives it the mouth.
// Returns how many rings it removed from the chain's start.
int cutArm(Chain& arm, bool isStart, int firstRing, const Trim& trim)
{
    const int kept = firstRing + trim.keptRings;
    const int dropped = arm.rings.size() - kept;
    if (isStart) {
        arm.rings.remove(0, dropped);
        arm.startMouth = trim.mouth;
        return dropped;
    }
    arm.rings.remove(kept, dropped);
    arm.endMouth = trim.mouth;
    return 0;
}

// The mouth in (ring, column) space, unwrapped across the column seam so it
// reads as one closed polygon; nullopt when a point lies off the through or
// the mouth wraps all the way around it.
std::optional<QVector<QPointF>> mouthPolygon(const QVector<std::optional<QPointF>>& parameters, int resolution)
{
    QVector<QPointF> polygon;
    polygon.reserve(parameters.size());
    for (const std::optional<QPointF>& parameter : parameters) {
        if (!parameter) {
            return std::nullopt;
        }
        QPointF unwrappedParameter = *parameter;
        if (!polygon.isEmpty()) {
            const double previous = polygon.last().y();
            unwrappedParameter.setY(parameter->y() + resolution * std::round((previous - parameter->y()) / resolution));
        }
        polygon.append(unwrappedParameter);
    }
    const double closingGap = std::abs(polygon.last().y() - polygon.first().y());
    if (closingGap > resolution / 2.0) {
        return std::nullopt;
    }
    return polygon;
}

double signedArea(const QVector<QPointF>& polygon)
{
    double area = 0.0;
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF& a = polygon.at(i);
        const QPointF& b = polygon.at((i + 1) % polygon.size());
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area / 2.0;
}

bool containsPoint(const QVector<QPointF>& polygon, const QPointF& point)
{
    bool inside = false;
    for (int i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const QPointF& a = polygon.at(i);
        const QPointF& b = polygon.at(j);
        if ((a.y() > point.y()) != (b.y() > point.y())) {
            const double x = a.x() + (point.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
            if (point.x() < x) {
                inside = !inside;
            }
        }
    }
    return inside;
}

double cross2d(const QPointF& a, const QPointF& b, const QPointF& c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

bool segmentsCross(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d)
{
    const double d1 = cross2d(c, d, a);
    const double d2 = cross2d(c, d, b);
    const double d3 = cross2d(a, b, c);
    const double d4 = cross2d(a, b, d);
    return ((d1 > 0.0) != (d2 > 0.0)) && ((d3 > 0.0) != (d4 > 0.0));
}

bool overlapsRectangle(const QVector<QPointF>& polygon, const QRectF& rectangle)
{
    const QVector<QPointF> corners {rectangle.topLeft(), rectangle.topRight(), rectangle.bottomRight(), rectangle.bottomLeft()};
    for (const QPointF& point : polygon) {
        if (rectangle.contains(point)) {
            return true;
        }
    }
    for (const QPointF& corner : corners) {
        if (containsPoint(polygon, corner)) {
            return true;
        }
    }
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF& a = polygon.at(i);
        const QPointF& b = polygon.at((i + 1) % polygon.size());
        for (int j = 0; j < corners.size(); ++j) {
            if (segmentsCross(a, b, corners.at(j), corners.at((j + 1) % corners.size()))) {
                return true;
            }
        }
    }
    return false;
}

// Widens the removed quads so each column's removed quads run unbroken and
// no two removed quads meet only at a corner.
void closeRemovedQuads(QSet<Vertex>& removed, int ringCount, int resolution)
{
    const int last = ringCount - 1;
    const auto wrapped = [resolution](int column) { return (column + resolution) % resolution; };
    bool changed = true;
    while (changed) {
        changed = false;
        for (int column = 0; column < resolution; ++column) {
            int low = last;
            int high = -1;
            for (int i = 0; i < last; ++i) {
                if (removed.contains({i, column})) {
                    low = std::min(low, i);
                    high = std::max(high, i);
                }
            }
            for (int i = low; i <= high; ++i) {
                if (!removed.contains({i, column})) {
                    removed.insert({i, column});
                    changed = true;
                }
            }
        }
        for (int i = 1; i < last; ++i) {
            for (int column = 0; column < resolution; ++column) {
                const Vertex lowLow {i - 1, wrapped(column - 1)};
                const Vertex highLow {i, wrapped(column - 1)};
                const Vertex lowHigh {i - 1, column};
                const Vertex highHigh {i, column};
                const bool diagonal = removed.contains(lowLow) && removed.contains(highHigh)
                                      && !removed.contains(highLow) && !removed.contains(lowHigh);
                const bool antiDiagonal = removed.contains(highLow) && removed.contains(lowHigh)
                                          && !removed.contains(lowLow) && !removed.contains(highHigh);
                if (diagonal || antiDiagonal) {
                    removed.unite({lowLow, highLow, lowHigh, highHigh});
                    changed = true;
                }
            }
        }
    }
}

// The through quads the mouth covers, widened by closeRemovedQuads.
QSet<Vertex> removedQuads(const QVector<Ring>& rings, const QVector<QPointF>& mouth, int resolution)
{
    QSet<Vertex> removed;
    for (int i = 0; i + 1 < rings.size(); ++i) {
        if (!hasValidFrame(rings.at(i)) || !hasValidFrame(rings.at(i + 1))) {
            continue;
        }
        for (int column = 0; column < resolution; ++column) {
            for (int wrap = -1; wrap <= 1; ++wrap) {
                const QRectF quad(i - kWindowMarginQuads,
                                  column + wrap * resolution - kWindowMarginQuads,
                                  1.0 + 2.0 * kWindowMarginQuads,
                                  1.0 + 2.0 * kWindowMarginQuads);
                if (overlapsRectangle(mouth, quad)) {
                    removed.insert({i, column});
                    break;
                }
            }
        }
    }
    closeRemovedQuads(removed, rings.size(), resolution);
    return removed;
}

// Every edge loop of the removed quads as through vertices, the quads on
// each loop's left in (ring, column) space: counter-clockwise around the
// hole, clockwise around a kept island, and one loop at each end of a hole
// that takes the whole circumference. Each loop starts at its lowest vertex
// and the loops come in the order of those vertices. nullopt when the quads
// pinch at a vertex.
std::optional<QVector<QVector<Vertex>>> windowBoundaries(const QSet<Vertex>& removed, int resolution)
{
    QSet<QPair<Vertex, Vertex>> edges;
    for (const Vertex& quad : removed) {
        const int i = quad.first;
        const int column = quad.second;
        const int next = (column + 1) % resolution;
        const Vertex corners[] {{i, column}, {i + 1, column}, {i + 1, next}, {i, next}};
        for (int corner = 0; corner < 4; ++corner) {
            edges.insert({corners[corner], corners[(corner + 1) % 4]});
        }
    }

    QHash<Vertex, Vertex> nextVertex;
    for (const QPair<Vertex, Vertex>& edge : std::as_const(edges)) {
        if (edges.contains({edge.second, edge.first})) {
            continue;
        }
        if (nextVertex.contains(edge.first)) {
            return std::nullopt;
        }
        nextVertex.insert(edge.first, edge.second);
    }
    if (nextVertex.isEmpty()) {
        return std::nullopt;
    }

    QVector<Vertex> starts = nextVertex.keys();
    std::sort(starts.begin(), starts.end());
    QSet<Vertex> visited;
    QVector<QVector<Vertex>> loops;
    for (const Vertex& start : std::as_const(starts)) {
        if (visited.contains(start)) {
            continue;
        }
        QVector<Vertex> loop {start};
        visited.insert(start);
        Vertex current = nextVertex.value(start);
        while (current != start) {
            if (!nextVertex.contains(current) || visited.contains(current)) {
                return std::nullopt;
            }
            loop.append(current);
            visited.insert(current);
            current = nextVertex.value(current);
        }
        loops.append(loop);
    }
    return loops;
}

// How much a zip triangle costs: its bridging edge, plus a penalty when it
// folds over in (ring, column) space or is a sliver.
double zipTriangleCost(const QPointF& a, const QPointF& b, const QPointF& c,
                       const QVector3D& pa, const QVector3D& pb, const QVector3D& pc,
                       const QVector3D& outward, double bridge)
{
    double cost = bridge;
    const QVector3D normal = QVector3D::crossProduct(pb - pa, pc - pa);
    if (cross2d(a, b, c) <= 0.0 || QVector3D::dotProduct(normal, outward) <= 0.0f) {
        cost += kFoldedZipPenalty;
    }
    const double area = normal.length() / 2.0;
    const double longest = std::max({pa.distanceToPoint(pb), pb.distanceToPoint(pc), pc.distanceToPoint(pa)});
    const double aspectRatio = area <= 0.0 ? std::numeric_limits<double>::max() : longest * longest / (2.0 * area);
    if (aspectRatio > kMaxZipAspectRatio) {
        cost += kSliverZipPenalty;
    }
    if (aspectRatio > kMaxThinZipAspectRatio) {
        cost += kThinSliverZipPenalty;
    }
    return cost;
}

// A vertex of a loop the zip band joins: its corner, a through vertex on a
// window boundary or where a mouth was welded to one, else a mouth point;
// its place on the through's grid and its position.
struct LoopVertex {
    ZipCorner corner;
    QPointF parameter;
    QVector3D position;
};

double columnNear(double column, double reference, int resolution)
{
    return column + resolution * std::round((reference - column) / resolution);
}

QPointF parameterNear(const QPointF& parameter, double referenceColumn, int resolution)
{
    return QPointF(parameter.x(), columnNear(parameter.y(), referenceColumn, resolution));
}

bool isWithinWeldQuads(const QPointF& a, const QPointF& b)
{
    return std::abs(a.x() - b.x()) <= kWeldQuads && std::abs(a.y() - b.y()) <= kWeldQuads;
}

// Triangulates the band between the outer loop and the inner loop, which
// run the same way round the band, starting from inner vertex 0 and outer
// vertex offset. Of all the ways to step around the two loops, it takes the
// one with the least total cost (Fuchs, Kedem and Uselton 1977): the
// shortest bridging edges, with unfolded, well-shaped triangles. A triangle
// with two equal corners, where a mouth is welded to the boundary, costs
// nothing and is left out.
QVector<std::array<ZipCorner, 3>> zipBand(const QVector<Ring>& rings,
                                          const QVector<LoopVertex>& outer,
                                          const QVector<LoopVertex>& inner,
                                          int offset,
                                          int resolution)
{
    const int innerCount = inner.size();
    const int outerCount = outer.size();
    const auto innerVertex = [&](int i) -> const LoopVertex& { return inner.at(i % innerCount); };
    const auto outerVertex = [&](int j) -> const LoopVertex& { return outer.at((j + offset) % outerCount); };
    // The through's outward direction at an outer vertex.
    const auto outwardAt = [&](int j) {
        const ZipCorner& corner = outerVertex(j).corner;
        return binDirection(rings.at(corner.ring), corner.column);
    };
    const auto triangleCost = [&](const LoopVertex& a, const LoopVertex& b, const LoopVertex& c, const QVector3D& outward, double bridge) {
        const double reference = a.parameter.y();
        return zipTriangleCost(a.parameter, parameterNear(b.parameter, reference, resolution), parameterNear(c.parameter, reference, resolution),
                               a.position, b.position, c.position, outward, bridge);
    };

    // State (i, j) has bridged inner vertex i to outer vertex j. The band
    // either opens with an outer step and closes with an inner step, or the
    // other way round, so no bridge between the two loops' first vertices is
    // made twice; it takes the cheaper of the two.
    const int columns = outerCount + 1;
    const auto stateIndex = [columns](int i, int j) { return i * columns + j; };
    struct Path {
        QVector<double> cost;
        QVector<bool> cameByOuterStep;
    };
    const auto cheapestPath = [&](bool opensWithOuterStep) {
        const auto isAllowed = [&](int i, int j) {
            if (opensWithOuterStep) {
                return !(j == 0 && i > 0) && !(i == innerCount && j < outerCount);
            }
            return !(i == 0 && j > 0) && !(j == outerCount && i < innerCount);
        };
        Path path {QVector<double>((innerCount + 1) * columns, std::numeric_limits<double>::max()),
                   QVector<bool>((innerCount + 1) * columns, false)};
        path.cost[0] = 0.0;
        for (int i = 0; i <= innerCount; ++i) {
            for (int j = 0; j <= outerCount; ++j) {
                const double here = path.cost.at(stateIndex(i, j));
                if (here == std::numeric_limits<double>::max()) {
                    continue;
                }
                const LoopVertex& in = innerVertex(i);
                const LoopVertex& out = outerVertex(j);
                if (j < outerCount && isAllowed(i, j + 1)) {
                    const LoopVertex& outNext = outerVertex(j + 1);
                    const bool isWelded = in.corner == out.corner || in.corner == outNext.corner;
                    const double step = here + (isWelded ? 0.0
                                                          : triangleCost(out, outNext, in, outwardAt(j) + outwardAt(j + 1),
                                                                         in.position.distanceToPoint(outNext.position)));
                    if (step < path.cost.at(stateIndex(i, j + 1))) {
                        path.cost[stateIndex(i, j + 1)] = step;
                        path.cameByOuterStep[stateIndex(i, j + 1)] = true;
                    }
                }
                if (i < innerCount && isAllowed(i + 1, j)) {
                    const LoopVertex& inNext = innerVertex(i + 1);
                    const bool isWelded = in.corner == out.corner || inNext.corner == out.corner || in.corner == inNext.corner;
                    const double step = here + (isWelded ? 0.0
                                                          : triangleCost(in, out, inNext, outwardAt(j),
                                                                         inNext.position.distanceToPoint(out.position)));
                    if (step < path.cost.at(stateIndex(i + 1, j))) {
                        path.cost[stateIndex(i + 1, j)] = step;
                        path.cameByOuterStep[stateIndex(i + 1, j)] = false;
                    }
                }
            }
        }
        return path;
    };
    const Path opensWithOuter = cheapestPath(true);
    const Path opensWithInner = cheapestPath(false);
    const int last = stateIndex(innerCount, outerCount);
    const QVector<bool>& cameByOuterStep = opensWithOuter.cost.at(last) <= opensWithInner.cost.at(last)
                                               ? opensWithOuter.cameByOuterStep
                                               : opensWithInner.cameByOuterStep;

    QVector<std::array<ZipCorner, 3>> triangles;
    triangles.reserve(innerCount + outerCount);
    const auto addTriangle = [&](const ZipCorner& a, const ZipCorner& b, const ZipCorner& c) {
        if (a != b && b != c && c != a) {
            triangles.append({a, b, c});
        }
    };
    int i = innerCount;
    int j = outerCount;
    while (i > 0 || j > 0) {
        if (cameByOuterStep.at(stateIndex(i, j))) {
            --j;
            addTriangle(outerVertex(j).corner, outerVertex(j + 1).corner, innerVertex(i).corner);
        } else {
            --i;
            addTriangle(innerVertex(i).corner, outerVertex(j).corner, innerVertex(i + 1).corner);
        }
    }
    std::reverse(triangles.begin(), triangles.end());
    return triangles;
}

// The outer vertex the zip band starts from for a single mouth: the one in
// the direction of the mouth's first vertex from its centroid, in (ring,
// column) space.
int centroidStartOffset(const QVector<LoopVertex>& outer, const QVector<LoopVertex>& mouth, int resolution)
{
    QPointF center;
    for (const LoopVertex& vertex : mouth) {
        center += vertex.parameter;
    }
    center /= mouth.size();
    const auto angleAbout = [&](const QPointF& point) {
        const QPointF toward = point - center;
        return std::atan2(toward.y(), toward.x());
    };

    const double startAngle = angleAbout(mouth.first().parameter);
    int offset = 0;
    double offsetGap = std::numeric_limits<double>::max();
    for (int j = 0; j < outer.size(); ++j) {
        const double angle = angleAbout(parameterNear(outer.at(j).parameter, center.y(), resolution));
        const double gap = std::abs(unwrapped(angle, startAngle) - startAngle);
        if (gap < offsetGap) {
            offsetGap = gap;
            offset = j;
        }
    }
    return offset;
}

// Rotates the inner loop to start at the vertex of the closest pair between
// the two loops, and returns the outer vertex of that pair. A pair qualifies
// only when the band lies between the two loops' next edges from it, so where
// a bridge runs both ways along one column the pair takes the copy of the
// bridge on the inner vertex's side; with no such pair, any pair does.
int closestStartOffset(const QVector<LoopVertex>& outer, QVector<LoopVertex>& inner, int resolution)
{
    const auto facesBand = [&](int i, int j) {
        const QPointF out = outer.at(j).parameter;
        const QPointF outNext = parameterNear(outer.at((j + 1) % outer.size()).parameter, out.y(), resolution);
        const QPointF in = parameterNear(inner.at(i).parameter, out.y(), resolution);
        const QPointF inNext = parameterNear(inner.at((i + 1) % inner.size()).parameter, out.y(), resolution);
        return cross2d(out, outNext, in) > 0.0 && cross2d(in, out, inNext) > 0.0;
    };
    const auto closestPair = [&](bool needsBandSide) -> std::optional<QPair<int, int>> {
        std::optional<QPair<int, int>> pair;
        double closest = std::numeric_limits<double>::max();
        for (int i = 0; i < inner.size(); ++i) {
            for (int j = 0; j < outer.size(); ++j) {
                const double distance = inner.at(i).position.distanceToPoint(outer.at(j).position);
                if (distance < closest && (!needsBandSide || facesBand(i, j))) {
                    closest = distance;
                    pair = qMakePair(i, j);
                }
            }
        }
        return pair;
    };
    std::optional<QPair<int, int>> pair = closestPair(true);
    if (!pair) {
        pair = closestPair(false);
    }
    std::rotate(inner.begin(), inner.begin() + pair->first, inner.end());
    return pair->second;
}

// Whether the segment from one loop vertex to another crosses an edge of
// any of loops in (ring, column) space, edges at either end aside.
bool crossesLoops(const LoopVertex& from, const LoopVertex& to, const QVector<const QVector<LoopVertex>*>& loops, int resolution)
{
    const QPointF start = from.parameter;
    const QPointF end = parameterNear(to.parameter, start.y(), resolution);
    const auto touches = [&](const ZipCorner& corner) { return corner == from.corner || corner == to.corner; };
    for (const QVector<LoopVertex>* loop : loops) {
        for (int k = 0; k < loop->size(); ++k) {
            const LoopVertex& u = loop->at(k);
            const LoopVertex& v = loop->at((k + 1) % loop->size());
            if (touches(u.corner) || touches(v.corner)) {
                continue;
            }
            const QPointF a = parameterNear(u.parameter, start.y(), resolution);
            const QPointF b = parameterNear(v.parameter, a.y(), resolution);
            for (int wrap = -1; wrap <= 1; ++wrap) {
                const QPointF shift(0.0, wrap * resolution);
                if (segmentsCross(start, end, a + shift, b + shift)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// A bridge from vertex joinedIndex of a joined loop to vertex loopIndex of
// loop number loop, length long in (ring, column) space.
struct Bridge {
    int loop = -1;
    int joinedIndex = 0;
    int loopIndex = 0;
    double length = std::numeric_limits<double>::max();
};

// The shortest bridge from the joined loop to one of loops that crosses no
// edge of the joined loop, of loops or of others.
std::optional<Bridge> shortestBridge(const QVector<LoopVertex>& joined,
                                     const QVector<QVector<LoopVertex>>& loops,
                                     int loop,
                                     const QVector<const QVector<LoopVertex>*>& others,
                                     int resolution)
{
    const QVector<LoopVertex>& target = loops.at(loop);
    QVector<Bridge> candidates;
    candidates.reserve(joined.size() * target.size());
    for (int i = 0; i < joined.size(); ++i) {
        for (int k = 0; k < target.size(); ++k) {
            const QPointF from = joined.at(i).parameter;
            const QPointF to = parameterNear(target.at(k).parameter, from.y(), resolution);
            candidates.append(Bridge {loop, i, k, std::hypot(to.x() - from.x(), to.y() - from.y())});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Bridge& a, const Bridge& b) { return a.length < b.length; });

    QVector<const QVector<LoopVertex>*> crossed = others;
    crossed.append(&joined);
    for (const QVector<LoopVertex>& other : loops) {
        crossed.append(&other);
    }
    for (const Bridge& candidate : std::as_const(candidates)) {
        if (!crossesLoops(joined.at(candidate.joinedIndex), target.at(candidate.loopIndex), crossed, resolution)) {
            return candidate;
        }
    }
    return std::nullopt;
}

// The through vertices strictly between two through vertices of one column,
// so a bridge between two window boundaries follows the through's surface
// rather than cutting a chord across it; none when the bridge leaves the
// column or a corner is a mouth point.
QVector<LoopVertex> throughColumnPath(const QVector<Ring>& rings, const LoopVertex& from, const LoopVertex& to)
{
    QVector<LoopVertex> path;
    const ZipCorner& a = from.corner;
    const ZipCorner& b = to.corner;
    if (rings.isEmpty() || a.ring == ZipCorner::kMouthRing || b.ring == ZipCorner::kMouthRing || a.column != b.column) {
        return path;
    }
    const int step = b.ring > a.ring ? 1 : -1;
    for (int ring = a.ring + step; ring != b.ring; ring += step) {
        path.append(LoopVertex {ZipCorner {ring, a.column}, QPointF(ring, from.parameter.y()), unpulledVertex(rings.at(ring), a.column)});
    }
    return path;
}

// Joins loops that all keep the band on the same side into one loop:
// starting from the first, it splices in the loop nearest the joined one
// through the shortest bridge that crosses neither another loop nor others.
// The bridge is walked once each way, so the zip band puts one triangle on
// each side of it; between two through vertices of one column it runs over
// the through's vertices between them. nullopt when a loop has no such
// bridge.
std::optional<QVector<LoopVertex>> joinedLoop(const QVector<Ring>& rings,
                                              QVector<QVector<LoopVertex>> loops,
                                              const QVector<const QVector<LoopVertex>*>& others,
                                              int resolution)
{
    QVector<LoopVertex> joined = loops.takeFirst();
    while (!loops.isEmpty()) {
        Bridge best;
        for (int loop = 0; loop < loops.size(); ++loop) {
            const std::optional<Bridge> bridge = shortestBridge(joined, loops, loop, others, resolution);
            if (bridge && bridge->length < best.length) {
                best = *bridge;
            }
        }
        if (best.loop < 0) {
            return std::nullopt;
        }
        const QVector<LoopVertex> loop = loops.takeAt(best.loop);
        const LoopVertex& from = joined.at(best.joinedIndex);
        const LoopVertex& to = loop.at(best.loopIndex);
        QVector<LoopVertex> path = throughColumnPath(rings, from, to);
        QVector<LoopVertex> spliced = joined.mid(0, best.joinedIndex + 1);
        spliced.append(path);
        for (int k = 0; k <= loop.size(); ++k) {
            spliced.append(loop.at((best.loopIndex + k) % loop.size()));
        }
        std::reverse(path.begin(), path.end());
        spliced.append(path);
        spliced.append(joined.mid(best.joinedIndex));
        joined = spliced;
    }
    return joined;
}

// The arm meshes the strip to its mouth only when the two rings at that end
// have frames.
bool hasMeshedMouthEnd(const Chain& arm, bool isStart)
{
    const int count = arm.rings.size();
    if (count < 2) {
        return false;
    }
    const Ring& end = isStart ? arm.rings.at(0) : arm.rings.at(count - 1);
    const Ring& next = isStart ? arm.rings.at(1) : arm.rings.at(count - 2);
    return hasValidFrame(end) && hasValidFrame(next);
}

// An arm whose mouth cannot be zipped closes with a fan over the strip to its
// mouth, its center pulled into the through along the arm so the fan stays
// hidden inside it. A mouth point that lies behind the arm's end ring, where
// a column that missed the through found a far part of it, would turn that
// strip inside out, so such an arm closes on its end ring instead.
void closeWithFan(Chain& arm, bool isStart, const RingTube& through)
{
    capAt(arm, isStart) = EndCap::Fan;
    QVector<MouthPoint>& mouth = isStart ? arm.startMouth : arm.endMouth;
    if (mouth.isEmpty() || arm.rings.isEmpty()) {
        return;
    }
    const Ring& end = junctionRing(arm, isStart);
    const QVector3D tangent = arrivingTangent(arm, isStart);
    const bool isAhead = std::all_of(mouth.cbegin(), mouth.cend(), [&](const MouthPoint& point) {
        return QVector3D::dotProduct(point.position - end.center, tangent) > 0.0f;
    });
    if (!isAhead) {
        mouth.clear();
        return;
    }

    QVector3D centroid;
    for (const MouthPoint& point : std::as_const(mouth)) {
        centroid += point.position;
    }
    centroid /= static_cast<float>(mouth.size());
    QVector3D inside = centroid;
    QVector3D outside = centroid + tangent * static_cast<float>(largestRadius(end) * kFanDepthFactor);
    if (through.inside(outside)) {
        inside = outside;
    } else if (through.inside(centroid)) {
        for (int step = 0; step < kGrowBisections; ++step) {
            const QVector3D middle = (inside + outside) / 2.0f;
            (through.inside(middle) ? inside : outside) = middle;
        }
    }
    (isStart ? arm.startFanCenter : arm.endFanCenter) = inside;
}

// Each mouth vertex's corner after welding: a window boundary vertex between
// the through's end rings within kWeldMeters of it and kWeldQuads of it on
// the through's grid, or else the mouth vertex before it when that close, or
// itself. A mouth vertex welded to a boundary vertex takes the mouth vertices
// welded to it along. Without welds when they would leave the mouth loop
// pinched or with fewer than three vertices.
QVector<ZipCorner> weldedMouthCorners(const QVector<Ring>& rings,
                                      const QVector<Vertex>& boundary,
                                      const QVector<MouthPoint>& mouth,
                                      const QVector<QPointF>& polygon,
                                      int mouthIndex,
                                      int resolution)
{
    const int count = mouth.size();
    QVector<ZipCorner> corners(count);
    for (int k = 0; k < count; ++k) {
        corners[k] = ZipCorner {ZipCorner::kMouthRing, k, mouthIndex};
    }
    const QVector<ZipCorner> unwelded = corners;

    const auto positionOf = [&](const ZipCorner& corner) {
        return corner.ring == ZipCorner::kMouthRing ? mouth.at(corner.column).position : unpulledVertex(rings.at(corner.ring), corner.column);
    };
    const auto parameterOf = [&](const ZipCorner& corner, double reference) {
        return parameterNear(corner.ring == ZipCorner::kMouthRing ? polygon.at(corner.column) : QPointF(corner.ring, corner.column),
                             reference, resolution);
    };
    // A through's end ring may close it or carry its own mouth strip, so a
    // mouth vertex welded there would join two fans.
    const auto isEndRing = [&](int ring) { return ring == 0 || ring == rings.size() - 1; };
    const auto isWeldable = [&](int k, const ZipCorner& corner) {
        return positionOf(corner).distanceToPoint(mouth.at(k).position) <= kWeldMeters
               && isWithinWeldQuads(parameterOf(corner, polygon.at(k).y()), polygon.at(k));
    };

    for (int k = 0; k < count; ++k) {
        double nearest = std::numeric_limits<double>::max();
        for (const Vertex& vertex : boundary) {
            if (isEndRing(vertex.first)) {
                continue;
            }
            const ZipCorner corner {vertex.first, vertex.second};
            const double distance = positionOf(corner).distanceToPoint(mouth.at(k).position);
            if (distance < nearest && isWeldable(k, corner)) {
                nearest = distance;
                corners[k] = corner;
            }
        }
    }

    const auto collapse = [&](int k, int previous) {
        const ZipCorner here = corners.at(k);
        const ZipCorner before = corners.at(previous);
        if (here == before || !isWeldable(k, before)) {
            return;
        }
        if (here.ring == ZipCorner::kMouthRing) {
            corners[k] = before;
        } else if (before.ring == ZipCorner::kMouthRing) {
            std::replace(corners.begin(), corners.end(), before, here);
        }
    };
    for (int k = 1; k < count; ++k) {
        collapse(k, k - 1);
    }
    collapse(0, count - 1);

    // The loop with consecutive repeats removed visits each corner once.
    QVector<ZipCorner> loop;
    for (int k = 0; k < count; ++k) {
        if (corners.at(k) != corners.at((k + count - 1) % count)) {
            loop.append(corners.at(k));
        }
    }
    QSet<QPair<int, int>> visited;
    for (const ZipCorner& corner : std::as_const(loop)) {
        visited.insert({corner.ring, corner.column});
    }
    if (loop.size() < kMinLoopSize || visited.size() != loop.size()) {
        return unwelded;
    }
    return corners;
}

// An arm's mouth placed on its through's grid, with the through quads it
// covers.
struct MouthCut {
    ArmEnd end;
    QUuid tripId;
    QString station;
    QVector<QPointF> polygon;
    QSet<Vertex> removed;
};

// One arm's mouth cut into the through, or why it cannot be zipped: off the
// through, or its columns run the wrong way around it.
struct MouthCutResult {
    std::optional<MouthCut> cut;
    bool isMouthReversed = false;
};

// Places one arm's mouth on the through and finds the quads it covers.
// parameters place each mouth point on the through's grid.
MouthCutResult cutMouth(const Chain& through,
                        const ArmEnd& end,
                        const QUuid& tripId,
                        const QString& station,
                        const QVector<std::optional<QPointF>>& parameters)
{
    const int resolution = through.rings.first().radii.size();
    const std::optional<QVector<QPointF>> polygon = mouthPolygon(parameters, resolution);
    if (!polygon) {
        return {};
    }

    // The arm's strip runs its mouth in column order; the zip band must run
    // it the other way, which needs the columns counter-clockwise around an
    // arm arriving at its end and clockwise around one leaving at its start.
    const bool isColumnOrderCounterClockwise = signedArea(*polygon) > 0.0;
    if (isColumnOrderCounterClockwise == end.isStart) {
        return MouthCutResult {std::nullopt, true};
    }
    return MouthCutResult {MouthCut {end, tripId, station, *polygon, removedQuads(through.rings, *polygon, resolution)}, false};
}

// Whether two mouths cross or one holds the other in (ring, column) space,
// on any turn of the through's columns.
bool mouthsIntersect(const QVector<QPointF>& a, const QVector<QPointF>& b, int resolution)
{
    for (int wrap = -kMaxMouthWraps; wrap <= kMaxMouthWraps; ++wrap) {
        QVector<QPointF> shifted = b;
        for (QPointF& point : shifted) {
            point.ry() += wrap * resolution;
        }
        if (containsPoint(a, shifted.first()) || containsPoint(shifted, a.first())) {
            return true;
        }
        for (int i = 0; i < a.size(); ++i) {
            for (int k = 0; k < shifted.size(); ++k) {
                if (segmentsCross(a.at(i), a.at((i + 1) % a.size()), shifted.at(k), shifted.at((k + 1) % shifted.size()))) {
                    return true;
                }
            }
        }
    }
    return false;
}

// The points of a window's mouths and their places on the through's grid.
struct WindowMouthPoints {
    QVector<const QVector<MouthPoint>*> points;
    QVector<QVector<QPointF>> polygons;

    LoopVertex vertex(const QVector<Ring>& rings, const ZipCorner& corner, double referenceColumn, int resolution) const
    {
        if (corner.ring == ZipCorner::kMouthRing) {
            return LoopVertex {corner, parameterNear(polygons.at(corner.mouth).at(corner.column), referenceColumn, resolution),
                               points.at(corner.mouth)->at(corner.column).position};
        }
        return LoopVertex {corner, QPointF(corner.ring, columnNear(corner.column, referenceColumn, resolution)),
                           unpulledVertex(rings.at(corner.ring), corner.column)};
    }
};

// Mouth number mouthIndex's vertices as the zip band runs them: against the
// arm's column order, so they run the same way round the band as the
// window's outer boundary, with consecutive vertices welded to one corner
// merged.
QVector<LoopVertex> mouthLoop(const QVector<Ring>& rings,
                              const WindowMouthPoints& mouths,
                              int mouthIndex,
                              const QVector<ZipCorner>& corners,
                              bool isArmStart,
                              int resolution)
{
    const QVector<QPointF>& polygon = mouths.polygons.at(mouthIndex);
    const int count = corners.size();
    QVector<LoopVertex> loop;
    loop.reserve(count);
    for (int k = 0; k < count; ++k) {
        const int point = isArmStart ? (count - k) % count : k;
        const ZipCorner& corner = corners.at(point);
        if (!loop.isEmpty() && (loop.last().corner == corner || loop.first().corner == corner)) {
            continue;
        }
        loop.append(mouths.vertex(rings, corner, polygon.at(point).y(), resolution));
    }
    return loop;
}

// Welds each point of mouth number mouthIndex that still stands alone to a
// point of an earlier mouth within kWeldMeters of it and kWeldQuads of it on
// the through's grid, where two mouths touch, taking the points welded to it
// along.
void weldToEarlierMouths(QVector<ZipCorner>& corners,
                         int mouthIndex,
                         const WindowMouthPoints& mouths,
                         const QVector<QVector<ZipCorner>>& earlierCorners,
                         int resolution)
{
    const QVector<MouthPoint>& points = *mouths.points.at(mouthIndex);
    const QVector<QPointF>& polygon = mouths.polygons.at(mouthIndex);
    for (int k = 0; k < corners.size(); ++k) {
        const ZipCorner self {ZipCorner::kMouthRing, k, mouthIndex};
        if (corners.at(k) != self) {
            continue;
        }
        double nearest = kWeldMeters;
        std::optional<ZipCorner> target;
        for (int m = 0; m < earlierCorners.size(); ++m) {
            for (int other = 0; other < earlierCorners.at(m).size(); ++other) {
                const ZipCorner candidate {ZipCorner::kMouthRing, other, m};
                if (earlierCorners.at(m).at(other) != candidate) {
                    continue;
                }
                const double distance = points.at(k).position.distanceToPoint(mouths.points.at(m)->at(other).position);
                const QPointF parameter = parameterNear(mouths.polygons.at(m).at(other), polygon.at(k).y(), resolution);
                if (distance <= nearest && isWithinWeldQuads(parameter, polygon.at(k))) {
                    nearest = distance;
                    target = candidate;
                }
            }
        }
        if (target) {
            std::replace(corners.begin(), corners.end(), self, *target);
        }
    }
}

// The loop without the walks that go out and straight back, as a joined
// loop has where two holes were welded together: a corner repeated at once,
// or one visited between two visits of its neighbor.
QVector<LoopVertex> withoutSpikes(QVector<LoopVertex> loop)
{
    bool changed = true;
    while (changed && loop.size() > kMinLoopSize) {
        changed = false;
        const int count = loop.size();
        for (int i = 0; i < count; ++i) {
            const int next = (i + 1) % count;
            const int after = (i + 2) % count;
            if (loop.at(i).corner == loop.at(next).corner) {
                loop.removeAt(next);
                changed = true;
                break;
            }
            if (loop.at(i).corner == loop.at(after).corner) {
                loop.removeAt(std::max(next, after));
                loop.removeAt(std::min(next, after));
                changed = true;
                break;
            }
        }
    }
    return loop;
}

// A window boundary loop's vertices, their columns unwrapped along the loop.
QVector<LoopVertex> boundaryLoop(const QVector<Ring>& rings, const QVector<Vertex>& boundary, int resolution)
{
    QVector<LoopVertex> loop;
    loop.reserve(boundary.size());
    for (const Vertex& vertex : boundary) {
        const double column = loop.isEmpty() ? vertex.second : columnNear(vertex.second, loop.last().parameter.y(), resolution);
        loop.append(LoopVertex {ZipCorner {vertex.first, vertex.second}, QPointF(vertex.first, column),
                                unpulledVertex(rings.at(vertex.first), vertex.second)});
    }
    return loop;
}

// Whether the loop runs all the way around the through's columns.
bool wrapsAround(const QVector<LoopVertex>& loop, int resolution)
{
    const double closingColumn = columnNear(loop.first().parameter.y(), loop.last().parameter.y(), resolution);
    return std::abs(closingColumn - loop.first().parameter.y()) > resolution / 2.0;
}

// Whether the zip band lies outside the boundary loop: a loop that runs
// clockwise around kept quads, an island of through inside the window. The
// band lies inside a loop counter-clockwise around the hole and between the
// two end loops of a hole that takes the whole circumference.
bool isIsland(const QVector<LoopVertex>& loop, int resolution)
{
    if (wrapsAround(loop, resolution)) {
        return false;
    }
    QVector<QPointF> polygon;
    polygon.reserve(loop.size());
    for (const LoopVertex& vertex : loop) {
        polygon.append(vertex.parameter);
    }
    return signedArea(polygon) < 0.0;
}

// The window that zips the mouths of members into the through: the union of
// their quads, its boundary loops and one zip band from the outer boundary
// to the other loops and the mouths, joined into one inner loop. nullopt
// when the union pinches or a loop cannot be joined.
std::optional<JunctionWindow> zippedWindow(const Chain& through,
                                           const QVector<Chain>& chains,
                                           const QVector<MouthCut>& cuts,
                                           const QVector<int>& members)
{
    const QVector<Ring>& rings = through.rings;
    const int resolution = rings.first().radii.size();
    QSet<Vertex> removed;
    for (int member : members) {
        removed.unite(cuts.at(member).removed);
    }
    closeRemovedQuads(removed, rings.size(), resolution);
    const std::optional<QVector<QVector<Vertex>>> boundaries = windowBoundaries(removed, resolution);
    if (!boundaries) {
        return std::nullopt;
    }
    QVector<Vertex> boundaryVertices;
    for (const QVector<Vertex>& boundary : *boundaries) {
        boundaryVertices.append(boundary);
    }

    JunctionWindow window;
    window.removedQuads = removed;
    window.boundaries = *boundaries;
    window.tripId = cuts.at(members.first()).tripId;
    WindowMouthPoints mouths;
    for (int member : members) {
        const MouthCut& cut = cuts.at(member);
        const Chain& arm = chains.at(cut.end.chain);
        mouths.points.append(cut.end.isStart ? &arm.startMouth : &arm.endMouth);
        mouths.polygons.append(cut.polygon);
    }
    QVector<QVector<ZipCorner>> mouthCorners;
    QVector<QVector<LoopVertex>> holes;
    for (int m = 0; m < members.size(); ++m) {
        const MouthCut& cut = cuts.at(members.at(m));
        QVector<ZipCorner> corners = weldedMouthCorners(rings, boundaryVertices, *mouths.points.at(m), cut.polygon, m, resolution);
        weldToEarlierMouths(corners, m, mouths, mouthCorners, resolution);
        holes.append(mouthLoop(rings, mouths, m, corners, cut.end.isStart, resolution));
        window.mouths.append(WindowMouth {cut.end.chain, cut.end.isStart, corners, cut.tripId});
        mouthCorners.append(corners);
    }

    // The band lies left of the outer loops and right of the holes: the
    // mouths and the islands, run backward.
    QVector<QVector<LoopVertex>> outers;
    for (const QVector<Vertex>& boundary : *boundaries) {
        QVector<LoopVertex> loop = boundaryLoop(rings, boundary, resolution);
        if (isIsland(loop, resolution)) {
            std::reverse(loop.begin(), loop.end());
            holes.append(loop);
        } else {
            outers.append(loop);
        }
    }
    if (outers.isEmpty()) {
        return std::nullopt;
    }

    const bool isSingleMouth = holes.size() == 1 && outers.size() == 1;
    QVector<const QVector<LoopVertex>*> outerLoops;
    for (const QVector<LoopVertex>& loop : std::as_const(outers)) {
        outerLoops.append(&loop);
    }
    std::optional<QVector<LoopVertex>> inner = joinedLoop({}, holes, outerLoops, resolution);
    if (!inner) {
        return std::nullopt;
    }
    inner = withoutSpikes(*inner);
    const std::optional<QVector<LoopVertex>> outer = joinedLoop(rings, outers, {&*inner}, resolution);
    if (!outer) {
        return std::nullopt;
    }
    const int offset = isSingleMouth ? centroidStartOffset(*outer, *inner, resolution) : closestStartOffset(*outer, *inner, resolution);
    window.zip = zipBand(rings, *outer, *inner, offset, resolution);
    return window;
}

using ArmKey = QPair<int, bool>;

ArmKey armKey(const ArmEnd& end)
{
    return {end.chain, end.isStart};
}

// An arm cut back to its mouth, with each mouth point's place on the
// through's grid.
struct TrimmedArm {
    ArmEnd end;
    QVector<std::optional<QPointF>> parameters;
};

// The arms each junction's growth and trims left with a mouth, and how many
// rings each chain lost from its start, which shifts the ring parameters of
// mouths on it.
struct Stitch {
    QVector<QVector<TrimmedArm>> trimmedArms;
    QVector<int> removedFromStart;
};

// Grows each junction's through to take its arms' sections, then trims the
// arms against it. The arms in leftOut are left out of the growth and the
// trims; an arm with no ring outside the through is left out and added to
// leftOut.
Stitch growAndTrim(const QVector<Leg>& legs,
                   QVector<Chain>& chains,
                   const QVector<Junction>& junctions,
                   const QVector<double>& spacings,
                   QSet<ArmKey>& leftOut,
                   QVector<Warning>& warnings)
{
    QVector<JunctionSolids> solids;
    solids.reserve(junctions.size());
    for (const Junction& junction : junctions) {
        Junction kept = junction;
        kept.arms.removeIf([&](const ArmEnd& end) { return leftOut.contains(armKey(end)); });
        solids.append(junctionSolids(chains, kept));
    }

    for (int j = 0; j < junctions.size(); ++j) {
        growThrough(chains[junctions.at(j).through], solids.at(j));
    }
    for (int j = 0; j < junctions.size(); ++j) {
        solids[j].grownThroughRings = unionThroughRings(chains.at(junctions.at(j).through).rings,
                                                        junctions.at(j).position, solids.at(j).unionReach);
    }

    // Before any trim shortens a through that is also an arm.
    QVector<QVector<SurfaceTriangle>> throughTriangles;
    throughTriangles.reserve(junctions.size());
    for (int j = 0; j < junctions.size(); ++j) {
        const int first = solids.at(j).throughFirst;
        throughTriangles.append(surfaceTriangles(chains.at(junctions.at(j).through), first, first + solids.at(j).grownThroughRings.size() - 1));
    }

    Stitch stitch {QVector<QVector<TrimmedArm>>(junctions.size()), QVector<int>(chains.size(), 0)};
    for (int j = 0; j < junctions.size(); ++j) {
        const JunctionSolids& junctionSolids = solids.at(j);
        for (const ArmEnd& end : junctions.at(j).arms) {
            Chain& arm = chains[end.chain];
            const int firstRing = armFirstRing(arm, end, junctions.at(j));
            if (leftOut.contains(armKey(end))) {
                leaveOutArm(legs, arm, end.isStart, firstRing, warnings);
                continue;
            }
            const QVector<Ring> rings = ringsTowardJunction(arm, end.isStart).mid(firstRing);
            if (rings.size() < 2) {
                continue;
            }
            const Trim trim = trimArm(rings, arrivingTangent(arm, end.isStart), junctionSolids, throughTriangles.at(j), firstRing > 0,
                                       spacings.at(j));
            if (trim.keptRings <= 0) {
                leaveOutArm(legs, arm, end.isStart, firstRing, warnings);
                leftOut.insert(armKey(end));
                continue;
            }
            stitch.removedFromStart[end.chain] += cutArm(arm, end.isStart, firstRing, trim);
            stitch.trimmedArms[j].append(TrimmedArm {end, trim.parameters});
        }
    }
    return stitch;
}

// Moves each mouth point of the window welded to a through vertex onto it,
// and each one welded to another mouth point onto that.
void applyWelds(QVector<Chain>& chains, const Chain& through, const JunctionWindow& window)
{
    const auto mouthOf = [&](const WindowMouth& windowMouth) -> QVector<MouthPoint>& {
        Chain& arm = chains[windowMouth.armChain];
        return windowMouth.isArmStart ? arm.startMouth : arm.endMouth;
    };
    QVector<QVector<QVector3D>> welded;
    for (const WindowMouth& windowMouth : window.mouths) {
        QVector<QVector3D> positions;
        positions.reserve(windowMouth.mouthCorners.size());
        for (const ZipCorner& corner : windowMouth.mouthCorners) {
            positions.append(corner.ring == ZipCorner::kMouthRing ? mouthOf(window.mouths.at(corner.mouth)).at(corner.column).position
                                                                   : unpulledVertex(through.rings.at(corner.ring), corner.column));
        }
        welded.append(positions);
    }
    for (int m = 0; m < window.mouths.size(); ++m) {
        QVector<MouthPoint>& mouth = mouthOf(window.mouths.at(m));
        for (int k = 0; k < mouth.size(); ++k) {
            mouth[k].position = welded.at(m).at(k);
        }
    }
}

// The windows of one through chain: each mouth joins the windows whose
// quads its own share, as long as it crosses none of their mouths; a mouth
// that does, or whose window cannot be zipped, closes with a fan inside the
// through.
void zipWindows(const QVector<Leg>& legs, QVector<Chain>& chains, int throughIndex, const QVector<MouthCut>& cuts, QVector<Warning>& warnings)
{
    struct Group {
        QVector<int> members;
        JunctionWindow window;
    };
    Chain& through = chains[throughIndex];
    const RingTube throughTube {&through.rings, {}};
    const int resolution = through.rings.first().radii.size();
    QVector<Group> groups;
    for (int c = 0; c < cuts.size(); ++c) {
        const MouthCut& cut = cuts.at(c);
        Chain& arm = chains[cut.end.chain];

        QSet<Vertex> removed = cut.removed;
        QVector<int> sharing;
        bool hasGrown = true;
        while (hasGrown) {
            hasGrown = false;
            for (int g = 0; g < groups.size(); ++g) {
                if (!sharing.contains(g) && groups.at(g).window.removedQuads.intersects(removed)) {
                    sharing.append(g);
                    removed.unite(groups.at(g).window.removedQuads);
                    hasGrown = true;
                }
            }
        }
        QVector<int> members;
        for (int g : std::as_const(sharing)) {
            members.append(groups.at(g).members);
        }
        const bool runsIntoAnother = std::any_of(members.cbegin(), members.cend(), [&](int member) {
            return mouthsIntersect(cut.polygon, cuts.at(member).polygon, resolution);
        });
        if (runsIntoAnother) {
            warnings.append(Warning {
                Warning::JunctionOverlap,
                cut.tripId,
                cut.station,
                QStringLiteral("Two passages open into station %1 and run into each other; the tube ends one inside the other.").arg(cut.station)
            });
            closeWithFan(arm, cut.end.isStart, throughTube);
            continue;
        }

        members.append(c);
        std::sort(members.begin(), members.end());
        const std::optional<JunctionWindow> window = zippedWindow(through, chains, cuts, members);
        if (!window) {
            warnings.append(armWarning(legs, arm, cut.end.isStart, Warning::JunctionUnzipped,
                                       QStringLiteral("The passage from station %1 could not be joined to the passage it opens into; the tube ends it inside that passage.")));
            closeWithFan(arm, cut.end.isStart, throughTube);
            continue;
        }
        std::sort(sharing.begin(), sharing.end(), std::greater<int>());
        for (int g : std::as_const(sharing)) {
            groups.removeAt(g);
        }
        groups.append(Group {members, *window});
    }

    for (const Group& group : std::as_const(groups)) {
        applyWelds(chains, through, group.window);
        through.windows.append(group.window);
    }
}

} // namespace

bool cwPassageTubeJunction::RingTube::inside(const QVector3D& point, double tolerance) const
{
    const std::optional<double> distance = wallDistance(point);
    return distance && *distance <= tolerance;
}

std::optional<double> cwPassageTubeJunction::RingTube::wallDistance(const QVector3D& point) const
{
    if (rings == nullptr || rings->isEmpty()) {
        return std::nullopt;
    }
    const Slab slab = slabOf(*rings, point, 0, rings->size() - 1, extendTangent);
    if (slab.segment < 0) {
        return std::nullopt;
    }
    const auto distanceFrom = [&](int ring) {
        const QPointF local = localCoordinates(point, ring);
        return std::hypot(local.x(), local.y()) - polarRadius(rings->at(ring), std::atan2(local.y(), local.x()));
    };
    const bool isPastLastRing = slab.segment == rings->size() - 1;
    if (isPastLastRing) {
        return distanceFrom(slab.segment);
    }
    return distanceFrom(slab.segment) * (1.0 - slab.fraction) + distanceFrom(slab.segment + 1) * slab.fraction;
}

int cwPassageTubeJunction::RingTube::nearestRing(const QVector3D& point) const
{
    if (rings == nullptr || rings->isEmpty()) {
        return -1;
    }
    const Slab slab = slabOf(*rings, point, 0, rings->size() - 1, extendTangent);
    if (slab.segment < 0) {
        return -1;
    }
    return slab.fraction <= 0.5 ? slab.segment : slab.segment + 1;
}

QPointF cwPassageTubeJunction::RingTube::localCoordinates(const QVector3D& point, int ring) const
{
    return localOf(rings->at(ring), point);
}

void cwPassageTubeJunction::stitchJunctions(const QVector<Leg>& legs,
                                            QVector<Chain>& chains,
                                            const Options& options,
                                            QVector<Warning>& warnings)
{
    QVector<Junction> junctions = findJunctions(legs, chains);
    junctions.removeIf([&](const Junction& junction) {
        const bool isValid = hasValidJunction(chains, junction);
        if (!isValid) {
            for (const ArmEnd& end : junction.arms) {
                capAt(chains[end.chain], end.isStart) = EndCap::Fan;
            }
        }
        return !isValid;
    });

    QVector<double> spacings;
    spacings.reserve(junctions.size());
    for (const Junction& junction : std::as_const(junctions)) {
        Chain& through = chains[junction.through];
        const QVector<ArmGeometry> arms = armGeometries(chains, junction);
        const Ring& throughRing = through.rings.at(junctionRingIndex(through.rings, junction.position));
        const double reach = junctionReach(throughRing.frame.tangent, arms);
        spacings.append(std::max(junctionSpacing(options, throughRing, arms), 2.0 * reach / kMaxJunctionRings));
        densify(through, junction.position, reach, spacings.last());
    }

    // An arm left out after the growth leaves the through grown where it
    // poked out, so the through grows again without it.
    const QVector<Chain> densified = chains;
    const qsizetype warningCount = warnings.size();
    QSet<ArmKey> leftOut;
    Stitch stitch = growAndTrim(legs, chains, junctions, spacings, leftOut, warnings);
    if (!leftOut.isEmpty()) {
        chains = densified;
        warnings.resize(warningCount);
        stitch = growAndTrim(legs, chains, junctions, spacings, leftOut, warnings);
    }

    QVector<QVector<MouthCut>> cutsByThrough(chains.size());
    for (int j = 0; j < junctions.size(); ++j) {
        const Junction& junction = junctions.at(j);
        Chain& through = chains[junction.through];
        const RingTube throughTube {&through.rings, {}};
        QVector<TrimmedArm> arms = stitch.trimmedArms.at(j);
        if (through.rings.size() < 2) {
            for (const TrimmedArm& trimmed : std::as_const(arms)) {
                closeWithFan(chains[trimmed.end.chain], trimmed.end.isStart, throughTube);
            }
            continue;
        }
        std::sort(arms.begin(), arms.end(), [](const TrimmedArm& a, const TrimmedArm& b) { return a.end.chain < b.end.chain; });

        for (const TrimmedArm& trimmed : std::as_const(arms)) {
            const ArmEnd& end = trimmed.end;
            Chain& arm = chains[end.chain];
            // An arm joining two passages can be left inside the second, or
            // with too little between its two mouths to mesh.
            if (arm.rings.isEmpty()) {
                continue;
            }
            if (!hasMeshedMouthEnd(arm, end.isStart)) {
                dropArm(legs, arm, end.isStart,
                        QStringLiteral("The passage from station %1 lies inside the passages it joins; the tube leaves it out."), warnings);
                for (QVector<MouthCut>& cuts : cutsByThrough) {
                    cuts.removeIf([&](const MouthCut& cut) { return cut.end.chain == end.chain; });
                }
                continue;
            }
            QVector<std::optional<QPointF>> parameters = trimmed.parameters;
            for (std::optional<QPointF>& parameter : parameters) {
                if (parameter) {
                    parameter->rx() -= stitch.removedFromStart.at(junction.through);
                }
            }
            const MouthCutResult result = cutMouth(through, end, junctionTrip(legs, arm, end.isStart), junction.station, parameters);
            if (result.cut) {
                cutsByThrough[junction.through].append(*result.cut);
                continue;
            }
            if (result.isMouthReversed) {
                warnings.append(armWarning(legs, arm, end.isStart, Warning::JunctionUnzipped,
                                           QStringLiteral("The mouth of the passage from station %1 runs backward around the passage it joins; the tube ends it inside that passage.")));
            } else {
                warnings.append(armWarning(legs, arm, end.isStart, Warning::JunctionUnzipped,
                                           QStringLiteral("The passage from station %1 could not be joined to the passage it opens into; the tube ends it inside that passage.")));
            }
            closeWithFan(arm, end.isStart, throughTube);
        }
    }

    for (int t = 0; t < chains.size(); ++t) {
        if (!cutsByThrough.at(t).isEmpty()) {
            zipWindows(legs, chains, t, cutsByThrough.at(t), warnings);
        }
    }
}
