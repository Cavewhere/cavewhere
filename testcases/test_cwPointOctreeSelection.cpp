// test_cwPointOctreeSelection.cpp
// Catch2 unit tests for the point octree's node selection and eviction planner.

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QMatrix4x4>
#include <QSet>
#include <QtMath>
#include <QVector>
#include <QVector3D>

//Std includes
#include <cmath>

//Our includes
#include "cwFrustum.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwPointOctreeSelection.h"
#include "cwScreenSpace.h"

using namespace cw::octree;

namespace {

    //A 128 m root cube makes spacing(0) exactly 1 m at kSampleGridResolution
    constexpr double kRootSize = 128.0;
    constexpr float kRootHalf = 64.0f;

    //Puts the root cube in front of an eye looking down -Z
    constexpr float kEyeDistance = 300.0f;

    constexpr int kViewportHeightPx = 800;

    //2 / (top - bottom) of the orthographic box below
    constexpr double kOrthoAbsP11 = 0.01;
    constexpr float kOrthoExtent = 100.0f;
    constexpr float kOrthoNear = 1.0f;
    constexpr float kOrthoFar = 1000.0f;

    //Splits the orthographic box just off the middle of the root cube
    constexpr float kHalfSpaceOffset = 1.0f;

    //cot(fov / 2) = 1.5, so |P(1, 1)| is 1.5 and one meter at w = 172 covers
    //3.49 px: the root and the near children refine, the far children do not
    constexpr double kPerspectiveCotHalfFov = 1.5;
    const double kPerspectiveFovDegrees =
        qRadiansToDegrees(2.0 * std::atan(1.0 / kPerspectiveCotHalfFov));
    constexpr float kPerspectiveAspect = 1.0f;

    constexpr double kDefaultSse = 1.5;

    /**
     * A hand-built three-level tree: the root, its eight children, and eight
     * grandchildren under each octant in @a parentsWithGrandchildren.
     */
    cwPointOctreeManifest buildManifest(const QVector<int>& parentsWithGrandchildren = {0})
    {
        cwPointOctreeManifest manifest;
        manifest.rootMin = QVector3D(0.0f, 0.0f, 0.0f);
        manifest.rootSize = kRootSize;

        cwPointOctreeNode root;
        root.level = 0;
        manifest.nodes.append(root);

        for(int octant = 0; octant < kChildCount; octant++) {
            cwPointOctreeNode child;
            child.level = 1;
            child.x = quint32(octant & 1);
            child.y = quint32((octant >> 1) & 1);
            child.z = quint32((octant >> 2) & 1);

            const int index = manifest.nodes.size();
            manifest.nodes.append(child);
            manifest.nodes[0].children[octant] = index;
        }

        for(int parentOctant : parentsWithGrandchildren) {
            const int parentIndex = manifest.nodes.at(0).children.at(parentOctant);
            const cwPointOctreeNode parent = manifest.nodes.at(parentIndex);

            for(int octant = 0; octant < kChildCount; octant++) {
                cwPointOctreeNode grandchild;
                grandchild.level = 2;
                grandchild.x = parent.x * 2 + quint32(octant & 1);
                grandchild.y = parent.y * 2 + quint32((octant >> 1) & 1);
                grandchild.z = parent.z * 2 + quint32((octant >> 2) & 1);

                const int index = manifest.nodes.size();
                manifest.nodes.append(grandchild);
                manifest.nodes[parentIndex].children[octant] = index;
            }
        }

        return manifest;
    }

    QMatrix4x4 view()
    {
        QMatrix4x4 matrix;
        matrix.translate(-kRootHalf, -kRootHalf, -kEyeDistance);
        return matrix;
    }

    QMatrix4x4 orthoProjection(float left = -kOrthoExtent, float right = kOrthoExtent)
    {
        QMatrix4x4 matrix;
        matrix.ortho(left, right, -kOrthoExtent, kOrthoExtent, kOrthoNear, kOrthoFar);
        return matrix;
    }

    SelectionInput orthoInput(const cwPointOctreeManifest& manifest)
    {
        SelectionInput input;
        input.manifest = &manifest;
        input.viewProjection = orthoProjection() * view();
        input.absP11 = kOrthoAbsP11;
        input.viewportHeightPx = kViewportHeightPx;
        return input;
    }

    QVector<int> nodeIndices(const QVector<SelectedNode>& selected)
    {
        QVector<int> indices;
        indices.reserve(selected.size());
        for(const SelectedNode& node : selected) {
            indices.append(node.node);
        }
        return indices;
    }

