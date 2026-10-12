/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPassageTubeMeshCheck.h"

//Qt includes
#include <QHash>
#include <QStringList>
#include <QtMath>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <utility>

namespace {

using Issue = cwPassageTubeMeshCheck::Issue;
using Range = cwPassageTubeMeshCheck::Range;
using Debug = cwPassageTubeBuilder::Debug;

constexpr double kPlaneEpsilon = 1e-7;
constexpr double kTouchTolerance = 1e-6;
constexpr double kOrientEpsilon = 1e-12;
// Triangles whose normals differ by less than about this angle in radians
// share a plane: the float positions of neighboring quads on a cone leave
// their planes a hair apart, where the line the planes cross along is noise.
constexpr double kParallelEpsilon = 1e-5;
constexpr double kRayEpsilon = 1e-9;
constexpr double kStationTolerance = 1e-4;
constexpr double kFoldBackTolerance = 1e-4;
constexpr double kMinCellSize = 1e-3;
constexpr double kMaxCellsPerTriangleAxis = 32.0;
constexpr int kMinLoopRings = 3;
// The builder leaves out an up or down within this angle of a leg.
constexpr double kAlongLegDegrees = 45.0;
// A ray along +x tilted by irrational amounts, so it misses edges and vertices.
constexpr double kRayTiltY = 1e-3 * std::numbers::sqrt2;
constexpr double kRayTiltZ = 1e-3 * std::numbers::sqrt3;

struct Vec {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec() = default;
    Vec(double x, double y, double z) : x(x), y(y), z(z) {}
    explicit Vec(const QVector3D& v) : x(v.x()), y(v.y()), z(v.z()) {}

    Vec operator+(const Vec& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec operator-(const Vec& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec operator*(double s) const { return {x * s, y * s, z * s}; }
    double operator[](int axis) const { return axis == 0 ? x : (axis == 1 ? y : z); }

    double length() const { return std::sqrt(dot(*this, *this)); }
    QVector3D toQVector3D() const { return QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)); }

