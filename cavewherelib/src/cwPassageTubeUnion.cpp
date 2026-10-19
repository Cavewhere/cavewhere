/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeUnion.h"
#include "cwPassageTubeGeometry.h"
#include "cwPassageTubeMeshCheck.h"
#include "cwStation.h"

//manifold includes
#include <manifold/manifold.h>

//Qt includes
#include <QHash>
#include <QPair>
#include <QSet>
#include <QUuid>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <vector>

namespace {

using Builder = cwPassageTubeBuilder;
using Status = cwPassageTubeUnion::Status;

constexpr int kPositionProperties = 3;
constexpr int kMinFanVertices = 3;
constexpr int kMinLoopRings = 3;
constexpr double kFourPi = 4.0 * std::numbers::pi;
// A winding number above this is inside a closed surface (1 inside, 0
// outside, 1/2 on a face).
constexpr double kInsideWinding = 0.5;
constexpr int kMaxCleanupPasses = 16;

Status toStatus(manifold::Manifold::Error error)
{
    using Error = manifold::Manifold::Error;
    switch (error) {
    case Error::NoError:
        return Status::NoError;
    case Error::NonFiniteVertex:
        return Status::NonFiniteVertex;
    case Error::NotManifold:
        return Status::NotManifold;
    case Error::VertexOutOfBounds:
        return Status::VertexOutOfBounds;
    case Error::PropertiesWrongLength:
        return Status::PropertiesWrongLength;
    case Error::MissingPositionProperties:
        return Status::MissingPositionProperties;
    case Error::MergeVectorsDifferentLengths:
        return Status::MergeVectorsDifferentLengths;
    case Error::MergeIndexOutOfBounds:
        return Status::MergeIndexOutOfBounds;
    case Error::TransformWrongLength:
        return Status::TransformWrongLength;
    case Error::RunIndexWrongLength:
        return Status::RunIndexWrongLength;
    case Error::FaceIDWrongLength:
        return Status::FaceIDWrongLength;
    case Error::InvalidConstruction:
        return Status::InvalidConstruction;
    case Error::ResultTooLarge:
        return Status::ResultTooLarge;
    case Error::InvalidTangents:
        return Status::InvalidTangents;
    case Error::Cancelled:
        return Status::Cancelled;
    }
    return Status::InvalidConstruction;
}

manifold::Manifold toManifold(const cwPassageTubeMesh& tube)
{
    manifold::MeshGL mesh;
    mesh.numProp = kPositionProperties;
    mesh.vertProperties.reserve(tube.positions.size() * kPositionProperties);
    for (const QVector3D& position : tube.positions) {
        mesh.vertProperties.push_back(position.x());
        mesh.vertProperties.push_back(position.y());
        mesh.vertProperties.push_back(position.z());
    }
    mesh.triVerts.assign(tube.indices.cbegin(), tube.indices.cend());
    return manifold::Manifold(mesh);
}

// One trip range over all of the mesh's triangles.
void setSingleTrip(cwPassageTubeMesh& mesh)
{
    mesh.tripIndexRanges = {cwPassageTubeMesh::VertexRange {0, static_cast<int>(mesh.indices.size())}};
    mesh.tripUuids = {QUuid()};
}

// manifold's output with the vertices it split between input runs welded
// back, so edges are shared by index.
cwPassageTubeMesh fromManifold(const manifold::Manifold& solid)
{
    const manifold::MeshGL mesh = solid.GetMeshGL();
    const int vertexCount = static_cast<int>(mesh.NumVert());
    QVector<quint32> mergedInto(vertexCount);
    std::iota(mergedInto.begin(), mergedInto.end(), 0u);
    for (size_t i = 0; i < mesh.mergeFromVert.size(); ++i) {
        mergedInto[mesh.mergeFromVert.at(i)] = mesh.mergeToVert.at(i);
    }

    cwPassageTubeMesh result;
    QVector<qint64> newIndex(vertexCount, -1);
    result.indices.reserve(static_cast<qsizetype>(mesh.triVerts.size()));
    for (const quint32 vertex : mesh.triVerts) {
        const quint32 kept = mergedInto.at(vertex);
        if (newIndex.at(kept) < 0) {
            newIndex[kept] = result.positions.size();
            const size_t offset = static_cast<size_t>(kept) * mesh.numProp;
            result.positions.append(QVector3D(mesh.vertProperties.at(offset),
                                              mesh.vertProperties.at(offset + 1),
                                              mesh.vertProperties.at(offset + 2)));
        }
        result.indices.append(static_cast<quint32>(newIndex.at(kept)));
    }
    setSingleTrip(result);
    return result;
}

double signedVolume(const cwPassageTubeMesh& mesh)
{
    double sum = 0.0;
    for (qsizetype i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const QVector3D& a = mesh.positions.at(mesh.indices.at(i));
        const QVector3D& b = mesh.positions.at(mesh.indices.at(i + 1));
        const QVector3D& c = mesh.positions.at(mesh.indices.at(i + 2));
        sum += QVector3D::dotProduct(a, QVector3D::crossProduct(b, c));
    }
    constexpr double kTetrahedronFactor = 1.0 / 6.0;
    return sum * kTetrahedronFactor;
}

QVector3D centroid(const QVector<QVector3D>& points)
{
    QVector3D sum;
    for (const QVector3D& point : points) {
        sum += point;
    }
    return sum / static_cast<float>(points.size());
}

struct Vec {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec() = default;
    Vec(double x, double y, double z) : x(x), y(y), z(z) {}
    explicit Vec(const QVector3D& v) : x(v.x()), y(v.y()), z(v.z()) {}

