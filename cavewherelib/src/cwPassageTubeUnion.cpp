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
// A point on the surface winds about 1/2 on a face and less on a convex edge
// or corner; one just outside it winds about 0.
constexpr double kOnSurfaceWinding = 0.1;
constexpr double kParallelEpsilon = 1e-12;
constexpr float kMinLegLengthSquared = 1e-8f;
// The smallest incidence cosine a slide's margin is scaled by, so a line
// grazing a wall still comes to rest kSlideMarginMeters inside it.
constexpr double kMinIncidence = 0.05;
// Crossings closer than this along a slide's line are one, a shared edge.
constexpr double kMinStepMeters = 1e-4;
constexpr int kBisectionSteps = 12;
constexpr int kMaxCleanupPasses = 16;
// A sliver's shortest edge up to this long collapses into its midpoint.
constexpr double kMaxNeedleEdgeMeters = 0.1;
// A closed piece of the union with less volume than this, in m³, is a speck
// of back-to-back triangles.
constexpr double kMinPieceVolume = 1e-6;
// A sliver's apex snaps onto its longest edge only between this fraction of
// the edge from either end; nearer an end it is a needle.
constexpr float kMinSnapFraction = 0.01f;

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

// With an originalID, the union's triangles name the tube and the face of
// it they are part of.
manifold::Manifold toManifold(const cwPassageTubeMesh& tube,
                              std::optional<quint32> originalID = std::nullopt,
                              const QVector<int>& faces = {})
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
    if (originalID) {
        mesh.runOriginalID = {*originalID};
        mesh.faceID.assign(mesh.NumTri(), cwPassageTubeUnion::kSides);
        if (static_cast<size_t>(faces.size()) == mesh.NumTri()) {
            mesh.faceID.assign(faces.cbegin(), faces.cend());
        }
    }
    return manifold::Manifold(mesh);
}

// One trip range over all of the mesh's triangles.
void setSingleTrip(cwPassageTubeMesh& mesh)
{
    mesh.tripIndexRanges = {cwPassageTubeMesh::VertexRange {0, static_cast<int>(mesh.indices.size())}};
    mesh.tripUuids = {QUuid()};
}

using Origin = cwPassageTubeUnion::Origin;

// A mesh with each triangle's origin, in step with its indices.
struct TracedMesh {
    cwPassageTubeMesh mesh;
    QVector<Origin> origins;
};

