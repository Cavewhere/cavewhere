// Picking a streamed point cloud: the pick set the render thread publishes,
// and the provider seam the intersecter consults it through. No RHI here —
// the set is fed the same quantized bytes a node would have uploaded.

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QBox3D>
#include <QMatrix4x4>
#include <QRay3D>
#include <QStringList>
#include <QVector3D>
#include <QtEndian>

//Std includes
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

//Our includes
#include "TestGeometryBuilders.h"
#include "cwGeometryItersecter.h"
#include "cwPickQuery.h"
#include "cwPointOctree.h"
#include "cwPointOctreePickIndex.h"
#include "cwPointOctreePickSet.h"
#include "cwProfileLog.h"
#include "cwRayHit.h"
#include "cwRenderObject.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "ProfileLogCapture.h"

using namespace Catch;

namespace {

    constexpr double kPointTolerance = 1e-3;

    //! A pick set node holding @a points, quantized into @a bounds exactly the
    //! way cw::octree::quantizeAll writes a node's payload to disk, and indexed
    //! the way cwRHIPointCloud::loadNode indexes it.
    cwPointOctreePickSet::Node makeNode(const QBox3D& bounds,
                                        const QVector<QVector3D>& points)
    {
        const QByteArray bytes = cw::octree::quantizeAll(points, bounds);
        return {bounds, bytes, cwPointOctreePickIndex::build(bytes)};
    }

    QBox3D cubeAt(const QVector3D& minimum, float size)
    {
        return QBox3D(minimum, minimum + QVector3D(size, size, size));
    }

    //! A ray down -Z through (x, y), starting well in front of everything.
    QRay3D rayDown(float x, float y)
    {
        return QRay3D(QVector3D(x, y, 100.0f), QVector3D(0.0f, 0.0f, -1.0f));
    }

    cwPickTolerance toleranceOf(double constant)
    {
        cwPickTolerance tolerance;
        tolerance.constant = constant;
        return tolerance;
    }

    // Two nodes side by side in x, each holding one point at its center. A ray
    // down either node's center can only reach that node's point.
    constexpr float kNodeSize = 10.0f;
    constexpr float kPickRadius = 1.0f;

    struct TwoNodes {
        cwPointOctreePickSet set;
        QVector3D pointA{5.0f, 5.0f, 5.0f};
        QVector3D pointB{25.0f, 5.0f, 5.0f};