    Vec operator-(const Vec& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec operator+(const Vec& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec operator*(double s) const { return {x * s, y * s, z * s}; }
    double length() const { return std::sqrt(dot(*this, *this)); }

    static double dot(const Vec& a, const Vec& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static Vec cross(const Vec& a, const Vec& b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
};

struct Triangle {
    Vec a;
    Vec b;
    Vec c;
};

Triangle triangleAt(const cwPassageTubeMesh& mesh, qsizetype first)
{
    return {Vec(mesh.positions.at(mesh.indices.at(first))),
            Vec(mesh.positions.at(mesh.indices.at(first + 1))),
            Vec(mesh.positions.at(mesh.indices.at(first + 2)))};
}

// The solid angle of the triangle seen from the origin (Van Oosterom and
// Strackee), signed by the triangle's winding.
double solidAngle(const Vec& a, const Vec& b, const Vec& c)
{
    const double la = a.length();
    const double lb = b.length();
    const double lc = c.length();
    const double numerator = Vec::dot(a, Vec::cross(b, c));
    const double denominator = la * lb * lc + Vec::dot(a, b) * lc + Vec::dot(b, c) * la + Vec::dot(c, a) * lb;
    return 2.0 * std::atan2(numerator, denominator);
}

// The closest point on the triangle to p (Ericson, Real-Time Collision
// Detection 5.1.5).
Vec closestPoint(const Vec& p, const Triangle& t)
{
    const Vec ab = t.b - t.a;
    const Vec ac = t.c - t.a;
    const Vec ap = p - t.a;
    const double d1 = Vec::dot(ab, ap);
    const double d2 = Vec::dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) {
        return t.a;
    }
    const Vec bp = p - t.b;
    const double d3 = Vec::dot(ab, bp);
    const double d4 = Vec::dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) {
        return t.b;
    }
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        return t.a + ab * (d1 / (d1 - d3));
    }
    const Vec cp = p - t.c;
    const double d5 = Vec::dot(ab, cp);
    const double d6 = Vec::dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) {
        return t.c;
    }
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        return t.a + ac * (d2 / (d2 - d6));
    }
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        return t.b + (t.c - t.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const double denominator = 1.0 / (va + vb + vc);
    return t.a + ab * (vb * denominator) + ac * (vc * denominator);
}

// Twice the triangle's area along its normal, wound a → b → c.
QVector3D normalOf(const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    return QVector3D::crossProduct(b - a, c - a);
}

// The triangle's shortest altitude, onto its longest edge.
double altitudeOf(const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    const double longest = std::max({a.distanceToPoint(b), b.distanceToPoint(c), c.distanceToPoint(a)});
    return longest > 0.0 ? normalOf(a, b, c).length() / longest : 0.0;
}

// The union leaves needles and specks where an intersection point lands
// within float rounding of an input edge or vertex, or where two walls touch
// along a line. Each pass collapses a triangle's edge shorter than
// kMergeToleranceMeters into its midpoint, or flips the longest edge of a
// triangle the mesh check calls degenerate or a sliver, wherever neither touches a triangle the
// pass already changed. Collapses keep the mesh manifold by the link
// condition; flips by never making an edge that exists.
void removeSlivers(cwPassageTubeMesh& mesh)
{
    using Corners = std::array<quint32, 3>;
    const auto edgeKey = [](quint32 from, quint32 to) { return (static_cast<quint64>(from) << 32) | to; };

    for (int pass = 0; pass < kMaxCleanupPasses; ++pass) {
        QVector<Corners> triangles;
        triangles.reserve(mesh.indices.size() / 3);
        for (qsizetype i = 0; i + 2 < mesh.indices.size(); i += 3) {
            triangles.append({mesh.indices.at(i), mesh.indices.at(i + 1), mesh.indices.at(i + 2)});
        }
        QHash<quint64, int> triangleOfEdge;
        QVector<QVector<int>> trianglesOfVertex(mesh.positions.size());
        for (int t = 0; t < triangles.size(); ++t) {
            for (int k = 0; k < 3; ++k) {
                triangleOfEdge.insert(edgeKey(triangles.at(t).at(k), triangles.at(t).at((k + 1) % 3)), t);
                trianglesOfVertex[triangles.at(t).at(k)].append(t);
            }
        }
        const auto neighborsOf = [&](quint32 vertex) {
            QSet<quint32> neighbors;
            for (const int t : trianglesOfVertex.at(vertex)) {
                for (const quint32 corner : triangles.at(t)) {
                    if (corner != vertex) {
                        neighbors.insert(corner);
                    }
                }
            }
            return neighbors;
        };
        const auto thirdOf = [&](int t, quint32 a, quint32 b) {
            for (const quint32 corner : triangles.at(t)) {
                if (corner != a && corner != b) {
                    return corner;
                }
            }
            return a;
        };

        QVector<bool> isTouched(mesh.positions.size(), false);
        QVector<bool> isRemoved(triangles.size(), false);
        const auto touchAround = [&](quint32 vertex) {
            for (const int t : trianglesOfVertex.at(vertex)) {
                for (const quint32 corner : triangles.at(t)) {
                    isTouched[corner] = true;
                }
            }
        };
        bool changed = false;
        for (int t = 0; t < triangles.size(); ++t) {
            const Corners corners = triangles.at(t);
            if (isRemoved.at(t) || std::any_of(corners.cbegin(), corners.cend(), [&](quint32 v) { return isTouched.at(v); })) {
                continue;
            }
            std::array<double, 3> lengths;
            for (int k = 0; k < 3; ++k) {
                lengths[k] = mesh.positions.at(corners.at(k)).distanceToPoint(mesh.positions.at(corners.at((k + 1) % 3)));
            }
            const int shortest = static_cast<int>(std::min_element(lengths.cbegin(), lengths.cend()) - lengths.cbegin());
            const int longest = static_cast<int>(std::max_element(lengths.cbegin(), lengths.cend()) - lengths.cbegin());
            const QVector3D& a = mesh.positions.at(corners.at(0));
            const double area = QVector3D::crossProduct(mesh.positions.at(corners.at(1)) - a, mesh.positions.at(corners.at(2)) - a).length() / 2.0;
            const double altitude = lengths.at(longest) > 0.0 ? 2.0 * area / lengths.at(longest) : 0.0;

            if (lengths.at(shortest) < cwPassageTubeUnion::kMergeToleranceMeters) {
                const quint32 kept = corners.at(shortest);
                const quint32 gone = corners.at((shortest + 1) % 3);
                const int other = triangleOfEdge.value(edgeKey(gone, kept), -1);
                if (other < 0 || isRemoved.at(other)) {
                    continue;
                }
                const QSet<quint32> shared = neighborsOf(kept) & neighborsOf(gone);
                if (shared != QSet<quint32> {thirdOf(t, kept, gone), thirdOf(other, kept, gone)}) {
                    continue;
                }
                touchAround(kept);
                touchAround(gone);
                mesh.positions[kept] = (mesh.positions.at(kept) + mesh.positions.at(gone)) / 2.0f;
                for (const int around : trianglesOfVertex.at(gone)) {
                    for (quint32& corner : triangles[around]) {
                        corner = corner == gone ? kept : corner;
                    }
                }
                isRemoved[t] = true;
                isRemoved[other] = true;
                changed = true;
            } else if (area < cwPassageTubeMeshCheck::kMinTriangleArea
                       || altitude < cwPassageTubeUnion::kMergeToleranceMeters
                       || (altitude < cwPassageTubeMeshCheck::kMinSliverAltitude
                           && lengths.at(longest) > cwPassageTubeMeshCheck::kMaxAspectRatio * altitude)) {
                const quint32 from = corners.at(longest);
                const quint32 to = corners.at((longest + 1) % 3);
                const quint32 apex = corners.at((longest + 2) % 3);
                const int other = triangleOfEdge.value(edgeKey(to, from), -1);
                if (other < 0 || isRemoved.at(other)) {
                    continue;
                }
                const quint32 opposite = thirdOf(other, from, to);
                if (std::any_of(triangles.at(other).cbegin(), triangles.at(other).cend(), [&](quint32 v) { return isTouched.at(v); })
                    || neighborsOf(apex).contains(opposite)) {
                    continue;
                }
                // The flip must leave both triangles thicker than this one
                // and facing the way the pair faced.
                const QVector3D& p = mesh.positions.at(from);
                const QVector3D& q = mesh.positions.at(to);
                const QVector3D& r = mesh.positions.at(apex);
                const QVector3D& o = mesh.positions.at(opposite);
                const QVector3D pairNormal = normalOf(p, q, r) + normalOf(q, p, o);
                if (std::min(altitudeOf(p, o, r), altitudeOf(o, q, r)) <= altitude
                    || QVector3D::dotProduct(normalOf(p, o, r), pairNormal) <= 0.0f
                    || QVector3D::dotProduct(normalOf(o, q, r), pairNormal) <= 0.0f) {
                    continue;
                }
                for (const quint32 corner : {from, to, apex, opposite}) {
                    isTouched[corner] = true;
                }
                triangles[t] = {from, opposite, apex};
                triangles[other] = {opposite, to, apex};
                changed = true;
            }
        }

        mesh.indices.clear();
        for (int t = 0; t < triangles.size(); ++t) {
            if (!isRemoved.at(t)) {
                mesh.indices.append({triangles.at(t).at(0), triangles.at(t).at(1), triangles.at(t).at(2)});
            }
        }
        if (!changed) {
            break;
        }
    }

    // Drop the vertices the collapses left unused.
    QVector<qint64> newIndex(mesh.positions.size(), -1);
    QVector<QVector3D> positions;
    for (quint32& index : mesh.indices) {
        if (newIndex.at(index) < 0) {
            newIndex[index] = positions.size();
            positions.append(mesh.positions.at(index));
        }
        index = static_cast<quint32>(newIndex.at(index));
    }
    mesh.positions = positions;
    setSingleTrip(mesh);
}

using StationKey = QPair<QUuid, QString>;

StationKey stationKey(const Builder::Leg& leg, const QString& station)
{
    return {leg.caveId, cwStation::canonicalKey(station)};
}

// The chains that have a leg at each station.
QHash<StationKey, QVector<int>> chainsByStation(const Builder::Debug& rings)
{
    QHash<StationKey, QVector<int>> chains;
    for (int c = 0; c < rings.chains.size(); ++c) {
        for (const Builder::ChainLeg& chainLeg : rings.chains.at(c).legs) {
            const Builder::Leg& leg = rings.legs.at(chainLeg.legIndex);
            for (const QString& station : {leg.from, leg.to}) {
                QVector<int>& list = chains[stationKey(leg, station)];
                if (!list.contains(c)) {
                    list.append(c);
                }
            }
        }
    }
    return chains;
}

// An open end of a chain: its end ring, the station it closes the tube at,
// and the rings of the leg it ends.
struct ChainEnd {
    int chain = -1;
    bool atStart = true;
    StationKey station;
    int capRing = -1;
    int farRing = -1;
    QVector3D outward;
};

QVector<ChainEnd> chainEnds(const Builder::Debug& rings)
{
    QVector<ChainEnd> ends;
    for (int c = 0; c < rings.chains.size(); ++c) {
        const Builder::Chain& chain = rings.chains.at(c);
        const bool isLoop = chain.startCap == Builder::EndCap::None;
        const bool hasRings = std::all_of(chain.rings.cbegin(), chain.rings.cend(), [](const Builder::Ring& ring) {
            return !ring.vertices.isEmpty();
        });
        if (isLoop || !hasRings || chain.rings.size() < 2) {
            continue;
        }
        const auto farRingOf = [&](int capRing, int step) {
            const int leg = chain.rings.at(capRing).chainLeg;
            int far = capRing;
            while (far + step >= 0 && far + step < chain.rings.size() && chain.rings.at(far + step).chainLeg == leg) {
                far += step;
            }
            return far;
        };
        const Builder::Leg& firstLeg = rings.legs.at(chain.legs.first().legIndex);
        const Builder::Leg& lastLeg = rings.legs.at(chain.legs.last().legIndex);
        const int lastRing = static_cast<int>(chain.rings.size()) - 1;
        ends.append({c, true,
                     stationKey(firstLeg, cwPassageTubeGeometry::chainStartStation(rings.legs, chain.legs.first())),
                     0, farRingOf(0, 1), -chain.rings.first().frame.tangent});
        ends.append({c, false,
                     stationKey(lastLeg, cwPassageTubeGeometry::chainEndStation(rings.legs, chain.legs.last())),
                     lastRing, farRingOf(lastRing, -1), chain.rings.last().frame.tangent});
    }
    return ends;
}

// The tubes of the chains other than end's that reach its station.
QVector<const cwPassageTubeMesh*> neighborTubes(const ChainEnd& end,
                                                const QHash<StationKey, QVector<int>>& chainsAt,
                                                const QVector<cwPassageTubeUnion::ChainTube>& tubes)
{
    QVector<const cwPassageTubeMesh*> neighbors;
    const QVector<int> chains = chainsAt.value(end.station);
    for (const cwPassageTubeUnion::ChainTube& tube : tubes) {
        if (tube.chain != end.chain && chains.contains(tube.chain)) {
            neighbors.append(&tube.mesh);
        }
    }
    return neighbors;
}

bool insideAny(const QVector<const cwPassageTubeMesh*>& tubes, const QVector3D& point)
{
    return std::any_of(tubes.cbegin(), tubes.cend(), [&](const cwPassageTubeMesh* tube) {
        return cwPassageTubeUnion::contains(*tube, point);
    });
}

} // namespace