    static double dot(const Vec& a, const Vec& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static Vec cross(const Vec& a, const Vec& b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    static Vec min(const Vec& a, const Vec& b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
    static Vec max(const Vec& a, const Vec& b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
};

struct Box {
    Vec lo;
    Vec hi;

    bool overlaps(const Box& o) const
    {
        return lo.x <= o.hi.x && o.lo.x <= hi.x
               && lo.y <= o.hi.y && o.lo.y <= hi.y
               && lo.z <= o.hi.z && o.lo.z <= hi.z;
    }

    bool contains(const Vec& p) const
    {
        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y && p.z >= lo.z && p.z <= hi.z;
    }

    double diagonal() const { return (hi - lo).length(); }
};

struct Triangle {
    std::array<quint32, 3> vertices;
    std::array<Vec, 3> points;
    int number = -1;
    Box box;

    Vec centroid() const { return (points[0] + points[1] + points[2]) * (1.0 / 3.0); }
    Vec normal() const { return Vec::cross(points[1] - points[0], points[2] - points[0]); }
};

QVector<Triangle> trianglesOf(const cwPassageTubeMesh& mesh, const Range& range)
{
    QVector<Triangle> triangles;
    triangles.reserve(range.count / 3);
    for (int i = range.first; i + 2 < range.first + range.count; i += 3) {
        Triangle triangle;
        triangle.number = i / 3;
        for (int corner = 0; corner < 3; ++corner) {
            triangle.vertices[corner] = mesh.indices.at(i + corner);
            triangle.points[corner] = Vec(mesh.positions.at(triangle.vertices[corner]));
        }
        triangle.box = {Vec::min(triangle.points[0], Vec::min(triangle.points[1], triangle.points[2])),
                        Vec::max(triangle.points[0], Vec::max(triangle.points[1], triangle.points[2]))};
        triangles.append(triangle);
    }
    return triangles;
}

class DisjointSet
{
public:
    explicit DisjointSet(int size) : m_parent(size)
    {
        std::iota(m_parent.begin(), m_parent.end(), 0);
    }

    int find(int i)
    {
        while (m_parent.at(i) != i) {
            m_parent[i] = m_parent.at(m_parent.at(i));
            i = m_parent.at(i);
        }
        return i;
    }

    void unite(int a, int b) { m_parent[find(a)] = find(b); }

private:
    QVector<int> m_parent;
};

quint64 edgeKey(quint32 from, quint32 to)
{
    return (static_cast<quint64>(from) << 32) | to;
}

quint32 edgeFrom(quint64 key) { return static_cast<quint32>(key >> 32); }
quint32 edgeTo(quint64 key) { return static_cast<quint32>(key & 0xffffffffu); }

Vec midpoint(const cwPassageTubeMesh& mesh, quint32 a, quint32 b)
{
    return (Vec(mesh.positions.at(a)) + Vec(mesh.positions.at(b))) * 0.5;
}

QString edgeDetail(quint32 a, quint32 b)
{
    return QStringLiteral("edge %1-%2").arg(a).arg(b);
}

// Dense ids for the vertices a range references.
QHash<quint32, int> vertexIds(const QVector<Triangle>& triangles)
{
    QHash<quint32, int> ids;
    for (const Triangle& triangle : triangles) {
        for (quint32 vertex : triangle.vertices) {
            ids.insert(vertex, ids.value(vertex, static_cast<int>(ids.size())));
        }
    }
    return ids;
}

// The component of each triangle, numbered 0..componentCount-1 in order of first triangle.
QVector<int> componentsOf(const QVector<Triangle>& triangles, int* componentCount)
{
    const QHash<quint32, int> ids = vertexIds(triangles);
    DisjointSet sets(static_cast<int>(ids.size()));
    for (const Triangle& triangle : triangles) {
        sets.unite(ids.value(triangle.vertices[0]), ids.value(triangle.vertices[1]));
        sets.unite(ids.value(triangle.vertices[0]), ids.value(triangle.vertices[2]));
    }

    QHash<int, int> componentOfRoot;
    QVector<int> components;
    components.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        const int root = sets.find(ids.value(triangle.vertices[0]));
        const int component = componentOfRoot.value(root, static_cast<int>(componentOfRoot.size()));
        componentOfRoot.insert(root, component);
        components.append(component);
    }
    *componentCount = static_cast<int>(componentOfRoot.size());
    return components;
}

double tetrahedronVolume(const Vec& origin, const Triangle& triangle)
{
    return Vec::dot(triangle.points[0] - origin,
                    Vec::cross(triangle.points[1] - origin, triangle.points[2] - origin)) / 6.0;
}

QVector<Issue> nonManifoldVertices(const cwPassageTubeMesh& mesh, const QVector<Triangle>& triangles)
{
    QHash<quint32, QVector<int>> incident;
    for (int t = 0; t < triangles.size(); ++t) {
        for (quint32 vertex : triangles.at(t).vertices) {
            incident[vertex].append(t);
        }
    }

    QVector<Issue> issues;
    for (auto it = incident.cbegin(); it != incident.cend(); ++it) {
        const quint32 vertex = it.key();
        const QVector<int>& around = it.value();
        DisjointSet fans(static_cast<int>(around.size()));
        QHash<quint32, int> triangleByNeighbor;
        for (int i = 0; i < around.size(); ++i) {
            for (quint32 neighbor : triangles.at(around.at(i)).vertices) {
                if (neighbor == vertex) {
                    continue;
                }
                const auto found = triangleByNeighbor.constFind(neighbor);
                if (found == triangleByNeighbor.cend()) {
                    triangleByNeighbor.insert(neighbor, i);
                } else {
                    fans.unite(i, found.value());
                }
            }
        }
        QSet<int> roots;
        for (int i = 0; i < around.size(); ++i) {
            roots.insert(fans.find(i));
        }
        if (roots.size() > 1) {
            issues.append(Issue {cwPassageTubeMeshCheck::NonManifoldVertex,
                                 mesh.positions.at(vertex),
                                 QStringLiteral("vertex %1 joins %2 separate fans").arg(vertex).arg(roots.size())});
        }
    }
    return issues;
}

struct Cell {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const Cell& o) const { return x == o.x && y == o.y && z == o.z; }
};

size_t qHash(const Cell& cell, size_t seed = 0)
{
    return qHashMulti(seed, cell.x, cell.y, cell.z);
}

// A uniform grid hash over triangles; each triangle sits in every cell its
// bounding box touches.
class TriangleGrid
{
public:
    explicit TriangleGrid(const QVector<Triangle>& triangles) :
        m_cellSize(cellSizeFor(triangles))
    {
        for (int t = 0; t < triangles.size(); ++t) {
            forEachCell(triangles.at(t).box, [&](const Cell& cell) { m_cells[cell].append(t); });
        }
    }

    Cell cellOf(const Vec& p) const
    {
        return {static_cast<int>(std::floor(p.x / m_cellSize)),
                static_cast<int>(std::floor(p.y / m_cellSize)),
                static_cast<int>(std::floor(p.z / m_cellSize))};
    }

    template<typename Visit>
    void forEachCell(const Box& box, Visit visit) const
    {
        const Cell lo = cellOf(box.lo);
        const Cell hi = cellOf(box.hi);
        for (int x = lo.x; x <= hi.x; ++x) {
            for (int y = lo.y; y <= hi.y; ++y) {
                for (int z = lo.z; z <= hi.z; ++z) {
                    visit(Cell {x, y, z});
                }
            }
        }
    }

    const QHash<Cell, QVector<int>>& cells() const { return m_cells; }

private:
    // The median edge length, coarsened so no triangle spans too many cells.
    static double cellSizeFor(const QVector<Triangle>& triangles)
    {
        QVector<double> lengths;
        lengths.reserve(triangles.size() * 3);
        double largestExtent = 0.0;
        for (const Triangle& triangle : triangles) {
            for (int corner = 0; corner < 3; ++corner) {
                lengths.append((triangle.points[(corner + 1) % 3] - triangle.points[corner]).length());
            }
            const Vec extent = triangle.box.hi - triangle.box.lo;
            largestExtent = std::max({largestExtent, extent.x, extent.y, extent.z});
        }
        if (lengths.isEmpty()) {
            return 1.0;
        }
        const auto middle = lengths.begin() + lengths.size() / 2;
        std::nth_element(lengths.begin(), middle, lengths.end());
        return std::max({*middle, largestExtent / kMaxCellsPerTriangleAxis, kMinCellSize});
    }

    double m_cellSize;
    QHash<Cell, QVector<int>> m_cells;
};

bool sharesVertex(const Triangle& a, const Triangle& b)
{
    for (quint32 va : a.vertices) {
        for (quint32 vb : b.vertices) {
            if (va == vb) {
                return true;
            }
        }
    }
    for (const Vec& pa : a.points) {
        for (const Vec& pb : b.points) {
            if ((pa - pb).length() < cwPassageTubeMeshCheck::kWeldTolerance) {
                return true;
            }
        }
    }
    return false;
}

std::array<double, 3> planeDistances(const Triangle& triangle, const Vec& unitNormal, const Vec& planePoint)
{
    std::array<double, 3> distances;
    for (int i = 0; i < 3; ++i) {
        const double distance = Vec::dot(unitNormal, triangle.points[i] - planePoint);
        distances[i] = std::abs(distance) < kPlaneEpsilon ? 0.0 : distance;
    }
    return distances;
}

bool allOneSide(const std::array<double, 3>& d)
{
    return (d[0] > 0.0 && d[1] > 0.0 && d[2] > 0.0) || (d[0] < 0.0 && d[1] < 0.0 && d[2] < 0.0);
}

// The points where triangle meets the plane its distances were taken to.
QVector<Vec> planeCrossing(const Triangle& triangle, const std::array<double, 3>& d)
{
    QVector<Vec> points;
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        if (d[i] == 0.0) {
            points.append(triangle.points[i]);
        }
        if (d[i] * d[j] < 0.0) {
            points.append(triangle.points[i] + (triangle.points[j] - triangle.points[i]) * (d[i] / (d[i] - d[j])));
        }
    }
    return points;
}

struct Interval {
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    Vec loPoint;
    Vec hiPoint;
};

Interval intervalAlong(const QVector<Vec>& points, const Vec& direction)
{
    Interval interval;
    for (const Vec& p : points) {
        const double t = Vec::dot(direction, p);
        if (t < interval.lo) {
            interval.lo = t;
            interval.loPoint = p;
        }
        if (t > interval.hi) {
            interval.hi = t;
            interval.hiPoint = p;
        }
    }
    return interval;
}

double orient2d(const std::array<double, 2>& a, const std::array<double, 2>& b, const std::array<double, 2>& c)
{
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

// Overlap of two triangles in one plane: the mean of the edge crossings and
// the corners strictly inside the other triangle.
std::optional<Vec> coplanarOverlap(const Triangle& a, const Triangle& b, const Vec& normal)
{
    const Vec absolute(std::abs(normal.x), std::abs(normal.y), std::abs(normal.z));
    const int dropped = absolute.x >= absolute.y && absolute.x >= absolute.z ? 0 : (absolute.y >= absolute.z ? 1 : 2);
    const int u = (dropped + 1) % 3;
    const int v = (dropped + 2) % 3;
    const auto flat = [&](const Vec& p) { return std::array<double, 2> {p[u], p[v]}; };

    Vec sum;
    int count = 0;
    const auto addInside = [&](const Triangle& corners, const Triangle& around) {
        for (const Vec& p : corners.points) {
            const double o0 = orient2d(flat(around.points[0]), flat(around.points[1]), flat(p));
            const double o1 = orient2d(flat(around.points[1]), flat(around.points[2]), flat(p));
            const double o2 = orient2d(flat(around.points[2]), flat(around.points[0]), flat(p));
            const bool inside = (o0 > kOrientEpsilon && o1 > kOrientEpsilon && o2 > kOrientEpsilon)
                                || (o0 < -kOrientEpsilon && o1 < -kOrientEpsilon && o2 < -kOrientEpsilon);
            if (inside) {
                sum = sum + p;
                ++count;
            }
        }
    };
    addInside(a, b);
    addInside(b, a);

    for (int i = 0; i < 3; ++i) {
        const Vec& p0 = a.points[i];
        const Vec& p1 = a.points[(i + 1) % 3];
        for (int j = 0; j < 3; ++j) {
            const Vec& q0 = b.points[j];
            const Vec& q1 = b.points[(j + 1) % 3];
            const double o1 = orient2d(flat(p0), flat(p1), flat(q0));
            const double o2 = orient2d(flat(p0), flat(p1), flat(q1));
            const double o3 = orient2d(flat(q0), flat(q1), flat(p0));
            const double o4 = orient2d(flat(q0), flat(q1), flat(p1));
            const bool crosses = ((o1 > kOrientEpsilon && o2 < -kOrientEpsilon) || (o1 < -kOrientEpsilon && o2 > kOrientEpsilon))
                                 && ((o3 > kOrientEpsilon && o4 < -kOrientEpsilon) || (o3 < -kOrientEpsilon && o4 > kOrientEpsilon));
            if (crosses) {
                sum = sum + p0 + (p1 - p0) * (o3 / (o3 - o4));
                ++count;
            }
        }
    }

    if (count == 0) {
        return std::nullopt;
    }
    return sum * (1.0 / count);
}

// Möller's interval test: the midpoint of the segment where the triangles
// cross, or nullopt when they only touch or stay apart.
std::optional<Vec> intersection(const Triangle& a, const Triangle& b)
{
    Vec normalA = a.normal();
    Vec normalB = b.normal();
    const double lengthA = normalA.length();
    const double lengthB = normalB.length();
    if (lengthA == 0.0 || lengthB == 0.0) {
        return std::nullopt;
    }
    normalA = normalA * (1.0 / lengthA);
    normalB = normalB * (1.0 / lengthB);

    const std::array<double, 3> distancesA = planeDistances(a, normalB, b.points[0]);
    if (allOneSide(distancesA)) {
        return std::nullopt;
    }
    const std::array<double, 3> distancesB = planeDistances(b, normalA, a.points[0]);
    if (allOneSide(distancesB)) {
        return std::nullopt;
    }

    Vec direction = Vec::cross(normalA, normalB);
    const double directionLength = direction.length();
    const bool coplanar = distancesA[0] == 0.0 && distancesA[1] == 0.0 && distancesA[2] == 0.0;
    if (coplanar || directionLength < kParallelEpsilon) {
        return coplanarOverlap(a, b, normalA);
    }
    direction = direction * (1.0 / directionLength);

    const Interval onA = intervalAlong(planeCrossing(a, distancesA), direction);
    const Interval onB = intervalAlong(planeCrossing(b, distancesB), direction);
    const double lo = std::max(onA.lo, onB.lo);
    const double hi = std::min(onA.hi, onB.hi);
    if (hi - lo <= kTouchTolerance) {
        return std::nullopt;
    }
    const Vec& loPoint = onA.lo >= onB.lo ? onA.loPoint : onB.loPoint;
    const Vec& hiPoint = onA.hi <= onB.hi ? onA.hiPoint : onB.hiPoint;
    return (loPoint + hiPoint) * 0.5;
}

// Möller–Trumbore: whether the ray from origin along direction crosses triangle.
bool rayCrosses(const Vec& origin, const Vec& direction, const Triangle& triangle)
{
    const Vec edge1 = triangle.points[1] - triangle.points[0];
    const Vec edge2 = triangle.points[2] - triangle.points[0];
    const Vec p = Vec::cross(direction, edge2);
    const double determinant = Vec::dot(edge1, p);
    if (std::abs(determinant) < kOrientEpsilon) {
        return false;
    }
    const double inverse = 1.0 / determinant;
    const Vec s = origin - triangle.points[0];
    const double u = Vec::dot(s, p) * inverse;
    if (u < 0.0 || u > 1.0) {
        return false;
    }
    const Vec q = Vec::cross(s, edge1);
    const double v = Vec::dot(direction, q) * inverse;
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }
    return Vec::dot(edge2, q) * inverse > kRayEpsilon;
}

// The closest point on triangle to p (Ericson, Real-Time Collision Detection 5.1.5).
Vec closestPoint(const Vec& p, const Triangle& triangle)
{
    const Vec& a = triangle.points[0];
    const Vec& b = triangle.points[1];
    const Vec& c = triangle.points[2];
    const Vec ab = b - a;
    const Vec ac = c - a;
    const Vec ap = p - a;
    const double d1 = Vec::dot(ab, ap);
    const double d2 = Vec::dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) {
        return a;
    }
    const Vec bp = p - b;
    const double d3 = Vec::dot(ab, bp);
    const double d4 = Vec::dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) {
        return b;
    }
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        return a + ab * (d1 / (d1 - d3));
    }
    const Vec cp = p - c;
    const double d5 = Vec::dot(ab, cp);
    const double d6 = Vec::dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) {
        return c;
    }
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        return a + ac * (d2 / (d2 - d6));
    }
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const double denominator = 1.0 / (va + vb + vc);
    return a + ab * (vb * denominator) + ac * (vc * denominator);
}

