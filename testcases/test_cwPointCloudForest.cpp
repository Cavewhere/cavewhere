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
#include "cwRenderMemoryLedger.h"

//Std includes
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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

    // ---- The governor's slew ------------------------------------------------

    constexpr double kQuarterSlew = 0.25;
    constexpr double kLogTolerance = 1e-9;

    //The inflation at which the level one nodes stop refining. Just above 1,
    //so the first slewed frame of a step up already clears it.
    constexpr double kLevelOneBreakpoint = 1.01;

    //The cut at inflation 1, and at any inflation past the breakpoint
    constexpr int kFullCutNodes = 17;
    constexpr int kCoarseCutNodes = 9;

    //Room for the coarse cut and short of the full one, and room for the full
    //one past the relax margin
    constexpr int kTightNodes = 13;
    constexpr int kRoomyNodes = 40;

    constexpr int kZeroBudgetFrames = 16;
    constexpr int kRoomyFrames = 12;
    constexpr int kMostFrames = 200;

    qint64 bytesOfNodes(int count)
    {
        return qint64(count) * kPointsPerNode * cw::octree::kBytesPerPoint;
    }

    //! What the forest takes off the GPU budget before the clouds see any of it
    qint64 bytesOutsideClouds()
    {
        const auto* ledger = cwRenderMemoryLedger::instance();
        return ledger->totalBytes(cwRenderMemoryLedger::Residency::Gpu)
               - ledger->bytes(cwRenderMemoryLedger::Category::PointCloudGeometry,
                               cwRenderMemoryLedger::Residency::Gpu);
    }

    //! The inflation @a k frames into a slew from @a from toward @a to
    double slewedInflation(double from, double to, double slew, int k)
    {
        const double closed = 1.0 - std::pow(1.0 - slew, k);
        return std::exp(std::log(from) + closed * (std::log(to) - std::log(from)));
    }

    //! The frame a slew of @a ratio arrives on: the first whose remaining
    //! log-space gap is within kSseSlewArrivalLogGap
    int framesToArrive(double slew, double ratio)
    {
        double gap = std::abs(std::log(ratio));
        int frames = 0;
        do {
            gap *= 1.0 - slew;
            frames++;
        } while (gap > cw::octree::kSseSlewArrivalLogGap);
        return frames;
    }

    QVector<int> nodesOf(const cw::octree::Selection& selection)
    {
        QVector<int> nodes;
        for (const cw::octree::SelectedNode& node : selection.nodes) {
            nodes.append(node.node);
        }
        return nodes;
    }

    //! One tree under one governed camera whose level one nodes stop refining
    //! at kLevelOneBreakpoint, framed the way the live view frames it: the
    //! governor's step at the top, then the cut
    struct GovernedTree {
        cwPointOctreeManifest manifest = buildManifest();
        StubCloud cloud;
        cwPointCloudForest forest;
        cwFrustum frustum;
        cwRHIObject::RenderData renderData = orthoRenderData();
        cwRenderBudgets budgets;

        explicit GovernedTree(double slew)
        {
            budgets.governorSlew = slew;
            budgets.screenSpaceErrorPx = levelOneSpacingPx() / kLevelOneBreakpoint;
            forest.setTree(&cloud, &manifest);
        }

        void frame()
        {
            forest.advance(budgets);
            renderData.budgets = budgets;
            forest.select(renderData, frustum, drawsEverything);
        }

        void setRoomFor(int nodes)
        {
            budgets.gpuBudgetBytes = bytesOutsideClouds() + bytesOfNodes(nodes);
        }

        int cutNodes() const
        {
            return int(forest.selectionFor(&cloud).nodes.size());
        }

        //! The cut a camera with this one's view and a fixed @a inflation selects
        QVector<int> cutAtFixedInflation(double inflation) const
        {
            cwPointCloudForest reference;
            reference.setTree(&cloud, &manifest);
            cwRHIObject::RenderData fixed = renderData;
            fixed.budgets.screenSpaceErrorPx = budgets.screenSpaceErrorPx * inflation;
            reference.select(fixed, frustum, drawsEverything);
            return nodesOf(reference.selectionFor(&cloud));
        }

        //! Frames until the governor's target leaves @a target, at most kMostFrames
        bool frameUntilTargetLeaves(double target)
        {
            for (int i = 0; i < kMostFrames; i++) {
                frame();
                if (forest.sseInflationTarget() != target) {
                    return true;
                }
            }
            return false;
        }

        //! Frames from the one the target moved on until the inflation takes
        //! it, at most kMostFrames
        int framesUntilArrived()
        {
            int frames = 1;
            while (forest.sseInflation() != forest.sseInflationTarget() && frames < kMostFrames) {
                frame();
                frames++;
            }
            return frames;
        }

    private:
        double levelOneSpacingPx() const
        {
            cwPointCloudForest probe;
            probe.setTree(&cloud, &manifest);
            probe.select(orthoRenderData(), frustum, drawsEverything);
            for (const cw::octree::SelectedNode& node : probe.selectionFor(&cloud).nodes) {
                if (manifest.nodes.at(node.node).level == 1) {
                    return node.projectedSpacingPx;
                }
            }
            return 0.0;
        }
    };

    //! One frame of a slew: the inflation the forest used, the one the slew
    //! puts there, and the governor's target
    struct SlewFrame {
        int frame = 0;
        double inflation = 1.0;
        double expected = 1.0;
        double target = 1.0;

        bool onCurve(double logTolerance) const
        {
            return std::abs(std::log(inflation) - std::log(expected)) <= logTolerance;
        }
    };

    std::string describe(const std::optional<SlewFrame>& frame)
    {
        if (!frame.has_value()) {
            return "every frame held";
        }
        std::ostringstream text;
        text.precision(17);
        text << "frame " << frame->frame << " used " << frame->inflation << " where the slew puts "
             << frame->expected << ", target " << frame->target;
        return text.str();
    }

    //! The first of the frames before @a arrival that reports itself settled
    //! while still short of its target, counted from the frame the target
    //! moved away from @a from on
    std::optional<SlewFrame> firstSettledFrame(GovernedTree& governed, double from, int arrival)
    {
        const double to = governed.forest.sseInflationTarget();
        for (int k = 1; k < arrival; k++) {
            if (!governed.forest.isSettling(governed.budgets)) {
                return SlewFrame{k, governed.forest.sseInflation(),
                                 slewedInflation(from, to, kQuarterSlew, k), to};
            }
            governed.frame();
        }
        return std::nullopt;
    }

    //! @a governed stepped up once from 1 to kSseInflationStep under a budget
    //! that holds the coarse cut alone, on the frame the target moved
    void stepUpOnce(GovernedTree& governed)
    {
        governed.setRoomFor(kTightNodes);
        governed.frame();
        REQUIRE(governed.cutNodes() == kFullCutNodes);
        REQUIRE(governed.frameUntilTargetLeaves(1.0));
        REQUIRE(governed.forest.sseInflationTarget() == cw::octree::kSseInflationStep);
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
        //This checks the target's step; the slew has its own test cases below
        budgets.governorSlew = cw::budgets::kMaxGovernorSlew;

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

TEST_CASE("A slew of one steps the inflation exactly as the governor always has",
          "[PointCloudForest][PointCloudStreaming]")
{
    GovernedTree governed(cw::budgets::kMaxGovernorSlew);

    std::vector<double> inflations;
    std::vector<double> targets;
    const auto frames = [&](int count) {
        for (int i = 0; i < count; i++) {
            governed.frame();
            inflations.push_back(governed.forest.sseInflation());
            targets.push_back(governed.forest.sseInflationTarget());
        }
    };

    governed.budgets.gpuBudgetBytes = 0;
    frames(kZeroBudgetFrames);
    governed.setRoomFor(kRoomyNodes);
    frames(kRoomyFrames);

    //The sequence the governor stepped through before it had a slew
    const std::vector<double> pinned = {
        1.0, 1.25, 1.5625, 1.953125, 2.44140625, 3.0517578125, 3.814697265625,
        4.76837158203125, 5.9604644775390625, 7.450580596923828125,
        9.31322574615478515625, 11.6415321826934814453125,
        14.551915228366851806640625,
        16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0,
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
    };
    CHECK(inflations == pinned);
    CHECK(targets == pinned);
}

TEST_CASE("A slewed step up closes the slew's share of the log-space gap each frame",
          "[PointCloudForest][PointCloudStreaming]")
{
    GovernedTree governed(kQuarterSlew);
    stepUpOnce(governed);

    constexpr double kFrom = 1.0;
    const double to = governed.forest.sseInflationTarget();
    const int arrival = framesToArrive(kQuarterSlew, to / kFrom);

    //The first frame short of arrival that is off the slewed curve, or that
    //has already arrived; none on a governor that slews
    std::optional<SlewFrame> firstOff;
    for (int k = 1; k < arrival && !firstOff.has_value(); k++) {
        const SlewFrame frame{k, governed.forest.sseInflation(),
                              slewedInflation(kFrom, to, kQuarterSlew, k),
                              governed.forest.sseInflationTarget()};
        if (!frame.onCurve(kLogTolerance) || frame.inflation == to) {
            firstOff = frame;
        }
        governed.frame();
    }

    INFO("arrival on frame " << arrival << "; " << describe(firstOff));
    CHECK_FALSE(firstOff.has_value());
    CHECK(governed.forest.sseInflation() == to);
    CHECK(governed.forest.sseInflationTarget() == to);
}

TEST_CASE("A slewed step down takes as many frames as the step up of the same ratio",
          "[PointCloudForest][PointCloudStreaming]")
{
    GovernedTree governed(kQuarterSlew);
    stepUpOnce(governed);
    const int upFrames = governed.framesUntilArrived();
    const double inflated = governed.forest.sseInflation();
    REQUIRE(inflated == cw::octree::kSseInflationStep);

    governed.setRoomFor(kRoomyNodes);
    REQUIRE(governed.frameUntilTargetLeaves(inflated));
    REQUIRE(governed.forest.sseInflationTarget() == 1.0);
    const int downFrames = governed.framesUntilArrived();

    const int expected = framesToArrive(kQuarterSlew, cw::octree::kSseInflationStep);
    INFO("up " << upFrames << " frames, down " << downFrames << ", expected " << expected);
    CHECK(upFrames == expected);
    CHECK(downFrames == upFrames);
    CHECK(governed.forest.sseInflation() == 1.0);
}

TEST_CASE("Every frame of a slew cuts what a fixed threshold at the slewed inflation selects",
          "[PointCloudForest][PointCloudStreaming]")
{
    GovernedTree governed(kQuarterSlew);
    stepUpOnce(governed);
    governed.framesUntilArrived();

    governed.setRoomFor(kRoomyNodes);
    REQUIRE(governed.frameUntilTargetLeaves(cw::octree::kSseInflationStep));
    REQUIRE(governed.forest.sseInflationTarget() == 1.0);

    const int arrival = framesToArrive(kQuarterSlew, cw::octree::kSseInflationStep);
    bool coarseMidSlew = false;
    std::optional<SlewFrame> cutOffItsOwnInflation;
    std::optional<SlewFrame> cutOffTheCurve;
    for (int k = 1; k <= arrival; k++) {
        const double expected = k < arrival
                                    ? slewedInflation(cw::octree::kSseInflationStep, 1.0,
                                                      kQuarterSlew, k)
                                    : 1.0;
        const SlewFrame frame{k, governed.forest.sseInflation(), expected,
                              governed.forest.sseInflationTarget()};
        const QVector<int> cut = nodesOf(governed.forest.selectionFor(&governed.cloud));
        if (!cutOffItsOwnInflation.has_value()
            && cut != governed.cutAtFixedInflation(frame.inflation)) {
            cutOffItsOwnInflation = frame;
        }
        if (!cutOffTheCurve.has_value() && cut != governed.cutAtFixedInflation(expected)) {
            cutOffTheCurve = frame;
        }
        coarseMidSlew = coarseMidSlew || cut.size() == kCoarseCutNodes;
        governed.frame();
    }

    {
        INFO("cut against its own inflation: " << describe(cutOffItsOwnInflation));
        CHECK_FALSE(cutOffItsOwnInflation.has_value());
    }
    {
        INFO("cut against the slewed curve: " << describe(cutOffTheCurve));
        CHECK_FALSE(cutOffTheCurve.has_value());
    }

    //The slew crosses the breakpoint, so the cut changes part way down
    CHECK(coarseMidSlew);
}

TEST_CASE("The forest settles only once the slewed inflation has reached its target",
          "[PointCloudForest][PointCloudStreaming]")
{
    GovernedTree governed(kQuarterSlew);
    stepUpOnce(governed);

    SECTION("on the way up") {
        const int arrival = framesToArrive(kQuarterSlew, cw::octree::kSseInflationStep);
        const std::optional<SlewFrame> settled = firstSettledFrame(governed, 1.0, arrival);
        INFO("arrival on frame " << arrival << "; " << describe(settled));
        CHECK_FALSE(settled.has_value());
    }

    SECTION("on the way down to an uninflated view, which has no probe left to run") {
        governed.framesUntilArrived();
        governed.setRoomFor(kRoomyNodes);
        REQUIRE(governed.frameUntilTargetLeaves(cw::octree::kSseInflationStep));
        REQUIRE(governed.forest.sseInflationTarget() == 1.0);

        const int arrival = framesToArrive(kQuarterSlew, cw::octree::kSseInflationStep);
        const std::optional<SlewFrame> settled =
            firstSettledFrame(governed, cw::octree::kSseInflationStep, arrival);
        INFO("arrival on frame " << arrival << "; " << describe(settled));
        CHECK_FALSE(settled.has_value());

        CHECK(governed.forest.sseInflation() == 1.0);
        CHECK_FALSE(governed.forest.isSettling(governed.budgets));
        governed.frame();
        CHECK_FALSE(governed.forest.isSettling(governed.budgets));
    }
}

TEST_CASE("The governor switched off drops a slewing inflation to 1 in one frame",
          "[PointCloudForest][PointCloudStreaming]")
{
    GovernedTree governed(kQuarterSlew);
    stepUpOnce(governed);
    REQUIRE(governed.forest.sseInflation() > 1.0);

    governed.budgets.inflationGovernor = false;
    governed.frame();

    CHECK(governed.forest.sseInflation() == 1.0);
    CHECK(governed.forest.sseInflationTarget() == 1.0);
    CHECK_FALSE(governed.forest.isSettling(governed.budgets));
}

TEST_CASE("A slewed governor short of GPU memory climbs at least every other frame",
          "[PointCloudForest][PointCloudStreaming]")
{
    GovernedTree governed(kQuarterSlew);
    governed.budgets.gpuBudgetBytes = 0;

    //The governor's own climb from 1 takes one frame a step; a slew whose cut
    //is over the budget ends on the frame after its step
    const int steps = int(std::ceil(std::log(cw::octree::kMaxSseInflation)
                                    / std::log(cw::octree::kSseInflationStep)));
    constexpr int kFirstFrameSelectsNothing = 1;
    constexpr int kFramesPerStep = 2;
    const int mostFrames = kFirstFrameSelectsNothing + kFramesPerStep * steps;

    int frames = 0;
    while (governed.forest.sseInflation() < cw::octree::kMaxSseInflation && frames < kMostFrames) {
        governed.frame();
        frames++;
    }

    INFO("reached " << governed.forest.sseInflation() << " in " << frames << " frames, at most "
         << mostFrames);
    CHECK(governed.forest.sseInflation() == cw::octree::kMaxSseInflation);
    CHECK(frames <= mostFrames);
}

TEST_CASE("The forest clamps a governor slew outside the knob's range",
          "[PointCloudForest][PointCloudStreaming]")
{
    SECTION("a slew of zero arrives at the knob's slowest") {
        GovernedTree governed(0.0);
        stepUpOnce(governed);
        const int expected =
            framesToArrive(cw::budgets::kMinGovernorSlew, cw::octree::kSseInflationStep);
        CHECK(governed.framesUntilArrived() == expected);
        CHECK(governed.forest.sseInflation() == cw::octree::kSseInflationStep);
    }

    SECTION("a slew past one takes the target at once") {
        constexpr double kPastWholeSlew = 2.0;
        GovernedTree governed(kPastWholeSlew);
        stepUpOnce(governed);
        CHECK(governed.forest.sseInflation() == cw::octree::kSseInflationStep);
    }

    SECTION("a slew that is not a number takes the target at once") {
        GovernedTree governed(std::numeric_limits<double>::quiet_NaN());
        stepUpOnce(governed);
        CHECK(governed.forest.sseInflation() == cw::octree::kSseInflationStep);
    }
}
