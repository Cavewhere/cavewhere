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
#include <algorithm>
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

    //! A perspective camera looking down -Z at the fixture's root cube
    SelectionInput perspectiveInput(const cwPointOctreeManifest& manifest)
    {
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
        return input;
    }

    //! The spacing @a manifest's node @a index projects to on screen
    double projectedSpacingOf(const cwPointOctreeManifest& manifest,
                              int index,
                              const QMatrix4x4& viewProjection,
                              double absP11,
                              int viewportHeightPx)
    {
        return cw::sse::projectedPixels(manifest.spacing(manifest.nodes.at(index).level),
                                        manifest.nodeBounds(index),
                                        viewProjection,
                                        absP11,
                                        viewportHeightPx);
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

    //! A resident entry for node @a index, which is what the plan names
    NodeResidency residentNode(int index, bool selectedThisFrame, quint64 lastDesiredFrame,
                               qint64 bytes)
    {
        NodeResidency node;
        node.node = index;
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

    //Puts a second tree of the same shape one root cube east of the first, so
    //both project the same and the tie-break decides the order
    constexpr float kSideBySideShift = float(kRootSize);

    //Far enough behind the first tree that its root projects under the error
    constexpr float kFarShift = -700.0f;

    //! @a manifest with its root cube moved by @a shift
    cwPointOctreeManifest shiftedManifest(const cwPointOctreeManifest& manifest,
                                          const QVector3D& shift)
    {
        cwPointOctreeManifest shifted = manifest;
        shifted.rootMin += shift;
        return shifted;
    }

    //! The one tree forest selectCut() cuts, for the camera of @a input
    ForestInput forestInput(const SelectionInput& input)
    {
        ForestInput forest;
        forest.trees.append({input.manifest});
        forest.frustum = input.frustum;
        forest.viewProjection = input.viewProjection;
        forest.absP11 = input.absP11;
        forest.viewportHeightPx = input.viewportHeightPx;
        forest.screenSpaceErrorPx = input.screenSpaceErrorPx;
        forest.sseInflation = input.sseInflation;
        forest.maxNodes = input.maxNodes;
        forest.maxPoints = input.maxPoints;
        return forest;
    }

    //! The forest of @a west and @a east under the orthographic camera
    ForestInput sideBySideInput(const cwPointOctreeManifest& west,
                                const cwPointOctreeManifest& east)
    {
        ForestInput forest = forestInput(orthoInput(west));
        forest.trees.append({&east});
        return forest;
    }

    //! Every cut takes its nodes coarsest first, so the spacings never climb
    bool spacingsFallOff(const Selection& selection)
    {
        for(int i = 1; i < selection.nodes.size(); i++) {
            if(selection.nodes.at(i).projectedSpacingPx
               > selection.nodes.at(i - 1).projectedSpacingPx) {
                return false;
            }
        }
        return true;
    }

    //What two trees tying on every spacing may differ by once a shared cap binds
    constexpr int kBalancedNodeSlack = 1;
    constexpr int kBalancedLevelSlack = 1;

    //! The deepest level @a selection reaches in @a manifest
    int finestLevelOf(const cwPointOctreeManifest& manifest, const Selection& selection)
    {
        int finest = 0;
        for(const SelectedNode& node : selection.nodes) {
            finest = std::max(finest, manifest.nodes.at(node.node).level);
        }
        return finest;
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

    const SelectionInput input = perspectiveInput(manifest);

    const auto spacingOf = [&](int index)
    {
        return projectedSpacingOf(manifest, index, input.viewProjection, input.absP11,
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

TEST_CASE("cw::octree::finestDrawnLevels: a node floors at the finest level drawn under it",
          "[PointOctree][PointOctreeSelection]")
{
    //Grandchildren under octant 0 alone, so one branch refines two levels deep
    const cwPointOctreeManifest manifest = buildManifest();
    const int refinedChild = manifest.nodes.at(0).children.at(0);
    const int grandchild = manifest.nodes.at(refinedChild).children.at(0);
    const int coarseChild = manifest.nodes.at(0).children.at(1);

    SECTION("the root alone floors at the root's level") {
        REQUIRE(finestDrawnLevels(manifest, {0}) == QVector<int>({0}));
    }

    SECTION("an ancestor over a refined subtree floors at its finest descendant") {
        const QVector<int> drawn {0, refinedChild, grandchild};
        REQUIRE(finestDrawnLevels(manifest, drawn) == QVector<int>({2, 2, 2}));
    }

    SECTION("a leaf of the drawn cut floors at its own level") {
        const QVector<int> drawn {0, refinedChild, grandchild, coarseChild};
        REQUIRE(finestDrawnLevels(manifest, drawn) == QVector<int>({2, 2, 2, 1}));
    }

    SECTION("a child that is not drawn leaves its parent at its own level") {
        const QVector<int> drawn {0, refinedChild};
        REQUIRE(finestDrawnLevels(manifest, drawn) == QVector<int>({1, 1}));
    }
}

TEST_CASE("cw::octree::planNodeEvictions: releases the least wanted nodes first",
          "[PointOctree][PointOctreeSelection]")
{
    SECTION("nodes the cut dropped go before the ones it kept") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(0, true, 3, kNodeBytes));
        nodes.append(residentNode(1, false, 9, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, kNodeBytes) == QVector<int>({1}));
    }

    SECTION("the oldest node goes first") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(0, false, 9, kNodeBytes));
        nodes.append(residentNode(1, false, 4, kNodeBytes));
        nodes.append(residentNode(2, false, 7, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, 3 * kNodeBytes) == QVector<int>({1, 2, 0}));
    }

    SECTION("a pinned node is skipped while its younger sibling is chosen") {
        QVector<NodeResidency> nodes;
        NodeResidency root = residentNode(0, false, 0, kNodeBytes);
        root.pinned = true;
        nodes.append(root);
        nodes.append(residentNode(1, false, 5, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, kNodeBytes) == QVector<int>({1}));

        nodes.removeLast();
        REQUIRE(planNodeEvictions(nodes, kNodeBytes).isEmpty());
    }

    SECTION("a resident node that holds no bytes is never chosen") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(0, false, 1, 0));

        REQUIRE(planNodeEvictions(nodes, kNodeBytes).isEmpty());
    }

    SECTION("a node that is not resident is never chosen") {
        QVector<NodeResidency> nodes;
        NodeResidency pending;
        pending.node = 0;
        pending.bytes = kNodeBytes;
        nodes.append(pending);

        REQUIRE(planNodeEvictions(nodes, kNodeBytes).isEmpty());
    }

    SECTION("the plan stops as soon as the overshoot is covered") {
        QVector<NodeResidency> nodes;
        for(int i = 0; i < 4; i++) {
            nodes.append(residentNode(i, false, quint64(i), kNodeBytes));
        }

        REQUIRE(planNodeEvictions(nodes, kNodeBytes + 1) == QVector<int>({0, 1}));
    }

    SECTION("the plan comes back partial when the resident set cannot cover the overshoot") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(0, false, 1, kNodeBytes));
        nodes.append(residentNode(1, true, 2, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, 10 * kNodeBytes) == QVector<int>({0, 1}));
    }

    SECTION("nothing is planned when the budget has room") {
        QVector<NodeResidency> nodes;
        nodes.append(residentNode(0, false, 1, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, 0).isEmpty());
    }

    SECTION("a sparse list plans the node indices it carries, coldest first") {
        // Residency is tracked as a list of the resident nodes alone, so what
        // the plan names is NodeResidency::node, never a place in the list.
        QVector<NodeResidency> nodes;
        NodeResidency root = residentNode(0, true, 12, kNodeBytes);
        root.pinned = true;
        nodes.append(root);
        nodes.append(residentNode(7, false, 9, kNodeBytes));
        nodes.append(residentNode(42, false, 4, kNodeBytes));
        nodes.append(residentNode(3, false, 7, kNodeBytes));

        REQUIRE(planNodeEvictions(nodes, 3 * kNodeBytes) == QVector<int>({42, 3, 7}));

        // The root is pinned however deep the overshoot goes.
        REQUIRE(planNodeEvictions(nodes, 100 * kNodeBytes) == QVector<int>({42, 3, 7}));
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

TEST_CASE("cw::octree::selectForest: a forest of one tree cuts what selectCut cuts",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest manifest = buildManifestWithPoints();

    const auto sameCut = [](const SelectionInput& input)
    {
        const Selection cut = selectCut(input);
        const ForestSelection forest = selectForest(forestInput(input));

        REQUIRE(forest.trees.size() == 1);
        const Selection& tree = forest.trees.at(0);

        CHECK(nodeIndices(tree.nodes) == nodeIndices(cut.nodes));
        CHECK(tree.points == cut.points);
        CHECK(tree.bytes == cut.bytes);
        CHECK(tree.pointCapped == cut.pointCapped);
        CHECK(forest.points == cut.points);
        CHECK(forest.bytes == cut.bytes);
        CHECK(forest.pointCapped == cut.pointCapped);
    };

    SECTION("an orthographic camera") {
        sameCut(orthoInput(manifest));
    }

    SECTION("a perspective camera") {
        sameCut(perspectiveInput(manifest));
    }

    SECTION("a frustum that culls half the tree") {
        const QMatrix4x4 viewProjection = orthoProjection(kHalfSpaceOffset, kOrthoExtent) * view();
        const cwFrustum frustum = cwFrustum::fromViewProjection(viewProjection);

        SelectionInput input = orthoInput(manifest);
        input.viewProjection = viewProjection;
        input.frustum = &frustum;

        sameCut(input);
    }

    SECTION("a frustum that culls the root") {
        const QMatrix4x4 viewProjection =
            orthoProjection(2.0f * kOrthoExtent, 4.0f * kOrthoExtent) * view();
        const cwFrustum frustum = cwFrustum::fromViewProjection(viewProjection);

        SelectionInput input = orthoInput(manifest);
        input.viewProjection = viewProjection;
        input.frustum = &frustum;

        sameCut(input);
    }

    SECTION("a point budget under the full cut") {
        SelectionInput input = orthoInput(manifest);
        input.maxPoints = selectCut(input).points / 2;

        sameCut(input);
    }
}

TEST_CASE("cw::octree::selectForest: two trees under one cap share it node for node",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest west = buildManifestWithPoints();
    const cwPointOctreeManifest east = shiftedManifest(west, QVector3D(kSideBySideShift, 0.0f, 0.0f));

    const qint64 treePoints = selectCut(orthoInput(west)).points;

    ForestInput input = sideBySideInput(west, east);
    input.maxPoints = treePoints + treePoints / 2;

    const ForestSelection forest = selectForest(input);
    REQUIRE(forest.trees.size() == 2);

    const Selection& westCut = forest.trees.at(0);
    const Selection& eastCut = forest.trees.at(1);

    CHECK(forest.pointCapped);
    CHECK(westCut.points + eastCut.points == forest.points);
    CHECK(forest.points <= input.maxPoints);
    CHECK(forest.points > treePoints);

    //Both roots go in before the cap, and the cut climbs down both trees together
    REQUIRE_FALSE(westCut.nodes.isEmpty());
    REQUIRE_FALSE(eastCut.nodes.isEmpty());
    CHECK(westCut.nodes.first().node == 0);
    CHECK(eastCut.nodes.first().node == 0);
    CHECK(spacingsFallOff(westCut));
    CHECK(spacingsFallOff(eastCut));

    //One ordered walk refines both trees alike, so the shorter cut is a prefix
    //of the longer one rather than a coarser cut of its own
    CHECK(eastCut.nodes.size() <= westCut.nodes.size());
    CHECK(nodeIndices(westCut.nodes).mid(0, eastCut.nodes.size()) == nodeIndices(eastCut.nodes));

    //The two trees tie on every spacing under this camera, so the cap has to be
    //spent on them alternately: one node of slack, and one level of slack, is
    //all the imbalance a seam can carry
    CHECK(westCut.nodes.size() - eastCut.nodes.size() <= kBalancedNodeSlack);
    CHECK(std::abs(finestLevelOf(west, westCut) - finestLevelOf(east, eastCut))
          <= kBalancedLevelSlack);

    //Both trees hold every level the cap reached, so neither side of the pair
    //stops a level coarser than the other
    for(int octant = 0; octant < kChildCount; octant++) {
        CHECK(nodeSet(westCut.nodes).contains(west.nodes.at(0).children.at(octant)));
        CHECK(nodeSet(eastCut.nodes).contains(east.nodes.at(0).children.at(octant)));
    }
}

TEST_CASE("cw::octree::selectForest: a far tree holds its root while the near one refines",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest nearTree = buildManifestWithPoints();
    const cwPointOctreeManifest farTree = shiftedManifest(nearTree, QVector3D(0.0f, 0.0f, kFarShift));

    ForestInput input = forestInput(perspectiveInput(nearTree));
    input.trees.append({&farTree});

    REQUIRE(projectedSpacingOf(nearTree, 0, input.viewProjection, input.absP11,
                               input.viewportHeightPx) > kDefaultSse);
    REQUIRE(projectedSpacingOf(farTree, 0, input.viewProjection, input.absP11,
                               input.viewportHeightPx) < kDefaultSse);

    const ForestSelection forest = selectForest(input);
    REQUIRE(forest.trees.size() == 2);

    CHECK(forest.trees.at(0).nodes.size() > 1);
    CHECK(nodeIndices(forest.trees.at(1).nodes) == QVector<int>({0}));
    CHECK(forest.trees.at(1).points == farTree.nodes.at(0).pointCount);
    CHECK_FALSE(forest.pointCapped);
}

TEST_CASE("cw::octree::selectForest: an empty tree keeps its place and leaves the others alone",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest west = buildManifestWithPoints();
    const cwPointOctreeManifest east = shiftedManifest(west, QVector3D(kSideBySideShift, 0.0f, 0.0f));

    const ForestSelection both = selectForest(sideBySideInput(west, east));

    ForestInput input = sideBySideInput(west, east);
    input.trees.insert(1, ForestTree());

    const ForestSelection forest = selectForest(input);
    REQUIRE(forest.trees.size() == 3);

    CHECK(forest.trees.at(1).nodes.isEmpty());
    CHECK(forest.trees.at(1).points == 0);
    CHECK(nodeIndices(forest.trees.at(0).nodes) == nodeIndices(both.trees.at(0).nodes));
    CHECK(nodeIndices(forest.trees.at(2).nodes) == nodeIndices(both.trees.at(1).nodes));
    CHECK(forest.points == both.points);
}

TEST_CASE("cw::octree::selectForest: an unknown camera takes every root",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest west = buildManifestWithPoints();
    const cwPointOctreeManifest east = shiftedManifest(west, QVector3D(kSideBySideShift, 0.0f, 0.0f));

    ForestInput input = sideBySideInput(west, east);

    SECTION("no projection scale") {
        input.absP11 = 0.0;
    }

    SECTION("no viewport") {
        input.viewportHeightPx = 0;
    }

    const ForestSelection forest = selectForest(input);
    REQUIRE(forest.trees.size() == 2);
    CHECK(nodeIndices(forest.trees.at(0).nodes) == QVector<int>({0}));
    CHECK(nodeIndices(forest.trees.at(1).nodes) == QVector<int>({0}));
    CHECK(forest.points == west.nodes.at(0).pointCount + east.nodes.at(0).pointCount);
}

TEST_CASE("cw::octree::selectForest: maxNodes counts across the whole forest",
          "[PointOctree][PointOctreeSelection]")
{
    const cwPointOctreeManifest west = buildManifestWithPoints();
    const cwPointOctreeManifest east = shiftedManifest(west, QVector3D(kSideBySideShift, 0.0f, 0.0f));

    ForestInput input = sideBySideInput(west, east);

    SECTION("a limit both trees fit under together") {
        constexpr int kNodeLimit = 5;
        input.maxNodes = kNodeLimit;

        const ForestSelection forest = selectForest(input);
        CHECK(forest.trees.at(0).nodes.size() + forest.trees.at(1).nodes.size() == kNodeLimit);
        CHECK_FALSE(forest.trees.at(0).nodes.isEmpty());
        CHECK_FALSE(forest.trees.at(1).nodes.isEmpty());
    }

    SECTION("a limit of one leaves the second tree out") {
        input.maxNodes = 1;

        const ForestSelection forest = selectForest(input);
        CHECK(nodeIndices(forest.trees.at(0).nodes) == QVector<int>({0}));
        CHECK(forest.trees.at(1).nodes.isEmpty());
    }

    SECTION("no nodes at all") {
        input.maxNodes = 0;

        const ForestSelection forest = selectForest(input);
        REQUIRE(forest.trees.size() == 2);
        CHECK(forest.trees.at(0).nodes.isEmpty());
        CHECK(forest.trees.at(1).nodes.isEmpty());
        CHECK(forest.points == 0);
    }
}