        TwoNodes()
        {
            const QBox3D boundsA = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);
            const QBox3D boundsB = cubeAt(QVector3D(20.0f, 0.0f, 0.0f), kNodeSize);
            set.publish({makeNode(boundsA, {pointA}), makeNode(boundsB, {pointB})},
                        QBox3D(boundsA.minimum(), boundsB.maximum()), kPickRadius);
        }
    };

    // --- A brute-force pick, written here so the indexed one has something
    // independent to agree with. It walks every point of every node, in the
    // order they were published, with the same acceptance rules.

    using cw::octree::kAxisCount;

    //! The world points of one node, dequantized straight out of its payload
    QVector<QVector3D> nodeWorldPoints(const cwPointOctreePickSet::Node& node)
    {
        const QVector3D origin = node.bounds.minimum();
        const double scale = (node.bounds.maximum().x() - origin.x())
                             / double(cw::octree::kQuantMax);

        QVector<QVector3D> points;
        const qsizetype count = node.bytes.size() / cw::octree::kBytesPerPoint;
        points.reserve(count);

        for (qsizetype i = 0; i < count; i++) {
            constexpr int kAxisBytes = int(sizeof(quint16));
            const char* axes = node.bytes.constData() + i * cw::octree::kBytesPerPoint;
            quint16 quantized[kAxisCount] = {0, 0, 0};
            for (int axis = 0; axis < kAxisCount; axis++) {
                quantized[axis] = qFromLittleEndian<quint16>(axes + axis * kAxisBytes);
            }

            points.append(QVector3D(origin.x() + float(double(quantized[0]) * scale),
                                    origin.y() + float(double(quantized[1]) * scale),
                                    origin.z() + float(double(quantized[2]) * scale)));
        }

        return points;
    }

    struct RaySphere {
        bool hit = false;
        double tNear = 0.0;
        double dSq = 0.0;
    };

    //! Ray against a sphere of @a radius around @a center, in double
    RaySphere raySphere(const QRay3D& ray, const QVector3D& center, double radius)
    {
        const QVector3D origin = ray.origin();
        const QVector3D direction = ray.direction();
        const double dDotD = double(direction.x()) * double(direction.x())
                             + double(direction.y()) * double(direction.y())
                             + double(direction.z()) * double(direction.z());
        if (dDotD <= 0.0) {
            return RaySphere();
        }

        const double toCenterX = double(center.x()) - double(origin.x());
        const double toCenterY = double(center.y()) - double(origin.y());
        const double toCenterZ = double(center.z()) - double(origin.z());
        const double tCenter = (toCenterX * double(direction.x())
                                + toCenterY * double(direction.y())
                                + toCenterZ * double(direction.z())) / dDotD;

        const double perpX = toCenterX - tCenter * double(direction.x());
        const double perpY = toCenterY - tCenter * double(direction.y());
        const double perpZ = toCenterZ - tCenter * double(direction.z());
        const double dSq = perpX * perpX + perpY * perpY + perpZ * perpZ;
        const double radiusSquared = radius * radius;

        if (dSq > radiusSquared) {
            return RaySphere {false, 0.0, dSq};
        }
        return RaySphere {true, tCenter - std::sqrt((radiusSquared - dSq) / dDotD), dSq};
    }

    //! What exactHit has to return: the accepted point of least sphere entry
    std::optional<QVector3D> bruteExactHit(const QVector<cwPointOctreePickSet::Node>& nodes,
                                           const QRay3D& ray, double radius)
    {
        std::optional<QVector3D> best;
        double bestDepth = 0.0;

        for (const cwPointOctreePickSet::Node& node : nodes) {
            for (const QVector3D& point : nodeWorldPoints(node)) {
                const RaySphere sphere = raySphere(ray, point, radius);
                if (!sphere.hit || sphere.tNear <= 0.0) {
                    continue;
                }
                if (best.has_value() && sphere.tNear >= bestDepth) {
                    continue;
                }

                best = point;
                bestDepth = sphere.tNear;
            }
        }

        return best;
    }

    //! What nearestPoint has to return: the accepted point of least ray depth
    std::optional<QVector3D> bruteNearestPoint(const QVector<cwPointOctreePickSet::Node>& nodes,
                                               const QRay3D& ray,
                                               const cwPickTolerance& tolerance)
    {
        std::optional<QVector3D> best;
        double bestDepth = 0.0;

        for (const cwPointOctreePickSet::Node& node : nodes) {
            for (const QVector3D& point : nodeWorldPoints(node)) {
                const double rayDepth = ray.projectedDistance(point);
                if (rayDepth <= 0.0) {
                    continue;
                }
                if (best.has_value() && rayDepth >= bestDepth) {
                    continue;
                }

                const double radius = tolerance.radiusAt(rayDepth);
                if (raySphere(ray, point, 0.0).dSq > radius * radius) {
                    continue;
                }

                best = point;
                bestDepth = rayDepth;
            }
        }

        return best;
    }

    void checkPointsEqual(const QVector3D& found, const QVector3D& expected)
    {
        CHECK(found.x() == Approx(expected.x()).margin(kPointTolerance));
        CHECK(found.y() == Approx(expected.y()).margin(kPointTolerance));
        CHECK(found.z() == Approx(expected.z()).margin(kPointTolerance));
    }

} // namespace

TEST_CASE("cwPointOctreePickSet picks the point of the node the ray crosses",
          "[PointOctreePick]")
{
    TwoNodes nodes;

    // Straight down node A's center. Node B's point is 20 m away in x, well
    // outside any pick sphere, so the returned point is the proof that only
    // A's node was reached.
    const auto hit = nodes.set.exactHit(rayDown(nodes.pointA.x(), nodes.pointA.y()));
    REQUIRE(hit.has_value());
    checkPointsEqual(hit->world, nodes.pointA);

    const auto other = nodes.set.exactHit(rayDown(nodes.pointB.x(), nodes.pointB.y()));
    REQUIRE(other.has_value());
    checkPointsEqual(other->world, nodes.pointB);

    // The gap between the two nodes holds nothing.
    CHECK_FALSE(nodes.set.exactHit(rayDown(15.0f, 5.0f)).has_value());
}

