//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QHash>
#include <QSet>
#include <QVector>
#include <QVector3D>

//Std includes
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <random>
#include <utility>

//Our includes
#include "cwMath.h"
#include "cwPointCloudClod.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwPointOctreeSampler.h"
#include "LocalSpacingFixtureHelper.h"

using namespace cw::octree;
using cwLocalSpacingFixture::estimateLocalSpacing;

namespace {
    constexpr int kPassagePointCount = 200000;
    constexpr quint32 kPassageSeed = 20260912;

    constexpr double kTubeLength = 120.0;
    constexpr double kTubeRadius = 3.0;
    constexpr double kTubeBend = 25.0;
    constexpr double kTubeWaveAmplitude = 6.0;
    constexpr double kTubeWaveCount = 3.0;
    constexpr double kTwoPi = 6.283185307179586;

    //Coordinates compare as ten-thousandths of a meter
    constexpr double kRoundScale = 10000.0;

    constexpr double kRootPadding = 1.02;

    //Fraction of a box's size allowed as containment slack
    constexpr float kContainmentSlack = 1e-4f;

    using RoundedPoint = std::array<qint64, 3>;

    RoundedPoint rounded(const QVector3D& point)
    {
        return RoundedPoint {
            std::llround(double(point.x()) * kRoundScale),
            std::llround(double(point.y()) * kRoundScale),
            std::llround(double(point.z()) * kRoundScale)
        };
    }

    QVector<RoundedPoint> sortedRounded(const QVector<QVector3D>& points)
    {
        QVector<RoundedPoint> values;
        values.reserve(points.size());
        for(const QVector3D& point : points) {
            values.append(rounded(point));
        }
        std::sort(values.begin(), values.end());
        return values;
    }

    //A bent, wavy tube standing in for a cave passage
    QVector<QVector3D> passagePoints(int count, quint32 seed)
    {
        std::mt19937 generator(seed);
        std::uniform_real_distribution<double> unit(0.0, 1.0);

        QVector<QVector3D> points;
        points.reserve(count);

        for(int i = 0; i < count; i++) {
            const double along = unit(generator);
            const double around = unit(generator) * kTwoPi;

            const double x = along * kTubeLength;
            const double y = kTubeBend * along * along;
            const double z = kTubeWaveAmplitude * std::sin(kTwoPi * kTubeWaveCount * along);

            points.append(QVector3D(static_cast<float>(x + kTubeRadius * std::cos(around) * 0.25),
                                    static_cast<float>(y + kTubeRadius * std::cos(around)),
                                    static_cast<float>(z + kTubeRadius * std::sin(around))));
        }

        return points;
    }

    struct RootCube {
        QVector3D minimum;
        double size = 0.0;
    };

    RootCube rootCubeOf(const QVector<QVector3D>& points)
    {
        QBox3D box;
        for(const QVector3D& point : points) {
            box.unite(point);
        }

        const QVector3D extent = box.size();
        const double size = std::max({double(extent.x()), double(extent.y()), double(extent.z()), 1.0}) * kRootPadding;
        const QVector3D center = box.center();
        const QVector3D half(static_cast<float>(size * 0.5), static_cast<float>(size * 0.5), static_cast<float>(size * 0.5));

        return RootCube {center - half, size};
    }

    QVector<QVector3D> allPoints(const QVector<SampledNode>& nodes)
    {
        QVector<QVector3D> points;
        for(const SampledNode& node : nodes) {
            points += node.points;
        }
        return points;
    }

    bool hasChildren(const SampledNode& node)
    {
        return std::any_of(node.children.begin(), node.children.end(), [](int child) { return child >= 0; });
    }

    //A point's cell in the node's own kSampleGridResolution grid, packed into one key
    quint64 packedSampleCell(const QVector3D& point, const QBox3D& bounds)
    {
        const QVector3D minimum = bounds.minimum();
        const QVector3D size = bounds.size();

        const auto coordinate = [](float value, float low, float extent) {
            const int cell = static_cast<int>(std::floor((value - low) / extent * kSampleGridResolution));
            return quint64(std::clamp(cell, 0, kSampleGridResolution - 1));
        };

        return (coordinate(point.z(), minimum.z(), size.z()) * kSampleGridResolution
                + coordinate(point.y(), minimum.y(), size.y())) * kSampleGridResolution
               + coordinate(point.x(), minimum.x(), size.x());
    }

    //Cell bounds are rebuilt per level, so containment allows an ulp of drift
    QBox3D withSlack(const QBox3D& box)
    {
        const QVector3D slack = box.size() * kContainmentSlack;
        return QBox3D(box.minimum() - slack, box.maximum() + slack);
    }

    bool sameTree(const QVector<SampledNode>& first, const QVector<SampledNode>& second)
    {
        if(first.size() != second.size()) {
            return false;
        }

        for(int i = 0; i < first.size(); i++) {
            const SampledNode& left = first.at(i);
            const SampledNode& right = second.at(i);

            if(left.cell.level != right.cell.level
               || left.cell.x != right.cell.x
               || left.cell.y != right.cell.y
               || left.cell.z != right.cell.z
               || left.children != right.children
               || left.points != right.points) {
                return false;
            }
        }

        return true;
    }
}

TEST_CASE("Cell bounds tile the root cube", "[PointOctree][PointOctreeSampler]") {
    const QVector3D rootMin(-4.0f, 10.0f, 0.5f);
    const double rootSize = 16.0;

    const QBox3D root = cellBounds(Cell{0, 0, 0, 0}, rootMin, rootSize);
    CHECK(root.minimum() == rootMin);
    CHECK(root.maximum() == rootMin + QVector3D(16.0f, 16.0f, 16.0f));

    for(int octant = 0; octant < kChildCount; octant++) {
        const Cell child = childCell(Cell{0, 0, 0, 0}, octant);
        CHECK(child.level == 1);
        CHECK(child.x == quint32(octant & 1));
        CHECK(child.y == quint32((octant >> 1) & 1));
        CHECK(child.z == quint32((octant >> 2) & 1));

        const QBox3D childBounds = cellBounds(child, rootMin, rootSize);
        CHECK(childBounds.size() == QVector3D(8.0f, 8.0f, 8.0f));
        CHECK(withSlack(root).contains(childBounds));
        CHECK(octantOf(childBounds.center(), root) == octant);
    }

    const Cell grandchild = childCell(childCell(Cell{0, 0, 0, 0}, 7), 0);
    CHECK(grandchild.level == 2);
    CHECK(grandchild.x == 2);
    CHECK(grandchild.y == 2);
    CHECK(grandchild.z == 2);
}

