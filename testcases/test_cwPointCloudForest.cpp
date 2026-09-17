// test_cwPointCloudForest.cpp
// Catch2 unit tests for the render thread's forest cut: the one cut every
// visible point cloud draws from, and the governor that sizes it. No RHI —
// the forest holds its clouds by address and never dereferences them.

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>

//Our includes
#include "cwFrustum.h"
#include "cwPointCloudForest.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwPointOctreeSelection.h"
#include "cwRHIObject.h"
#include "cwRenderBudgets.h"

namespace {

    //A 128 m root cube makes spacing(0) exactly 1 m at kSampleGridResolution
    constexpr double kRootSize = 128.0;
    constexpr float kRootHalf = 64.0f;

    constexpr float kEyeDistance = 300.0f;
    constexpr int kViewportHeightPx = 800;
    constexpr int kViewportWidthPx = 800;

    //2 / (top - bottom) of the orthographic box below
    constexpr float kOrthoExtent = 100.0f;
    constexpr float kOrthoNear = 1.0f;
    constexpr float kOrthoFar = 1000.0f;

    constexpr quint32 kPointsPerNode = 1000;

    //One root cube east of the first tree, so the two project the same and the
    //cut has to interleave them
    constexpr float kSideBySideShift = float(kRootSize);

    //! A three-level tree: the root, its eight children, and the first child's
    //! eight grandchildren, every node carrying points and bytes
    cwPointOctreeManifest buildManifest()
    {
        cwPointOctreeManifest manifest;
        manifest.rootMin = QVector3D(0.0f, 0.0f, 0.0f);
        manifest.rootSize = kRootSize;

        cwPointOctreeNode root;
        root.level = 0;
        manifest.nodes.append(root);

        for(int octant = 0; octant < cw::octree::kChildCount; octant++) {
            cwPointOctreeNode child;
            child.level = 1;
            child.x = quint32(octant & 1);
            child.y = quint32((octant >> 1) & 1);
            child.z = quint32((octant >> 2) & 1);

            const int index = manifest.nodes.size();
            manifest.nodes.append(child);
            manifest.nodes[0].children[octant] = index;
        }

        const int parentIndex = manifest.nodes.at(0).children.at(0);
        const cwPointOctreeNode parent = manifest.nodes.at(parentIndex);
        for(int octant = 0; octant < cw::octree::kChildCount; octant++) {
            cwPointOctreeNode grandchild;
            grandchild.level = 2;
            grandchild.x = parent.x * 2 + quint32(octant & 1);
            grandchild.y = parent.y * 2 + quint32((octant >> 1) & 1);
            grandchild.z = parent.z * 2 + quint32((octant >> 2) & 1);

            const int index = manifest.nodes.size();
            manifest.nodes.append(grandchild);
            manifest.nodes[parentIndex].children[octant] = index;
        }

        for(cwPointOctreeNode& node : manifest.nodes) {
            node.pointCount = kPointsPerNode;
            node.byteSize = qint64(kPointsPerNode) * cw::octree::kBytesPerPoint;
        }

        return manifest;
    }

    cwPointOctreeManifest shiftedManifest(const cwPointOctreeManifest& manifest,
                                          const QVector3D& shift)
    {
        cwPointOctreeManifest shifted = manifest;
        shifted.rootMin += shift;
        return shifted;
    }

    //! A cloud the forest can key on. Nothing here is ever called: the forest
    //! holds clouds by address alone.
    class StubCloud : public cwRHIObject
    {
    public:
        void initialize(const ResourceUpdateData&) override {}
        void synchronize(const SynchronizeData&) override {}
        void updateResources(const ResourceUpdateData&) override {}
        bool gather(const GatherContext&, QVector<PipelineBatch>&) override { return false; }
    };

    QMatrix4x4 view()
    {
        QMatrix4x4 matrix;
        matrix.translate(-kRootHalf, -kRootHalf, -kEyeDistance);
        return matrix;
    }

    QMatrix4x4 orthoProjection()
    {
        QMatrix4x4 matrix;
        matrix.ortho(-kOrthoExtent, kOrthoExtent, -kOrthoExtent, kOrthoExtent,
                     kOrthoNear, kOrthoFar);
        return matrix;
    }

    //! An orthographic camera looking down -Z at the fixture's root cubes
    cwRHIObject::RenderData orthoRenderData()
    {
        cwRHIObject::RenderData data;
        data.cb = nullptr;
        data.renderer = nullptr;
        data.updateFlag = cwSceneUpdate::Flag::None;
        data.projectionMatrix = orthoProjection();
        data.viewProjectionMatrix = orthoProjection() * view();
        data.viewportSize = QSize(kViewportWidthPx, kViewportHeightPx);
        return data;
    }

    //! Every cloud of the forest draws
    bool drawsEverything(const cwRHIObject*)
    {
        return true;
    }

    qint64 pointsOf(const cw::octree::Selection& selection,
                    const cwPointOctreeManifest& manifest)
    {
        qint64 points = 0;
        for(const cw::octree::SelectedNode& node : selection.nodes) {
            points += manifest.nodes.at(node.node).pointCount;
        }
        return points;
    }
}