bool isNear(const QVector3D& a, const QVector3D& b)
{
    return a.distanceToPoint(b) < kStationTolerance;
}

// A station's ring, or a ring of the elbow that carries a sharp corner
// station's readings.
bool isStationRing(const Debug& debug, const cwPassageTubeBuilder::Chain& chain, const cwPassageTubeBuilder::Ring& ring)
{
    const cwPassageTubeBuilder::Leg& leg = debug.legs.at(chain.legs.at(ring.chainLeg).legIndex);
    return ring.isElbow || isNear(ring.center, leg.fromPosition) || isNear(ring.center, leg.toPosition);
}

std::optional<Issue> foldBack(const cwPassageTubeBuilder::Ring& previous,
                              const cwPassageTubeBuilder::Ring& ring,
                              const QString& detail)
{
    double deepest = -kFoldBackTolerance;
    std::optional<QVector3D> worst;
    for (const QVector3D& vertex : ring.vertices) {
        const double along = QVector3D::dotProduct(vertex - previous.center, previous.frame.tangent);
        if (along < deepest) {
            deepest = along;
            worst = vertex;
        }
    }
    if (!worst) {
        return std::nullopt;
    }
    return Issue {cwPassageTubeMeshCheck::FoldBack, *worst,
                  QStringLiteral("%1 lies %2 m behind the previous ring").arg(detail).arg(-deepest)};
}