bool cwPassageTubeUnion::Result::isComplete() const
{
    return status == Status::NoError
           && std::all_of(inputStatus.cbegin(), inputStatus.cend(), [](Status input) { return input == Status::NoError; });
}

cwPassageTubeUnion::Result cwPassageTubeUnion::unite(const QVector<cwPassageTubeMesh>& tubes)
{
    Result result;
    std::vector<manifold::Manifold> accepted;
    accepted.reserve(tubes.size());
    for (const cwPassageTubeMesh& tube : tubes) {
        manifold::Manifold solid = toManifold(tube);
        const Status inputStatus = toStatus(solid.Status());
        result.inputStatus.append(inputStatus);
        if (inputStatus == Status::NoError) {
            accepted.push_back(std::move(solid));
        }
    }

    const manifold::Manifold united = manifold::Manifold::BatchBoolean(accepted, manifold::OpType::Add);
    result.status = toStatus(united.Status());
    if (result.status == Status::NoError) {
        result.mesh = fromManifold(united);
        removeSlivers(result.mesh);
        result.volume = united.Volume();
        result.genus = united.Genus();
    }
    return result;
}

cwPassageTubeUnion::Status cwPassageTubeUnion::status(const cwPassageTubeMesh& tube)
{
    return toStatus(toManifold(tube).Status());
}