TEST_CASE("cwPointCloudForest cuts every registered tree in one selection",
          "[PointCloudForest]")
{
    const cwPointOctreeManifest west = buildManifest();
    const cwPointOctreeManifest east =
        shiftedManifest(west, QVector3D(kSideBySideShift, 0.0f, 0.0f));

    StubCloud westCloud;
    StubCloud eastCloud;

    cwPointCloudForest forest;
    forest.setTree(&westCloud, &west);
    forest.setTree(&eastCloud, &east);

    const cwFrustum frustum;
    cwRHIObject::RenderData renderData = orthoRenderData();

    SECTION("each cloud gets its own slice and the slices add up to the cut") {
        forest.select(renderData, frustum, drawsEverything);

        const cw::octree::Selection& westCut = forest.selectionFor(&westCloud);
        const cw::octree::Selection& eastCut = forest.selectionFor(&eastCloud);

        REQUIRE_FALSE(westCut.nodes.isEmpty());
        REQUIRE_FALSE(eastCut.nodes.isEmpty());

        //The root of each tree, first in its own slice
        CHECK(westCut.nodes.first().node == 0);
        CHECK(eastCut.nodes.first().node == 0);

        CHECK(westCut.points == pointsOf(westCut, west));
        CHECK(eastCut.points == pointsOf(eastCut, east));
        CHECK(westCut.points + eastCut.points == forest.points());
        CHECK(westCut.bytes + eastCut.bytes == forest.bytes());
        CHECK_FALSE(forest.pointCapped());
    }

    SECTION("one cap over the whole forest stops the cut and marks it capped") {
        //Room for both roots and a handful of children, so the cap binds on a
        //node rather than on a root
        constexpr int kAffordableNodes = 6;
        renderData.budgets.pointBudget = qint64(kPointsPerNode) * kAffordableNodes;

        forest.select(renderData, frustum, drawsEverything);

        const cw::octree::Selection& westCut = forest.selectionFor(&westCloud);
        const cw::octree::Selection& eastCut = forest.selectionFor(&eastCloud);

        CHECK(forest.pointCapped());
        CHECK(forest.points() <= renderData.budgets.pointBudget);

        //Both trees are still drawn, and neither one spent the whole budget
        CHECK_FALSE(westCut.nodes.isEmpty());
        CHECK_FALSE(eastCut.nodes.isEmpty());
        CHECK(westCut.points + eastCut.points == forest.points());

        //Two trees the camera sees alike land at the same depth, which is the
        //seam the forest cut exists to close
        CHECK(westCut.nodes.size() == eastCut.nodes.size());
    }

    SECTION("a cloud the gate rejects gets an empty slice") {
        forest.select(renderData, frustum, [&](const cwRHIObject* cloud) {
            return cloud != &eastCloud;
        });

        CHECK_FALSE(forest.selectionFor(&westCloud).nodes.isEmpty());
        CHECK(forest.selectionFor(&eastCloud).nodes.isEmpty());
        CHECK(forest.points() == forest.selectionFor(&westCloud).points);
    }

    SECTION("a cloud the forest never saw has no slice") {
        StubCloud strangerCloud;
        forest.select(renderData, frustum, drawsEverything);
        CHECK(forest.selectionFor(&strangerCloud).nodes.isEmpty());
    }

    SECTION("removeTree drops the cloud from the next cut") {
        forest.removeTree(&eastCloud);
        forest.select(renderData, frustum, drawsEverything);

        CHECK_FALSE(forest.selectionFor(&westCloud).nodes.isEmpty());
        CHECK(forest.selectionFor(&eastCloud).nodes.isEmpty());
        CHECK(forest.points() == forest.selectionFor(&westCloud).points);
    }

}

TEST_CASE("cwPointCloudForest coarsens one governor for every cloud",
          "[PointCloudForest]")
{
    const cwPointOctreeManifest west = buildManifest();
    const cwPointOctreeManifest east =
        shiftedManifest(west, QVector3D(kSideBySideShift, 0.0f, 0.0f));

    StubCloud westCloud;
    StubCloud eastCloud;

    cwPointCloudForest forest;
    forest.setTree(&westCloud, &west);
    forest.setTree(&eastCloud, &east);

    const cwFrustum frustum;
    const cwRHIObject::RenderData renderData = orthoRenderData();

    REQUIRE(forest.sseInflation() == 1.0);

    //Nothing is resident, so a zero byte budget is exactly what the cut cannot
    //fit in, whatever the rest of the ledger holds
    cwRenderBudgets budgets;
    budgets.gpuBudgetBytes = 0;

    SECTION("a cut that outruns the budget steps the inflation up") {
        forest.select(renderData, frustum, drawsEverything);
        REQUIRE(forest.bytes() > 0);

        forest.advance(budgets);
        CHECK(forest.sseInflation() == Catch::Approx(cw::octree::kSseInflationStep));

        forest.select(renderData, frustum, drawsEverything);
        forest.advance(budgets);
        CHECK(forest.sseInflation()
              == Catch::Approx(cw::octree::kSseInflationStep * cw::octree::kSseInflationStep));
    }

    SECTION("a cut that fits leaves the inflation where it is") {
        forest.select(renderData, frustum, drawsEverything);
        const qint64 cutBytes = forest.bytes();
        REQUIRE(cutBytes > 0);

        cwRenderBudgets roomy;
        roomy.gpuBudgetBytes = cutBytes * 2;

        forest.advance(roomy);
        CHECK(forest.sseInflation() == 1.0);
    }

    SECTION("a frame that selected nothing holds the inflation") {
        forest.select(renderData, frustum, drawsEverything);
        forest.advance(budgets);
        const double inflated = forest.sseInflation();
        REQUIRE(inflated > 1.0);

        //Every cloud left the view, so there is no cut to measure
        forest.select(renderData, frustum, [](const cwRHIObject*) { return false; });
        forest.advance(budgets);
        CHECK(forest.sseInflation() == inflated);
    }

    SECTION("the last cloud leaving takes the governor back to where it started") {
        forest.select(renderData, frustum, drawsEverything);
        forest.advance(budgets);
        REQUIRE(forest.sseInflation() > 1.0);

        forest.removeTree(&westCloud);
        CHECK(forest.sseInflation() > 1.0);

        forest.removeTree(&eastCloud);
        CHECK(forest.sseInflation() == 1.0);
    }
}