TEST_CASE("cwPointOctreePickSet reaches a point whose sphere leaves its node",
          "[PointOctreePick]")
{
    // A point sitting on a node's face: its pick sphere pokes out past the
    // node's bounds, so a ray that misses the bounds by less than the radius
    // still has to find it. Inflating each node's box by the radius is what
    // makes that true.
    const QBox3D bounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);
    const QVector3D onFace(kNodeSize, 5.0f, 5.0f);

    cwPointOctreePickSet set;
    set.publish({makeNode(bounds, {onFace})}, bounds, kPickRadius);

    constexpr float kJustOutside = kNodeSize + kPickRadius * 0.5f;
    const auto hit = set.exactHit(rayDown(kJustOutside, onFace.y()));
    REQUIRE(hit.has_value());
    checkPointsEqual(hit->world, onFace);

    // Past the radius there is nothing to reach.
    CHECK_FALSE(set.exactHit(rayDown(kNodeSize + kPickRadius * 2.0f, onFace.y())).has_value());
}

TEST_CASE("cwPointOctreePickSet answers nothing before anything is resident",
          "[PointOctreePick]")
{
    cwPointOctreePickSet set;

    // Never published: no snapshot at all.
    CHECK(set.bounds().isNull());
    CHECK_FALSE(set.exactHit(rayDown(0.0f, 0.0f)).has_value());
    CHECK_FALSE(set.nearestPoint(rayDown(0.0f, 0.0f), toleranceOf(5.0)).has_value());

    // Published empty — a cloud whose manifest has arrived but whose root has
    // not landed yet. The root bounds still frame it.
    const QBox3D root = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);
    set.publish({}, root, kPickRadius);
    CHECK(set.bounds() == root);
    CHECK_FALSE(set.exactHit(rayDown(5.0f, 5.0f)).has_value());
    CHECK_FALSE(set.nearestPoint(rayDown(5.0f, 5.0f), toleranceOf(5.0)).has_value());
}

TEST_CASE("cwPointOctreePickSet nearestPoint reaches past the pick radius",
          "[PointOctreePick]")
{
    TwoNodes nodes;

    // The anchor rule ignores the pick radius and uses the caller's tolerance,
    // so a ray 3 m from the point — three pick radii away — still anchors on it.
    const QRay3D ray = rayDown(nodes.pointA.x() + 3.0f, nodes.pointA.y());
    CHECK_FALSE(nodes.set.exactHit(ray).has_value());

    const auto anchored = nodes.set.nearestPoint(ray, toleranceOf(5.0));
    REQUIRE(anchored.has_value());
    checkPointsEqual(anchored->world, nodes.pointA);

    // A tolerance shorter than the miss reaches nothing, and a disabled one
    // leaves the anchor off entirely.
    CHECK_FALSE(nodes.set.nearestPoint(ray, toleranceOf(1.0)).has_value());
    CHECK_FALSE(nodes.set.nearestPoint(ray, cwPickTolerance()).has_value());
}