double cwPassageTubeUnion::volume(const cwPassageTubeMesh& tube)
{
    const manifold::Manifold solid = toManifold(tube);
    return solid.Status() == manifold::Manifold::Error::NoError ? solid.Volume() : 0.0;
}

QString cwPassageTubeUnion::statusName(Status status)
{
    switch (status) {
    case Status::NoError:
        return QStringLiteral("NoError");
    case Status::NonFiniteVertex:
        return QStringLiteral("NonFiniteVertex");
    case Status::NotManifold:
        return QStringLiteral("NotManifold");
    case Status::VertexOutOfBounds:
        return QStringLiteral("VertexOutOfBounds");
    case Status::PropertiesWrongLength:
        return QStringLiteral("PropertiesWrongLength");
    case Status::MissingPositionProperties:
        return QStringLiteral("MissingPositionProperties");
    case Status::MergeVectorsDifferentLengths:
        return QStringLiteral("MergeVectorsDifferentLengths");
    case Status::MergeIndexOutOfBounds:
        return QStringLiteral("MergeIndexOutOfBounds");
    case Status::TransformWrongLength:
        return QStringLiteral("TransformWrongLength");
    case Status::RunIndexWrongLength:
        return QStringLiteral("RunIndexWrongLength");
    case Status::FaceIDWrongLength:
        return QStringLiteral("FaceIDWrongLength");
    case Status::InvalidConstruction:
        return QStringLiteral("InvalidConstruction");
    case Status::ResultTooLarge:
        return QStringLiteral("ResultTooLarge");
    case Status::InvalidTangents:
        return QStringLiteral("InvalidTangents");
    case Status::Cancelled:
        return QStringLiteral("Cancelled");
    }
    return QString();
}