TEST_CASE("Faces and the max corner name a real octant", "[PointOctree][PointOctreeSampler]") {
    const QBox3D bounds(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(2.0f, 2.0f, 2.0f));

    CHECK(octantOf(QVector3D(0.0f, 0.0f, 0.0f), bounds) == 0);
    CHECK(octantOf(QVector3D(1.5f, 0.5f, 0.5f), bounds) == 1);
    CHECK(octantOf(QVector3D(0.5f, 1.5f, 0.5f), bounds) == 2);
    CHECK(octantOf(QVector3D(0.5f, 0.5f, 1.5f), bounds) == 4);

    //The split planes belong to the upper octant
    CHECK(octantOf(QVector3D(1.0f, 1.0f, 1.0f), bounds) == 7);

    //Faces, the max corner, and points beyond the box clamp inside
    CHECK(octantOf(QVector3D(2.0f, 2.0f, 2.0f), bounds) == 7);
    CHECK(octantOf(QVector3D(0.5f, 2.0f, 0.5f), bounds) == 2);
    CHECK(octantOf(QVector3D(-5.0f, -5.0f, -5.0f), bounds) == 0);
    CHECK(octantOf(QVector3D(9.0f, 9.0f, 9.0f), bounds) == 7);

    //A coordinate far outside the box, or one that is not a number, still names an octant
    constexpr float kNotANumber = std::numeric_limits<float>::quiet_NaN();
    CHECK(octantOf(QVector3D(1e30f, 0.5f, 0.5f), bounds) == 1);
    CHECK(octantOf(QVector3D(-1e30f, 0.5f, 0.5f), bounds) == 0);
    CHECK(octantOf(QVector3D(kNotANumber, 0.5f, 0.5f), bounds) == 0);

    //A flat box has a single octant
    const QBox3D flat(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(octantOf(QVector3D(1.0f, 1.0f, 1.0f), flat) == 0);
}

TEST_CASE("A grid sample takes one point per occupied cell", "[PointOctree][PointOctreeSampler]") {
    constexpr int kResolution = 4;
    const QBox3D bounds(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(4.0f, 4.0f, 4.0f));

    QVector<QVector3D> points;
    QVector<QVector3D> expectedSurvivors;
    QVector<QVector3D> expectedSample;
    for(int z = 0; z < kResolution; z++) {
        for(int y = 0; y < kResolution; y++) {
            for(int x = 0; x < kResolution; x++) {
                //An off-center point first, then the one on the cell center
                const QVector3D corner(x + 0.9f, y + 0.9f, z + 0.9f);
                const QVector3D center(x + 0.5f, y + 0.5f, z + 0.5f);
                points.append(corner);
                points.append(center);
                expectedSurvivors.append(corner);
                expectedSample.append(center);
            }
        }
    }

    const QVector<QVector3D> sampled = gridSample(bounds, kResolution, points);

    CHECK(sampled.size() == 64);
    CHECK(points.size() == 64);

    //The point on the cell center wins every cell, and both halves keep their order
    CHECK(sampled == expectedSample);
    CHECK(points == expectedSurvivors);

    QVector<QVector3D> empty;
    CHECK(gridSample(bounds, kResolution, empty).isEmpty());
}

TEST_CASE("Out of range coordinates land in a real grid cell", "[PointOctree][PointOctreeSampler]") {
    constexpr int kResolution = 4;
    constexpr float kNotANumber = std::numeric_limits<float>::quiet_NaN();
    const QBox3D bounds(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(4.0f, 4.0f, 4.0f));

    QVector<QVector3D> points {
        QVector3D(1e30f, 2.0f, 2.0f),
        QVector3D(-1e30f, 2.0f, 2.0f),
        QVector3D(kNotANumber, 3.0f, 3.0f)
    };

    const QVector<QVector3D> sampled = gridSample(bounds, kResolution, points);

    //Each one clamps to a different cell, so all three are taken
    CHECK(sampled.size() == 3);
    CHECK(points.isEmpty());
}

TEST_CASE("sampleUp takes one grid sample of the union of its children", "[PointOctree][PointOctreeSampler]") {
    const QBox3D bounds(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(8.0f, 8.0f, 8.0f));

    //With a 128 cell grid over an 8 m cube a cell is 0.0625 m wide, centered a half cell in
    const QVector3D sharedCenter(1.03125f, 1.03125f, 1.03125f);
    const QVector3D sharedOffCenter(1.0f, 1.03125f, 1.03125f);
    const QVector3D middleOffCenter(5.0f, 1.0f, 1.0f);
    const QVector3D middleCenter(5.03125f, 1.0f, 1.0f);
    const QVector3D farLow(7.0f, 7.0f, 7.0f);
    const QVector3D farMid(7.01f, 7.0f, 7.0f);
    const QVector3D farCenter(7.03125f, 7.0f, 7.0f);
    const QVector3D lone(3.0f, 3.0f, 3.0f);

    QVector<QVector3D> childA {sharedCenter, middleOffCenter, middleCenter, farLow, farMid, farCenter};
    QVector<QVector3D> childB {sharedOffCenter, lone};

    const QVector<QVector3D> input = childA + childB;

    const QVector<QVector3D> sampled = sampleUp(bounds, {&childA, &childB});

    //Four occupied parent cells, and the point nearest each center wins it
    CHECK(sampled == QVector<QVector3D>({sharedCenter, middleCenter, farCenter, lone}));

    //Each child dropped exactly the points the parent took, survivors in their original order
    CHECK(childA == QVector<QVector3D>({middleOffCenter, farLow, farMid}));
    CHECK(childB == QVector<QVector3D>({sharedOffCenter}));

    CHECK(sortedRounded(sampled + childA + childB) == sortedRounded(input));
}

TEST_CASE("Children drained by the parent sample are dropped", "[PointOctree][PointOctreeSampler]") {
    constexpr int kLeafMax = 2;
    const QVector3D rootMin(0.0f, 0.0f, 0.0f);
    const double rootSize = 8.0;

    SECTION("Every child drains, so the root is childless") {
        const QVector<QVector3D> points {
            QVector3D(1.0f, 1.0f, 1.0f),
            QVector3D(7.0f, 1.0f, 1.0f),
            QVector3D(1.0f, 7.0f, 1.0f)
        };

        const QVector<SampledNode> nodes = buildSubtree(points, Cell{0, 0, 0, 0}, rootMin, rootSize, kLeafMax);

        REQUIRE(nodes.size() == 1);
        CHECK(!hasChildren(nodes.at(0)));
        CHECK(sortedRounded(nodes.at(0).points) == sortedRounded(points));
    }

    SECTION("A child that keeps a survivor stays") {
        //The first two share a cell of the root's grid, so one of them survives in octant 0
        const QVector<QVector3D> points {
            QVector3D(1.0f, 1.0f, 1.0f),
            QVector3D(1.03125f, 1.0f, 1.0f),
            QVector3D(7.0f, 1.0f, 1.0f),
            QVector3D(1.0f, 7.0f, 1.0f)
        };

        const QVector<SampledNode> nodes = buildSubtree(points, Cell{0, 0, 0, 0}, rootMin, rootSize, kLeafMax);

        REQUIRE(nodes.size() == 2);
        CHECK(nodes.at(0).children.at(0) == 1);
        for(int octant = 1; octant < kChildCount; octant++) {
            CHECK(nodes.at(0).children.at(octant) == -1);
        }

        CHECK(nodes.at(1).cell.level == 1);
        CHECK(nodes.at(1).points == QVector<QVector3D>({QVector3D(1.0f, 1.0f, 1.0f)}));
        CHECK(sortedRounded(allPoints(nodes)) == sortedRounded(points));
    }
}

TEST_CASE("Cell bounds agree with the manifest", "[PointOctree][PointOctreeSampler]") {
    cwPointOctreeManifest manifest;
    manifest.rootMin = QVector3D(-3.5f, 7.25f, -100.0f);
    manifest.rootSize = 137.0;

    const QVector<Cell> cells {
        Cell{0, 0, 0, 0},
        Cell{1, 1, 0, 1},
        Cell{3, 5, 2, 7},
        Cell{kMaxLevel, 1u << (kMaxLevel - 1), 0, 3}
    };

    for(const Cell& cell : cells) {
        cwPointOctreeNode node;
        node.level = cell.level;
        node.x = cell.x;
        node.y = cell.y;
        node.z = cell.z;
        manifest.nodes.append(node);
    }

    for(int i = 0; i < cells.size(); i++) {
        const QBox3D fromSampler = cellBounds(cells.at(i), manifest.rootMin, manifest.rootSize);
        const QBox3D fromManifest = manifest.nodeBounds(i);
        CHECK(fromSampler.minimum() == fromManifest.minimum());
        CHECK(fromSampler.maximum() == fromManifest.maximum());
    }
}

TEST_CASE("An empty cloud builds one empty root", "[PointOctree][PointOctreeSampler]") {
    const QVector<SampledNode> nodes = buildSubtree(QVector<QVector3D>(),
                                                    Cell{0, 0, 0, 0},
                                                    QVector3D(0.0f, 0.0f, 0.0f),
                                                    8.0);

    REQUIRE(nodes.size() == 1);
    CHECK(nodes.at(0).points.isEmpty());
    CHECK(nodes.at(0).cell.level == 0);
    CHECK(!hasChildren(nodes.at(0)));
}

TEST_CASE("Duplicate points terminate at the deepest level", "[PointOctree][PointOctreeSampler]") {
    constexpr int kDuplicateCount = 10000;
    constexpr int kLeafMax = 10;

    const QVector3D duplicate(1.0f, 1.0f, 1.0f);
    const QVector<QVector3D> points(kDuplicateCount, duplicate);

    const QVector<SampledNode> nodes = buildSubtree(points,
                                                    Cell{0, 0, 0, 0},
                                                    QVector3D(0.0f, 0.0f, 0.0f),
                                                    8.0,
                                                    kLeafMax);

    //The duplicates share one cell at every level, so the tree is a single chain
    REQUIRE(nodes.size() == kMaxLevel + 1);
    CHECK(allPoints(nodes).size() == kDuplicateCount);

    const SampledNode& deepest = nodes.constLast();
    CHECK(deepest.cell.level == kMaxLevel);
    CHECK(!hasChildren(deepest));
    CHECK(deepest.points.size() == kDuplicateCount - 1);

    //Each level hands its one sampled point up, so only the root keeps one
    CHECK(nodes.at(0).points.size() == 1);
    for(int i = 0; i < kMaxLevel; i++) {
        CHECK(nodes.at(i).cell.level == i);
        CHECK(hasChildren(nodes.at(i)));
        CHECK(nodes.at(i).points.size() <= 1);
    }
}

TEST_CASE("A synthetic passage builds an additive octree", "[PointOctree][PointOctreeSampler]") {
    const QVector<QVector3D> points = passagePoints(kPassagePointCount, kPassageSeed);
    const RootCube root = rootCubeOf(points);

    const QVector<SampledNode> nodes = buildSubtree(points, Cell{0, 0, 0, 0}, root.minimum, root.size);

    REQUIRE(nodes.size() > 1);
    CHECK(nodes.at(0).cell.level == 0);

    //The root samples the passage rather than swallowing it
    CHECK(nodes.at(0).points.size() > 0);
    CHECK(nodes.at(0).points.size() < points.size());

    int deepestLevel = 0;
    for(const SampledNode& node : nodes) {
        deepestLevel = std::max(deepestLevel, node.cell.level);
    }
    CHECK(deepestLevel >= 2);

    //Every input point lands in exactly one node
    CHECK(sortedRounded(allPoints(nodes)) == sortedRounded(points));

    QVector<int> parents(nodes.size(), -1);
    for(int i = 0; i < nodes.size(); i++) {
        const SampledNode& node = nodes.at(i);

        if(hasChildren(node)) {
            //A non-leaf holds at most one point per cell of its own sample grid
            const QBox3D bounds = cellBounds(node.cell, root.minimum, root.size);
            QSet<quint64> cells;
            cells.reserve(node.points.size());
            for(const QVector3D& point : node.points) {
                cells.insert(packedSampleCell(point, bounds));
            }
            CHECK(cells.size() == node.points.size());
        } else {
            CHECK(node.points.size() <= kLeafMaxPoints);
            CHECK(node.points.size() > 0);
        }

        for(int octant = 0; octant < kChildCount; octant++) {
            const int child = node.children.at(octant);
            if(child < 0) {
                continue;
            }

            REQUIRE(child > i);
            REQUIRE(child < nodes.size());
            CHECK(parents.at(child) == -1);
            parents[child] = i;

            const Cell& childCellValue = nodes.at(child).cell;
            const Cell expected = childCell(node.cell, octant);
            CHECK(childCellValue.level == node.cell.level + 1);
            CHECK(childCellValue.x == expected.x);
            CHECK(childCellValue.y == expected.y);
            CHECK(childCellValue.z == expected.z);

            const QBox3D parentBounds = cellBounds(node.cell, root.minimum, root.size);
            const QBox3D childBounds = cellBounds(childCellValue, root.minimum, root.size);
            CHECK(withSlack(parentBounds).contains(childBounds));
        }
    }

    //Depth first with the root at index 0 means every node but the root has a parent
    for(int i = 1; i < nodes.size(); i++) {
        CHECK(parents.at(i) >= 0);
    }
    CHECK(parents.at(0) == -1);

    //Every point of a node is inside that node's cube
    for(const SampledNode& node : nodes) {
        const QBox3D bounds = cellBounds(node.cell, root.minimum, root.size);
        const auto outside = std::find_if(node.points.begin(), node.points.end(),
                                          [grown = withSlack(bounds)](const QVector3D& point) { return !grown.contains(point); });
        CHECK(outside == node.points.end());
    }
}

TEST_CASE("The same cloud builds the same tree twice", "[PointOctree][PointOctreeSampler]") {
    const QVector<QVector3D> points = passagePoints(kPassagePointCount / 4, kPassageSeed);
    const RootCube root = rootCubeOf(points);

    const QVector<SampledNode> first = buildSubtree(points, Cell{0, 0, 0, 0}, root.minimum, root.size, 2000);
    const QVector<SampledNode> second = buildSubtree(points, Cell{0, 0, 0, 0}, root.minimum, root.size, 2000);

    CHECK(sameTree(first, second));
}

namespace {
    //A flat ground grid, as coarse as the root's own sample spacing
    constexpr double kGroundSide = 64.0;
    constexpr double kGroundStep = 0.5;

    //Off the root's mid planes, so the plane lands inside cells instead of on their faces
    constexpr float kGroundHeight = 13.0f;

    //A dense patch, fine enough that a shallow cell holding it is levels off its own spacing
    constexpr double kPatchSide = 0.4;
    constexpr double kPatchStep = 0.01;

    //Small enough that the patch cell stays under it, so the sampler leaves the patch whole
    constexpr int kSpacingLeafMaxPoints = 2000;

    //A node holding fewer points than this has too few neighbors for a stable median
    constexpr qsizetype kMinPointsForSpacing = 64;

    //The pairwise walk is quadratic, and a sample is enough for a median
    constexpr qsizetype kSpacingSampleCount = 200;

    /**
     * How far the median nearest-neighbor spacing of a node's own points may
     * fall below the spacing its level promises. A grid sample keeps one point
     * per cell, and two winners in neighboring cells can sit closer together
     * than a cell, so the floor is a fraction rather than the spacing itself.
     */
    constexpr double kMinSpacingFraction = 0.5;

    QVector<QVector3D> gridPatch(const QVector3D& corner, double side, double step)
    {
        const int perAxis = int(side / step) + 1;

        QVector<QVector3D> points;
        points.reserve(qsizetype(perAxis) * perAxis);
        for(int row = 0; row < perAxis; row++) {
            for(int column = 0; column < perAxis; column++) {
                points.append(corner + QVector3D(float(column * step), float(row * step), 0.0f));
            }
        }
        return points;
    }

    //! The median nearest-neighbor distance among a sample of @a points: the
    //! spacing the node really holds, whatever level the tree calls it.
    double medianNearestNeighbor(const QVector<QVector3D>& points)
    {
        if(points.size() < 2) {
            return 0.0;
        }

        const qsizetype stride = std::max<qsizetype>(1, points.size() / kSpacingSampleCount);

        QVector<double> nearest;
        for(qsizetype i = 0; i < points.size(); i += stride) {
            double best = std::numeric_limits<double>::max();
            for(qsizetype j = 0; j < points.size(); j++) {
                if(i != j) {
                    best = std::min(best, double((points.at(j) - points.at(i)).length()));
                }
            }
            nearest.append(best);
        }

        std::sort(nearest.begin(), nearest.end());
        return nearest.at(nearest.size() / 2);
    }
}

TEST_CASE("Every node holds points spaced for its own level",
          "[PointOctree][PointOctreeSampler][OversizeSprite]") {
    const QVector3D rootMin(0.0f, 0.0f, 0.0f);
    const double rootSize = kGroundSide;

    QVector<QVector3D> points = gridPatch(QVector3D(0.0f, 0.0f, kGroundHeight),
                                          kGroundSide, kGroundStep);
    points += gridPatch(QVector3D(float(kGroundSide * 0.3), float(kGroundSide * 0.3),
                                  kGroundHeight),
                        kPatchSide, kPatchStep);

    const QVector<SampledNode> nodes = buildSubtree(points, Cell{0, 0, 0, 0}, rootMin, rootSize,
                                                    kSpacingLeafMaxPoints);

    cwPointOctreeManifest manifest;
    manifest.rootMin = rootMin;
    manifest.rootSize = rootSize;

    for(const SampledNode& node : nodes) {
        if(node.points.size() < kMinPointsForSpacing) {
            continue;
        }

        const double measured = medianNearestNeighbor(node.points);
        const double promised = manifest.spacing(node.cell.level);

        INFO("level " << node.cell.level << " holds " << node.points.size()
             << " points at " << measured << " m, its level promises " << promised << " m");
        CHECK(measured >= kMinSpacingFraction * promised);
    }
}

namespace {
    //A node cube whose grid spacing is 16 / 128 = 0.125 m
    constexpr double kMeasureNodeSize = 16.0;
    constexpr float kMeasurePlaneHeight = 5.0f;

    //Four grid cells between points: sparser than the node's grid
    constexpr double kSparseStep = 0.5;
    //Under half a grid cell between points: denser than the node's grid
    constexpr double kDenseStep = 0.05;

    /**
     * How far a measured spacing may sit from a regular grid's step. A plane
     * of points measured over its own footprint loses half a step along each
     * edge, which on these grids is a few percent; the rest is room for the
     * estimator to choose its own occupied-extent rule.
     */
    constexpr double kMeasuredSpacingTolerance = 0.1;

    QBox3D measureNodeBounds()
    {
        const float size = float(kMeasureNodeSize);
        return QBox3D(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(size, size, size));
    }

    double gridSpacingOf(const QBox3D& bounds)
    {
        return double(bounds.size().x()) / kSampleGridResolution;
    }
}

TEST_CASE("A node sparser than its grid measures the spacing its points really have",
          "[PointOctree][PointOctreeSampler]") {
    const QBox3D bounds = measureNodeBounds();
    REQUIRE(kSparseStep > gridSpacingOf(bounds));

    SECTION("a plane across the whole node") {
        const QVector<QVector3D> points = gridPatch(QVector3D(0.0f, 0.0f, kMeasurePlaneHeight),
                                                    kMeasureNodeSize, kSparseStep);
        const double measured = measureNodeSpacing(points);
        INFO("measured " << measured << " m for a " << kSparseStep << " m step, grid "
             << gridSpacingOf(bounds) << " m");
        CHECK(std::abs(measured - kSparseStep) <= kMeasuredSpacingTolerance * kSparseStep);
    }

    SECTION("a plane over one quarter of the node's footprint") {
        //The spacing is the points' own, so the empty three quarters of the
        //node's footprint are left out of the area it is measured over
        const QVector<QVector3D> points = gridPatch(QVector3D(0.0f, 0.0f, kMeasurePlaneHeight),
                                                    kMeasureNodeSize * 0.5, kSparseStep);
        const double measured = measureNodeSpacing(points);
        INFO("measured " << measured << " m for a " << kSparseStep << " m step over a quarter");
        CHECK(std::abs(measured - kSparseStep) <= kMeasuredSpacingTolerance * kSparseStep);
    }
}

namespace {
    /**
     * How far a measured spacing may sit from the true step on a footprint
     * that fills little of its bounding rectangle. Footprint cells that catch a
     * strip's edges count their whole area, so these read a little sparse; the
     * bounding rectangle alone reads the strip below 2.1 times and the
     * clusters 3.2 times their step.
     */
    constexpr double kFootprintShapeTolerance = 0.35;

    //A road two meters wide, five rows of points across
    constexpr double kStripHalfWidth = 1.0;

    //Two patches of seven by seven points, in opposite corners of the node
    constexpr double kClusterSide = 3.0;

    //! A strip of points kSparseStep apart running corner to corner across
    //! the node's footprint
    QVector<QVector3D> diagonalStrip()
    {
        const double length = kMeasureNodeSize * std::sqrt(2.0);
        const double inverseRootTwo = 1.0 / std::sqrt(2.0);
        const int alongCount = int(length / kSparseStep) + 1;
        const int acrossHalf = int(kStripHalfWidth / kSparseStep);

        QVector<QVector3D> points;
        for(int along = 0; along < alongCount; along++) {
            for(int across = -acrossHalf; across <= acrossHalf; across++) {
                const double t = along * kSparseStep;
                const double offset = across * kSparseStep;
                const double x = (t - offset) * inverseRootTwo;
                const double y = (t + offset) * inverseRootTwo;
                if(x >= 0.0 && x <= kMeasureNodeSize && y >= 0.0 && y <= kMeasureNodeSize) {
                    points.append(QVector3D(float(x), float(y), kMeasurePlaneHeight));
                }
            }
        }
        return points;
    }
}

TEST_CASE("A footprint that fills little of its bounding rectangle measures its points' own spacing",
          "[PointOctree][PointOctreeSampler]") {
    SECTION("a strip running diagonally across the node") {
        const QVector<QVector3D> points = diagonalStrip();
        const double measured = measureNodeSpacing(points);
        INFO(points.size() << " points measured " << measured << " m for a " << kSparseStep
             << " m step");
        CHECK(std::abs(measured - kSparseStep) <= kFootprintShapeTolerance * kSparseStep);
    }

    SECTION("two clusters in opposite corners of the node") {
        const double farCorner = kMeasureNodeSize - kClusterSide;
        QVector<QVector3D> points = gridPatch(QVector3D(0.0f, 0.0f, kMeasurePlaneHeight),
                                              kClusterSide, kSparseStep);
        points += gridPatch(QVector3D(float(farCorner), float(farCorner), kMeasurePlaneHeight),
                            kClusterSide, kSparseStep);
        const double measured = measureNodeSpacing(points);
        INFO(points.size() << " points measured " << measured << " m for a " << kSparseStep
             << " m step");
        CHECK(std::abs(measured - kSparseStep) <= kFootprintShapeTolerance * kSparseStep);
    }
}

TEST_CASE("A node denser than its grid measures under its grid, so measured spacing keeps the grid",
          "[PointOctree][PointOctreeSampler]") {
    const QBox3D bounds = measureNodeBounds();
    const double grid = gridSpacingOf(bounds);
    REQUIRE(kDenseStep < grid);

    //A patch small enough to stay well under a leaf's worth of points
    constexpr double kDensePatchSide = 2.0;
    const QVector<QVector3D> points = gridPatch(QVector3D(1.0f, 1.0f, kMeasurePlaneHeight),
                                                kDensePatchSide, kDenseStep);
    const double measured = measureNodeSpacing(points);

    INFO("measured " << measured << " m for a " << kDenseStep << " m step, grid " << grid << " m");
    CHECK(measured > 0.0);
    CHECK(std::abs(measured - kDenseStep) <= kMeasuredSpacingTolerance * kDenseStep);
    CHECK(measured < grid);
}

TEST_CASE("Nothing to measure measures zero", "[PointOctree][PointOctreeSampler]") {
    CHECK(measureNodeSpacing({}) == 0.0f);
}

namespace {
    //A root whose own grid cells are 8 / 128 = 0.0625 m
    constexpr double kSplitRootSize = 8.0;
    //Points two to a root grid cell, a sixth of a cell apart, so the root's
    //own sample takes one of each pair and leaves the other behind
    constexpr float kPairOffset = 0.01f;
    constexpr int kPairCount = 12;
    constexpr int kSplitLeafMax = 1000;

    //The pairs sit one per root cell along a diagonal, in different octants,
    //so leftovers that split would make real children
    QVector<QVector3D> pairedPoints()
    {
        QVector<QVector3D> points;
        const float step = float(kSplitRootSize) / float(kPairCount);
        for(int i = 0; i < kPairCount; i++) {
            const float along = (float(i) + 0.5f) * step;
            const QVector3D first(along, along, along);
            points.append(first);
            points.append(first + QVector3D(kPairOffset, 0.0f, 0.0f));
        }
        return points;
    }

    QVector<SampledNode> buildPaired(int minSplitPoints)
    {
        return buildSubtree(pairedPoints(), Cell{0, 0, 0, 0}, QVector3D(0.0f, 0.0f, 0.0f),
                            kSplitRootSize, kSplitLeafMax, minSplitPoints);
    }
}

TEST_CASE("Leftovers under the minimum to split stay in their node as a leaf",
          "[PointOctree][PointOctreeSampler]") {
    const QVector<QVector3D> points = pairedPoints();
    REQUIRE(points.size() <= kSplitLeafMax);

    //The root's grid takes one point of every pair, so as many are left over
    const int leftovers = kPairCount;

    SECTION("fewer leftovers than the minimum keep the whole cloud in the root") {
        const QVector<SampledNode> nodes = buildPaired(leftovers + 1);
        REQUIRE(nodes.size() == 1);
        CHECK(!hasChildren(nodes.at(0)));
        CHECK(sortedRounded(nodes.at(0).points) == sortedRounded(points));
        CHECK(nodes.at(0).points.size() <= kSplitLeafMax);
    }

    SECTION("as many leftovers as the minimum still split into children") {
        const QVector<SampledNode> nodes = buildPaired(leftovers);
        REQUIRE(nodes.size() > 1);
        CHECK(hasChildren(nodes.at(0)));
        CHECK(nodes.at(0).points.size() == kPairCount);
        CHECK(sortedRounded(allPoints(nodes)) == sortedRounded(points));
    }

    SECTION("no minimum splits every leftover, as the sampler always did") {
        const QVector<SampledNode> nodes = buildPaired(0);
        REQUIRE(nodes.size() > 1);
        CHECK(nodes.at(0).points.size() == kPairCount);
        CHECK(sortedRounded(allPoints(nodes)) == sortedRounded(points));
    }
}

TEST_CASE("A node over the leaf cap splits whatever the minimum to split",
          "[PointOctree][PointOctreeSampler]") {
    //Under the cap's worth of points no node samples first, so there are no
    //leftovers for the minimum to keep, and the tree is the one it always was
    constexpr int kTinyLeafMax = kPairCount;
    const QVector<QVector3D> points = pairedPoints();
    REQUIRE(points.size() > kTinyLeafMax);

    const QVector<SampledNode> unbounded =
        buildSubtree(points, Cell{0, 0, 0, 0}, QVector3D(0.0f, 0.0f, 0.0f), kSplitRootSize,
                     kTinyLeafMax, 0);
    const QVector<SampledNode> bounded =
        buildSubtree(points, Cell{0, 0, 0, 0}, QVector3D(0.0f, 0.0f, 0.0f), kSplitRootSize,
                     kTinyLeafMax, kTinyLeafMax);

    REQUIRE(bounded.size() > 1);
    CHECK(hasChildren(bounded.at(0)));
    CHECK(bounded.at(0).points.size() < points.size());
    for(const SampledNode& node : bounded) {
        CHECK(node.points.size() <= kTinyLeafMax);
    }
    CHECK(sortedRounded(allPoints(bounded)) == sortedRounded(points));
    CHECK(hasChildren(unbounded.at(0)));
}

TEST_CASE("The minimum to split prunes a passage's deep leaves and keeps every point under the leaf cap",
          "[PointOctree][PointOctreeSampler]") {
    const QVector<QVector3D> points = passagePoints(kPassagePointCount, kPassageSeed);
    const RootCube root = rootCubeOf(points);

    const QVector<SampledNode> unbounded =
        buildSubtree(points, Cell{0, 0, 0, 0}, root.minimum, root.size, kLeafMaxPoints, 0);
    const QVector<SampledNode> bounded =
        buildSubtree(points, Cell{0, 0, 0, 0}, root.minimum, root.size, kLeafMaxPoints,
                     kMinSplitPoints);

    INFO("nodes without a minimum " << unbounded.size() << ", with " << kMinSplitPoints << " "
         << bounded.size());
    CHECK(bounded.size() < unbounded.size());

    //Every input point still lands in exactly one node, and a kept leaf holds
    //no more than the leaf cap the renderer sizes its buffers by
    CHECK(sortedRounded(allPoints(bounded)) == sortedRounded(points));
    for(const SampledNode& node : bounded) {
        CHECK(node.points.size() <= kLeafMaxPoints);
    }

    //Pruning only ever stops a split: no level gains nodes
    const auto deepest = [](const QVector<SampledNode>& nodes) {
        int level = 0;
        for(const SampledNode& node : nodes) {
            level = std::max(level, node.cell.level);
        }
        return level;
    };
    CHECK(deepest(bounded) <= deepest(unbounded));
}

TEST_CASE("buildSubtree defaults to kMinSplitPoints", "[PointOctree][PointOctreeSampler]") {
    const QVector<QVector3D> points = passagePoints(kPassagePointCount / 4, kPassageSeed);
    const RootCube root = rootCubeOf(points);

    const QVector<SampledNode> byDefault =
        buildSubtree(points, Cell{0, 0, 0, 0}, root.minimum, root.size);
    const QVector<SampledNode> named =
        buildSubtree(points, Cell{0, 0, 0, 0}, root.minimum, root.size, kLeafMaxPoints,
                     kMinSplitPoints);
    CHECK(sameTree(byDefault, named));
    CHECK(kMinSplitPoints > 0);
    CHECK(kMinSplitPoints <= kLeafMaxPoints);
}

// ---- WP13: per-point native spacing -------------------------------------------

namespace {
    //! A leaf cap past every fixture's point count, so a root samples first
    constexpr int kLocalLeafMaxPoints = 100000;

    //! The plane fixtures: kLocalPlaneStep apart over a kLocalPlaneRootSize
    //! root at the origin, so the root's grid (0.5 m) is wider than the data
    //! and level 1's (0.25 m) finer, and the plane sits mid-cell in z
    constexpr double kLocalPlaneRootSize = 64.0;
    constexpr double kLocalPlaneStep = 0.3;
    constexpr float kLocalPlaneHeight = 16.25f;
    //! The plan's tolerance for a plane point's native spacing
    constexpr double kNativeSpacingTolerance = 0.25;
    //! Points this close to the data's own edge have neighbors on one side only
    constexpr double kLocalPlaneEdgeMargin = 1.0;

    //! The streak: groups of kStreakMinCopies to kStreakMaxCopies bit-identical
    //! points at kStreakGroups of the plane's own positions along one row near
    //! a corner, the duplicate returns a 1 cm LAZ lattice stacks up
    constexpr int kStreakRow = 20;
    constexpr int kStreakFirstColumn = 10;
    constexpr int kStreakGroups = 100;
    constexpr int kStreakMinCopies = 4;
    constexpr int kStreakMaxCopies = 8;

    //! Every point of the duplicated plane four times over
    constexpr int kPlaneCopies = 4;

    //! The decoded spacings of two points that see the same distinct
    //! neighbors agree to float rounding of their coordinates
    constexpr double kSameNeighborhoodTolerance = 1e-3;

    //! The widest native spacing a point of a node at @a level may carry,
    //! spelled out rather than read from localSpacingCap() so the cap is checked
    double nativeSpacingCap(double rootSize, int level)
    {
        return std::min(kMaxLocalSpacingGridRatio * levelGridSpacing(rootSize, level),
                        kMaxLocalSpacingUnits);
    }

    //! The regular plane, one point per kLocalPlaneStep over the root's footprint
    QVector<QVector3D> nativePlanePoints()
    {
        const int perAxis = int(kLocalPlaneRootSize / kLocalPlaneStep);
        QVector<QVector3D> points;
        points.reserve(qsizetype(perAxis) * perAxis);
        for(int row = 0; row < perAxis; row++) {
            for(int column = 0; column < perAxis; column++) {
                points.append(QVector3D(float((column + 0.5) * kLocalPlaneStep),
                                        float((row + 0.5) * kLocalPlaneStep),
                                        kLocalPlaneHeight));
            }
        }
        return points;
    }

    //! Whether @a point sits at least kLocalPlaneEdgeMargin inside the plane
    bool insidePlane(const QVector3D& point)
    {
        const double low = kLocalPlaneEdgeMargin;
        const double high = kLocalPlaneRootSize - kLocalPlaneEdgeMargin;
        return point.x() >= low && point.x() <= high && point.y() >= low && point.y() <= high;
    }

    //! The plane with the streak's extra copies appended
    QVector<QVector3D> streakPlanePoints()
    {
        QVector<QVector3D> points = nativePlanePoints();
        const int perAxis = int(kLocalPlaneRootSize / kLocalPlaneStep);
        const int copyRange = kStreakMaxCopies - kStreakMinCopies + 1;
        for(int group = 0; group < kStreakGroups; group++) {
            const QVector3D position = points.at(kStreakRow * perAxis + kStreakFirstColumn + group);
            //The plane's own point is the first of the group
            const int extra = kStreakMinCopies + group % copyRange - 1;
            for(int copy = 0; copy < extra; copy++) {
                points.append(position);
            }
        }
        return points;
    }

    //! Whether @a point sits on one of the streak's positions
    bool onStreak(const QVector3D& point)
    {
        const double row = double(point.y()) / kLocalPlaneStep - 0.5;
        const double column = double(point.x()) / kLocalPlaneStep - 0.5;
        return std::lround(row) == kStreakRow && std::abs(row - kStreakRow) < 0.1
               && column > kStreakFirstColumn - 0.1
               && column < kStreakFirstColumn + kStreakGroups - 0.9;
    }

    //! A position's key, exact for points that are bit-identical
    quint64 positionKey(const QVector3D& point)
    {
        const quint64 x = std::bit_cast<quint32>(point.x());
        const quint64 y = std::bit_cast<quint32>(point.y());
        return (x << 32) | y;
    }

    //! One coded point: where it is, the level of the node holding it, and
    //! the native spacing its code decodes to
    struct CodedPoint {
        QVector3D point;
        int level = 0;
        quint16 code = 0;
        double spacing = 0.0;
    };

    QVector<CodedPoint> codedPoints(const QVector<SampledNode>& nodes,
                                    const QVector<QVector<quint16>>& codes, double rootSize)
    {
        REQUIRE(codes.size() == nodes.size());
        QVector<CodedPoint> coded;
        for(qsizetype n = 0; n < nodes.size(); n++) {
            const SampledNode& node = nodes.at(n);
            REQUIRE(codes.at(n).size() == node.points.size());
            const double grid = levelGridSpacing(rootSize, node.cell.level);
            for(qsizetype i = 0; i < node.points.size(); i++) {
                const quint16 code = codes.at(n).at(i);
                coded.append(CodedPoint {node.points.at(i), node.cell.level, code,
                                         decodeLocalSpacing(code, grid)});
            }
        }
        return coded;
    }

    QVector<CodedPoint> estimatedPlane(const QVector<QVector3D>& points)
    {
        const QVector<SampledNode> nodes = buildSubtree(points, Cell {0, 0, 0, 0}, QVector3D(),
                                                        kLocalPlaneRootSize, kLocalLeafMaxPoints);
        REQUIRE(nodes.size() > 1);
        return codedPoints(nodes, estimateLocalSpacing(nodes, QVector3D(), kLocalPlaneRootSize),
                           kLocalPlaneRootSize);
    }

    double relativeError(double value, double expected)
    {
        return std::abs(value - expected) / expected;
    }
}

TEST_CASE("Native spacing estimates one code per point of every node, every one measured",
          "[PointOctree][PointOctreeSampler]") {
    const QVector<SampledNode> nodes = buildSubtree(nativePlanePoints(), Cell {0, 0, 0, 0},
                                                    QVector3D(), kLocalPlaneRootSize,
                                                    kLocalLeafMaxPoints);
    REQUIRE(nodes.size() > 1);

    const QVector<QVector<quint16>> codes =
        estimateLocalSpacing(nodes, QVector3D(), kLocalPlaneRootSize);
    REQUIRE(codes.size() == nodes.size());

    int unmeasured = 0;
    for(qsizetype n = 0; n < nodes.size(); n++) {
        REQUIRE(codes.at(n).size() == nodes.at(n).points.size());
        unmeasured += int(std::count(codes.at(n).cbegin(), codes.at(n).cend(),
                                     kUnmeasuredLocalSpacingCode));
    }
    CHECK(unmeasured == 0);
}

TEST_CASE("A sparse 0.3 m plane's native spacing is its data spacing at every point of every level",
          "[PointOctree][PointOctreeSampler]") {
    const QVector<CodedPoint> coded = estimatedPlane(nativePlanePoints());

    //The root holds one point a 0.5 m cell and level 1 the rest: an absolute
    //density, so both read the data's 0.3 m rather than their own grid
    QSet<int> levels;
    int checked = 0;
    int outside = 0;
    double worst = 0.0;
    for(const CodedPoint& point : coded) {
        if(!insidePlane(point.point)) {
            continue;
        }
        levels.insert(point.level);
        const double error = relativeError(point.spacing, kLocalPlaneStep);
        worst = std::max(worst, error);
        outside += error > kNativeSpacingTolerance ? 1 : 0;
        checked++;
    }
    INFO(checked << " interior points over " << levels.size() << " levels, " << outside
         << " off by more than " << kNativeSpacingTolerance << ", worst " << worst);
    REQUIRE(levels.size() > 1);
    CHECK(outside == 0);
}

TEST_CASE("A duplicate streak on a sparse 0.3 m plane codes to the plane's spacing, and so does the plane around it",
          "[PointOctree][PointOctreeSampler]") {
    const QVector<CodedPoint> coded = estimatedPlane(streakPlanePoints());

    //A sphere count reads a stack of identical returns as a dense patch; the
    //k-th distinct neighbor counts each position once, so the streak and the
    //plane beside it read the plane's spacing. A copy pushed into a node whose
    //grid is fine enough codes to that node's cap instead.
    int planeChecked = 0;
    int planeOutside = 0;
    int streakChecked = 0;
    int streakUncapped = 0;
    int streakOutside = 0;
    double worstPlane = 0.0;
    double worstStreak = 0.0;
    int deepestStreakLevel = 0;
    for(const CodedPoint& point : coded) {
        if(!insidePlane(point.point)) {
            continue;
        }
        const double cap = nativeSpacingCap(kLocalPlaneRootSize, point.level);
        const double expected = std::min(kLocalPlaneStep, cap);
        const double error = relativeError(point.spacing, expected);
        if(onStreak(point.point)) {
            streakChecked++;
            streakUncapped += cap >= kLocalPlaneStep ? 1 : 0;
            streakOutside += error > kNativeSpacingTolerance ? 1 : 0;
            worstStreak = std::max(worstStreak, error);
            deepestStreakLevel = std::max(deepestStreakLevel, point.level);
        } else {
            planeChecked++;
            planeOutside += error > kNativeSpacingTolerance ? 1 : 0;
            worstPlane = std::max(worstPlane, error);
        }
    }
    INFO("plane: " << planeChecked << " points, " << planeOutside << " outside, worst "
         << worstPlane << "; streak: " << streakChecked << " points (" << streakUncapped
         << " under no cap) down to level " << deepestStreakLevel << ", " << streakOutside
         << " outside, worst " << worstStreak);
    //The streak's copies really do stack up through the levels
    REQUIRE(streakChecked > kStreakGroups * kStreakMinCopies - 1);
    REQUIRE(deepestStreakLevel > 1);
    REQUIRE(streakUncapped > kStreakGroups);
    CHECK(planeOutside == 0);
    CHECK(streakOutside == 0);
}

TEST_CASE("A plane with every point four times over codes the same as the plain plane",
          "[PointOctree][PointOctreeSampler]") {
    const QVector<QVector3D> plain = nativePlanePoints();
    QVector<QVector3D> duplicated;
    duplicated.reserve(plain.size() * kPlaneCopies);
    for(int copy = 0; copy < kPlaneCopies; copy++) {
        duplicated.append(plain);
    }

    QHash<quint64, CodedPoint> plainByPosition;
    for(const CodedPoint& point : estimatedPlane(plain)) {
        plainByPosition.insert(positionKey(point.point), point);
    }
    REQUIRE(plainByPosition.size() == plain.size());

    //Every copy codes what the plain plane's point there does, up to the cap
    //of the node it was pushed into
    int compared = 0;
    int uncapped = 0;
    int differing = 0;
    int differingUncapped = 0;
    for(const CodedPoint& point : estimatedPlane(duplicated)) {
        const auto found = plainByPosition.constFind(positionKey(point.point));
        REQUIRE(found != plainByPosition.constEnd());
        const double cap = nativeSpacingCap(kLocalPlaneRootSize, point.level);
        const bool underCap = cap >= found->spacing * (1.0 + kSameNeighborhoodTolerance);
        const double expected = std::min(found->spacing, cap);
        differing += relativeError(point.spacing, expected) > kSameNeighborhoodTolerance ? 1 : 0;
        if(underCap) {
            uncapped++;
            differingUncapped += point.code != found->code ? 1 : 0;
        }
        compared++;
    }
    INFO(compared << " copies, " << uncapped << " under no cap; " << differing
         << " differ from the plain plane, " << differingUncapped << " of them by code");
    REQUIRE(compared == duplicated.size());
    REQUIRE(uncapped > plain.size());
    CHECK(differing == 0);
    CHECK(differingUncapped == 0);
}

TEST_CASE("A plane point beside a node boundary measures the same as an interior one",
          "[PointOctree][PointOctreeSampler]") {
    const QVector<CodedPoint> coded = estimatedPlane(nativePlanePoints());

    //Level 1's four children meet at x = 32 and y = 32. Their neighbors
    //across the boundary are in the same subtree, so they count.
    const double boundary = kLocalPlaneRootSize * 0.5;
    constexpr double kInteriorDistance = 3.0;
    QVector<double> interior;
    QVector<CodedPoint> beside;
    for(const CodedPoint& point : coded) {
        if(point.level != 1 || !insidePlane(point.point)) {
            continue;
        }
        const double toBoundary = std::min(std::abs(point.point.x() - boundary),
                                           std::abs(point.point.y() - boundary));
        if(toBoundary < kLocalPlaneStep) {
            beside.append(point);
        } else if(toBoundary > kInteriorDistance) {
            interior.append(point.spacing);
        }
    }
    REQUIRE_FALSE(beside.isEmpty());
    REQUIRE_FALSE(interior.isEmpty());
    const double typical = cwMedian(interior);

    int differing = 0;
    double worst = 0.0;
    for(const CodedPoint& point : std::as_const(beside)) {
        const double error = relativeError(point.spacing, typical);
        worst = std::max(worst, error);
        differing += error > kSameNeighborhoodTolerance ? 1 : 0;
    }
    INFO(beside.size() << " points beside the boundary against an interior median of "
         << typical << " m, worst off by " << worst);
    CHECK(differing == 0);
}

TEST_CASE("A node holding dense blobs far apart codes each blob point near the blob spacing, where Measured draws the node at several times it",
          "[PointOctree][PointOctreeSampler]") {
    //The treetop case: a node at level 0 (1 m grid in a 128 m root) holding
    //two 4 x 4 blobs of returns kBlobSpacing apart, one at each end of it
    constexpr double kTreetopRootSize = 128.0;
    constexpr double kBlobSpacing = 0.2;
    constexpr int kBlobSide = 4;
    constexpr std::array<float, 2> kBlobCorners {10.5f, 110.5f};
    //On a 4 x 4 lattice the k-th distinct neighbor of a corner sits sqrt(8)
    //spacings away, so a corner reads sqrt(8) sqrt(pi / 8) = 1.77 spacings,
    //an edge point 1.40 and an interior one 0.89
    constexpr double kLargestBlobPointSpacings = 1.8;
    constexpr double kLargestMedianSpacings = 1.5;
    //Several times
    constexpr double kMeasuredOverMedian = 3.0;

    QVector<QVector3D> points;
    for(float corner : kBlobCorners) {
        for(int dy = 0; dy < kBlobSide; dy++) {
            for(int dx = 0; dx < kBlobSide; dx++) {
                points.append(QVector3D(corner + float(dx * kBlobSpacing),
                                        corner + float(dy * kBlobSpacing), kLocalPlaneHeight));
            }
        }
    }
    const QVector<SampledNode> nodes {SampledNode {Cell {0, 0, 0, 0}, points}};

    const QVector<QVector<quint16>> codes = estimateLocalSpacing(nodes, QVector3D(),
                                                                 kTreetopRootSize);
    QVector<double> spacings;
    for(const CodedPoint& point : codedPoints(nodes, codes, kTreetopRootSize)) {
        INFO("a blob point at (" << point.point.x() << ", " << point.point.y()
             << ") decodes to " << point.spacing << " m");
        CHECK(point.code != kUnmeasuredLocalSpacingCode);
        CHECK(point.spacing <= kLargestBlobPointSpacings * kBlobSpacing);
        spacings.append(point.spacing);
    }
    //What Measured sizes and thins the node by, cw::clod::drawSpacing: the
    //larger of its grid and its measured spacing. measureNodeSpacing alone
    //reads each blob's own density here, since only the blobs' footprint
    //cells are occupied, so it is the grid that holds the node wide. The
    //tile's sparse leaves measured wide themselves (about 1 m under a 0.27 m
    //native spacing); either way Measured draws the node at several times
    //what its points' own spacing is.
    REQUIRE_FALSE(spacings.isEmpty());
    const double typical = cwMedian(spacings);
    const double measured = measureNodeSpacing(points);
    const double measuredNodeSpacing =
        cw::clod::drawSpacing(levelGridSpacing(kTreetopRootSize, 0), measured,
                              cw::pointcloud::SpacingSource::Measured);
    INFO("the node measures " << measured << " m and Measured spaces it at "
         << measuredNodeSpacing << " m; its points' median native spacing is " << typical
         << " m for blobs " << kBlobSpacing << " m apart");
    CHECK(typical <= kLargestMedianSpacings * kBlobSpacing);
    CHECK(measuredNodeSpacing >= kMeasuredOverMedian * typical);
}

TEST_CASE("A lone point caps at kMaxLocalSpacingGridRatio times its own node's grid spacing, and at kMaxLocalSpacingUnits",
          "[PointOctree][PointOctreeSampler]") {
    struct Case {
        double rootSize = 0.0;
        int level = 0;
    };
    //A coarse node, a deep one, and one so coarse that the grid ratio passes
    //the absolute cap
    constexpr double kSmallRoot = 256.0;
    constexpr double kHugeRoot = 4096.0;
    constexpr int kDeepLevel = 5;
    for(const Case& lone : {Case {kSmallRoot, 0}, Case {kSmallRoot, kDeepLevel},
                            Case {kHugeRoot, 0}}) {
        const double cellSize = std::ldexp(lone.rootSize, -lone.level);
        const QVector3D center(float(cellSize * 0.5), float(cellSize * 0.5),
                               float(cellSize * 0.5));
        const QVector<SampledNode> nodes {SampledNode {Cell {lone.level, 0, 0, 0}, {center}}};
        const QVector<CodedPoint> coded =
            codedPoints(nodes, estimateLocalSpacing(nodes, QVector3D(), lone.rootSize),
                        lone.rootSize);
        REQUIRE(coded.size() == 1);
        const double cap = nativeSpacingCap(lone.rootSize, lone.level);
        INFO("root " << lone.rootSize << " level " << lone.level << ": decodes to "
             << coded.constFirst().spacing << " m against a cap of " << cap << " m");
        CHECK(coded.constFirst().code != kUnmeasuredLocalSpacingCode);
        CHECK(relativeError(coded.constFirst().spacing, cap) <= kSameNeighborhoodTolerance);
    }
}

TEST_CASE("A dense stack codes to kMinLocalSpacingUnits", "[PointOctree][PointOctreeSampler]") {
    //Points 1 mm apart, far under the absolute floor
    constexpr int kStackSide = 20;
    constexpr float kStackStep = 0.001f;
    constexpr double kStackRootSize = 256.0;
    const QVector3D corner(64.5f, 64.5f, kLocalPlaneHeight);
    QVector<QVector3D> stack;
    for(int i = 0; i < kStackSide * kStackSide; i++) {
        stack.append(corner + QVector3D(kStackStep * float(i % kStackSide),
                                        kStackStep * float(i / kStackSide), 0.0f));
    }
    const QVector<SampledNode> nodes {SampledNode {Cell {0, 0, 0, 0}, stack}};
    const QVector<CodedPoint> coded =
        codedPoints(nodes, estimateLocalSpacing(nodes, QVector3D(), kStackRootSize),
                    kStackRootSize);
    REQUIRE(coded.size() == stack.size());
    int offFloor = 0;
    double widest = 0.0;
    for(const CodedPoint& point : coded) {
        offFloor += point.code != encodeLocalSpacing(kMinLocalSpacingUnits)
                            || relativeError(point.spacing, kMinLocalSpacingUnits)
                                   > kSameNeighborhoodTolerance
                        ? 1 : 0;
        widest = std::max(widest, point.spacing);
    }
    INFO(offFloor << " of " << coded.size() << " points off the floor, widest " << widest << " m");
    CHECK(offFloor == 0);
}