    QSet<int> nodeSet(const QVector<SelectedNode>& selected)
    {
        const QVector<int> indices = nodeIndices(selected);
        return QSet<int>(indices.begin(), indices.end());
    }

    NodeResidency residentNode(bool selectedThisFrame, quint64 lastDesiredFrame, qint64 bytes)
    {
        NodeResidency node;
        node.resident = true;
        node.selectedThisFrame = selectedThisFrame;
        node.lastDesiredFrame = lastDesiredFrame;
        node.bytes = bytes;
        return node;
    }

    constexpr qint64 kNodeBytes = 100;

    //Points a node of each level holds in buildManifestWithPoints()
    constexpr quint32 kPointsPerNode = 1000;

    //! buildManifest() with a point count and byte size on every node
    cwPointOctreeManifest buildManifestWithPoints()
    {
        cwPointOctreeManifest manifest = buildManifest();
        for(cwPointOctreeNode& node : manifest.nodes) {
            node.pointCount = kPointsPerNode;
            node.byteSize = qint64(kPointsPerNode) * cw::octree::kBytesPerPoint;
        }
        return manifest;
    }

    //! Every node between @a index and the root, root last
    QVector<int> ancestorsOf(const cwPointOctreeManifest& manifest, int index)
    {
        QVector<int> ancestors;
        for(int parent = 0; parent < manifest.nodes.size(); parent++) {
            for(int child : manifest.nodes.at(parent).children) {
                if(child == index) {
                    ancestors.append(parent);
                    ancestors.append(ancestorsOf(manifest, parent));
                    return ancestors;
                }
            }
        }
        return ancestors;
    }
}

TEST_CASE("cw::octree::selectNodes: an orthographic camera cuts the tree at the screen space error",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest manifest = buildManifest();
    const SelectionInput input = orthoInput(manifest);

    //The chosen camera makes the root spacing exactly 4 px
    REQUIRE(cw::sse::projectedPixels(manifest.spacing(0),
                                     manifest.nodeBounds(0),
                                     input.viewProjection,
                                     input.absP11,
                                     input.viewportHeightPx) == Catch::Approx(4.0));

    const QVector<SelectedNode> selected = selectNodes(input);

    SECTION("the root, every child, and the grandchildren of the refined child are selected") {
        REQUIRE(selected.size() == manifest.nodes.size());
        REQUIRE(nodeSet(selected).size() == manifest.nodes.size());
    }

    SECTION("the root comes first and the walk runs coarse to fine") {
        REQUIRE(selected.first().node == 0);
        REQUIRE(selected.first().projectedSpacingPx == Catch::Approx(4.0));

        for(int i = 1; i < selected.size(); i++) {
            REQUIRE(selected.at(i).projectedSpacingPx <= selected.at(i - 1).projectedSpacingPx);
        }
    }

    SECTION("a coarser threshold stops one level earlier") {
        SelectionInput coarse = input;
        coarse.screenSpaceErrorPx = 2.0 * kDefaultSse;

        const QVector<SelectedNode> coarseSelected = selectNodes(coarse);
        REQUIRE(coarseSelected.size() == 1 + kChildCount);

        for(const SelectedNode& node : coarseSelected) {
            REQUIRE(manifest.nodes.at(node.node).level <= 1);
        }
    }

    SECTION("inflating the threshold stops one level earlier too") {
        SelectionInput inflated = input;
        inflated.sseInflation = 2.0;

        const QVector<SelectedNode> inflatedSelected = selectNodes(inflated);
        REQUIRE(inflatedSelected.size() == 1 + kChildCount);
    }

    SECTION("a node sitting exactly on the threshold is selected but stays whole") {
        SelectionInput boundary = input;

        //The children project to exactly 2 px, so only the 4 px root refines
        boundary.screenSpaceErrorPx = 2.0;

        const QVector<SelectedNode> boundarySelected = selectNodes(boundary);
        REQUIRE(boundarySelected.size() == 1 + kChildCount);

        for(const SelectedNode& node : boundarySelected) {
            REQUIRE(manifest.nodes.at(node.node).level <= 1);
        }
        for(int i = 1; i < boundarySelected.size(); i++) {
            REQUIRE(boundarySelected.at(i).projectedSpacingPx == Catch::Approx(2.0));
        }
    }

    SECTION("maxNodes keeps the root and the two coarsest-projecting children") {
        SelectionInput capped = input;
        capped.maxNodes = 3;

        const QVector<int> indices = nodeIndices(selectNodes(capped));
        REQUIRE(indices == QVector<int>({0, 1, 2}));
    }

    SECTION("a view that wants no nodes gets none") {
        SelectionInput empty = input;
        empty.maxNodes = 0;

        REQUIRE(selectNodes(empty).isEmpty());
    }
}