bool isAlongLeg(const QVector3D& direction, const QVector<QVector3D>& legDirections)
{
    const float minCosine = static_cast<float>(std::cos(qDegreesToRadians(kAlongLegDegrees)));
    return std::any_of(legDirections.cbegin(), legDirections.cend(), [&](const QVector3D& legDirection) {
        return QVector3D::dotProduct(direction, legDirection) > minCosine;
    });
}

} // namespace

cwPassageTubeMeshCheck::Range cwPassageTubeMeshCheck::wholeMesh(const cwPassageTubeMesh& mesh)
{
    return {0, static_cast<int>(mesh.indices.size())};
}

cwPassageTubeMeshCheck::Report cwPassageTubeMeshCheck::closed(const cwPassageTubeMesh& mesh, const Range& range)
{
    const QVector<Triangle> triangles = trianglesOf(mesh, range);
    Report report;
    const QVector<int> componentOfTriangle = componentsOf(triangles, &report.componentCount);
    report.components.resize(report.componentCount);

    QHash<quint64, int> directedEdges;
    QHash<quint32, int> componentOfVertex;
    for (int t = 0; t < triangles.size(); ++t) {
        const Triangle& triangle = triangles.at(t);
        for (int corner = 0; corner < 3; ++corner) {
            directedEdges[edgeKey(triangle.vertices[corner], triangle.vertices[(corner + 1) % 3])]++;
            componentOfVertex.insert(triangle.vertices[corner], componentOfTriangle.at(t));
        }
    }

    QVector<bool> isSound(report.componentCount, true);
    for (auto it = directedEdges.cbegin(); it != directedEdges.cend(); ++it) {
        const quint32 from = edgeFrom(it.key());
        const quint32 to = edgeTo(it.key());
        const int forward = it.value();
        const int backward = directedEdges.value(edgeKey(to, from));
        const bool isFirstOfPair = from < to || backward == 0;
        if (!isFirstOfPair) {
            continue;
        }

        const int component = componentOfVertex.value(from);
        report.components[component].edgeCount++;
        if (forward == 1 && backward == 1) {
            continue;
        }
        isSound[component] = false;
        const Type type = forward + backward == 1 ? OpenEdge : InconsistentWinding;
        report.issues.append(Issue {type, midpoint(mesh, from, to).toQVector3D(), edgeDetail(from, to)});
    }

    report.issues.append(nonManifoldVertices(mesh, triangles));

    QVector<std::optional<Vec>> origins(report.componentCount);
    for (int t = 0; t < triangles.size(); ++t) {
        const Triangle& triangle = triangles.at(t);
        ComponentStats& stats = report.components[componentOfTriangle.at(t)];
        std::optional<Vec>& origin = origins[componentOfTriangle.at(t)];
        if (!origin) {
            origin = triangle.points[0];
        }
        stats.triangleCount++;
        stats.volume += tetrahedronVolume(*origin, triangle);
    }
    for (auto it = componentOfVertex.cbegin(); it != componentOfVertex.cend(); ++it) {
        report.components[it.value()].vertexCount++;
    }

    for (int c = 0; c < report.componentCount; ++c) {
        ComponentStats& stats = report.components[c];
        const int eulerCharacteristic = stats.vertexCount - stats.edgeCount + stats.triangleCount;
        stats.genus = (2 - eulerCharacteristic) / 2;
        if (isSound.at(c) && stats.volume <= 0.0) {
            report.issues.append(Issue {NegativeVolume, origins.at(c)->toQVector3D(),
                                        QStringLiteral("component %1 encloses %2 m³").arg(c).arg(stats.volume)});
        }
    }
    return report;
}

