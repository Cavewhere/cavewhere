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
using ZipCorner = Builder::ZipCorner;
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
// How far, in quads, the window reaches past the mouth, so its edge stays
// clear of the mouth and the zip band keeps a width.
constexpr double kWindowMarginQuads = 0.25;
// Zip triangles that fold over or are slivers cost this much more than any
// bridging edge, so the band takes them only when it has no other way round.
constexpr double kFoldedZipPenalty = 1e6;
constexpr double kSliverZipPenalty = 1e3;
constexpr double kMaxZipAspectRatio = 20.0;
constexpr float kDegenerateLengthSquared = 1e-12f;
constexpr double kParallelEpsilon = 1e-12;
// A point this close to a through wall counts as on it, so a column that
// starts on the wall can grow past it.
constexpr double kOnWallMeters = 1e-5;
constexpr double kMaxCarryDegrees = 45.0;
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

// The polar angle theta as a fractional bin index around the ring.
double binPosition(const Ring& ring, double theta)
{
    const BinSpan span = binSpan(ring, theta);
    return span.low + span.fraction;
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

// A ring square to the chord from a to b, fraction of the way along it, with
// radii and up blended between the two.
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

    const QVector3D tangent = (b.center - a.center).normalized();
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

// Whether two rings face the same way, as they do along a straight leg; at a
// bend's corner rings inserted between them would crease its inner wall.
bool isParallel(const Ring& a, const Ring& b)
{
    return QVector3D::dotProduct(a.frame.tangent, b.frame.tangent) >= kParallelRingCosine;
}

// Inserts rings between the through rings around the junction so no two
// within reach lie farther apart than spacing. A ring that would fold
// against its neighbors, as one near a sharp bend's corner would, is left
// out.
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
        if (!overlapsReach || !hasValidFrame(ring) || !hasValidFrame(next) || !isParallel(ring, next)) {
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

// The through rings the union reads. It reaches past the growth: a grazing
// arm's far wall meets the through well beyond the arm's own radius.
ThroughSpan unionSpan(const QVector<Ring>& rings, const QVector3D& junction, double reach)
{
    return ThroughSpan(rings, junction, kUnionReachFactor * reach);
}

QVector<Ring> unionThroughRings(const QVector<Ring>& rings, const QVector3D& junction, double reach)
{
    const ThroughSpan span = unionSpan(rings, junction, reach);
    return rings.mid(span.first, span.last - span.first + 1);
}

JunctionSolids junctionSolids(const QVector<Chain>& chains, const Junction& junction)
{
    JunctionSolids solids;
    const Chain& through = chains.at(junction.through);
    solids.arms = armGeometries(chains, junction);
    solids.position = junction.position;
    solids.throughTangent = through.rings.at(junctionRingIndex(through.rings, junction.position)).frame.tangent;
    solids.reach = junctionReach(solids.throughTangent, solids.arms);

    solids.throughRings = unionThroughRings(through.rings, junction.position, solids.reach);
    solids.throughFirst = unionSpan(through.rings, junction.position, solids.reach).first;

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

// The arm cut where it enters the grown through tube: its mouth, one point
// per column, and how many of its rings, counted from the far end, stay
// entirely outside.
struct Trim {
    QVector<QVector3D> mouth;
    int keptRings = 0;
};

// An arm that is also the through chain starts on the through, so it is cut
// where it comes back into the through rather than where it starts there.
Trim trimArm(const QVector<Ring>& rings, const QVector3D& tangent, const JunctionSolids& solids, bool startsOnThrough)
{
    const RingTube grownTube {&solids.grownThroughRings, {}};
    const auto inGrownThrough = [&](const QVector3D& point) {
        return solids.isNear(point) && grownTube.inside(point, kJunctionTolerance);
    };
    const auto crossing = [&](QVector3D outside, QVector3D inside) {
        for (int step = 0; step < kGrowBisections; ++step) {
            const QVector3D middle = (outside + inside) / 2.0f;
            (inGrownThrough(middle) ? inside : outside) = middle;
        }
        return (outside + inside) / 2.0f;
    };

    // Moves a point along its ring's radial direction onto the wall: the
    // crossing of the wall between the ring's axis and well past the point.
    const auto ontoThroughWall = [&](const QVector3D& point) {
        const int index = grownTube.nearestRing(point);
        const std::optional<double> distance = grownTube.wallDistance(point);
        if (index < 0 || !distance || std::abs(*distance) <= kJunctionTolerance) {
            return point;
        }
        const Ring& ring = solids.grownThroughRings.at(index);
        const QVector3D offset = point - ring.center;
        const QVector3D axisPoint = ring.center + ring.frame.tangent * QVector3D::dotProduct(offset, ring.frame.tangent);
        const QVector3D radial = point - axisPoint;
        if (radial.length() < kMinRingSpacingMeters) {
            return point;
        }
        QVector3D inside = axisPoint;
        QVector3D outside = axisPoint + radial * static_cast<float>(kWallSearchFactor);
        if (!grownTube.inside(inside) || grownTube.inside(outside, kJunctionTolerance)) {
            return point;
        }
        for (int step = 0; step < kGrowBisections; ++step) {
            const QVector3D middle = (inside + outside) / 2.0f;
            (grownTube.inside(middle, kJunctionTolerance) ? inside : outside) = middle;
        }
        return (inside + outside) / 2.0f;
    };

    const int count = rings.size();
    const int resolution = rings.last().radii.size();
    const int extensionSteps = static_cast<int>(std::ceil(solids.reach / kGrowStep));
    Trim trim;
    trim.mouth.resize(resolution);
    trim.keptRings = count;

    for (int bin = 0; bin < resolution; ++bin) {
        QVector<QVector3D> polyline;
        polyline.reserve(count + extensionSteps);
        for (const Ring& ring : rings) {
            polyline.append(unpulledVertex(ring, bin));
        }
        for (int step = 1; step <= extensionSteps; ++step) {
            polyline.append(polyline.at(count - 1) + tangent * static_cast<float>(step * kGrowStep));
        }

        // Rings up to lastOutside keep this column outside the through. A
        // column that misses the through ends on its wall, level with its
        // own last vertex.
        int lastOutside = count - 2;
        trim.mouth[bin] = ontoThroughWall(polyline.at(count - 1));
        const auto findEntry = [&]() {
            bool hasLeftThrough = !inGrownThrough(polyline.first());
            if (!hasLeftThrough && !startsOnThrough) {
                lastOutside = -1;
                trim.mouth[bin] = polyline.first();
                return;
            }
            for (int index = 1; index < polyline.size(); ++index) {
                const QVector3D from = polyline.at(index - 1);
                const QVector3D to = polyline.at(index);
                const int steps = std::max(1, static_cast<int>(std::ceil(from.distanceToPoint(to) / kGrowStep)));
                QVector3D previous = from;
                for (int step = 1; step <= steps; ++step) {
                    const QVector3D sample = from + (to - from) * (static_cast<float>(step) / steps);
                    const bool isInside = inGrownThrough(sample);
                    if (isInside && hasLeftThrough) {
                        trim.mouth[bin] = ontoThroughWall(crossing(previous, sample));
                        lastOutside = std::min(index - 1, count - 1);
                        return;
                    }
                    hasLeftThrough = hasLeftThrough || !isInside;
                    previous = sample;
                }
            }
        };
        findEntry();
        trim.keptRings = std::min(trim.keptRings, lastOutside + 1);
    }

    // A kept ring right at the mouth would leave a strip of slivers.
    const auto isAtMouth = [&](int ring) {
        for (int bin = 0; bin < resolution; ++bin) {
            if (unpulledVertex(rings.at(ring), bin).distanceToPoint(trim.mouth.at(bin)) < kMinRingSpacingMeters) {
                return true;
            }
        }
        return false;
    };
    while (trim.keptRings > 1 && isAtMouth(trim.keptRings - 1)) {
        --trim.keptRings;
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

// Cuts the arm back to the rings outside the union and gives it the mouth.
// Returns false when no ring stays outside: the arm lies inside the through
// and is left out, or, when it is the through itself, keeps a fan.
bool applyTrim(const QVector<Leg>& legs, Chain& arm, const ArmGeometry& geometry, const JunctionSolids& solids, QVector<Warning>& warnings)
{
    const bool isStart = geometry.end.isStart;
    const QVector<Ring> rings = ringsTowardJunction(arm, isStart).mid(geometry.firstRing);
    if (rings.size() < 2) {
        return false;
    }
    const Trim trim = trimArm(rings, arrivingTangent(arm, isStart), solids, geometry.firstRing > 0);
    if (trim.keptRings <= 0) {
        if (geometry.firstRing > 0) {
            capAt(arm, isStart) = EndCap::Fan;
        } else {
            dropArm(legs, arm, isStart,
                    QStringLiteral("The passage from station %1 lies inside the passage it joins; the tube leaves it out."), warnings);
        }
        return false;
    }
    const int kept = geometry.firstRing + trim.keptRings;
    const int dropped = arm.rings.size() - kept;
    if (isStart) {
        arm.rings.remove(0, dropped);
        arm.startMouth = trim.mouth;
    } else {
        arm.rings.remove(kept, dropped);
        arm.endMouth = trim.mouth;
    }
    return true;
}

// A point on the through surface as (ring, column): ring fractional between
// neighbors, column fractional from its angle around the rings.
std::optional<QPointF> surfaceParameter(const QVector<Ring>& rings, const ThroughSpan& span, const QVector3D& point)
{
    const Slab slab = slabOf(rings, point, span.first, span.last, QVector3D());
    if (slab.segment < 0) {
        return std::nullopt;
    }
    const Ring& a = rings.at(slab.segment);
    const Ring& b = rings.at(slab.segment + 1);
    const QPointF localA = localOf(a, point);
    const QPointF localB = localOf(b, point);
    const int resolution = a.radii.size();
    const double columnA = binPosition(a, std::atan2(localA.y(), localA.x()));
    const double columnB = binPosition(b, std::atan2(localB.y(), localB.x()));
    const double nearColumnB = columnB + resolution * std::round((columnA - columnB) / resolution);
    const double column = std::fmod(columnA + (nearColumnB - columnA) * slab.fraction + resolution, resolution);
    return QPointF(slab.segment + slab.fraction, column);
}

// The mouth in (ring, column) space, unwrapped across the column seam so it
// reads as one closed polygon; nullopt when a point lies off the through or
// the mouth wraps all the way around it.
std::optional<QVector<QPointF>> mouthPolygon(const QVector<Ring>& rings, const ThroughSpan& span, const QVector<QVector3D>& mouth)
{
    const int resolution = rings.at(span.junctionRing).radii.size();
    QVector<QPointF> polygon;
    polygon.reserve(mouth.size());
    for (const QVector3D& point : mouth) {
        const std::optional<QPointF> parameter = surfaceParameter(rings, span, point);
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

// The through quads the mouth covers, widened so each column's removed
// quads run unbroken and no two removed quads meet only at a corner.
QSet<Vertex> removedQuads(const QVector<Ring>& rings, const ThroughSpan& span, const QVector<QPointF>& mouth)
{
    const int resolution = rings.at(span.junctionRing).radii.size();
    QSet<Vertex> removed;
    for (int i = span.first; i < span.last; ++i) {
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

    const auto wrapped = [resolution](int column) { return (column + resolution) % resolution; };
    bool changed = true;
    while (changed) {
        changed = false;
        for (int column = 0; column < resolution; ++column) {
            int low = span.last;
            int high = span.first - 1;
            for (int i = span.first; i < span.last; ++i) {
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
        for (int i = span.first + 1; i < span.last; ++i) {
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
    return removed;
}

// The edge of the removed quads as one loop of through vertices,
// counter-clockwise in (ring, column) space, or nullopt when the quads leave
// more than one edge loop or pinch at a vertex.
std::optional<QVector<Vertex>> windowBoundary(const QSet<Vertex>& removed, int resolution)
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

    // Start at the lowest vertex so the loop is the same on every run.
    Vertex start = nextVertex.cbegin().key();
    for (auto it = nextVertex.cbegin(); it != nextVertex.cend(); ++it) {
        start = std::min(start, it.key());
    }
    QVector<Vertex> boundary {start};
    Vertex current = nextVertex.value(start);
    while (current != start) {
        if (boundary.size() > nextVertex.size() || !nextVertex.contains(current)) {
            return std::nullopt;
        }
        boundary.append(current);
        current = nextVertex.value(current);
    }
    if (boundary.size() != nextVertex.size()) {
        return std::nullopt;
    }
    return boundary;
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
    if (area <= 0.0 || longest * longest / (2.0 * area) > kMaxZipAspectRatio) {
        cost += kSliverZipPenalty;
    }
    return cost;
}

// Triangulates the band between the window boundary and the mouth, both
// counter-clockwise in (ring, column) space, starting from the boundary
// vertex in the direction of the first mouth vertex from the mouth's
// centroid. Of all the ways to step around the two loops, it takes the one
// with the least total cost (Fuchs, Kedem and Uselton 1977): the shortest
// bridging edges, with unfolded, well-shaped triangles.
QVector<std::array<ZipCorner, 3>> zipBand(const QVector<Ring>& rings,
                                          const QVector<Vertex>& boundary,
                                          const QVector<QVector3D>& mouth,
                                          const QVector<QPointF>& mouthPolygon,
                                          const QVector<int>& mouthOrder,
                                          int resolution)
{
    QPointF center;
    for (const QPointF& point : mouthPolygon) {
        center += point;
    }
    center /= mouthPolygon.size();

    const int mouthCount = mouthOrder.size();
    const int boundaryCount = boundary.size();
    const auto boundaryParameter = [&](const Vertex& vertex) {
        const double column = vertex.second + resolution * std::round((center.y() - vertex.second) / resolution);
        return QPointF(vertex.first, column);
    };
    const auto angleAbout = [&](const QPointF& point) {
        const QPointF toward = point - center;
        return std::atan2(toward.y(), toward.x());
    };

    const double startAngle = angleAbout(mouthPolygon.at(mouthOrder.first()));
    int offset = 0;
    double offsetGap = std::numeric_limits<double>::max();
    for (int j = 0; j < boundaryCount; ++j) {
        const double gap = std::abs(unwrapped(angleAbout(boundaryParameter(boundary.at(j))), startAngle) - startAngle);
        if (gap < offsetGap) {
            offsetGap = gap;
            offset = j;
        }
    }

    const auto mouthAt = [&](int i) { return mouthPolygon.at(mouthOrder.at(i % mouthCount)); };
    const auto mouth3d = [&](int i) { return mouth.at(mouthOrder.at(i % mouthCount)); };
    const auto boundaryVertex = [&](int j) { return boundary.at((j + offset) % boundaryCount); };
    const auto boundaryAt = [&](int j) { return boundaryParameter(boundaryVertex(j)); };
    const auto boundary3d = [&](int j) {
        const Vertex& vertex = boundaryVertex(j);
        return unpulledVertex(rings.at(vertex.first), vertex.second);
    };
    // The through's outward direction at a boundary vertex.
    const auto outwardAt = [&](int j) {
        const Vertex& vertex = boundaryVertex(j);
        return binDirection(rings.at(vertex.first), vertex.second);
    };

    // State (i, j) has bridged mouth vertex i to boundary vertex j. The band
    // either opens with a boundary step and closes with a mouth step, or the
    // other way round, so no bridge between the two loops' first vertices is
    // made twice; it takes the cheaper of the two.
    const int columns = boundaryCount + 1;
    const auto stateIndex = [columns](int i, int j) { return i * columns + j; };
    struct Path {
        QVector<double> cost;
        QVector<bool> cameByBoundaryStep;
    };
    const auto cheapestPath = [&](bool opensWithBoundaryStep) {
        const auto isAllowed = [&](int i, int j) {
            if (opensWithBoundaryStep) {
                return !(j == 0 && i > 0) && !(i == mouthCount && j < boundaryCount);
            }
            return !(i == 0 && j > 0) && !(j == boundaryCount && i < mouthCount);
        };
        Path path {QVector<double>((mouthCount + 1) * columns, std::numeric_limits<double>::max()),
                   QVector<bool>((mouthCount + 1) * columns, false)};
        path.cost[0] = 0.0;
        for (int i = 0; i <= mouthCount; ++i) {
            for (int j = 0; j <= boundaryCount; ++j) {
                const double here = path.cost.at(stateIndex(i, j));
                if (here == std::numeric_limits<double>::max()) {
                    continue;
                }
                if (j < boundaryCount && isAllowed(i, j + 1)) {
                    const double step = here + zipTriangleCost(boundaryAt(j), boundaryAt(j + 1), mouthAt(i),
                                                               boundary3d(j), boundary3d(j + 1), mouth3d(i),
                                                               outwardAt(j) + outwardAt(j + 1),
                                                               mouth3d(i).distanceToPoint(boundary3d(j + 1)));
                    if (step < path.cost.at(stateIndex(i, j + 1))) {
                        path.cost[stateIndex(i, j + 1)] = step;
                        path.cameByBoundaryStep[stateIndex(i, j + 1)] = true;
                    }
                }
                if (i < mouthCount && isAllowed(i + 1, j)) {
                    const double step = here + zipTriangleCost(mouthAt(i), boundaryAt(j), mouthAt(i + 1),
                                                               mouth3d(i), boundary3d(j), mouth3d(i + 1),
                                                               outwardAt(j),
                                                               mouth3d(i + 1).distanceToPoint(boundary3d(j)));
                    if (step < path.cost.at(stateIndex(i + 1, j))) {
                        path.cost[stateIndex(i + 1, j)] = step;
                        path.cameByBoundaryStep[stateIndex(i + 1, j)] = false;
                    }
                }
            }
        }
        return path;
    };
    const Path opensWithBoundary = cheapestPath(true);
    const Path opensWithMouth = cheapestPath(false);
    const int last = stateIndex(mouthCount, boundaryCount);
    const QVector<bool>& cameByBoundaryStep = opensWithBoundary.cost.at(last) <= opensWithMouth.cost.at(last)
                                                  ? opensWithBoundary.cameByBoundaryStep
                                                  : opensWithMouth.cameByBoundaryStep;

    const auto boundaryCorner = [&](int j) {
        const Vertex& vertex = boundaryVertex(j);
        return ZipCorner {vertex.first, vertex.second};
    };
    const auto mouthCorner = [&](int i) {
        return ZipCorner {ZipCorner::kMouthRing, mouthOrder.at(i % mouthCount)};
    };

    QVector<std::array<ZipCorner, 3>> triangles;
    triangles.reserve(mouthCount + boundaryCount);
    int i = mouthCount;
    int j = boundaryCount;
    while (i > 0 || j > 0) {
        if (cameByBoundaryStep.at(stateIndex(i, j))) {
            --j;
            triangles.append({boundaryCorner(j), boundaryCorner(j + 1), mouthCorner(i)});
        } else {
            --i;
            triangles.append({mouthCorner(i), boundaryCorner(j), mouthCorner(i + 1)});
        }
    }
    std::reverse(triangles.begin(), triangles.end());
    return triangles;
}

// Whether the window shares a quad with any of windows. Windows that only
// touch at a vertex or along an edge stay one surface once both are zipped.
bool overlapsWindows(const QVector<JunctionWindow>& windows, const JunctionWindow& window)
{
    return std::any_of(windows.cbegin(), windows.cend(), [&](const JunctionWindow& other) {
        return other.removedQuads.intersects(window.removedQuads);
    });
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
// mouth. A mouth point that lies behind the arm's end ring, where a column
// that missed the through found a far part of it, would turn that strip
// inside out, so such an arm closes on its end ring instead.
void closeWithFan(Chain& arm, bool isStart)
{
    capAt(arm, isStart) = EndCap::Fan;
    QVector<QVector3D>& mouth = isStart ? arm.startMouth : arm.endMouth;
    if (mouth.isEmpty() || arm.rings.isEmpty()) {
        return;
    }
    const Ring& end = junctionRing(arm, isStart);
    const QVector3D tangent = arrivingTangent(arm, isStart);
    const bool isAhead = std::all_of(mouth.cbegin(), mouth.cend(), [&](const QVector3D& point) {
        return QVector3D::dotProduct(point - end.center, tangent) > 0.0f;
    });
    if (!isAhead) {
        mouth.clear();
    }
}

// Cuts the window for one arm's mouth into the through and zips them, or
// returns nullopt when the mouth cannot be zipped.
std::optional<JunctionWindow> cutWindow(const Chain& through, const ThroughSpan& span, const Chain& arm, const ArmEnd& end)
{
    const QVector<QVector3D>& mouth = end.isStart ? arm.startMouth : arm.endMouth;
    const std::optional<QVector<QPointF>> polygon = mouthPolygon(through.rings, span, mouth);
    if (!polygon) {
        return std::nullopt;
    }

    // The arm's strip runs its mouth in column order; the zip band must run
    // it the other way, which needs the columns counter-clockwise around an
    // arm arriving at its end and clockwise around one leaving at its start.
    const bool isColumnOrderCounterClockwise = signedArea(*polygon) > 0.0;
    if (isColumnOrderCounterClockwise == end.isStart) {
        return std::nullopt;
    }

    const int resolution = through.rings.at(span.junctionRing).radii.size();
    const QSet<Vertex> removed = removedQuads(through.rings, span, *polygon);
    const std::optional<QVector<Vertex>> boundary = windowBoundary(removed, resolution);
    if (!boundary) {
        return std::nullopt;
    }

    QVector<int> mouthOrder;
    mouthOrder.reserve(mouth.size());
    for (int k = 0; k < mouth.size(); ++k) {
        mouthOrder.append(end.isStart ? (mouth.size() - k) % mouth.size() : k);
    }

    JunctionWindow window;
    window.removedQuads = removed;
    window.boundary = *boundary;
    window.zip = zipBand(through.rings, *boundary, mouth, *polygon, mouthOrder, resolution);
    window.armChain = end.chain;
    window.isArmStart = end.isStart;
    return window;
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

    for (const Junction& junction : std::as_const(junctions)) {
        Chain& through = chains[junction.through];
        const QVector<ArmGeometry> arms = armGeometries(chains, junction);
        const Ring& throughRing = through.rings.at(junctionRingIndex(through.rings, junction.position));
        const double reach = junctionReach(throughRing.frame.tangent, arms);
        const double spacing = std::max(junctionSpacing(options, throughRing, arms), 2.0 * reach / kMaxJunctionRings);
        densify(through, junction.position, reach, spacing);
    }

    QVector<JunctionSolids> solids;
    solids.reserve(junctions.size());
    for (const Junction& junction : std::as_const(junctions)) {
        solids.append(junctionSolids(chains, junction));
    }

    for (int j = 0; j < junctions.size(); ++j) {
        growThrough(chains[junctions.at(j).through], solids.at(j));
    }
    for (int j = 0; j < junctions.size(); ++j) {
        solids[j].grownThroughRings = unionThroughRings(chains.at(junctions.at(j).through).rings,
                                                        junctions.at(j).position, solids.at(j).reach);
    }

    QVector<QVector<ArmEnd>> trimmedArms(junctions.size());
    for (int j = 0; j < junctions.size(); ++j) {
        for (const ArmGeometry& arm : solids.at(j).arms) {
            if (applyTrim(legs, chains[arm.end.chain], arm, solids.at(j), warnings)) {
                trimmedArms[j].append(arm.end);
            }
        }
    }

    for (int j = 0; j < junctions.size(); ++j) {
        const Junction& junction = junctions.at(j);
        Chain& through = chains[junction.through];
        QVector<ArmEnd> arms = trimmedArms.at(j);
        if (through.rings.size() < 2) {
            for (const ArmEnd& end : std::as_const(arms)) {
                closeWithFan(chains[end.chain], end.isStart);
            }
            continue;
        }
        const ThroughSpan span(through.rings, junction.position, std::numeric_limits<double>::max());
        std::sort(arms.begin(), arms.end(), [](const ArmEnd& a, const ArmEnd& b) { return a.chain < b.chain; });

        for (const ArmEnd& end : std::as_const(arms)) {
            Chain& arm = chains[end.chain];
            // An arm joining two passages can be left inside the second, or
            // with too little between its two mouths to mesh.
            if (arm.rings.isEmpty()) {
                continue;
            }
            if (!hasMeshedMouthEnd(arm, end.isStart)) {
                dropArm(legs, arm, end.isStart,
                        QStringLiteral("The passage from station %1 lies inside the passages it joins; the tube leaves it out."), warnings);
                for (Chain& chain : chains) {
                    chain.windows.removeIf([&](const JunctionWindow& window) { return window.armChain == end.chain; });
                }
                continue;
            }
            const QUuid tripId = junctionTrip(legs, arm, end.isStart);
            std::optional<JunctionWindow> window = cutWindow(through, span, arm, end);
            if (window && overlapsWindows(through.windows, *window)) {
                warnings.append(Warning {
                    Warning::JunctionOverlap,
                    tripId,
                    junction.station,
                    QStringLiteral("Two passages open into station %1 through the same wall; the tube caps one inside the other.").arg(junction.station)
                });
                window.reset();
            }
            if (window) {
                window->tripId = tripId;
                through.windows.append(*window);
            } else {
                closeWithFan(arm, end.isStart);
            }
        }
    }
}
