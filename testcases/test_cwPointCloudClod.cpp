// test_cwPointCloudClod.cpp
// Catch2 unit tests for the continuous level of detail thinning rule that
// PointCloud.vert draws the cut with.

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QVector>

//Std includes
#include <cmath>
#include <limits>

//Our includes
#include "cwPointCloudClod.h"
#include "cwPointOctree.h"

using namespace cw::clod;

namespace {

    //! A point on the node's sample grid: cell (@a x, @a y, @a z) of the
    //! 128-wide grid, offset inside the cell so the hash has something to mix.
    cw::octree::QuantizedPoint gridPoint(int x, int y, int z, int subCell = 0)
    {
        const auto coordinate = [subCell](int cell) {
            return quint16(cell * kQuantStepsPerCell + subCell);
        };

        cw::octree::QuantizedPoint point;
        point.x = coordinate(x);
        point.y = coordinate(y);
        point.z = coordinate(z);
        return point;
    }

    //! Every cell of a @a side-wide corner of the sample grid, one point each,
    //! with the cell index folded into the offset inside the cell so no two
    //! points share a hash.
    QVector<cw::octree::QuantizedPoint> gridPoints(int side)
    {
        QVector<cw::octree::QuantizedPoint> points;
        points.reserve(qsizetype(side) * side * side);
        int subCell = 0;
        for (int z = 0; z < side; z++) {
            for (int y = 0; y < side; y++) {
                for (int x = 0; x < side; x++) {
                    points.append(gridPoint(x, y, z, subCell));
                    subCell = (subCell + 1) % kQuantStepsPerCell;
                }
            }
        }
        return points;
    }

    double drawnFraction(const QVector<cw::octree::QuantizedPoint>& points, double weight)
    {
        int count = 0;
        for (const cw::octree::QuantizedPoint& point : points) {
            if (double(rank(point)) < weight) {
                count++;
            }
        }
        return double(count) / double(points.size());
    }

    //! The cell side of the grid the fraction cases sweep. A full 128 cubed
    //! grid is two million ranks, which the sweep would pay for five times over
    //! for no more precision than a 64 cubed corner of it gives.
    constexpr int kFractionGridSide = 64;

    //! The rank's regular steps, which is how far a measured fraction may sit
    //! from the weight that asked for it
    constexpr double kFractionTolerance = 1.0 / double(kRankSteps);

    constexpr double kNodeSpacing = 1.0;

} // namespace

TEST_CASE("Every rank lands in [0, 1)", "[PointCloudClod]")
{
    for (const cw::octree::QuantizedPoint& point : gridPoints(kFractionGridSide)) {
        const float value = rank(point);
        REQUIRE(value >= 0.0f);
        REQUIRE(value < 1.0f);
    }
}

TEST_CASE("The points drawn at one weight are drawn at every larger weight",
          "[PointCloudClod]")
{
    const QVector<cw::octree::QuantizedPoint> points = gridPoints(kFractionGridSide);

    //Targets from a full level finer than the node up to the node's own
    //spacing, so the weight they ask for climbs from nothing to everything
    const QVector<double> targets = {2.0, 1.8, 1.6, 1.4, 1.2, 1.0};

    for (int i = 1; i < targets.size(); i++) {
        const double coarserTarget = targets.at(i - 1);
        const double finerTarget = targets.at(i);
        REQUIRE(weight(kNodeSpacing, coarserTarget) < weight(kNodeSpacing, finerTarget));

        for (const cw::octree::QuantizedPoint& point : points) {
            if (drawn(point, kNodeSpacing, coarserTarget)) {
                REQUIRE(drawn(point, kNodeSpacing, finerTarget));
            }
        }
    }
}

TEST_CASE("The share of a grid that draws follows the weight", "[PointCloudClod]")
{
    const QVector<cw::octree::QuantizedPoint> points = gridPoints(kFractionGridSide);

    for (double weight : {0.1, 0.25, 0.5, 0.75, 0.9}) {
        const double fraction = drawnFraction(points, weight);
        INFO("weight " << weight << " drew " << fraction);
        CHECK(std::abs(fraction - weight) <= kFractionTolerance);
    }
}