TEST_CASE("cwPointOctreePickSet returns the front-most point along the ray",
          "[PointOctreePick]")
{
    const QVector3D nearPoint(5.0f, 5.0f, 8.0f);
    const QVector3D farPoint(5.0f, 5.0f, 2.0f);
    const QRay3D ray = rayDown(nearPoint.x(), nearPoint.y());

    SECTION("two points in one node") {
        const QBox3D bounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);

        cwPointOctreePickSet set;
        set.publish({makeNode(bounds, {farPoint, nearPoint})}, bounds, kPickRadius);

        const auto hit = set.exactHit(ray);
        REQUIRE(hit.has_value());
        checkPointsEqual(hit->world, nearPoint);
    }

    SECTION("two points in two nodes") {
        // The far node is listed first, so the ranking — not the publish order
        // — is what picks the near one.
        const QBox3D farBounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);
        const QBox3D nearBounds = cubeAt(QVector3D(0.0f, 0.0f, 20.0f), kNodeSize);
        const QVector3D inNearNode(5.0f, 5.0f, 25.0f);

        cwPointOctreePickSet set;
        set.publish({makeNode(farBounds, {farPoint}), makeNode(nearBounds, {inNearNode})},
                    QBox3D(farBounds.minimum(), nearBounds.maximum()), kPickRadius);

        const auto hit = set.exactHit(ray);
        REQUIRE(hit.has_value());
        checkPointsEqual(hit->world, inNearNode);
    }

    SECTION("the anchor takes the near one too") {
        const QBox3D bounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);

        // Both are the same distance off the ray, so depth is all that
        // separates them.
        cwPointOctreePickSet set;
        set.publish({makeNode(bounds, {farPoint, nearPoint})}, bounds, kPickRadius);

        const auto anchored = set.nearestPoint(rayDown(nearPoint.x() + 3.0f, nearPoint.y()),
                                               toleranceOf(5.0));
        REQUIRE(anchored.has_value());
        checkPointsEqual(anchored->world, nearPoint);
    }
}

TEST_CASE("cwPointOctreePickSet leaves points behind the ray origin alone",
          "[PointOctreePick]")
{
    const QBox3D bounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);
    const QVector3D point(5.0f, 5.0f, 5.0f);

    cwPointOctreePickSet set;
    set.publish({makeNode(bounds, {point})}, bounds, kPickRadius);

    // Aimed away: the point is behind the origin, which is a miss and never a
    // hit at a negative depth.
    const QRay3D away(QVector3D(point.x(), point.y(), 0.0f), QVector3D(0.0f, 0.0f, -1.0f));
    CHECK_FALSE(set.exactHit(away).has_value());
    CHECK_FALSE(set.nearestPoint(away, toleranceOf(5.0)).has_value());

    // The same ray turned around finds it.
    const QRay3D toward(away.origin(), QVector3D(0.0f, 0.0f, 1.0f));
    CHECK(set.exactHit(toward).has_value());
    CHECK(set.nearestPoint(toward, toleranceOf(5.0)).has_value());
}