double cwPassageTubeMeshCheck::signedVolume(const cwPassageTubeMesh& mesh, const Range& range)
{
    const QVector<Triangle> triangles = trianglesOf(mesh, range);
    if (triangles.isEmpty()) {
        return 0.0;
    }
    const Vec origin = triangles.first().points[0];
    double volume = 0.0;
    for (const Triangle& triangle : triangles) {
        volume += tetrahedronVolume(origin, triangle);
    }
    return volume;
}

QVector<cwPassageTubeMeshCheck::Issue> cwPassageTubeMeshCheck::degenerateTriangles(const cwPassageTubeMesh& mesh,
                                                                                   const Range& range,
                                                                                   const Options& options)
{
    QVector<Issue> issues;
    for (const Triangle& triangle : trianglesOf(mesh, range)) {
        const double area = triangle.normal().length() / 2.0;
        double longest = 0.0;
        for (int corner = 0; corner < 3; ++corner) {
            longest = std::max(longest, (triangle.points[(corner + 1) % 3] - triangle.points[corner]).length());
        }
        const QVector3D centroid = triangle.centroid().toQVector3D();
        if (area < options.minTriangleArea) {
            issues.append(Issue {DegenerateTriangle, centroid, QStringLiteral("area %1 m²").arg(area), triangle.number});
            continue;
        }
        // Longest edge over shortest altitude; the shortest altitude falls on the longest edge.
        const double aspectRatio = longest * longest / (2.0 * area);
        if (aspectRatio > options.maxAspectRatio) {
            issues.append(Issue {SliverTriangle, centroid, QStringLiteral("aspect ratio %1").arg(aspectRatio), triangle.number});
        }
    }
    return issues;
}

QVector<cwPassageTubeMeshCheck::Issue> cwPassageTubeMeshCheck::selfIntersections(const cwPassageTubeMesh& mesh, const Range& range)
{
    const QVector<Triangle> triangles = trianglesOf(mesh, range);
    const TriangleGrid grid(triangles);

    QVector<Issue> issues;
    for (auto it = grid.cells().cbegin(); it != grid.cells().cend(); ++it) {
        const QVector<int>& inCell = it.value();
        for (int i = 0; i < inCell.size(); ++i) {
            const Triangle& a = triangles.at(inCell.at(i));
            for (int j = i + 1; j < inCell.size(); ++j) {
                const Triangle& b = triangles.at(inCell.at(j));
                if (!a.box.overlaps(b.box)) {
                    continue;
                }
                // Test each pair once: in the cell holding the low corner of their overlap.
                if (!(grid.cellOf(Vec::max(a.box.lo, b.box.lo)) == it.key())) {
                    continue;
                }
                if (sharesVertex(a, b)) {
                    continue;
                }
                const std::optional<Vec> crossing = intersection(a, b);
                if (crossing) {
                    const int first = std::min(a.number, b.number);
                    const int second = std::max(a.number, b.number);
                    issues.append(Issue {SelfIntersection, crossing->toQVector3D(),
                                         QStringLiteral("triangles %1 and %2 cross").arg(first).arg(second),
                                         first, second});
                }
            }
        }
    }
    std::sort(issues.begin(), issues.end(), [](const Issue& x, const Issue& y) {
        return std::pair(x.triangle, x.otherTriangle) < std::pair(y.triangle, y.otherTriangle);
    });
    return issues;
}