TEST_CASE("An eighth of the weight keeps one point per parent cell",
          "[PointCloudClod]")
{
    //An eighth is the first octant's whole share, so what survives is the one
    //sub-octant of every parent cell that the order picks first
    constexpr double kOneOctantWeight = 1.0 / double(cw::octree::kChildCount);
    constexpr int kPlaneSide = cw::octree::kSampleGridResolution;

    QVector<int> keptPerParentCell(qsizetype(kPlaneSide / 2) * (kPlaneSide / 2), 0);
    int subCell = 0;
    for (int y = 0; y < kPlaneSide; y++) {
        for (int x = 0; x < kPlaneSide; x++) {
            const cw::octree::QuantizedPoint point = gridPoint(x, y, 0, subCell);
            subCell = (subCell + 1) % kQuantStepsPerCell;
            if (double(rank(point)) < kOneOctantWeight) {
                keptPerParentCell[(y / 2) * (kPlaneSide / 2) + (x / 2)]++;
            }
        }
    }

    for (int kept : keptPerParentCell) {
        REQUIRE(kept == 1);
    }
}

TEST_CASE("A node's weight falls from one to zero over the octave below its spacing",
          "[PointCloudClod]")
{
    CHECK(weight(kNodeSpacing, kNodeSpacing) == Catch::Approx(1.0));
    CHECK(weight(kNodeSpacing, 2.0 * kNodeSpacing) == Catch::Approx(0.0));
    CHECK(weight(kNodeSpacing, std::sqrt(2.0) * kNodeSpacing) == Catch::Approx(0.5));

    //Coarser than the target draws whole, finer than the octave draws nothing
    CHECK(weight(kNodeSpacing, 0.5 * kNodeSpacing) == Catch::Approx(1.0));
    CHECK(weight(kNodeSpacing, 4.0 * kNodeSpacing) == Catch::Approx(0.0));
}

TEST_CASE("A node's sample spacing is its quantization step over the sample grid",
          "[PointCloudClod]")
{
    constexpr double kNodeSize = 128.0;
    const double quantizationStep = kNodeSize / double(cw::octree::kQuantMax);

    CHECK(nodeSpacingFromQuantizationStep(quantizationStep)
          == Catch::Approx(kNodeSize / double(cw::octree::kSampleGridResolution)));
}

TEST_CASE("The target spacing stops widening at the root's own spacing",
          "[PointCloudClod]")
{
    constexpr double kSseThresholdPx = 6.0;
    constexpr double kRootSpacing = 2.0;

    //Zoomed in, the threshold is what the cut aims for
    const double closePixelsPerMeter = 100.0;
    CHECK(targetSpacing(kSseThresholdPx, closePixelsPerMeter, kRootSpacing)
          == Catch::Approx(kSseThresholdPx / closePixelsPerMeter));

    //Zoomed out past the root, there is no coarser level to fade toward, so
    //the root keeps every point it has
    const double farPixelsPerMeter = 1.0;
    const double farTarget = targetSpacing(kSseThresholdPx, farPixelsPerMeter, kRootSpacing);
    CHECK(farTarget == Catch::Approx(kRootSpacing));
    CHECK(weight(kRootSpacing, farTarget) == Catch::Approx(1.0));
}