cwPassageTubeMesh cwPassageTubeUnion::loft(const QVector<QVector<QVector3D>>& rings, bool isLoop)
{
    cwPassageTubeMesh mesh;
    if (rings.size() < 2) {
        return mesh;
    }
    const int resolution = static_cast<int>(rings.first().size());
    const bool closesLoop = isLoop && rings.size() >= kMinLoopRings;
    const int ringCount = static_cast<int>(rings.size()) - (closesLoop ? 1 : 0);
    for (int r = 0; r < ringCount; ++r) {
        mesh.positions.append(rings.at(r));
    }
    const auto vertex = [&](int ring, int bin) {
        return static_cast<quint32>((ring % ringCount) * resolution + bin % resolution);
    };
    const auto addTriangle = [&](quint32 a, quint32 b, quint32 c) {
        mesh.indices.append({a, b, c});
    };

    const int segments = closesLoop ? ringCount : ringCount - 1;
    for (int r = 0; r < segments; ++r) {
        for (int k = 0; k < resolution; ++k) {
            const quint32 a = vertex(r, k);
            const quint32 b = vertex(r, k + 1);
            const quint32 c = vertex(r + 1, k + 1);
            const quint32 d = vertex(r + 1, k);
            addTriangle(a, b, c);
            addTriangle(a, c, d);
        }
    }

    if (!closesLoop && resolution >= kMinFanVertices) {
        const auto addFan = [&](int ring, bool atEnd) {
            const quint32 center = static_cast<quint32>(mesh.positions.size());
            mesh.positions.append(centroid(rings.at(ring)));
            for (int k = 0; k < resolution; ++k) {
                if (atEnd) {
                    addTriangle(center, vertex(ring, k), vertex(ring, k + 1));
                } else {
                    addTriangle(center, vertex(ring, k + 1), vertex(ring, k));
                }
            }
        };
        addFan(0, false);
        addFan(ringCount - 1, true);
    }

    if (signedVolume(mesh) < 0.0) {
        for (qsizetype i = 0; i + 2 < mesh.indices.size(); i += 3) {
            std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
        }
    }
    setSingleTrip(mesh);
    return mesh;
}