TEST_CASE("cwGeometryItersecter picks through a registered provider",
          "[PointOctreePick][cwGeometryItersecter]")
{
    cwScene scene;
    cwRenderObject owner;
    owner.setScene(&scene);

    auto set = std::make_shared<cwPointOctreePickSet>();
    const QBox3D bounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);
    const QVector3D point(5.0f, 5.0f, 5.0f);
    set->publish({makeNode(bounds, {point})}, bounds, kPickRadius);

    constexpr uint64_t kSubId = 0;
    auto* intersecter = scene.geometryItersecter();
    const QFuture<void> ready = intersecter->addProvider(&owner, kSubId, set);

    // No BVH to wait for: a provider is pickable the instant it is registered.
    CHECK(ready.isFinished());
    const cwGeometryItersecter::Key key{owner.renderObjectId(), kSubId};
    CHECK(intersecter->isObjectPickReady(key));
    CHECK(intersecter->boundingBox(key) == bounds);
    CHECK(intersecter->visibleBoundingBox() == bounds);
    CHECK_FALSE(intersecter->isPickableEmpty());

    const QRay3D ray = rayDown(point.x(), point.y());

    SECTION("the exact pick attributes the hit to the provider's owner") {
        const cwRayHit hit = intersecter->intersectsDetailed(ray);
        REQUIRE(hit.hit());
        checkPointsEqual(hit.pointWorld(), point);
        CHECK(hit.object() == &owner);
        CHECK(hit.objectId() == kSubId);
        CHECK(hit.tWorld() > 0.0);
    }

    SECTION("a query that excludes points never consults it") {
        cwPickQuery linesOnly;
        linesOnly.kinds = cwPickQuery::Kinds(cwPickQuery::Kind::Lines);
        CHECK_FALSE(intersecter->intersectsDetailed(ray, linesOnly).hit());
    }

    SECTION("the visibility store hides it") {
        scene.visibility()->setSubVisible(owner.renderObjectId(), kSubId, false);
        CHECK_FALSE(intersecter->intersectsDetailed(ray).hit());
        CHECK(intersecter->visibleBoundingBox().isNull());

        scene.visibility()->setSubVisible(owner.renderObjectId(), kSubId, true);
        CHECK(intersecter->intersectsDetailed(ray).hit());
    }

    SECTION("removeObject drops it") {
        intersecter->removeObject(key);
        CHECK_FALSE(intersecter->isObjectPickReady(key));
        CHECK_FALSE(intersecter->intersectsDetailed(ray).hit());
        CHECK(intersecter->boundingBox().isNull());
    }

    SECTION("geometry registered under the same Key replaces it") {
        // The Key holds one or the other, so the provider's point stops
        // answering the moment vertices take its place.
        const QVector3D vertex(5.0f, 5.0f, 30.0f);
        intersecter->addObject(cwGeometryItersecter::Object(&owner, kSubId,
                                                            cwTestGeometry::points({vertex}),
                                                            QMatrix4x4(), kPickRadius));
        intersecter->waitForFinish();

        const cwRayHit hit = intersecter->intersectsDetailed(ray);
        REQUIRE(hit.hit());
        checkPointsEqual(hit.pointWorld(), vertex);
        CHECK(intersecter->boundingBox(key) != bounds);
    }

    SECTION("a provider registered over geometry replaces it") {
        constexpr uint64_t kOtherSubId = 1;
        const cwGeometryItersecter::Key otherKey{owner.renderObjectId(), kOtherSubId};
        const QVector3D vertex(40.0f, 5.0f, 5.0f);

        intersecter->addObject(cwGeometryItersecter::Object(&owner, kOtherSubId,
                                                            cwTestGeometry::points({vertex}),
                                                            QMatrix4x4(), kPickRadius));
        intersecter->waitForFinish();
        const QRay3D atVertex = rayDown(vertex.x(), vertex.y());
        REQUIRE(intersecter->intersectsDetailed(atVertex).hit());

        auto second = std::make_shared<cwPointOctreePickSet>();
        const QBox3D secondBounds = cubeAt(QVector3D(60.0f, 0.0f, 0.0f), kNodeSize);
        const QVector3D secondPoint(65.0f, 5.0f, 5.0f);
        second->publish({makeNode(secondBounds, {secondPoint})}, secondBounds, kPickRadius);
        intersecter->addProvider(&owner, kOtherSubId, second);
        intersecter->waitForFinish();

        CHECK_FALSE(intersecter->intersectsDetailed(atVertex).hit());
        CHECK(intersecter->boundingBox(otherKey) == secondBounds);

        const cwRayHit hit = intersecter->intersectsDetailed(rayDown(secondPoint.x(),
                                                                    secondPoint.y()));
        REQUIRE(hit.hit());
        checkPointsEqual(hit.pointWorld(), secondPoint);
    }

    SECTION("boundingBox unites the provider with the BVH's geometry") {
        constexpr uint64_t kLineSubId = 2;
        const QVector3D end(100.0f, 100.0f, 100.0f);
        intersecter->addObject(cwGeometryItersecter::Object(&owner, kLineSubId,
                                                            cwTestGeometry::lines({QVector3D(),
                                                                                   end})));
        intersecter->waitForFinish();

        QBox3D united = bounds;
        united.unite(QBox3D(QVector3D(), end));
        CHECK(intersecter->boundingBox() == united);
        CHECK(intersecter->visibleBoundingBox() == united);

        CHECK(intersecter->debugStatistics().providerCount == 1);
    }

    SECTION("nearestGeometryPoint skips it for a query without points") {
        cwPickQuery linesOnly;
        linesOnly.kinds = cwPickQuery::Kinds(cwPickQuery::Kind::Lines);
        linesOnly.tolerance = toleranceOf(5.0);

        CHECK_FALSE(intersecter->nearestGeometryPoint(rayDown(point.x() + 3.0f, point.y()),
                                                      linesOnly).has_value());
    }

    SECTION("nearestGeometryPoint anchors on it") {
        cwPickQuery query;
        query.tolerance = toleranceOf(5.0);

        // A near miss the exact pick cannot reach, but the anchor can.
        const QRay3D nearMiss = rayDown(point.x() + 3.0f, point.y());
        REQUIRE_FALSE(intersecter->intersectsDetailed(nearMiss, query).hit());

        const auto anchored = intersecter->nearestGeometryPoint(nearMiss, query);
        REQUIRE(anchored.has_value());
        checkPointsEqual(anchored.value(), point);
    }
}