TEST_CASE("Discrete thinning keeps a level whole until its ramp starts, and off keeps it always",
          "[PointCloudClod]")
{
    using cw::pointcloud::ThinningMode;

    //Continuous is the ramp itself
    for (double target : {0.5, 1.0, std::sqrt(2.0), 2.0, 4.0}) {
        CHECK(weight(kNodeSpacing, target * kNodeSpacing, ThinningMode::Continuous)
              == weight(kNodeSpacing, target * kNodeSpacing));
    }

    //Discrete steps to zero the moment the ramp leaves one
    CHECK(weight(kNodeSpacing, 0.5 * kNodeSpacing, ThinningMode::Discrete) == 1.0);
    CHECK(weight(kNodeSpacing, kNodeSpacing, ThinningMode::Discrete) == 1.0);
    CHECK(weight(kNodeSpacing, std::sqrt(2.0) * kNodeSpacing, ThinningMode::Discrete) == 0.0);
    CHECK(weight(kNodeSpacing, 4.0 * kNodeSpacing, ThinningMode::Discrete) == 0.0);

    //Off draws everything, however far past the octave the target is
    CHECK(weight(kNodeSpacing, std::sqrt(2.0) * kNodeSpacing, ThinningMode::Off) == 1.0);
    CHECK(weight(kNodeSpacing, 4.0 * kNodeSpacing, ThinningMode::Off) == 1.0);
}

TEST_CASE("Off draws every point continuous does, which draws every point discrete does",
          "[PointCloudClod]")
{
    using cw::pointcloud::ThinningMode;

    constexpr int kGridSide = 16;
    const QVector<cw::octree::QuantizedPoint> points = gridPoints(kGridSide);

    for (double target : {0.75, 1.0, 1.25, std::sqrt(2.0), 1.75, 2.0, 3.0}) {
        for (const cw::octree::QuantizedPoint& point : points) {
            const bool discrete =
                drawn(point, kNodeSpacing, target * kNodeSpacing, ThinningMode::Discrete);
            const bool continuous =
                drawn(point, kNodeSpacing, target * kNodeSpacing, ThinningMode::Continuous);
            const bool off = drawn(point, kNodeSpacing, target * kNodeSpacing, ThinningMode::Off);

            REQUIRE(continuous == drawn(point, kNodeSpacing, target * kNodeSpacing));
            if (discrete) {
                REQUIRE(continuous);
            }
            REQUIRE(off);
        }
    }
}

TEST_CASE("Measured spacing takes the larger of a node's grid and measured spacing, and grid ignores it",
          "[PointCloudClod]")
{
    using cw::pointcloud::SpacingSource;

    constexpr double kGridSpacing = 0.25;
    constexpr double kSparseSpacing = 0.6;
    constexpr double kDenseSpacing = 0.1;
    constexpr double kUnmeasured = 0.0;

    //A node sparser than its grid answers for the gap it really has
    CHECK(drawSpacing(kGridSpacing, kSparseSpacing, SpacingSource::Measured) == kSparseSpacing);

    //A node denser than its grid, or one nothing measured, keeps its grid
    CHECK(drawSpacing(kGridSpacing, kDenseSpacing, SpacingSource::Measured) == kGridSpacing);
    CHECK(drawSpacing(kGridSpacing, kUnmeasured, SpacingSource::Measured) == kGridSpacing);

    //Grid is the grid spacing, whatever was measured
    for (double measured : {kUnmeasured, kDenseSpacing, kSparseSpacing}) {
        CHECK(drawSpacing(kGridSpacing, measured, SpacingSource::Grid) == kGridSpacing);
    }
}

TEST_CASE("Under measured spacing a sparse node thins and sizes by its measured spacing",
          "[PointCloudClod]")
{
    using cw::pointcloud::SpacingSource;
    using cw::pointcloud::ThinningMode;

    constexpr double kGridSpacing = 1.0;
    //Half an octave sparser than the grid
    const double sparseSpacing = std::sqrt(2.0) * kGridSpacing;
    //A target the grid spacing has half faded toward
    const double target = std::sqrt(2.0) * kGridSpacing;

    const double gridSpacing = drawSpacing(kGridSpacing, sparseSpacing, SpacingSource::Grid);
    const double measuredSpacing =
        drawSpacing(kGridSpacing, sparseSpacing, SpacingSource::Measured);

    //Grid draws what it drew before the measured spacing existed
    CHECK(weight(gridSpacing, target) == weight(kGridSpacing, target));
    CHECK(weight(gridSpacing, target) == Catch::Approx(0.5));

    //Measured draws the sparse node whole at the same target, under every
    //thinning mode that reshapes the ramp
    CHECK(weight(measuredSpacing, target) == Catch::Approx(1.0));
    CHECK(weight(measuredSpacing, target, ThinningMode::Discrete) == 1.0);
    CHECK(weight(gridSpacing, target, ThinningMode::Discrete) == 0.0);

    //Own-spacing sizing measures the sprite from the same spacing, so it is
    //the measured gap's share wider
    CHECK(measuredSpacing / gridSpacing == Catch::Approx(sparseSpacing / kGridSpacing));
}