QVector<cwPassageTubeMeshCheck::Issue> cwPassageTubeMeshCheck::nestedShells(const cwPassageTubeMesh& mesh,
                                                                            const Range& range,
                                                                            const QVector<Issue>& selfIntersections)
{
    const QVector<Triangle> triangles = trianglesOf(mesh, range);
    int componentCount = 0;
    const QVector<int> componentOfTriangle = componentsOf(triangles, &componentCount);

    using ComponentPair = std::pair<int, int>;
    const int firstTriangle = range.first / 3;
    QSet<ComponentPair> crossingComponents;
    for (const Issue& crossing : selfIntersections) {
        const int a = componentOfTriangle.at(crossing.triangle - firstTriangle);
        const int b = componentOfTriangle.at(crossing.otherTriangle - firstTriangle);
        crossingComponents.insert({std::min(a, b), std::max(a, b)});
    }

    struct Shell {
        QVector<int> triangles;
        Box box;
    };
    QVector<Shell> shells(componentCount);
    for (int t = 0; t < triangles.size(); ++t) {
        Shell& shell = shells[componentOfTriangle.at(t)];
        const Box& box = triangles.at(t).box;
        shell.box = shell.triangles.isEmpty() ? box : Box {Vec::min(shell.box.lo, box.lo), Vec::max(shell.box.hi, box.hi)};
        shell.triangles.append(t);
    }

    const Vec rayDirection(1.0, kRayTiltY, kRayTiltZ);
    QVector<Issue> issues;
    for (int inner = 0; inner < componentCount; ++inner) {
        const Shell& innerShell = shells.at(inner);
        const Vec probe = triangles.at(innerShell.triangles.first()).points[0];
        for (int outer = 0; outer < componentCount; ++outer) {
            const Shell& outerShell = shells.at(outer);
            if (inner == outer
                || innerShell.box.diagonal() >= outerShell.box.diagonal()
                || crossingComponents.contains({std::min(inner, outer), std::max(inner, outer)})
                || !outerShell.box.contains(probe)) {
                continue;
            }
            const auto crossings = std::count_if(outerShell.triangles.cbegin(), outerShell.triangles.cend(), [&](int t) {
                return rayCrosses(probe, rayDirection, triangles.at(t));
            });
            if (crossings % 2 == 1) {
                issues.append(Issue {NestedShell, probe.toQVector3D(),
                                     QStringLiteral("component %1 lies inside component %2").arg(inner).arg(outer),
                                     triangles.at(innerShell.triangles.first()).number,
                                     triangles.at(outerShell.triangles.first()).number});
            }
        }
    }
    return issues;
}

QVector<cwPassageTubeMeshCheck::Issue> cwPassageTubeMeshCheck::creases(const cwPassageTubeMesh& mesh,
                                                                       const Range& range,
                                                                       const QSet<quint32>& intendedCreaseVertices,
                                                                       const Options& options)
{
    const QVector<Triangle> triangles = trianglesOf(mesh, range);
    QHash<quint64, QVector<int>> trianglesByEdge;
    for (int t = 0; t < triangles.size(); ++t) {
        const Triangle& triangle = triangles.at(t);
        for (int corner = 0; corner < 3; ++corner) {
            const quint32 a = triangle.vertices[corner];
            const quint32 b = triangle.vertices[(corner + 1) % 3];
            trianglesByEdge[edgeKey(std::min(a, b), std::max(a, b))].append(t);
        }
    }

    const double minCosine = std::cos(qDegreesToRadians(options.maxCreaseDegrees));
    QVector<Issue> issues;
    for (auto it = trianglesByEdge.cbegin(); it != trianglesByEdge.cend(); ++it) {
        const quint32 a = edgeFrom(it.key());
        const quint32 b = edgeTo(it.key());
        if (it.value().size() != 2 || intendedCreaseVertices.contains(a) || intendedCreaseVertices.contains(b)) {
            continue;
        }
        const Triangle& first = triangles.at(it.value().at(0));
        const Triangle& second = triangles.at(it.value().at(1));
        const Vec n1 = first.normal();
        const Vec n2 = second.normal();
        const double lengths = n1.length() * n2.length();
        if (lengths == 0.0) {
            continue;
        }
        const double cosine = Vec::dot(n1, n2) / lengths;
        if (cosine < minCosine) {
            const double degrees = qRadiansToDegrees(std::acos(std::clamp(cosine, -1.0, 1.0)));
            issues.append(Issue {Crease, midpoint(mesh, a, b).toQVector3D(),
                                 QStringLiteral("%1 folds %2°").arg(edgeDetail(a, b)).arg(degrees),
                                 first.number, second.number});
        }
    }
    return issues;
}