// manifold's output with the vertices it split between input runs welded
// back, so edges are shared by index, and each triangle traced to the input
// whose original ID its run carries.
TracedMesh fromManifold(const manifold::Manifold& solid, const QHash<quint32, int>& inputOfOriginalID)
{
    const manifold::MeshGL mesh = solid.GetMeshGL();
    const int vertexCount = static_cast<int>(mesh.NumVert());
    QVector<quint32> mergedInto(vertexCount);
    std::iota(mergedInto.begin(), mergedInto.end(), 0u);
    for (size_t i = 0; i < mesh.mergeFromVert.size(); ++i) {
        mergedInto[mesh.mergeFromVert.at(i)] = mesh.mergeToVert.at(i);
    }

    TracedMesh result;
    QVector<qint64> newIndex(vertexCount, -1);
    result.mesh.indices.reserve(static_cast<qsizetype>(mesh.triVerts.size()));
    for (const quint32 vertex : mesh.triVerts) {
        const quint32 kept = mergedInto.at(vertex);
        if (newIndex.at(kept) < 0) {
            newIndex[kept] = result.mesh.positions.size();
            const size_t offset = static_cast<size_t>(kept) * mesh.numProp;
            result.mesh.positions.append(QVector3D(mesh.vertProperties.at(offset),
                                                   mesh.vertProperties.at(offset + 1),
                                                   mesh.vertProperties.at(offset + 2)));
        }
        result.mesh.indices.append(static_cast<quint32>(newIndex.at(kept)));
    }

    const size_t triangleCount = mesh.NumTri();
    result.origins.resize(static_cast<qsizetype>(triangleCount));
    for (size_t run = 0; run < mesh.runOriginalID.size(); ++run) {
        const int input = inputOfOriginalID.value(mesh.runOriginalID.at(run), -1);
        const size_t end = run + 1 < mesh.runIndex.size() ? mesh.runIndex.at(run + 1) / 3 : triangleCount;
        for (size_t t = mesh.runIndex.at(run) / 3; t < end && t < triangleCount; ++t) {
            const int face = t < mesh.faceID.size() ? static_cast<int>(mesh.faceID.at(t)) : -1;
            result.origins[static_cast<qsizetype>(t)] = Origin {input, face};
        }
    }
    setSingleTrip(result.mesh);
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

// Where the line origin + t·direction crosses the triangle, by t
// (Möller–Trumbore, both directions along the line).
std::optional<double> lineCrossing(const Vec& origin, const Vec& direction, const Triangle& t)
{
    const Vec edge1 = t.b - t.a;
    const Vec edge2 = t.c - t.a;
    const Vec p = Vec::cross(direction, edge2);
    const double determinant = Vec::dot(edge1, p);
    if (std::abs(determinant) < kParallelEpsilon) {
        return std::nullopt;
    }
    const double inverse = 1.0 / determinant;
    const Vec s = origin - t.a;
    const double u = Vec::dot(s, p) * inverse;
    if (u < 0.0 || u > 1.0) {
        return std::nullopt;
    }
    const Vec q = Vec::cross(s, edge1);
    const double v = Vec::dot(direction, q) * inverse;
    if (v < 0.0 || u + v > 1.0) {
        return std::nullopt;
    }
    return Vec::dot(edge2, q) * inverse;
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
// along a line. Each pass collapses into its midpoint a triangle's edge
// shorter than kMergeToleranceMeters, or the shortest edge of a sliver the
// mesh check would report when that edge is shorter than
// kMaxNeedleEdgeMeters, and otherwise flips the sliver's longest edge, or
// moves its apex onto that edge where the flip would fold, wherever neither
// touches a triangle the pass already changed. Collapses keep the mesh
// manifold by the link condition; flips and snaps by never making an edge
// that exists.
void removeSlivers(TracedMesh& traced)
{
    cwPassageTubeMesh& mesh = traced.mesh;
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

            const auto collapseShortest = [&]() {
                const quint32 kept = corners.at(shortest);
                const quint32 gone = corners.at((shortest + 1) % 3);
                const int other = triangleOfEdge.value(edgeKey(gone, kept), -1);
                if (other < 0 || isRemoved.at(other)) {
                    return false;
                }
                const QSet<quint32> shared = neighborsOf(kept) & neighborsOf(gone);
                if (shared != QSet<quint32> {thirdOf(t, kept, gone), thirdOf(other, kept, gone)}) {
                    return false;
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
                return true;
            };
            // The triangle across the longest edge from apex, when neither
            // that triangle nor apex's neighbors forbid turning the edge.
            struct EdgePair {
                quint32 from;
                quint32 to;
                quint32 apex;
                int other;
                quint32 opposite;
            };
            const auto pairAcrossLongest = [&]() -> std::optional<EdgePair> {
                const quint32 from = corners.at(longest);
                const quint32 to = corners.at((longest + 1) % 3);
                const quint32 apex = corners.at((longest + 2) % 3);
                const int other = triangleOfEdge.value(edgeKey(to, from), -1);
                if (other < 0 || isRemoved.at(other)) {
                    return std::nullopt;
                }
                const quint32 opposite = thirdOf(other, from, to);
                if (std::any_of(triangles.at(other).cbegin(), triangles.at(other).cend(), [&](quint32 v) { return isTouched.at(v); })
                    || neighborsOf(apex).contains(opposite)) {
                    return std::nullopt;
                }
                return EdgePair {from, to, apex, other, opposite};
            };
            const auto replacePair = [&](const EdgePair& pair) {
                triangles[t] = {pair.from, pair.opposite, pair.apex};
                triangles[pair.other] = {pair.opposite, pair.to, pair.apex};
            };
            const auto flipLongest = [&](const EdgePair& pair) {
                // The flip must leave both triangles thicker than this one
                // and facing the way the pair faced.
                const QVector3D& p = mesh.positions.at(pair.from);
                const QVector3D& q = mesh.positions.at(pair.to);
                const QVector3D& r = mesh.positions.at(pair.apex);
                const QVector3D& o = mesh.positions.at(pair.opposite);
                const QVector3D pairNormal = normalOf(p, q, r) + normalOf(q, p, o);
                if (std::min(altitudeOf(p, o, r), altitudeOf(o, q, r)) <= altitude
                    || QVector3D::dotProduct(normalOf(p, o, r), pairNormal) <= 0.0f
                    || QVector3D::dotProduct(normalOf(o, q, r), pairNormal) <= 0.0f) {
                    return false;
                }
                for (const quint32 corner : {pair.from, pair.to, pair.apex, pair.opposite}) {
                    isTouched[corner] = true;
                }
                replacePair(pair);
                return true;
            };
            // A sliver whose apex lies over its longest edge: the apex moves
            // onto that edge, splitting the triangle across it in two, where
            // a flip would fold the pair.
            const auto snapApex = [&](const EdgePair& pair) {
                const QVector3D& p = mesh.positions.at(pair.from);
                const QVector3D edge = mesh.positions.at(pair.to) - p;
                const float along = QVector3D::dotProduct(mesh.positions.at(pair.apex) - p, edge) / edge.lengthSquared();
                if (along <= kMinSnapFraction || along >= 1.0f - kMinSnapFraction) {
                    return false;
                }
                touchAround(pair.apex);
                for (const quint32 corner : {pair.from, pair.to, pair.opposite}) {
                    isTouched[corner] = true;
                }
                mesh.positions[pair.apex] = p + edge * along;
                replacePair(pair);
                return true;
            };
            const auto turnLongest = [&]() {
                const std::optional<EdgePair> pair = pairAcrossLongest();
                return pair && (flipLongest(*pair) || snapApex(*pair));
            };

            const bool isSliver = area < cwPassageTubeMeshCheck::kMinTriangleArea
                                  || altitude < cwPassageTubeUnion::kMergeToleranceMeters
                                  || (altitude < cwPassageTubeMeshCheck::kMinSliverAltitude
                                      && lengths.at(longest) > cwPassageTubeMeshCheck::kMaxAspectRatio * altitude);
            // A needle, thin along a short edge, closes by collapsing it.
            const bool isShort = lengths.at(shortest) < cwPassageTubeUnion::kMergeToleranceMeters
                                 || (isSliver && lengths.at(shortest) < kMaxNeedleEdgeMeters);
            if ((isShort && collapseShortest()) || (isSliver && turnLongest())) {
                changed = true;
            }
        }

        mesh.indices.clear();
        QVector<Origin> origins;
        origins.reserve(triangles.size());
        for (int t = 0; t < triangles.size(); ++t) {
            if (!isRemoved.at(t)) {
                mesh.indices.append({triangles.at(t).at(0), triangles.at(t).at(1), triangles.at(t).at(2)});
                origins.append(traced.origins.at(t));
            }
        }
        traced.origins = origins;
        if (!changed) {
            break;
        }
    }
}

// The pieces of the mesh joined across manifold edges, as each triangle's
// piece: shells that touch only at a vertex, or along an edge four
// triangles share, are apart.
QVector<int> componentsOf(const cwPassageTubeMesh& mesh, int* componentCount)
{
    const int triangleCount = static_cast<int>(mesh.indices.size() / 3);
    QVector<int> parent(triangleCount);
    std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](int t) {
        while (parent.at(t) != t) {
            parent[t] = parent.at(parent.at(t));
            t = parent.at(t);
        }
        return t;
    };
    QHash<quint64, QVector<int>> trianglesOfEdge;
    for (int t = 0; t < triangleCount; ++t) {
        for (int k = 0; k < 3; ++k) {
            const quint32 a = mesh.indices.at(3 * t + k);
            const quint32 b = mesh.indices.at(3 * t + (k + 1) % 3);
            trianglesOfEdge[(static_cast<quint64>(std::min(a, b)) << 32) | std::max(a, b)].append(t);
        }
    }
    for (const QVector<int>& sharing : std::as_const(trianglesOfEdge)) {
        if (sharing.size() == 2) {
            parent[root(sharing.at(1))] = root(sharing.at(0));
        }
    }
    QHash<int, int> componentOfRoot;
    QVector<int> components;
    components.reserve(triangleCount);
    for (int t = 0; t < triangleCount; ++t) {
        const int r = root(t);
        const auto it = componentOfRoot.constFind(r);
        if (it == componentOfRoot.cend()) {
            components.append(static_cast<int>(componentOfRoot.size()));
            componentOfRoot.insert(r, components.last());
        } else {
            components.append(it.value());
        }
    }
    *componentCount = static_cast<int>(componentOfRoot.size());
    return components;
}

