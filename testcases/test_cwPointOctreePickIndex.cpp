// The two level box index a pick descends: leaves of kPickLeafPoints
// consecutive points, groups of kPickGroupLeaves consecutive leaves, over the
// Morton ordered payload cw::octree::quantizeAll writes.

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QBox3D>
#include <QByteArray>
#include <QVector>
#include <QVector3D>
#include <QtEndian>

//Our includes
#include "cwPointOctree.h"
#include "cwPointOctreePickIndex.h"

using namespace cw::octree;

namespace {

    constexpr float kNodeSize = 64.0f;
    using cw::octree::kAxisCount;

    //! Enough points for many leaves and more than one group
    constexpr int kManyPoints = 5000;

    QBox3D unitNode()
    {
        return QBox3D(QVector3D(0.0f, 0.0f, 0.0f),
                      QVector3D(kNodeSize, kNodeSize, kNodeSize));
    }

    //! @a count points spread through the node by a repeatable sequence
    QVector<QVector3D> scatteredPoints(int count)
    {
        constexpr quint32 kSeed = 424242u;
        constexpr quint32 kMultiplier = 1664525u;
        constexpr quint32 kIncrement = 1013904223u;
        constexpr quint32 kFractionBits = 24;

        quint32 random = kSeed;
        const auto nextAxis = [&random]() {
            random = random * kMultiplier + kIncrement;
            return float(double(random >> 8) / double(1u << kFractionBits) * double(kNodeSize));
        };

        QVector<QVector3D> points;
        points.reserve(count);
        for(int i = 0; i < count; i++) {
            points.append(QVector3D(nextAxis(), nextAxis(), nextAxis()));
        }
        return points;
    }

    //! The quantized axes of one point of a payload
    void readPoint(const QByteArray& bytes, qsizetype index, quint16 axes[kAxisCount])
    {
        constexpr int kAxisBytes = int(sizeof(quint16));
        const char* point = bytes.constData() + index * kBytesPerPoint;
        for(int axis = 0; axis < kAxisCount; axis++) {
            axes[axis] = qFromLittleEndian<quint16>(point + axis * kAxisBytes);
        }
    }

    bool boxHolds(const cwPointOctreePickIndex::QuantizedBox& box, const quint16 axes[kAxisCount])
    {
        for(int axis = 0; axis < kAxisCount; axis++) {
            if(axes[axis] < box.min[axis] || axes[axis] > box.max[axis]) {
                return false;
            }
        }
        return true;
    }

    bool boxHolds(const cwPointOctreePickIndex::QuantizedBox& box,
                  const cwPointOctreePickIndex::QuantizedBox& child)
    {
        for(int axis = 0; axis < kAxisCount; axis++) {
            if(child.min[axis] < box.min[axis] || child.max[axis] > box.max[axis]) {
                return false;
            }
        }
        return true;
    }
}

TEST_CASE("An empty payload gives an empty index", "[PointOctreePickIndex]") {
    const cwPointOctreePickIndex index = cwPointOctreePickIndex::build(QByteArray());

    CHECK(index.isEmpty());
    CHECK(index.leaves.isEmpty());
    CHECK(index.groups.isEmpty());
    CHECK(index.byteSize() == 0);
    CHECK(cwPointOctreePickIndex::estimatedBytes(0) == 0);
}

TEST_CASE("One point past a leaf opens a second leaf", "[PointOctreePickIndex]") {
    const QBox3D node = unitNode();
    const QByteArray bytes = quantizeAll(scatteredPoints(kPickLeafPoints + 1), node);

    const cwPointOctreePickIndex index = cwPointOctreePickIndex::build(bytes);

    CHECK_FALSE(index.isEmpty());
    CHECK(index.leaves.size() == 2);
    CHECK(index.groups.size() == 1);
}

TEST_CASE("Leaf and group counts follow the payload's point count",
          "[PointOctreePickIndex]") {
    const QBox3D node = unitNode();

    const QVector<int> counts {1, kPickLeafPoints, kPickLeafPoints * kPickGroupLeaves,
                               kPickLeafPoints * kPickGroupLeaves + 1,
                               kManyPoints};

    for(const int count : counts) {
        const QByteArray bytes = quantizeAll(scatteredPoints(count), node);
        const cwPointOctreePickIndex index = cwPointOctreePickIndex::build(bytes);

        const qsizetype leaves = (count + kPickLeafPoints - 1) / kPickLeafPoints;
        const qsizetype groups = (leaves + kPickGroupLeaves - 1) / kPickGroupLeaves;

        CHECK(index.leaves.size() == leaves);
        CHECK(index.groups.size() == groups);

        //The streamer sizes a load before it runs, so its estimate is the truth
        CHECK(cwPointOctreePickIndex::estimatedBytes(bytes.size()) == index.byteSize());
    }
}

TEST_CASE("byteSize is what the two vectors hold", "[PointOctreePickIndex]") {
    const QBox3D node = unitNode();
    const QByteArray bytes = quantizeAll(scatteredPoints(kManyPoints), node);
    const cwPointOctreePickIndex index = cwPointOctreePickIndex::build(bytes);

    const qint64 boxBytes = qint64(sizeof(cwPointOctreePickIndex::QuantizedBox));
    CHECK(index.byteSize() == (index.leaves.size() + index.groups.size()) * boxBytes);

    //A little over 2 % of the points it indexes
    CHECK(index.byteSize() < bytes.size() / 20);
}

TEST_CASE("Every point is inside its leaf and every leaf inside its group",
          "[PointOctreePickIndex]") {
    const QBox3D node = unitNode();
    const QVector<QVector3D> points = scatteredPoints(kManyPoints);
    const QByteArray bytes = quantizeAll(points, node);

    const cwPointOctreePickIndex index = cwPointOctreePickIndex::build(bytes);
    REQUIRE(index.leaves.size() > kPickGroupLeaves);

    const qsizetype count = bytes.size() / kBytesPerPoint;
    REQUIRE(count == points.size());

    for(qsizetype i = 0; i < count; i++) {
        quint16 axes[kAxisCount] = {0, 0, 0};
        readPoint(bytes, i, axes);

        const qsizetype leaf = i / kPickLeafPoints;
        REQUIRE(leaf < index.leaves.size());
        CHECK(boxHolds(index.leaves.at(leaf), axes));
    }

    for(qsizetype leaf = 0; leaf < index.leaves.size(); leaf++) {
        const qsizetype group = leaf / kPickGroupLeaves;
        REQUIRE(group < index.groups.size());
        CHECK(boxHolds(index.groups.at(group), index.leaves.at(leaf)));
    }
}