TEST_CASE("cw::octree::selectNodes: a culled node takes its subtree with it",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest manifest = buildManifest();

    //The children of octant 0 are the only grandchildren, so the subtree test
    //culls the low-x half; the high-x half covers the plan's "child 3" case
    const QVector<int> lowXOctants = {0, 2, 4, 6};
    const QVector<int> highXOctants = {1, 3, 5, 7};

    SECTION("culling the low-x half drops octant 0 and all of its grandchildren") {
        const QMatrix4x4 projection = orthoProjection(kHalfSpaceOffset, kOrthoExtent);
        const QMatrix4x4 viewProjection = projection * view();
        const cwFrustum frustum = cwFrustum::fromViewProjection(viewProjection);

        SelectionInput input = orthoInput(manifest);
        input.viewProjection = viewProjection;
        input.frustum = &frustum;

        const QSet<int> selected = nodeSet(selectNodes(input));

        REQUIRE(selected.size() == 1 + highXOctants.size());
        REQUIRE(selected.contains(0));
        for(int octant : highXOctants) {
            REQUIRE(selected.contains(manifest.nodes.at(0).children.at(octant)));
        }

        //Every grandchild lives under octant 0, which never entered the cut
        for(int i = 1 + kChildCount; i < manifest.nodes.size(); i++) {
            REQUIRE_FALSE(selected.contains(i));
        }
    }

    SECTION("culling the high-x half drops octant 3 and its subtree") {
        //Octant 3 gets grandchildren of its own here, so the cull has a subtree to drop
        const cwPointOctreeManifest subtreeManifest = buildManifest({0, 3});

        const QMatrix4x4 projection = orthoProjection(-kOrthoExtent, -kHalfSpaceOffset);
        const QMatrix4x4 viewProjection = projection * view();
        const cwFrustum frustum = cwFrustum::fromViewProjection(viewProjection);

        SelectionInput input = orthoInput(subtreeManifest);
        input.viewProjection = viewProjection;
        input.frustum = &frustum;

        const QSet<int> selected = nodeSet(selectNodes(input));

        REQUIRE(selected.size() == 1 + lowXOctants.size() + kChildCount);
        for(int octant : highXOctants) {
            REQUIRE_FALSE(selected.contains(subtreeManifest.nodes.at(0).children.at(octant)));
        }
        for(int octant : lowXOctants) {
            REQUIRE(selected.contains(subtreeManifest.nodes.at(0).children.at(octant)));
        }

        const int culledChild = subtreeManifest.nodes.at(0).children.at(3);
        for(int grandchild : subtreeManifest.nodes.at(culledChild).children) {
            REQUIRE(grandchild > 0);
            REQUIRE_FALSE(selected.contains(grandchild));
        }
    }

    SECTION("a frustum that excludes the root selects nothing") {
        const QMatrix4x4 projection = orthoProjection(2.0f * kOrthoExtent, 4.0f * kOrthoExtent);
        const QMatrix4x4 viewProjection = projection * view();
        const cwFrustum frustum = cwFrustum::fromViewProjection(viewProjection);

        SelectionInput input = orthoInput(manifest);
        input.viewProjection = viewProjection;
        input.frustum = &frustum;

        REQUIRE(selectNodes(input).isEmpty());
    }
}

TEST_CASE("cw::octree::selectNodes: an unknown camera selects the root alone",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest manifest = buildManifest();

    SECTION("no projection scale") {
        SelectionInput input = orthoInput(manifest);
        input.absP11 = 0.0;

        REQUIRE(nodeIndices(selectNodes(input)) == QVector<int>({0}));
    }

    SECTION("no viewport") {
        SelectionInput input = orthoInput(manifest);
        input.viewportHeightPx = 0;

        REQUIRE(nodeIndices(selectNodes(input)) == QVector<int>({0}));
    }

    SECTION("no manifest") {
        SelectionInput input;
        REQUIRE(selectNodes(input).isEmpty());
    }
}