QVector<cwPassageTubeUnion::ChainTube> cwPassageTubeUnion::chainTubes(const cwPassageTubeBuilder::Debug& rings)
{
    QVector<ChainTube> tubes;
    for (int c = 0; c < rings.chains.size(); ++c) {
        const Builder::Chain& chain = rings.chains.at(c);
        QVector<QVector<QVector<QVector3D>>> runs(1);
        for (const Builder::Ring& ring : chain.rings) {
            if (!ring.vertices.isEmpty()) {
                runs.last().append(ring.vertices);
            } else if (!runs.last().isEmpty()) {
                runs.append(QVector<QVector<QVector3D>>());
            }
        }
        const bool isTorn = runs.size() > 1 || runs.first().size() < chain.rings.size();
        const bool isLoop = chain.startCap == Builder::EndCap::None && !isTorn;
        for (const QVector<QVector<QVector3D>>& run : std::as_const(runs)) {
            if (run.size() >= 2) {
                tubes.append({c, loft(run, isLoop)});
            }
        }
    }
    return tubes;
}

QVector<QVector<int>> cwPassageTubeUnion::chainComponents(const cwPassageTubeBuilder::Debug& rings)
{
    QVector<int> parent(rings.chains.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](int c) {
        while (parent.at(c) != c) {
            parent[c] = parent.at(parent.at(c));
            c = parent.at(c);
        }
        return c;
    };
    const QHash<StationKey, QVector<int>> chainsAt = chainsByStation(rings);
    for (const QVector<int>& chains : chainsAt) {
        for (int i = 1; i < chains.size(); ++i) {
            parent[root(chains.at(i))] = root(chains.first());
        }
    }

    QHash<int, int> componentOfRoot;
    QVector<QVector<int>> components;
    for (int c = 0; c < rings.chains.size(); ++c) {
        const int r = root(c);
        if (!componentOfRoot.contains(r)) {
            componentOfRoot.insert(r, static_cast<int>(components.size()));
            components.append(QVector<int>());
        }
        components[componentOfRoot.value(r)].append(c);
    }
    return components;
}