namespace {
    //! A reveal ramp about as long as the bench candidates run
    constexpr int kTestRevealFrames = 8;
    constexpr double kChildFloor = 0.25;
    constexpr double kParentFloor = 4.0;
    //! Ages well past any ramp, which must all draw whole
    constexpr quint32 kLongAfter = 1000;
    //! Rounding in a sum of logs whose exact value is 0
    constexpr double kLogSumMargin = 1e-12;
    //! Rounding in a fade share whose exact value is 0
    constexpr double kZeroShareMargin = 1e-12;
}

TEST_CASE("The reveal ramp scales an arrival's weight up over revealFrames frames",
          "[PointCloudClod]")
{
    SECTION("the arrival frame draws one revealFrames-th and the last ramp frame draws all") {
        CHECK(revealScale(0, kTestRevealFrames) == Catch::Approx(1.0 / kTestRevealFrames));
        CHECK(revealScale(kTestRevealFrames - 1, kTestRevealFrames) == 1.0);
        CHECK(revealScale(kTestRevealFrames, kTestRevealFrames) == 1.0);
        CHECK(revealScale(kLongAfter, kTestRevealFrames) == 1.0);
    }

    SECTION("every frame of the ramp draws more than the one before, by the same share") {
        for (quint32 age = 1; age < quint32(kTestRevealFrames); age++) {
            INFO("age " << age);
            const double step = revealScale(age, kTestRevealFrames)
                                - revealScale(age - 1, kTestRevealFrames);
            CHECK(step == Catch::Approx(1.0 / kTestRevealFrames));
        }
    }

    SECTION("a ramp is in progress until the frame it reaches one") {
        for (quint32 age = 0; age + 1 < quint32(kTestRevealFrames); age++) {
            INFO("age " << age);
            CHECK(revealing(age, kTestRevealFrames));
        }
        CHECK_FALSE(revealing(kTestRevealFrames - 1, kTestRevealFrames));
        CHECK_FALSE(revealing(kLongAfter, kTestRevealFrames));
    }

    SECTION("no ramp draws every arrival whole and is never in progress") {
        for (quint32 age : {0u, 1u, kLongAfter}) {
            INFO("age " << age);
            CHECK(revealScale(age, cw::pointcloud::kDefaultRevealFrames) == 1.0);
            CHECK_FALSE(revealing(age, cw::pointcloud::kDefaultRevealFrames));
        }
        CHECK(cw::pointcloud::kDefaultRevealFrames == 0);
    }
}