TEST_CASE("The pick profile category reports one line per query", "[PointOctreePick]")
{
    TwoNodes nodes;

    const ProfileLogCapture capture(QStringLiteral("cw.profile.pick.debug=true"));

    const auto hit = nodes.set.exactHit(rayDown(nodes.pointA.x(), nodes.pointA.y()));
    REQUIRE(hit.has_value());

    QStringList lines = ProfileLogCapture::linesStartingWith(QStringLiteral("pick"));
    REQUIRE(lines.size() == 1);

    QString line = lines.first();

    // The runner reads the line by key, in this order.
    const QStringList pairs = line.split(QLatin1Char(' '));
    const QStringList expectedKeys = {
        QStringLiteral("kind"), QStringLiteral("nodes"), QStringLiteral("passing"),
        QStringLiteral("groups"), QStringLiteral("leaves"), QStringLiteral("points"),
        QStringLiteral("us"), QStringLiteral("hit"), QStringLiteral("prunable")
    };
    REQUIRE(pairs.size() == expectedKeys.size() + 1);
    CHECK(pairs.at(0) == QStringLiteral("pick"));
    for (int i = 0; i < expectedKeys.size(); i++) {
        CHECK(pairs.at(i + 1).startsWith(expectedKeys.at(i) + QLatin1Char('=')));
    }

    CHECK(line.contains(QStringLiteral("kind=exactHit")));
    CHECK(line.contains(QStringLiteral("hit=1")));

    // Node A holds one point, and only node A's box is on the ray, so the query
    // descended that node's single group and leaf and scanned exactly that point.
    CHECK(line.contains(QStringLiteral("nodes=2")));
    CHECK(line.contains(QStringLiteral("passing=1")));
    CHECK(line.contains(QStringLiteral("groups=1")));
    CHECK(line.contains(QStringLiteral("leaves=1")));
    CHECK(line.contains(QStringLiteral("points=1")));

    // A ray through the gap between the two nodes reaches neither.
    CHECK_FALSE(nodes.set.exactHit(rayDown(15.0f, 5.0f)).has_value());

    lines = ProfileLogCapture::linesStartingWith(QStringLiteral("pick"));
    REQUIRE(lines.size() == 2);
    line = lines.at(1);
    CHECK(line.contains(QStringLiteral("hit=0")));
    CHECK(line.contains(QStringLiteral("passing=0")));
    CHECK(line.contains(QStringLiteral("groups=0")));
    CHECK(line.contains(QStringLiteral("leaves=0")));
    CHECK(line.contains(QStringLiteral("points=0")));
    CHECK(line.contains(QStringLiteral("prunable=0")));

    const auto nearest = nodes.set.nearestPoint(rayDown(nodes.pointB.x(), nodes.pointB.y()),
                                                toleranceOf(kPickRadius));
    REQUIRE(nearest.has_value());

    lines = ProfileLogCapture::linesStartingWith(QStringLiteral("pick"));
    REQUIRE(lines.size() == 3);
    CHECK(lines.at(2).contains(QStringLiteral("kind=nearestPoint")));
    CHECK(lines.at(2).contains(QStringLiteral("hit=1")));
}