QVector<cwPassageTubeMeshCheck::Issue> cwPassageTubeMeshCheck::ringIssues(const cwPassageTubeBuilder::Debug& debug,
                                                                          const Options& options)
{
    QVector<Issue> issues;
    for (int c = 0; c < debug.chains.size(); ++c) {
        const cwPassageTubeBuilder::Chain& chain = debug.chains.at(c);
        const auto ringDetail = [c](int r) { return QStringLiteral("chain %1 ring %2").arg(c).arg(r); };

        for (int r = 1; r < chain.rings.size(); ++r) {
            const cwPassageTubeBuilder::Ring& previous = chain.rings.at(r - 1);
            const cwPassageTubeBuilder::Ring& ring = chain.rings.at(r);
            if (previous.vertices.isEmpty() || ring.vertices.isEmpty()) {
                continue;
            }

            if (QVector3D::dotProduct(ring.frame.right, previous.frame.right) <= 0.0f) {
                issues.append(Issue {FrameTwist, ring.center,
                                     QStringLiteral("%1 turns more than 90° from the previous ring").arg(ringDetail(r))});
            }

            if (const std::optional<Issue> folded = foldBack(previous, ring, ringDetail(r))) {
                issues.append(*folded);
            }

            // A ring grown at a junction follows the arm's outline, which may
            // drop sharply at the arm's walls.
            const bool isBetweenStations = !isStationRing(debug, chain, previous) && !isStationRing(debug, chain, ring);
            const bool isGrown = previous.isGrown || ring.isGrown;
            if (isBetweenStations && !isGrown && previous.radii.size() == ring.radii.size()) {
                for (int bin = 0; bin < ring.radii.size(); ++bin) {
                    const double smaller = std::min(ring.radii.at(bin), previous.radii.at(bin));
                    const double larger = std::max(ring.radii.at(bin), previous.radii.at(bin));
                    if (larger > options.maxRadiusRatio * smaller) {
                        issues.append(Issue {RadiusJump, ring.center,
                                             QStringLiteral("%1 bin %2 jumps from %3 m to %4 m")
                                                 .arg(ringDetail(r)).arg(bin)
                                                 .arg(previous.radii.at(bin)).arg(ring.radii.at(bin))});
                        break;
                    }
                }
            }
        }

        const bool isLoop = chain.startCap == cwPassageTubeBuilder::EndCap::None && chain.rings.size() >= kMinLoopRings;
        if (isLoop) {
            const cwPassageTubeBuilder::Ring& closing = chain.rings.at(chain.rings.size() - 2);
            const cwPassageTubeBuilder::Ring& first = chain.rings.first();
            if (!closing.vertices.isEmpty() && !first.vertices.isEmpty()) {
                if (const std::optional<Issue> folded = foldBack(closing, first, ringDetail(0))) {
                    issues.append(*folded);
                }
            }
        }
    }
    return issues;
}