// Drops each piece with negative volume, an inside-out shell around a pocket
// the union enclosed, and returns them; drops each speck without a word.
QVector<cwPassageTubeUnion::Void> dropVoids(TracedMesh& traced)
{
    int componentCount = 0;
    const QVector<int> componentOfTriangle = componentsOf(traced.mesh, &componentCount);
    // Each piece's volume about one of its own vertices, in doubles, so a
    // speck far from the origin sums to its own size rather than rounding.
    QVector<std::optional<Vec>> pieceOrigins(componentCount);
    QVector<double> volumes(componentCount, 0.0);
    QVector<QVector3D> sums(componentCount);
    QVector<int> corners(componentCount, 0);
    constexpr double kTetrahedronFactor = 1.0 / 6.0;
    for (int t = 0; t < componentOfTriangle.size(); ++t) {
        const int c = componentOfTriangle.at(t);
        const Triangle triangle = triangleAt(traced.mesh, 3 * t);
        if (!pieceOrigins.at(c)) {
            pieceOrigins[c] = triangle.a;
        }
        const Vec& origin = *pieceOrigins.at(c);
        volumes[c] += Vec::dot(triangle.a - origin, Vec::cross(triangle.b - origin, triangle.c - origin)) * kTetrahedronFactor;
        for (int k = 0; k < 3; ++k) {
            sums[c] += traced.mesh.positions.at(traced.mesh.indices.at(3 * t + k));
        }
        corners[c] += 3;
    }

    QVector<cwPassageTubeUnion::Void> voids;
    bool hasSpecks = false;
    for (int c = 0; c < componentCount; ++c) {
        if (volumes.at(c) <= -kMinPieceVolume) {
            voids.append({volumes.at(c), sums.at(c) / static_cast<float>(corners.at(c))});
        } else if (volumes.at(c) < kMinPieceVolume) {
            hasSpecks = true;
        }
    }
    if (voids.isEmpty() && !hasSpecks) {
        return voids;
    }
    QVector<quint32> indices;
    QVector<Origin> origins;
    for (int t = 0; t < componentOfTriangle.size(); ++t) {
        if (volumes.at(componentOfTriangle.at(t)) >= kMinPieceVolume) {
            indices.append({traced.mesh.indices.at(3 * t), traced.mesh.indices.at(3 * t + 1), traced.mesh.indices.at(3 * t + 2)});
            origins.append(traced.origins.at(t));
        }
    }
    traced.mesh.indices = indices;
    traced.origins = origins;
    return voids;
}

