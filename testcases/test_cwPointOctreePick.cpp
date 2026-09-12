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
#include <QVector3D>

//Std includes
#include <memory>

//Our includes
#include "TestGeometryBuilders.h"
#include "cwGeometryItersecter.h"
#include "cwPickQuery.h"
#include "cwPointOctree.h"
#include "cwPointOctreePickSet.h"
#include "cwRayHit.h"
#include "cwRenderObject.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"

using namespace Catch;

namespace {

    constexpr double kPointTolerance = 1e-3;

    //! A pick set node holding @a points, quantized into @a bounds exactly the
    //! way cw::octree::quantizeAll writes a node's payload to disk.
    cwPointOctreePickSet::Node makeNode(const QBox3D& bounds,
                                        const QVector<QVector3D>& points)
    {
        return {bounds, cw::octree::quantizeAll(points, bounds)};
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
