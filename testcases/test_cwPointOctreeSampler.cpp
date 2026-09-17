//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSet>
#include <QVector>
#include <QVector3D>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>

//Our includes
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwPointOctreeSampler.h"

using namespace cw::octree;

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