// Drops the vertices no triangle uses.
void dropUnusedVertices(cwPassageTubeMesh& mesh)
{
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

// An open end of a chain closed by a fan: its end ring, the station it
// closes the tube at, and the rings of the leg it ends. A dome rounds a
// hairpin's end past its station, where the two chains overlap, and a
// through that ends past its junction ends the passage there, as a dead end
// does.
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

const QUuid& tripOfRing(const Builder::Debug& rings, const Builder::Chain& chain, int ring)
{
    return rings.legs.at(chain.legs.at(chain.rings.at(ring).chainLeg).legIndex).tripId;
}

// The triangles in each of the tube's two end fans.
int fanTriangleCount(const cwPassageTubeUnion::ChainTube& tube)
{
    return (static_cast<int>(tube.mesh.indices.size() / 3) - tube.firstCapTriangle) / 2;
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

cwPassageTubeUnion::Result cwPassageTubeUnion::unite(const QVector<cwPassageTubeMesh>& tubes, const QVector<QVector<int>>& faces)
{
    Result result;
    std::vector<manifold::Manifold> accepted;
    accepted.reserve(tubes.size());
    QHash<quint32, int> inputOfOriginalID;
    const quint32 firstID = manifold::Manifold::ReserveIDs(static_cast<quint32>(tubes.size()));
    for (int i = 0; i < tubes.size(); ++i) {
        const quint32 originalID = firstID + static_cast<quint32>(i);
        manifold::Manifold solid = toManifold(tubes.at(i), originalID, faces.value(i));
        const Status inputStatus = toStatus(solid.Status());
        result.inputStatus.append(inputStatus);
        if (inputStatus == Status::NoError) {
            inputOfOriginalID.insert(originalID, i);
            accepted.push_back(std::move(solid));
        }
    }

    const manifold::Manifold united = manifold::Manifold::BatchBoolean(accepted, manifold::OpType::Add);
    result.status = toStatus(united.Status());
    if (result.status == Status::NoError) {
        TracedMesh traced = fromManifold(united, inputOfOriginalID);
        removeSlivers(traced);
        result.voids = dropVoids(traced);
        dropUnusedVertices(traced.mesh);
        result.mesh = traced.mesh;
        result.triangleOrigins = traced.origins;
        result.volume = std::accumulate(result.voids.cbegin(), result.voids.cend(), united.Volume(),
                                        [](double volume, const Void& pocket) { return volume - pocket.volume; });
        result.genus = united.Genus();
    }
    return result;
}

cwPassageTubeUnion::Result cwPassageTubeUnion::coveredSurface(const QVector<cwPassageTubeMesh>& tubes,
                                                              const QVector<QVector<int>>& faces,
                                                              const QVector<cwPassageTubeMesh>& around)
{
    Result result;
    std::vector<manifold::Manifold> inside;
    inside.reserve(tubes.size());
    QHash<quint32, int> inputOfOriginalID;
    const quint32 firstID = manifold::Manifold::ReserveIDs(static_cast<quint32>(tubes.size()));
    for (int i = 0; i < tubes.size(); ++i) {
        const quint32 originalID = firstID + static_cast<quint32>(i);
        manifold::Manifold solid = toManifold(tubes.at(i), originalID, faces.value(i));
        const Status inputStatus = toStatus(solid.Status());
        result.inputStatus.append(inputStatus);
        if (inputStatus == Status::NoError) {
            inputOfOriginalID.insert(originalID, i);
            inside.push_back(std::move(solid));
        }
    }
    std::vector<manifold::Manifold> outside;
    outside.reserve(around.size());
    for (const cwPassageTubeMesh& tube : around) {
        manifold::Manifold solid = toManifold(tube);
        if (solid.Status() == manifold::Manifold::Error::NoError) {
            outside.push_back(std::move(solid));
        }
    }
    if (inside.empty() || outside.empty()) {
        return result;
    }

    const manifold::Manifold covered = manifold::Manifold::BatchBoolean(inside, manifold::OpType::Add)
                                       ^ manifold::Manifold::BatchBoolean(outside, manifold::OpType::Add);
    result.status = toStatus(covered.Status());
    if (result.status == Status::NoError) {
        const TracedMesh traced = fromManifold(covered, inputOfOriginalID);
        for (int t = 0; t < traced.origins.size(); ++t) {
            if (traced.origins.at(t).input >= 0) {
                result.mesh.indices.append({traced.mesh.indices.at(3 * t), traced.mesh.indices.at(3 * t + 1), traced.mesh.indices.at(3 * t + 2)});
                result.triangleOrigins.append(traced.origins.at(t));
            }
        }
        result.mesh.positions = traced.mesh.positions;
        dropUnusedVertices(result.mesh);
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

QVector<int> cwPassageTubeUnion::ChainTube::faces() const
{
    const int triangleCount = static_cast<int>(mesh.indices.size() / 3);
    const int endCapStart = firstCapTriangle + fanTriangleCount(*this);
    QVector<int> result(triangleCount, kSides);
    for (int t = firstCapTriangle; t < triangleCount; ++t) {
        result[t] = t < endCapStart ? kStartCap : kEndCap;
    }
    return result;
}

QVector3D cwPassageTubeUnion::ChainTube::capCenter(int face) const
{
    const int fanTriangle = firstCapTriangle + (face == kEndCap ? fanTriangleCount(*this) : 0);
    // loft starts each fan triangle at the fan's center.
    return mesh.positions.at(mesh.indices.at(3 * fanTriangle));
}

QVector<cwPassageTubeUnion::ChainTube> cwPassageTubeUnion::chainTubes(const cwPassageTubeBuilder::Debug& rings)
{
    QVector<ChainTube> tubes;
    for (int c = 0; c < rings.chains.size(); ++c) {
        const Builder::Chain& chain = rings.chains.at(c);
        const auto tripOf = [&](int ring) { return tripOfRing(rings, chain, ring); };

        // Runs of ring indices with vertices; the segment ending at each
        // ring belongs to that ring's trip. A station whose legs read two
        // sections ends one run on the first and starts the next on the
        // second, so the two tubes overlap there.
        QVector<QVector<int>> runs(1);
        for (int r = 0; r < chain.rings.size(); ++r) {
            const Builder::Ring& ring = chain.rings.at(r);
            if (ring.vertices.isEmpty()) {
                if (!runs.last().isEmpty()) {
                    runs.append(QVector<int>());
                }
                continue;
            }
            if (!runs.last().isEmpty() && splitsBefore(rings, chain, r)) {
                runs.append(QVector<int>());
            }
            runs.last().append(r);
        }
        const bool isTorn = runs.size() > 1 || runs.first().size() < chain.rings.size();
        const bool isLoop = chain.startCap == Builder::EndCap::None && !isTorn && chain.rings.size() >= kMinLoopRings;

        if (isLoop) {
            // The last ring repeats the first: start at a ring where the trip
            // changes, so each trip's stretch is one run.
            const int last = static_cast<int>(chain.rings.size()) - 1;
            int cut = -1;
            for (int r = 1; r <= last && cut < 0; ++r) {
                if (tripOf(r) != tripOf(r == last ? 1 : r + 1)) {
                    cut = r;
                }
            }
            if (cut < 0) {
                QVector<QVector<QVector3D>> loop;
                for (const Builder::Ring& ring : chain.rings) {
                    loop.append(ring.vertices);
                }
                QVector<int> ringIndices(chain.rings.size());
                std::iota(ringIndices.begin(), ringIndices.end(), 0);
                ChainTube tube {c, tripOf(last), loft(loop, true), ringIndices, true};
                tube.firstCapTriangle = static_cast<int>(tube.mesh.indices.size() / 3);
                tubes.append(tube);
                continue;
            }
            QVector<int> rotated;
            for (int r = cut; r <= last; ++r) {
                rotated.append(r);
            }
            for (int r = 1; r <= cut; ++r) {
                rotated.append(r);
            }
            runs = {rotated};
        }

        for (const QVector<int>& run : std::as_const(runs)) {
            if (run.size() < 2) {
                continue;
            }
            QVector<ChainTube> pieces;
            int start = 0;
            for (int end = 1; end < run.size(); ++end) {
                const QUuid trip = tripOf(run.at(start + 1));
                const bool isRunEnd = end == run.size() - 1;
                if (!isRunEnd && tripOf(run.at(end + 1)) == trip) {
                    continue;
                }
                QVector<QVector<QVector3D>> pieceRings;
                for (int i = start; i <= end; ++i) {
                    pieceRings.append(chain.rings.at(run.at(i)).vertices);
                }
                ChainTube tube {c, trip, loft(pieceRings, false), run.mid(start, end - start + 1), false};
                const int resolution = static_cast<int>(pieceRings.first().size());
                const int triangleCount = static_cast<int>(tube.mesh.indices.size() / 3);
                tube.firstCapTriangle = resolution >= kMinFanVertices ? triangleCount - 2 * resolution : triangleCount;
                pieces.append(tube);
                start = end;
            }
            for (int i = 0; i + 1 < pieces.size(); ++i) {
                pieces[i].endNeighborTrip = pieces.at(i + 1).tripId;
                pieces[i + 1].startNeighborTrip = pieces.at(i).tripId;
            }
            if (isLoop) {
                pieces.first().startNeighborTrip = pieces.last().tripId;
                pieces.last().endNeighborTrip = pieces.first().tripId;
            }
            tubes.append(pieces);
        }
    }

    // A chain's end ring that another trip's chain ends on too runs on into
    // that trip there, as a chain cut between trips does.
    QHash<StationKey, QVector<std::pair<int, bool>>> tubeEndsAt;
    for (int t = 0; t < tubes.size(); ++t) {
        const ChainTube& tube = tubes.at(t);
        const Builder::Chain& chain = rings.chains.at(tube.chain);
        if (tube.isLoop) {
            continue;
        }
        if (tube.rings.first() == 0) {
            const Builder::ChainLeg& first = chain.legs.first();
            tubeEndsAt[stationKey(rings.legs.at(first.legIndex), cwPassageTubeGeometry::chainStartStation(rings.legs, first))].append({t, true});
        }
        if (tube.rings.last() == chain.rings.size() - 1) {
            const Builder::ChainLeg& last = chain.legs.last();
            tubeEndsAt[stationKey(rings.legs.at(last.legIndex), cwPassageTubeGeometry::chainEndStation(rings.legs, last))].append({t, false});
        }
    }
    const auto endRing = [&](int tubeIndex, bool atStart) -> const QVector<QVector3D>& {
        const ChainTube& end = tubes.at(tubeIndex);
        return rings.chains.at(end.chain).rings.at(atStart ? end.rings.first() : end.rings.last()).vertices;
    };
    const auto isSameRing = [](const QVector<QVector3D>& a, const QVector<QVector3D>& b) {
        return std::all_of(a.cbegin(), a.cend(), [&](const QVector3D& vertex) {
            return std::any_of(b.cbegin(), b.cend(), [&](const QVector3D& other) {
                return other.distanceToPoint(vertex) <= kMergeToleranceMeters;
            });
        });
    };
    for (const QVector<std::pair<int, bool>>& ends : std::as_const(tubeEndsAt)) {
        for (const auto& [t, isStart] : ends) {
            const auto otherTrip = std::find_if(ends.cbegin(), ends.cend(), [&](const std::pair<int, bool>& other) {
                return tubes.at(other.first).tripId != tubes.at(t).tripId
                       && isSameRing(endRing(t, isStart), endRing(other.first, other.second));
            });
            ChainTube& tube = tubes[t];
            std::optional<QUuid>& neighborTrip = isStart ? tube.startNeighborTrip : tube.endNeighborTrip;
            if (!neighborTrip && otherTrip != ends.cend()) {
                neighborTrip = tubes.at(otherTrip->first).tripId;
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

bool cwPassageTubeUnion::splitsBefore(const cwPassageTubeBuilder::Debug& rings, const cwPassageTubeBuilder::Chain& chain, int ring)
{
    if (ring <= 0 || ring >= chain.rings.size()) {
        return false;
    }
    const float step = chain.rings.at(ring - 1).center.distanceToPoint(chain.rings.at(ring).center);
    return step < Builder::kMinLegLength && tripOfRing(rings, chain, ring - 1) == tripOfRing(rings, chain, ring);
}

bool cwPassageTubeUnion::ChainTube::holdsLeg(const cwPassageTubeBuilder::Debug& rings, int legIndex) const
{
    const Builder::Chain& chainRings = rings.chains.at(chain);
    return std::any_of(this->rings.cbegin() + 1, this->rings.cend(), [&](int ring) {
        return chainRings.legs.at(chainRings.rings.at(ring).chainLeg).legIndex == legIndex;
    });
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
    const double winding = angleSum / kFourPi;
    return winding > (nearest <= kSurfaceToleranceMeters ? kOnSurfaceWinding : kInsideWinding);
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

QVector<cwPassageTubeUnion::CapVertex> cwPassageTubeUnion::slideCaps(cwPassageTubeBuilder::Debug& rings)
{
    const QVector<ChainTube> tubes = chainTubes(rings);
    const QHash<StationKey, QVector<int>> chainsAt = chainsByStation(rings);
    QVector<CapVertex> notch;
    for (const ChainEnd& end : chainEnds(rings)) {
        const Builder::Chain& endChain = rings.chains.at(end.chain);
        if (endChain.legPads.at(endChain.rings.at(end.capRing).chainLeg).isShaft) {
            continue;
        }
        const QVector<const cwPassageTubeMesh*> neighbors = neighborTubes(end, chainsAt, tubes);
        if (neighbors.isEmpty()) {
            continue;
        }
        Builder::Chain& chain = rings.chains[end.chain];
        const double legLength = chain.rings.at(end.capRing).center.distanceToPoint(chain.rings.at(end.farRing).center);
        const Vec axis(end.outward.normalized());

        for (int bin = 0; bin < chain.rings.at(end.capRing).vertices.size(); ++bin) {
            const QVector3D from = chain.rings.at(end.capRing).vertices.at(bin);
            if (insideAny(neighbors, from)) {
                continue;
            }

            // Each crossing by its distance along the axis and how far along
            // the axis the margin lies square to the wall it crosses.
            QVector<QPair<double, double>> crossings;
            for (const cwPassageTubeMesh* tube : neighbors) {
                for (qsizetype i = 0; i + 2 < tube->indices.size(); i += 3) {
                    const Triangle triangle = triangleAt(*tube, i);
                    if (const std::optional<double> t = lineCrossing(Vec(from), axis, triangle)) {
                        if (std::abs(*t) <= legLength) {
                            const Vec normal = Vec::cross(triangle.b - triangle.a, triangle.c - triangle.a);
                            const double incidence = std::abs(Vec::dot(normal, axis)) / normal.length();
                            crossings.append({*t, kSlideMarginMeters / std::max(incidence, kMinIncidence)});
                        }
                    }
                }
            }
            std::sort(crossings.begin(), crossings.end(), [](const QPair<double, double>& a, const QPair<double, double>& b) {
                return std::abs(a.first) < std::abs(b.first);
            });

            // The nearest crossing past which the line runs inside: the
            // margin past it, else the point nearest it that is inside
            // toward the middle of the stretch to the next crossing.
            const auto pointAt = [&](double t) { return from + end.outward.normalized() * static_cast<float>(t); };
            QVector3D to = from;
            for (int i = 0; i < crossings.size(); ++i) {
                const double crossing = crossings.at(i).first;
                const double sign = crossing < 0.0 ? -1.0 : 1.0;
                double next = sign * legLength;
                for (int j = i + 1; j < crossings.size(); ++j) {
                    if (crossings.at(j).first * sign > crossing * sign + kMinStepMeters) {
                        next = crossings.at(j).first;
                        break;
                    }
                }
                const double past = crossing + sign * std::min(crossings.at(i).second, (next - crossing) * sign / 2.0);
                if (insideAny(neighbors, pointAt(past))) {
                    to = pointAt(past);
                    break;
                }
                double outside = crossing;
                double inside = (crossing + next) / 2.0;
                if (!insideAny(neighbors, pointAt(inside))) {
                    continue;
                }
                for (int step = 0; step < kBisectionSteps; ++step) {
                    const double middle = (outside + inside) / 2.0;
                    if (insideAny(neighbors, pointAt(middle))) {
                        inside = middle;
                    } else {
                        outside = middle;
                    }
                }
                to = pointAt(inside);
                break;
            }
            notch.append({end.chain, end.atStart, bin, from, to});
            if (to == from) {
                continue;
            }

            // The bin's wall along the end leg runs from the slid vertex to
            // the leg's far ring, at each ring's original fraction.
            const QVector3D farVertex = chain.rings.at(end.farRing).vertices.at(bin);
            const QVector3D wall = farVertex - from;
            const float wallLengthSquared = wall.lengthSquared();
            const int step = end.farRing > end.capRing ? 1 : -1;
            for (int r = end.capRing + step; r != end.farRing && wallLengthSquared > kMinLegLengthSquared; r += step) {
                QVector3D& vertex = chain.rings[r].vertices[bin];
                const float fraction = QVector3D::dotProduct(vertex - from, wall) / wallLengthSquared;
                vertex = to + (farVertex - to) * fraction;
            }
            chain.rings[end.capRing].vertices[bin] = to;
        }
    }
    return notch;
}