TEST_CASE("cw::octree::selectNodes: a perspective camera refines the near child deeper",
          "[PointOctree][PointOctreeSelection]")
{
    //Octant 0 sits at low z, octant 4 at high z, so octant 4 is the near one
    const cwPointOctreeManifest manifest = buildManifest({0, 4});
    const int farChild = manifest.nodes.at(0).children.at(0);
    const int nearChild = manifest.nodes.at(0).children.at(4);

    QMatrix4x4 projection;
    projection.perspective(float(kPerspectiveFovDegrees),
                           kPerspectiveAspect,
                           kOrthoNear,
                           kOrthoFar);

    SelectionInput input;
    input.manifest = &manifest;
    input.viewProjection = projection * view();
    input.absP11 = std::abs(double(projection(1, 1)));
    input.viewportHeightPx = kViewportHeightPx;

    const auto spacingOf = [&](int index)
    {
        return cw::sse::projectedPixels(manifest.spacing(manifest.nodes.at(index).level),
                                        manifest.nodeBounds(index),
                                        input.viewProjection,
                                        input.absP11,
                                        input.viewportHeightPx);
    };

    REQUIRE(spacingOf(nearChild) > kDefaultSse);
    REQUIRE(spacingOf(farChild) < kDefaultSse);

    const QSet<int> selected = nodeSet(selectNodes(input));

    REQUIRE(selected.contains(nearChild));
    REQUIRE(selected.contains(farChild));

    for(int child : manifest.nodes.at(nearChild).children) {
        REQUIRE(selected.contains(child));
    }
    for(int child : manifest.nodes.at(farChild).children) {
        REQUIRE_FALSE(selected.contains(child));
    }
}

TEST_CASE("cw::octree::planNodeEvictions: releases the least wanted nodes first",
          "[PointOctree][PointOctreeSelection]")
{
    SECTION("nodes the cut dropped go before the ones it kept") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(true, 3, kNodeBytes));
        nodes.append(residentNode(false, 9, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, kNodeBytes) == QVector<int>({1}));
    }

    SECTION("the oldest node goes first") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(false, 9, kNodeBytes));
        nodes.append(residentNode(false, 4, kNodeBytes));
        nodes.append(residentNode(false, 7, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, 3 * kNodeBytes) == QVector<int>({1, 2, 0}));
    }

    SECTION("a pinned node is skipped while its younger sibling is chosen") {
        QVector<NodeResidency> nodes;
        NodeResidency root = residentNode(false, 0, kNodeBytes);
        root.pinned = true;
        nodes.append(root);
        nodes.append(residentNode(false, 5, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, kNodeBytes) == QVector<int>({1}));

        nodes.removeLast();
        REQUIRE(planNodeEvictions(nodes, kNodeBytes).isEmpty());
    }

    SECTION("a resident node that holds no bytes is never chosen") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(false, 1, 0));

        REQUIRE(planNodeEvictions(nodes, kNodeBytes).isEmpty());
    }

    SECTION("a node that is not resident is never chosen") {
        QVector<NodeResidency> nodes;
        NodeResidency pending;
        pending.bytes = kNodeBytes;
        nodes.append(pending);

        REQUIRE(planNodeEvictions(nodes, kNodeBytes).isEmpty());
    }

    SECTION("the plan stops as soon as the overshoot is covered") {
        QVector<NodeResidency> nodes;
        for(int i = 0; i < 4; i++) {
            nodes.append(residentNode(false, quint64(i), kNodeBytes));
        }

        REQUIRE(planNodeEvictions(nodes, kNodeBytes + 1) == QVector<int>({0, 1}));
    }

    SECTION("the plan comes back partial when the resident set cannot cover the overshoot") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(false, 1, kNodeBytes));
        nodes.append(residentNode(true, 2, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, 10 * kNodeBytes) == QVector<int>({0, 1}));
    }

    SECTION("nothing is planned when the budget has room") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(false, 1, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, 0).isEmpty());
    }
}