TEST_CASE("A node behind the hit is never opened", "[PointOctreePick]")
{
    // Two nodes stacked along the ray, each with a point at its center. The
    // near point wins, and the far node starts deeper than that hit, so the
    // near-to-far walk stops before it.
    const QBox3D nearBounds = cubeAt(QVector3D(0.0f, 0.0f, 10.0f), kNodeSize);
    const QBox3D farBounds = cubeAt(QVector3D(0.0f, 0.0f, -10.0f), kNodeSize);
    const QVector3D nearPoint(5.0f, 5.0f, 15.0f);
    const QVector3D farPoint(5.0f, 5.0f, -5.0f);

    cwPointOctreePickSet set;
    set.publish({makeNode(nearBounds, {nearPoint}), makeNode(farBounds, {farPoint})},
                QBox3D(farBounds.minimum(), nearBounds.maximum()), kPickRadius);

    const ProfileLogCapture capture(QStringLiteral("cw.profile.pick.debug=true"));

    const auto hit = set.exactHit(rayDown(nearPoint.x(), nearPoint.y()));
    REQUIRE(hit.has_value());
    checkPointsEqual(hit->world, nearPoint);

    const QStringList lines = ProfileLogCapture::linesStartingWith(QStringLiteral("pick"));
    REQUIRE(lines.size() == 1);
    CHECK(lines.first().contains(QStringLiteral("passing=1")));
    CHECK(lines.first().contains(QStringLiteral("points=1")));
    CHECK(lines.first().contains(QStringLiteral("prunable=0")));
}

TEST_CASE("An indexed node answers the same picks a full scan would",
          "[PointOctreePick]")
{
    // A node of 5 000 points, indexed the way the streamer's loader indexes it.
    // Every ray's answer has to match a brute-force walk of the same bytes.
    constexpr int kPointCount = 5000;
    constexpr int kRayCount = 20;
    constexpr float kCloudSize = 50.0f;
    constexpr double kTolerance = 1.5;

    quint32 random = 777u;
    constexpr quint32 kMultiplier = 1664525u;
    constexpr quint32 kIncrement = 1013904223u;
    constexpr quint32 kFractionBits = 24;
    const auto nextFraction = [&random]() {
        random = random * kMultiplier + kIncrement;
        return float(double(random >> 8) / double(1u << kFractionBits));
    };

    //! A fraction over [-1, 1], so a direction reaches every octant
    const auto nextSigned = [&nextFraction]() {
        return 2.0f * nextFraction() - 1.0f;
    };

    QVector<QVector3D> points;
    points.reserve(kPointCount);
    for (int i = 0; i < kPointCount; i++) {
        points.append(QVector3D(nextFraction() * kCloudSize,
                                nextFraction() * kCloudSize,
                                nextFraction() * kCloudSize));
    }

    const QBox3D bounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kCloudSize);
    const QVector<cwPointOctreePickSet::Node> nodes {makeNode(bounds, points)};
    REQUIRE_FALSE(nodes.constFirst().index.isEmpty());

    cwPointOctreePickSet set;
    set.publish(nodes, bounds, kPickRadius);

    // Rays straight down through the cloud, then oblique ones from every
    // direction: an axis-aligned ray leaves two thirds of the slab test at
    // "is the origin between the faces", so only the oblique half exercises
    // the per-axis inverse direction, the near/far swap on a negative
    // component, and exactHit's radius-to-ray-units depth bias. Every third
    // oblique direction is three times unit length, which the bias divides by.
    QVector<QRay3D> rays;
    rays.reserve(2 * kRayCount);
    for (int i = 0; i < kRayCount; i++) {
        rays.append(rayDown(nextFraction() * kCloudSize, nextFraction() * kCloudSize));
    }

    constexpr float kRayStandoff = 120.0f;
    constexpr float kLongDirectionScale = 3.0f;
    for (int i = 0; i < kRayCount; i++) {
        const QVector3D target(nextFraction() * kCloudSize,
                               nextFraction() * kCloudSize,
                               nextFraction() * kCloudSize);
        const QVector3D away = QVector3D(nextSigned(), nextSigned(), nextSigned()).normalized();
        const QVector3D origin = target + away * kRayStandoff;
        const QVector3D direction = (i % 3 == 0)
                                        ? (target - origin) * kLongDirectionScale
                                        : (target - origin).normalized();
        rays.append(QRay3D(origin, direction));
    }

    // The loop is only worth its name if the directions really do span the
    // octants, so the signs are counted rather than assumed.
    std::array<int, kAxisCount> negativeDirections {0, 0, 0};
    std::array<int, kAxisCount> positiveDirections {0, 0, 0};
    for (const QRay3D& ray : std::as_const(rays)) {
        const QVector3D direction = ray.direction();
        const float components[kAxisCount] = {direction.x(), direction.y(), direction.z()};
        for (int axis = 0; axis < kAxisCount; axis++) {
            if (components[axis] < 0.0f) {
                negativeDirections[axis]++;
            } else if (components[axis] > 0.0f) {
                positiveDirections[axis]++;
            }
        }
    }
    for (int axis = 0; axis < kAxisCount; axis++) {
        CHECK(negativeDirections.at(axis) > 0);
        CHECK(positiveDirections.at(axis) > 0);
    }

    for (const QRay3D& ray : std::as_const(rays)) {
        const std::optional<QVector3D> expectedExact =
            bruteExactHit(nodes, ray, double(kPickRadius));
        const auto exact = set.exactHit(ray);
        REQUIRE(exact.has_value() == expectedExact.has_value());
        if (expectedExact.has_value()) {
            checkPointsEqual(exact->world, expectedExact.value());
        }

        const cwPickTolerance tolerance = toleranceOf(kTolerance);
        const std::optional<QVector3D> expectedNearest =
            bruteNearestPoint(nodes, ray, tolerance);
        const auto nearest = set.nearestPoint(ray, tolerance);
        REQUIRE(nearest.has_value() == expectedNearest.has_value());
        if (expectedNearest.has_value()) {
            checkPointsEqual(nearest->world, expectedNearest.value());
        }
    }
}

