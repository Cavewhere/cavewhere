//Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QBox3D>
#include <QMatrix4x4>
#include <QVector3D>

// std
#include <algorithm>
#include <atomic>
#include <thread>

// SUT
#include "cwGeometryItersecter.h"
#include "TestGeometryBuilders.h"
#include "cwRenderObject.h"

namespace {

constexpr float kPickRadius = 0.1f;

cwGeometryItersecter::Object makePointCloud(uint64_t id,
                                            const QVector<QVector3D>& points,
                                            const QMatrix4x4& modelMatrix = QMatrix4x4())
{
    return cwGeometryItersecter::Object(nullptr, id, cwTestGeometry::points(points),
                                        modelMatrix, kPickRadius);
}

//! Integer lattice points (0..n-1)^3 — a deterministic cloud with n^3 vertices.
QVector<QVector3D> latticeGrid(int n)
{
    QVector<QVector3D> points;
    points.reserve(n * n * n);
    for (int x = 0; x < n; ++x) {
        for (int y = 0; y < n; ++y) {
            for (int z = 0; z < n; ++z) {
                points.append(QVector3D(float(x), float(y), float(z)));
            }
        }
    }
    return points;
}

//! Reference (brute-force) membership using the same predicate pointsInBox uses.
QList<QVector3D> bruteForceInside(const QVector<QVector3D>& points,
                                  const QBox3D& box,
                                  const QMatrix4x4& modelMatrix = QMatrix4x4())
{
    QList<QVector3D> inside;
    for (const QVector3D& p : points) {
        const QVector3D world = modelMatrix.isIdentity() ? p : modelMatrix.map(p);
        if (box.contains(world)) {
            inside.append(world);
        }
    }
    return inside;
}

//! Order-independent comparison of two point lists (lexicographic sort).
void requireSameSet(QList<QVector3D> a, QList<QVector3D> b)
{
    const auto less = [](const QVector3D& l, const QVector3D& r) {
        if (l.x() != r.x()) return l.x() < r.x();
        if (l.y() != r.y()) return l.y() < r.y();
        return l.z() < r.z();
    };
    std::sort(a.begin(), a.end(), less);
    std::sort(b.begin(), b.end(), less);
    REQUIRE(a == b);
}

} // namespace

TEST_CASE("pointsInBox returns exactly the points inside an interior box",
          "[cwGeometryItersecter][pointsInBox]")
{
    const QVector<QVector3D> grid = latticeGrid(10);

    cwGeometryItersecter intersector;
    intersector.addObject(makePointCloud(1, grid));
    intersector.waitForFinish();

    const QBox3D box(QVector3D(2.5f, 2.5f, 2.5f), QVector3D(5.5f, 5.5f, 5.5f));
    const QList<QVector3D> expected = bruteForceInside(grid, box);
    REQUIRE(expected.size() == 27); // coords {3,4,5}^3

    const auto snapshot = intersector.snapshotForQuery();
    REQUIRE_FALSE(snapshot.isNull());

    requireSameSet(cwGeometryItersecter::pointsInBox(snapshot, box, {cwRenderObjectId{}, 1}),
                   expected);
}

TEST_CASE("pointsInBox on an edge-straddling box returns only the interior points",
          "[cwGeometryItersecter][pointsInBox]")
{
    const QVector<QVector3D> grid = latticeGrid(10);

    cwGeometryItersecter intersector;
    intersector.addObject(makePointCloud(1, grid));
    intersector.waitForFinish();

    // Box hangs off the lattice on the low side; only coords {0,1,2} survive.
    const QBox3D box(QVector3D(-5.0f, -5.0f, -5.0f), QVector3D(2.5f, 2.5f, 2.5f));
    const QList<QVector3D> expected = bruteForceInside(grid, box);
    REQUIRE(expected.size() == 27);

    const auto snapshot = intersector.snapshotForQuery();
    requireSameSet(cwGeometryItersecter::pointsInBox(snapshot, box, {cwRenderObjectId{}, 1}),
                   expected);
}