TEST_CASE("The reveal ramp moves a floor in log space over revealFrames frames, both ways",
          "[PointCloudClod]")
{
    const auto logStep = [](double from, double to) { return std::log(to / from); };

    SECTION("a falling floor shrinks by one ratio per frame and lands on the new floor") {
        const double ratio = std::pow(kChildFloor / kParentFloor, 1.0 / kTestRevealFrames);
        double previous = kParentFloor;
        for (quint32 age = 0; age < quint32(kTestRevealFrames); age++) {
            INFO("age " << age);
            const double floor = revealedFloor(kParentFloor, kChildFloor, age, kTestRevealFrames);
            CHECK(floor < previous);
            CHECK(logStep(previous, floor) == Catch::Approx(std::log(ratio)));
            previous = floor;
        }
        CHECK(previous == Catch::Approx(kChildFloor));
    }

    SECTION("a rising floor grows by the inverse ratio, so a rise takes as long as a fall") {
        double previous = kChildFloor;
        for (quint32 age = 0; age < quint32(kTestRevealFrames); age++) {
            INFO("age " << age);
            const double up = revealedFloor(kChildFloor, kParentFloor, age, kTestRevealFrames);
            const double down = revealedFloor(kParentFloor, kChildFloor, age, kTestRevealFrames);
            CHECK(up > previous);
            //Mirror images in log space: the geometric mean is the midpoint
            //The two floors multiply to 1, so the sum is 0 and needs an
            //absolute margin: Approx's relative epsilon of 0 is 0
            CHECK(std::log(up) + std::log(down)
                  == Catch::Approx(std::log(kChildFloor) + std::log(kParentFloor))
                         .margin(kLogSumMargin));
            CHECK(std::log(up / kChildFloor)
                  == Catch::Approx(revealScale(age, kTestRevealFrames)
                                   * std::log(kParentFloor / kChildFloor)));
            previous = up;
        }
        CHECK(previous == Catch::Approx(kParentFloor));
    }

    SECTION("the floor holds at its new value once the ramp is over") {
        CHECK(revealedFloor(kParentFloor, kChildFloor, kLongAfter, kTestRevealFrames)
              == kChildFloor);
        CHECK(revealedFloor(kChildFloor, kParentFloor, kLongAfter, kTestRevealFrames)
              == kParentFloor);
    }

    SECTION("no ramp, or no previous floor, takes the new floor at once") {
        CHECK(revealedFloor(kParentFloor, kChildFloor, 0, cw::pointcloud::kDefaultRevealFrames)
              == kChildFloor);
        CHECK(revealedFloor(0.0, kChildFloor, 0, kTestRevealFrames) == kChildFloor);
    }
}

TEST_CASE("The reveal ramp's age wraps as the shader's unsigned difference does",
          "[PointCloudClod]")
{
    constexpr quint64 kFrame = 100;
    CHECK(revealAge(kFrame, kFrame) == 0u);
    CHECK(revealAge(kFrame, kFrame - 3) == 3u);

    //A stamp ahead of the frame read wraps to an age past every ramp, so the
    //twin draws it whole as the GPU does
    CHECK(revealAge(kFrame, kFrame + 1) == std::numeric_limits<quint32>::max());
    CHECK(revealScale(revealAge(kFrame, kFrame + 1), kTestRevealFrames) == 1.0);

    //Truncated to the 32 bits the constants slot carries
    constexpr quint64 kPastWrap = (quint64(1) << 32) + 5;
    CHECK(revealAge(kPastWrap, kPastWrap - 7) == 7u);
}

TEST_CASE("A fade that takes over from another carries on from the share it drew",
          "[PointCloudClod]")
{
    SECTION("a fade-in from nothing starts at its first step, from all of it ends at once") {
        CHECK(revealScale(fadeInAgeFor(0.0, kTestRevealFrames), kTestRevealFrames)
              == Catch::Approx(1.0 / kTestRevealFrames));
        CHECK(revealScale(fadeInAgeFor(1.0, kTestRevealFrames), kTestRevealFrames) == 1.0);
    }

    SECTION("a fade-out from all of it starts one step down, as a departure does") {
        CHECK(fadeOutAgeFor(1.0, kTestRevealFrames) == 0u);
        CHECK(departureScale(fadeOutAgeFor(1.0, kTestRevealFrames), kTestRevealFrames)
              == Catch::Approx(1.0 - 1.0 / kTestRevealFrames));
    }

    SECTION("turning around mid-fade moves one step from the share it stood at") {
        for (int step = 1; step < kTestRevealFrames; step++) {
            const double share = double(step) / kTestRevealFrames;
            INFO("share " << share);
            CHECK(revealScale(fadeInAgeFor(share, kTestRevealFrames), kTestRevealFrames)
                  == Catch::Approx(share + 1.0 / kTestRevealFrames));
            CHECK(departureScale(fadeOutAgeFor(share, kTestRevealFrames), kTestRevealFrames)
                  == Catch::Approx(share - 1.0 / kTestRevealFrames).margin(kZeroShareMargin));
        }
    }
}

