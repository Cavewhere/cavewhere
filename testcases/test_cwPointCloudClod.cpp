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