cwPassageTubeMesh cwPassageTubeUnion::merged(const QVector<cwPassageTubeMesh>& meshes)
{
    cwPassageTubeMesh result;
    for (const cwPassageTubeMesh& mesh : meshes) {
        const quint32 offset = static_cast<quint32>(result.positions.size());
        result.positions.append(mesh.positions);
        for (const quint32 index : mesh.indices) {
            result.indices.append(index + offset);
        }
    }
    setSingleTrip(result);
    return result;
}

bool cwPassageTubeUnion::contains(const cwPassageTubeMesh& tube, const QVector3D& point)
{
    const Vec p(point);
    double angleSum = 0.0;
    double nearest = std::numeric_limits<double>::max();
    for (qsizetype i = 0; i + 2 < tube.indices.size(); i += 3) {
        const Triangle t = triangleAt(tube, i);
        angleSum += solidAngle(t.a - p, t.b - p, t.c - p);
        nearest = std::min(nearest, (closestPoint(p, t) - p).length());
    }
    return angleSum / kFourPi > kInsideWinding && nearest > kSurfaceToleranceMeters;
}

QVector<cwPassageTubeUnion::CapVertex> cwPassageTubeUnion::capNotch(const cwPassageTubeBuilder::Debug& rings)
{
    const QVector<ChainTube> tubes = chainTubes(rings);
    const QHash<StationKey, QVector<int>> chainsAt = chainsByStation(rings);
    QVector<CapVertex> notch;
    for (const ChainEnd& end : chainEnds(rings)) {
        const QVector<const cwPassageTubeMesh*> neighbors = neighborTubes(end, chainsAt, tubes);
        if (neighbors.isEmpty()) {
            continue;
        }
        const QVector<QVector3D>& cap = rings.chains.at(end.chain).rings.at(end.capRing).vertices;
        for (int bin = 0; bin < cap.size(); ++bin) {
            if (!insideAny(neighbors, cap.at(bin))) {
                notch.append({end.chain, end.atStart, bin, cap.at(bin), cap.at(bin)});
            }
        }
    }
    return notch;
}