TEST_CASE("A node published with no index is scanned whole", "[PointOctreePick]")
{
    // The pick set is total: a Node whose index was never built still answers,
    // by walking every point it holds. Nothing publishes one today, so this is
    // what keeps that path honest.
    const QBox3D bounds = cubeAt(QVector3D(0.0f, 0.0f, 0.0f), kNodeSize);
    const QVector<QVector3D> points {
        QVector3D(5.0f, 5.0f, 1.0f), QVector3D(5.0f, 5.0f, 5.0f), QVector3D(1.0f, 1.0f, 5.0f)
    };

    const cwPointOctreePickSet::Node indexed = makeNode(bounds, points);
    REQUIRE_FALSE(indexed.index.isEmpty());

    cwPointOctreePickSet unindexedSet;
    unindexedSet.publish({cwPointOctreePickSet::Node {bounds, indexed.bytes, {}}},
                         bounds, kPickRadius);

    cwPointOctreePickSet indexedSet;
    indexedSet.publish({indexed}, bounds, kPickRadius);

    const QRay3D ray = rayDown(5.0f, 5.0f);
    const cwPickTolerance tolerance = toleranceOf(kPickRadius);

    const ProfileLogCapture capture(QStringLiteral("cw.profile.pick.debug=true"));

    const auto unindexedExact = unindexedSet.exactHit(ray);
    const auto indexedExact = indexedSet.exactHit(ray);
    REQUIRE(unindexedExact.has_value());
    REQUIRE(indexedExact.has_value());
    checkPointsEqual(unindexedExact->world, indexedExact->world);

    const auto unindexedNearest = unindexedSet.nearestPoint(ray, tolerance);
    const auto indexedNearest = indexedSet.nearestPoint(ray, tolerance);
    REQUIRE(unindexedNearest.has_value());
    REQUIRE(indexedNearest.has_value());
    checkPointsEqual(unindexedNearest->world, indexedNearest->world);

    const QStringList lines = ProfileLogCapture::linesStartingWith(QStringLiteral("pick"));
    REQUIRE(lines.size() == 4);

    // No boxes to descend, so every point of the node is read.
    CHECK(lines.at(0).contains(QStringLiteral("groups=0")));
    CHECK(lines.at(0).contains(QStringLiteral("leaves=0")));
    CHECK(lines.at(0).contains(QStringLiteral("points=3")));

    // The indexed node reaches the same answer through its one group and leaf.
    CHECK(lines.at(1).contains(QStringLiteral("groups=1")));
    CHECK(lines.at(1).contains(QStringLiteral("leaves=1")));
}