namespace {
    //! A settled sprite side and a long ramp, the user's case
    constexpr double kSettledSidePx = 4.0;
    constexpr int kLongRevealFrames = 60;
    //! A side whose square is not exact in binary, so "exactly the settled
    //! side" cannot pass by sqrt(side * side) happening to round back
    constexpr double kInexactSidePx = 3.7;
    //! How close to one pixel the arrival frame's side sits under the long
    //! ramp: sqrt(1 + 15 / 60) is about 1.118
    constexpr double kNearOnePixel = 1.2;
}

TEST_CASE("A Grow arrival's sprite grows from one pixel to its size with its area linear in the ramp",
          "[PointCloudClod]")
{
    const auto area = [](double side) { return side * side; };
    const double minArea = area(cw::pointcloud::kMinPointSizePx);

    SECTION("the arrival frame draws a sprite near one pixel wide") {
        const double first = grownSize(kSettledSidePx, 0, kLongRevealFrames);
        CHECK(first >= cw::pointcloud::kMinPointSizePx);
        CHECK(first < kNearOnePixel);
        CHECK(first == Catch::Approx(std::sqrt(minArea + (area(kSettledSidePx) - minArea)
                                                         / kLongRevealFrames)));
    }

    SECTION("the area above one pixel is the reveal share of the settled area above it") {
        for (quint32 age = 0; age < quint32(kTestRevealFrames); age++) {
            INFO("age " << age);
            const double side = grownSize(kSettledSidePx, age, kTestRevealFrames);
            CHECK((area(side) - minArea) / (area(kSettledSidePx) - minArea)
                  == Catch::Approx(revealScale(age, kTestRevealFrames)));
        }
    }

    SECTION("every frame of the growth is at least as wide as the one before") {
        double previous = 0.0;
        for (quint32 age = 0; age <= quint32(kLongRevealFrames); age++) {
            INFO("age " << age);
            const double side = grownSize(kSettledSidePx, age, kLongRevealFrames);
            CHECK(side >= previous);
            CHECK(side <= kSettledSidePx);
            previous = side;
        }
    }

    SECTION("the last ramp frame and every frame after draw exactly the settled side") {
        CHECK(grownSize(kInexactSidePx, kTestRevealFrames - 1, kTestRevealFrames)
              == kInexactSidePx);
        CHECK(grownSize(kInexactSidePx, kTestRevealFrames, kTestRevealFrames) == kInexactSidePx);
        CHECK(grownSize(kInexactSidePx, kLongAfter, kTestRevealFrames) == kInexactSidePx);
        CHECK(grownSize(kSettledSidePx, kTestRevealFrames - 2, kTestRevealFrames)
              < kSettledSidePx);
    }

    SECTION("no ramp draws every arrival at its settled side") {
        for (quint32 age : {0u, 1u, kLongAfter}) {
            INFO("age " << age);
            CHECK(grownSize(kInexactSidePx, age, cw::pointcloud::kDefaultRevealFrames)
                  == kInexactSidePx);
        }
    }

    SECTION("a one pixel sprite stays one pixel") {
        CHECK(grownSize(cw::pointcloud::kMinPointSizePx, 0, kTestRevealFrames)
              == cw::pointcloud::kMinPointSizePx);
    }

    SECTION("a re-entry grows on from the share it carried rather than from one pixel") {
        for (int step = 1; step < kTestRevealFrames; step++) {
            const double share = double(step) / kTestRevealFrames;
            INFO("share " << share);
            const quint32 age = fadeInAgeFor(share, kTestRevealFrames);
            const double side = grownSize(kSettledSidePx, age, kTestRevealFrames);
            //One step past the carried share, as the Cover fade-in takes over
            CHECK((area(side) - minArea) / (area(kSettledSidePx) - minArea)
                  == Catch::Approx(std::min(1.0, share + 1.0 / kTestRevealFrames)));
            CHECK(side > grownSize(kSettledSidePx, 0, kTestRevealFrames));
        }
    }
}