TEST_CASE("cw::octree::selectCut: the point budget caps the cut at the coarsest nodes",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest manifest = buildManifestWithPoints();
    const SelectionInput input = orthoInput(manifest);

    const Selection full = selectCut(input);
    REQUIRE(full.nodes.size() == manifest.nodes.size());
    REQUIRE(full.points > 0);
    REQUIRE_FALSE(full.pointCapped);

    SECTION("an unlimited budget selects what selectNodes does and reports no cap") {
        CHECK(nodeIndices(full.nodes) == nodeIndices(selectNodes(input)));
        CHECK(full.pointCapped == false);

        qint64 points = 0;
        qint64 bytes = 0;
        for(const SelectedNode& node : full.nodes) {
            points += manifest.nodes.at(node.node).pointCount;
            bytes += manifest.nodes.at(node.node).byteSize;
        }
        CHECK(full.points == points);
        CHECK(full.bytes == bytes);
    }

    SECTION("a budget under the full cut stops the walk with every ancestor in place") {
        SelectionInput capped = input;
        capped.maxPoints = full.points / 2;

        const Selection selection = selectCut(capped);
        CHECK(selection.points <= capped.maxPoints);
        CHECK(selection.points > 0);
        CHECK(selection.pointCapped);
        CHECK(selection.nodes.size() < full.nodes.size());
        CHECK(selection.nodes.first().node == 0);

        const QSet<int> selected = nodeSet(selection.nodes);
        for(int index : selected) {
            for(int ancestor : ancestorsOf(manifest, index)) {
                CHECK(selected.contains(ancestor));
            }
        }
    }

    SECTION("a budget under the root's own count leaves the root alone") {
        SelectionInput capped = input;
        capped.maxPoints = qint64(manifest.nodes.at(0).pointCount) - 1;

        const Selection selection = selectCut(capped);
        REQUIRE(selection.nodes.size() == 1);
        CHECK(selection.nodes.first().node == 0);
        CHECK(selection.points == manifest.nodes.at(0).pointCount);
        CHECK(selection.pointCapped);
    }
}

TEST_CASE("cw::octree::nextSseInflation: follows what the cut costs against what it may spend",
          "[PointOctree][PointOctreeSelection]")
{
    constexpr qint64 kAvailableBytes = 1000;
    constexpr qint64 kFittingBytes = 500;

    SECTION("up a step when the cut wants more bytes than it may spend") {
        InflationInput input;
        input.desiredBytes = kAvailableBytes + 1;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kSseInflationStep));
    }

    SECTION("up a step on a point-capped cut whose bytes fit") {
        InflationInput input;
        input.desiredBytes = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        input.pointCapped = true;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kSseInflationStep));
    }

    SECTION("held while the cut fits and nothing was probed") {
        InflationInput input;
        input.current = kSseInflationStep;
        input.desiredBytes = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kSseInflationStep));
    }

    SECTION("down a step once the probed cut fits with the relax margin to spare") {
        InflationInput input;
        input.current = kSseInflationStep * kSseInflationStep;
        input.desiredBytes = kFittingBytes;
        input.desiredBytesRelaxed = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kSseInflationStep));
    }

    SECTION("held while the probed cut only just fits") {
        InflationInput input;
        input.current = kSseInflationStep;
        input.desiredBytes = kFittingBytes;
        input.desiredBytesRelaxed = kAvailableBytes;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kSseInflationStep));
    }

    SECTION("held while the probed cut would hit the point budget") {
        InflationInput input;
        input.current = kSseInflationStep;
        input.desiredBytes = kFittingBytes;
        input.desiredBytesRelaxed = kFittingBytes;
        input.pointCappedRelaxed = true;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kSseInflationStep));
    }

    SECTION("a probe that walked down several steps takes them all at once") {
        constexpr int kSteps = 3;
        InflationInput input;
        input.current = std::pow(kSseInflationStep, kSteps + 1);
        input.desiredBytes = kFittingBytes;
        input.desiredBytesRelaxed = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        input.relaxedSteps = kSteps;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kSseInflationStep));
    }

    SECTION("several steps are still floored at one") {
        constexpr int kMoreStepsThanTaken = 6;
        InflationInput input;
        input.current = kSseInflationStep * kSseInflationStep;
        input.desiredBytes = kFittingBytes;
        input.desiredBytesRelaxed = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        input.relaxedSteps = kMoreStepsThanTaken;
        REQUIRE(nextSseInflation(input) == Catch::Approx(1.0));
    }

    SECTION("floored at one when asked to relax below it") {
        InflationInput input;
        input.desiredBytes = kFittingBytes;
        input.desiredBytesRelaxed = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(1.0));
    }

    SECTION("capped at the maximum") {
        InflationInput input;
        input.current = kMaxSseInflation;
        input.desiredBytes = kAvailableBytes + 1;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kMaxSseInflation));
    }

    SECTION("an inflation below one comes back at one") {
        InflationInput input;
        input.current = 0.5;
        input.desiredBytes = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(1.0));
    }

    SECTION("an inflation above the cap comes back at the cap") {
        InflationInput input;
        input.current = 2.0 * kMaxSseInflation;
        input.desiredBytes = kFittingBytes;
        input.availableBytes = kAvailableBytes;
        REQUIRE(nextSseInflation(input) == Catch::Approx(kMaxSseInflation));
    }
}