TEST_CASE("pointsInBox over an empty region returns nothing",
          "[cwGeometryItersecter][pointsInBox]")
{
    cwGeometryItersecter intersector;
    intersector.addObject(makePointCloud(1, latticeGrid(8)));
    intersector.waitForFinish();

    const QBox3D box(QVector3D(100.0f, 100.0f, 100.0f),
                     QVector3D(200.0f, 200.0f, 200.0f));
    const auto snapshot = intersector.snapshotForQuery();

    REQUIRE(cwGeometryItersecter::pointsInBox(snapshot, box, {cwRenderObjectId{}, 1}).isEmpty());
}

TEST_CASE("pointsInBox follows a translated model matrix into world space",
          "[cwGeometryItersecter][pointsInBox]")
{
    const QVector<QVector3D> grid = latticeGrid(5);
    QMatrix4x4 modelMatrix;
    modelMatrix.translate(100.0f, 0.0f, 0.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makePointCloud(1, grid, modelMatrix));
    intersector.waitForFinish();

    // World box around the translated cloud; selects model coords {2,3}^3.
    const QBox3D box(QVector3D(101.5f, 1.5f, 1.5f), QVector3D(103.5f, 3.5f, 3.5f));
    const QList<QVector3D> expected = bruteForceInside(grid, box, modelMatrix);
    REQUIRE(expected.size() == 8);

    const auto snapshot = intersector.snapshotForQuery();
    const QList<QVector3D> got =
        cwGeometryItersecter::pointsInBox(snapshot, box, {cwRenderObjectId{}, 1});

    requireSameSet(got, expected);
    for (const QVector3D& p : got) {
        REQUIRE(p.x() >= 100.0f); // returned in world space, not model space
    }
}

TEST_CASE("pointsInBox rejects unknown keys, null boxes, and null snapshots",
          "[cwGeometryItersecter][pointsInBox]")
{
    cwGeometryItersecter intersector;
    intersector.addObject(makePointCloud(1, latticeGrid(6)));
    intersector.waitForFinish();

    const auto snapshot = intersector.snapshotForQuery();
    const QBox3D box(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(3.0f, 3.0f, 3.0f));

    SECTION("unknown key") {
        REQUIRE(cwGeometryItersecter::pointsInBox(snapshot, box, {cwRenderObjectId{}, 99}).isEmpty());
    }
    SECTION("null box") {
        REQUIRE(cwGeometryItersecter::pointsInBox(snapshot, QBox3D(), {cwRenderObjectId{}, 1}).isEmpty());
    }
    SECTION("null snapshot") {
        const cwGeometryItersecter::QuerySnapshot empty;
        REQUIRE(empty.isNull());
        REQUIRE(cwGeometryItersecter::pointsInBox(empty, box, {cwRenderObjectId{}, 1}).isEmpty());
    }
}

TEST_CASE("a snapshot stays stable and safe to read while the BVH is rebuilt",
          "[cwGeometryItersecter][pointsInBox]")
{
    const QVector<QVector3D> grid = latticeGrid(12); // 1728 points

    cwGeometryItersecter intersector;
    intersector.addObject(makePointCloud(1, grid));
    intersector.waitForFinish();

    const QBox3D box(QVector3D(2.5f, 2.5f, 2.5f), QVector3D(8.5f, 8.5f, 8.5f));
    const qsizetype expected = bruteForceInside(grid, box).size();
    REQUIRE(expected > 0);

    const auto snapshot = intersector.snapshotForQuery();

    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};
    std::atomic<int> mismatches{0};

    // The worker reads the immutable snapshot continuously; it must keep
    // returning the same set no matter how the live BVH churns underneath.
    std::thread worker([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            const QList<QVector3D> got =
                cwGeometryItersecter::pointsInBox(snapshot, box, {cwRenderObjectId{}, 1});
            if (got.size() != expected) {
                mismatches.fetch_add(1, std::memory_order_relaxed);
            }
            reads.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Hammer the live BVH on the main thread: each addObject copy-swaps m_bvh
    // (invalidatePublishedSlot) and waitForFinish publishes a freshly built
    // one — neither must disturb the worker's snapshot.
    for (int i = 0; i < 20; ++i) {
        intersector.addObject(makePointCloud(1, grid));
        intersector.waitForFinish();
    }

    stop.store(true, std::memory_order_relaxed);
    worker.join();

    REQUIRE(reads.load() > 0);
    REQUIRE(mismatches.load() == 0);
}