QVector<cwPassageTubeMeshCheck::Issue> cwPassageTubeMeshCheck::wallsOnSurface(const cwPassageTubeBuilder::Debug& debug,
                                                                              const Options& options)
{
    const QVector<Triangle> triangles = trianglesOf(debug.mesh, wholeMesh(debug.mesh));
    const TriangleGrid grid(triangles);
    const double tolerance = options.wallToleranceMeters;

    const auto isOnSurface = [&](const Vec& point) {
        const Vec reach(tolerance, tolerance, tolerance);
        const Box query {point - reach, point + reach};
        bool found = false;
        grid.forEachCell(query, [&](const Cell& cell) {
            if (found) {
                return;
            }
            const QVector<int> inCell = grid.cells().value(cell);
            found = std::any_of(inCell.cbegin(), inCell.cend(), [&](int t) {
                const Triangle& triangle = triangles.at(t);
                return triangle.box.overlaps(query) && (closestPoint(point, triangle) - point).length() <= tolerance;
            });
        });
        return found;
    };

    using StationKey = std::pair<QUuid, QString>;
    QHash<StationKey, QVector<QVector3D>> legDirections;
    for (const cwPassageTubeBuilder::Leg& leg : debug.legs) {
        const QVector3D direction = (leg.toPosition - leg.fromPosition).normalized();
        legDirections[{leg.caveId, leg.from}].append(direction);
        legDirections[{leg.caveId, leg.to}].append(-direction);
    }

    // The builder reshapes the inside of a bend too sharp for its walls and
    // warns at the corner station; its walls there are no longer the tube's.
    // Where an arm joins a through passage, the walls of the junction station
    // open into the other passage, and an arm left inside the through passage
    // has its walls inside it.
    QSet<QString> reshaped;
    for (const cwPassageTubeMesh::Warning& warning : debug.warnings) {
        if (warning.type == cwPassageTubeMesh::Warning::BendClamped) {
            reshaped.insert(cwStation::canonicalKey(warning.station));
        }
    }
    const auto stationOf = [&](const cwPassageTubeBuilder::ChainLeg& chainLeg, bool atEnd) {
        const cwPassageTubeBuilder::Leg& leg = debug.legs.at(chainLeg.legIndex);
        return cwStation::canonicalKey(atEnd != chainLeg.reversed ? leg.to : leg.from);
    };
    for (const cwPassageTubeBuilder::Chain& chain : debug.chains) {
        if (chain.rings.isEmpty()) {
            for (const cwPassageTubeBuilder::ChainLeg& chainLeg : chain.legs) {
                reshaped.insert(stationOf(chainLeg, false));
                reshaped.insert(stationOf(chainLeg, true));
            }
            continue;
        }
        if (!chain.startMouth.isEmpty()) {
            reshaped.insert(stationOf(chain.legs.first(), false));
        }
        if (!chain.endMouth.isEmpty()) {
            reshaped.insert(stationOf(chain.legs.last(), true));
        }
    }

    constexpr QVector3D kWorldUp(0.0f, 0.0f, 1.0f);
    using Occurrence = std::pair<QUuid, std::pair<int, int>>;
    QSet<Occurrence> visited;
    QVector<Issue> issues;

    const auto checkStation = [&](const cwPassageTubeBuilder::Leg& leg,
                                  int stationIndex,
                                  const QString& station,
                                  const QVector3D& position,
                                  const cwStationLrud& lrud,
                                  const cwStationLrud& passage,
                                  const QVector3D& facing) {
        const Occurrence occurrence {leg.tripId, {leg.chunkIndex, stationIndex}};
        if (visited.contains(occurrence) || reshaped.contains(cwStation::canonicalKey(station))) {
            return;
        }
        visited.insert(occurrence);

        const QVector<QVector3D> directions = legDirections.value({leg.caveId, station});
        const QVector3D right(facing.y(), -facing.x(), 0.0f);
        const auto checkWall = [&](const std::optional<double>& reading,
                                   const std::optional<double>& passageReading,
                                   const QVector3D& direction,
                                   const QString& name) {
            const bool isReplaced = reading != passageReading;
            const bool isDroppedAlongLeg = direction.z() != 0.0f && isAlongLeg(direction, directions);
            if (!passageReading || isReplaced || isDroppedAlongLeg) {
                return;
            }
            const Vec wall = Vec(position) + Vec(direction) * *passageReading;
            if (!isOnSurface(wall)) {
                issues.append(Issue {WallMissed, wall.toQVector3D(),
                                     QStringLiteral("station %1's %2 wall at %3 m lies off the tube").arg(station, name).arg(*passageReading)});
            }
        };
        checkWall(lrud.left, passage.left, -right, QStringLiteral("left"));
        checkWall(lrud.right, passage.right, right, QStringLiteral("right"));
        checkWall(lrud.up, passage.up, kWorldUp, QStringLiteral("up"));
        checkWall(lrud.down, passage.down, -kWorldUp, QStringLiteral("down"));
    };

    for (const cwPassageTubeBuilder::Leg& leg : debug.legs) {
        checkStation(leg, leg.shotIndex, leg.from, leg.fromPosition, leg.fromLrud, leg.fromPassageLrud, leg.fromFacing);
        checkStation(leg, leg.shotIndex + 1, leg.to, leg.toPosition, leg.toLrud, leg.toPassageLrud, leg.toFacing);
    }
    return issues;
}

cwPassageTubeMeshCheck::Report cwPassageTubeMeshCheck::check(const cwPassageTubeMesh& mesh,
                                                             const cwPassageTubeBuilder::Debug& debug,
                                                             const Options& options)
{
    Report report;
    const Range whole = wholeMesh(mesh);
    const QVector<Range> ranges = mesh.tripIndexRanges.isEmpty() ? QVector<Range> {whole} : mesh.tripIndexRanges;
    for (const Range& range : ranges) {
        const Report closedReport = closed(mesh, range);
        report.issues.append(closedReport.issues);
        report.componentCount += closedReport.componentCount;
        report.components.append(closedReport.components);
        report.issues.append(creases(mesh, range, debug.intendedCreaseVertices, options));
    }

    report.issues.append(degenerateTriangles(mesh, whole, options));

    const QVector<Issue> crossings = selfIntersections(mesh, whole);
    report.issues.append(crossings);
    report.issues.append(nestedShells(mesh, whole, crossings));

    if (!debug.chains.isEmpty()) {
        report.issues.append(ringIssues(debug, options));
        report.issues.append(wallsOnSurface(debug, options));
    }
    return report;
}

QString cwPassageTubeMeshCheck::typeName(Type type)
{
    switch (type) {
    case OpenEdge:
        return QStringLiteral("OpenEdge");
    case InconsistentWinding:
        return QStringLiteral("InconsistentWinding");
    case NonManifoldVertex:
        return QStringLiteral("NonManifoldVertex");
    case NegativeVolume:
        return QStringLiteral("NegativeVolume");
    case DegenerateTriangle:
        return QStringLiteral("DegenerateTriangle");
    case SliverTriangle:
        return QStringLiteral("SliverTriangle");
    case SelfIntersection:
        return QStringLiteral("SelfIntersection");
    case NestedShell:
        return QStringLiteral("NestedShell");
    case Crease:
        return QStringLiteral("Crease");
    case RadiusJump:
        return QStringLiteral("RadiusJump");
    case FrameTwist:
        return QStringLiteral("FrameTwist");
    case FoldBack:
        return QStringLiteral("FoldBack");
    case WallMissed:
        return QStringLiteral("WallMissed");
    }
    return QString();
}

QString cwPassageTubeMeshCheck::toText(const Report& report)
{
    QStringList lines;
    lines.reserve(report.issues.size());
    for (const Issue& issue : report.issues) {
        lines.append(QStringLiteral("%1 at (%2, %3, %4): %5")
                         .arg(typeName(issue.type))
                         .arg(issue.position.x())
                         .arg(issue.position.y())
                         .arg(issue.position.z())
                         .arg(issue.detail));
    }
    return lines.join(QLatin1Char('\n'));
}
