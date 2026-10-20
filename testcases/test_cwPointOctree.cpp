//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QVector3D>
#include <QtEndian>

//Std includes
#include <cmath>
#include <numbers>

//Our includes
#include "cavewhere.pb.h"
#include "cwDiskCacher.h"
#include "cwImageProvider.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"

namespace {
    constexpr double kNodeSize = 16.0;
    constexpr float kEpsilon = 1e-4f;

    QBox3D unitNode()
    {
        return QBox3D(QVector3D(0.0f, 0.0f, 0.0f),
                      QVector3D(static_cast<float>(kNodeSize),
                                static_cast<float>(kNodeSize),
                                static_cast<float>(kNodeSize)));
    }

    //A repeatable pseudo-random sequence, so a failure is reproducible
    class RandomSequence
    {
    public:
        explicit RandomSequence(quint32 seed) : m_state(seed) {}

        quint32 next()
        {
            constexpr quint32 kMultiplier = 1664525u;
            constexpr quint32 kIncrement = 1013904223u;
            m_state = m_state * kMultiplier + kIncrement;
            return m_state;
        }

    private:
        quint32 m_state;
    };

    //The little-endian uint16 of one axis (0 = x, 3 = reserved) of one point
    quint16 readAxis(const QByteArray& bytes, int point, int axis)
    {
        const qsizetype offset = point * cw::octree::kBytesPerPoint + axis * qsizetype(sizeof(quint16));
        return qFromLittleEndian<quint16>(bytes.constData() + offset);
    }

    //A three level manifest: root, two children, and one grandchild
    cwPointOctreeManifest threeLevelManifest()
    {
        cwPointOctreeManifest manifest;
        manifest.rootMin = QVector3D(-8.0f, -4.0f, 2.0f);
        manifest.rootSize = kNodeSize;
        manifest.pointCount = 1234;
        manifest.bboxMin = QVector3D(-7.5f, -3.5f, 2.5f);
        manifest.bboxMax = QVector3D(6.0f, 7.0f, 15.0f);
        manifest.meanSpacingXY = 0.125f;
        manifest.fingerprint = QStringLiteral("abc123");

        cwPointOctreeNode root;
        root.level = 0;
        root.pointCount = 100;
        root.byteSize = 100 * cw::octree::kBytesPerPoint;
        root.children[3] = 1;
        root.children[5] = 2;

        cwPointOctreeNode child3;
        child3.level = 1;
        child3.x = 0;
        child3.y = 1;
        child3.z = 1;
        child3.pointCount = 700;
        child3.byteSize = 700 * cw::octree::kBytesPerPoint;
        child3.children[7] = 3;

        cwPointOctreeNode child5;
        child5.level = 1;
        child5.x = 1;
        child5.y = 0;
        child5.z = 1;
        child5.pointCount = 234;
        child5.byteSize = 234 * cw::octree::kBytesPerPoint;

        cwPointOctreeNode grandChild;
        grandChild.level = 2;
        grandChild.x = 1;
        grandChild.y = 3;
        grandChild.z = 3;
        grandChild.pointCount = 200;
        grandChild.byteSize = 200 * cw::octree::kBytesPerPoint;

        manifest.nodes = {root, child3, child5, grandChild};
        return manifest;
    }

    //The deepest level the sprite-spacing check walks
    constexpr int kSpineMaxLevel = 5;

    //A single chain of nodes, one per level 0..kSpineMaxLevel, each the first
    //octant of the one above it
    cwPointOctreeManifest spineManifest()
    {
        cwPointOctreeManifest manifest;
        manifest.rootMin = QVector3D(-8.0f, -4.0f, 2.0f);
        manifest.rootSize = kNodeSize;
        manifest.pointCount = kSpineMaxLevel + 1;
        manifest.bboxMin = manifest.rootMin;
        manifest.bboxMax = manifest.rootMin + QVector3D(static_cast<float>(kNodeSize),
                                                        static_cast<float>(kNodeSize),
                                                        static_cast<float>(kNodeSize));
        manifest.meanSpacingXY = 0.125f;
        manifest.fingerprint = QStringLiteral("spine");

        for(int level = 0; level <= kSpineMaxLevel; level++) {
            cwPointOctreeNode node;
            node.level = level;
            node.pointCount = 1;
            node.byteSize = cw::octree::kBytesPerPoint;
            if(level < kSpineMaxLevel) {
                node.children[0] = level + 1;
            }
            manifest.nodes.append(node);
        }

        return manifest;
    }

    CavewhereProto::PointOctreeManifest parseManifest(const QByteArray& bytes)
    {
        CavewhereProto::PointOctreeManifest proto;
        REQUIRE(proto.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size())));
        return proto;
    }

    std::optional<cwPointOctreeManifest> deserializeProto(const CavewhereProto::PointOctreeManifest& proto)
    {
        const std::string bytes = proto.SerializeAsString();
        return cwPointOctreeManifest::deserialize(QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size())));
    }

    //An extra node that nothing points at, so only its own fields decide the outcome
    CavewhereProto::PointOctreeNode* addOrphan(CavewhereProto::PointOctreeManifest* proto)
    {
        CavewhereProto::PointOctreeNode* orphan = proto->add_nodes();
        for(int octant = 0; octant < cw::octree::kChildCount; octant++) {
            orphan->add_children(-1);
        }
        return orphan;
    }

    QString writeFile(const QString& path, const QByteArray& contents)
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
        file.close();
        return path;
    }
}

TEST_CASE("Quantized points round-trip inside the node", "[PointOctree]") {
    const QBox3D node = unitNode();
    const double tolerance = kNodeSize / cw::octree::kQuantMax;

    const QVector<QVector3D> points {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.25f, 7.75f, 15.5f),
        QVector3D(8.0f, 8.0f, 8.0f),
        QVector3D(16.0f, 16.0f, 16.0f)
    };

    for(const QVector3D& point : points) {
        const QVector3D result = cw::octree::dequantize(cw::octree::quantize(point, node), node);
        CHECK(std::abs(result.x() - point.x()) <= tolerance);
        CHECK(std::abs(result.y() - point.y()) <= tolerance);
        CHECK(std::abs(result.z() - point.z()) <= tolerance);
    }
}

TEST_CASE("Node corners quantize to the ends of the range", "[PointOctree]") {
    const QBox3D node = unitNode();

    const cw::octree::QuantizedPoint minimum = cw::octree::quantize(node.minimum(), node);
    CHECK(minimum.x == 0);
    CHECK(minimum.y == 0);
    CHECK(minimum.z == 0);
    CHECK(minimum.reserved == 0);

    const cw::octree::QuantizedPoint maximum = cw::octree::quantize(node.maximum(), node);
    CHECK(maximum.x == cw::octree::kQuantMax);
    CHECK(maximum.y == cw::octree::kQuantMax);
    CHECK(maximum.z == cw::octree::kQuantMax);
}

TEST_CASE("A point outside the node clamps to the closest face", "[PointOctree]") {
    const QBox3D node = unitNode();

    const cw::octree::QuantizedPoint below = cw::octree::quantize(QVector3D(-100.0f, -1.0f, 8.0f), node);
    CHECK(below.x == 0);
    CHECK(below.y == 0);
    CHECK(below.z > 0);

    const cw::octree::QuantizedPoint above = cw::octree::quantize(QVector3D(100.0f, 20.0f, 8.0f), node);
    CHECK(above.x == cw::octree::kQuantMax);
    CHECK(above.y == cw::octree::kQuantMax);
}

TEST_CASE("A degenerate axis quantizes to zero", "[PointOctree]") {
    const QBox3D flat(QVector3D(0.0f, 0.0f, 4.0f), QVector3D(16.0f, 16.0f, 4.0f));

    const cw::octree::QuantizedPoint quantized = cw::octree::quantize(QVector3D(8.0f, 8.0f, 4.0f), flat);
    CHECK(quantized.z == 0);
    CHECK(quantized.x == (cw::octree::kQuantMax + 1) / 2);
}

TEST_CASE("quantizeAll writes little-endian uint16 quads", "[PointOctree]") {
    const QBox3D node = unitNode();

    //The middle point sorts between the two corners, so the payload's order is
    //minimum corner, middle, maximum corner
    const QVector<QVector3D> points {
        node.minimum(),
        node.maximum(),
        QVector3D(8.0f, 0.0f, 16.0f)
    };

    const QByteArray bytes = cw::octree::quantizeAll(points, node);
    REQUIRE(bytes.size() == points.size() * cw::octree::kBytesPerPoint);

    //Point 0 is the minimum corner: all zeros
    for(int i = 0; i < cw::octree::kBytesPerPoint; i++) {
        CHECK(bytes.at(i) == '\0');
    }

    //Point 1 is the middle of x, the minimum of y, the maximum of z
    CHECK(readAxis(bytes, 1, 0) == (cw::octree::kQuantMax + 1) / 2);
    CHECK(readAxis(bytes, 1, 1) == 0);
    CHECK(readAxis(bytes, 1, 2) == cw::octree::kQuantMax);

    //Point 2 is the maximum corner: 0xFF 0xFF per axis, reserved stays zero
    const qsizetype lastPoint = 2 * cw::octree::kBytesPerPoint;
    CHECK(static_cast<quint8>(bytes.at(lastPoint)) == 0xFF);
    CHECK(static_cast<quint8>(bytes.at(lastPoint + 1)) == 0xFF);
    CHECK(readAxis(bytes, 2, 0) == cw::octree::kQuantMax);
    CHECK(readAxis(bytes, 2, 1) == cw::octree::kQuantMax);
    CHECK(readAxis(bytes, 2, 2) == cw::octree::kQuantMax);
    CHECK(readAxis(bytes, 2, 3) == 0);
}

TEST_CASE("Morton keys interleave the quantized axes", "[PointOctree]") {
    using cw::octree::mortonKey;
    using cw::octree::QuantizedPoint;

    //x at bit 3i, y at 3i + 1, z at 3i + 2
    CHECK(mortonKey(QuantizedPoint {0, 0, 0}) == 0);
    CHECK(mortonKey(QuantizedPoint {1, 0, 0}) == 1);
    CHECK(mortonKey(QuantizedPoint {0, 1, 0}) == 2);
    CHECK(mortonKey(QuantizedPoint {0, 0, 1}) == 4);
    CHECK(mortonKey(QuantizedPoint {1, 1, 1}) == 7);

    CHECK(mortonKey(QuantizedPoint {0, 0, 0}) < mortonKey(QuantizedPoint {1, 0, 0}));
    CHECK(mortonKey(QuantizedPoint {1, 0, 0}) < mortonKey(QuantizedPoint {0, 1, 0}));
    CHECK(mortonKey(QuantizedPoint {0, 1, 0}) < mortonKey(QuantizedPoint {0, 0, 1}));
    CHECK(mortonKey(QuantizedPoint {0, 0, 1}) < mortonKey(QuantizedPoint {1, 1, 1}));

    //Every bit of every axis reaches the key, and only the low 48 bits are used
    const quint64 full = mortonKey(QuantizedPoint {cw::octree::kQuantMax,
                                                   cw::octree::kQuantMax,
                                                   cw::octree::kQuantMax});
    CHECK(full == (quint64(1) << (3 * cw::octree::kMortonBitsPerAxis)) - 1);
    CHECK(mortonKey(QuantizedPoint {cw::octree::kQuantMax, 0, 0})
          + mortonKey(QuantizedPoint {0, cw::octree::kQuantMax, 0})
          + mortonKey(QuantizedPoint {0, 0, cw::octree::kQuantMax}) == full);

    //Injective: no two distinct points share a key
    QSet<quint64> keys;
    constexpr int kRandomPoints = 4000;
    RandomSequence random {12345u};
    const auto nextAxis = [&random]() { return quint16(random.next() >> 16); };

    for(int i = 0; i < kRandomPoints; i++) {
        const QuantizedPoint point {nextAxis(), nextAxis(), nextAxis()};
        const quint64 key = mortonKey(point);
        CHECK_FALSE(keys.contains(key));
        keys.insert(key);
    }
}

TEST_CASE("quantizeAll writes its points in Morton order", "[PointOctree]") {
    const QBox3D node = unitNode();

    QVector<QVector3D> points;
    constexpr int kRandomPoints = 3000;
    constexpr quint32 kFractionBits = 24;
    RandomSequence random {98765u};
    const auto nextAxis = [&random]() {
        return float(double(random.next() >> 8) / double(1u << kFractionBits) * kNodeSize);
    };

    points.reserve(kRandomPoints);
    for(int i = 0; i < kRandomPoints; i++) {
        points.append(QVector3D(nextAxis(), nextAxis(), nextAxis()));
    }

    const QByteArray bytes = cw::octree::quantizeAll(points, node);
    REQUIRE(bytes.size() == points.size() * cw::octree::kBytesPerPoint);

    quint64 previous = 0;
    for(int i = 0; i < points.size(); i++) {
        const cw::octree::QuantizedPoint written {
            readAxis(bytes, i, 0), readAxis(bytes, i, 1), readAxis(bytes, i, 2), 0
        };
        const quint64 key = cw::octree::mortonKey(written);
        CHECK(key >= previous);
        previous = key;
    }

    //Deterministic: the same input gives the same bytes every time
    CHECK(cw::octree::quantizeAll(points, node) == bytes);
}

TEST_CASE("Octant paths name nodes", "[PointOctree]") {
    CHECK(cw::octree::nodeName({}) == QStringLiteral("r"));
    CHECK(cw::octree::nodeName({3}) == QStringLiteral("r3"));
    CHECK(cw::octree::nodeName({3, 5}) == QStringLiteral("r35"));
    CHECK(cw::octree::nodeName({0, 7, 1}) == QStringLiteral("r071"));
}

TEST_CASE("The manifest derives node bounds and spacing", "[PointOctree]") {
    const cwPointOctreeManifest manifest = threeLevelManifest();

    SECTION("The root node is the root cube") {
        const QBox3D bounds = manifest.nodeBounds(0);
        CHECK(bounds.minimum() == manifest.rootMin);
        CHECK(bounds.maximum() == manifest.rootMin + QVector3D(16.0f, 16.0f, 16.0f));
    }

    SECTION("A level two cell is a quarter of the root on each axis") {
        const QBox3D bounds = manifest.nodeBounds(3);
        const float cellSize = 4.0f;
        CHECK(bounds.minimum() == manifest.rootMin + QVector3D(1.0f * cellSize, 3.0f * cellSize, 3.0f * cellSize));
        CHECK(bounds.maximum() == bounds.minimum() + QVector3D(cellSize, cellSize, cellSize));
    }

    SECTION("Spacing halves with every level") {
        const double rootSpacing = kNodeSize / cw::octree::kSampleGridResolution;
        CHECK_THAT(manifest.spacing(0), Catch::Matchers::WithinAbs(rootSpacing, 1e-12));
        CHECK_THAT(manifest.spacing(1), Catch::Matchers::WithinAbs(rootSpacing / 2.0, 1e-12));
        CHECK_THAT(manifest.spacing(4), Catch::Matchers::WithinAbs(rootSpacing / 16.0, 1e-12));
    }

    SECTION("Node names walk back to the root") {
        CHECK(manifest.nodeName(0) == QStringLiteral("r"));
        CHECK(manifest.nodeName(1) == QStringLiteral("r3"));
        CHECK(manifest.nodeName(2) == QStringLiteral("r5"));
        CHECK(manifest.nodeName(3) == QStringLiteral("r37"));
    }
}

TEST_CASE("A node's spacing is its own cube divided by the sample grid", "[PointOctree]") {
    // spacing(level) is derived from the level alone — it is what selectCut
    // projects to the screen and what cwRHIPointCloud floors a sprite at — so
    // it has to agree with the grid the sampler laid over each node's cube.
    const cwPointOctreeManifest manifest = spineManifest();
    REQUIRE(manifest.isValid());

    for(int level = 0; level <= kSpineMaxLevel; level++) {
        //The spine puts the node of each level at the index of that level
        const double fromCube = double(manifest.nodeBounds(level).size().x())
                                / cw::octree::kSampleGridResolution;

        CHECK(manifest.nodes.at(level).level == level);
        CHECK_THAT(fromCube, Catch::Matchers::WithinRel(manifest.spacing(level), 1e-6));
    }
}

TEST_CASE("A manifest is valid only when its node table hangs together", "[PointOctree]") {
    CHECK(threeLevelManifest().isValid());

    SECTION("An empty node table") {
        CHECK_FALSE(cwPointOctreeManifest().isValid());
    }

    SECTION("A root that sits below level zero") {
        cwPointOctreeManifest manifest = threeLevelManifest();
        manifest.nodes[0].level = 1;
        CHECK_FALSE(manifest.isValid());
    }

    SECTION("A child that skips a level") {
        cwPointOctreeManifest manifest = threeLevelManifest();
        manifest.nodes[1].level = 2;
        CHECK_FALSE(manifest.isValid());
    }

    SECTION("A child at the same level as its parent") {
        cwPointOctreeManifest manifest = threeLevelManifest();
        manifest.nodes[3].level = 1;
        CHECK_FALSE(manifest.isValid());
    }

    SECTION("A child index outside the table") {
        cwPointOctreeManifest manifest = threeLevelManifest();
        manifest.nodes[0].children[3] = manifest.nodes.size();
        CHECK_FALSE(manifest.isValid());
    }

    SECTION("A cycle still names a node") {
        cwPointOctreeManifest manifest = threeLevelManifest();
        manifest.nodes[3].children[0] = 0;
        CHECK_FALSE(manifest.isValid());
        CHECK_FALSE(manifest.nodeName(3).isEmpty());
    }
}

TEST_CASE("A manifest round-trips through serialization", "[PointOctree]") {
    const cwPointOctreeManifest manifest = threeLevelManifest();
    REQUIRE(manifest.isValid());

    const QByteArray bytes = manifest.serialize();
    const auto restored = cwPointOctreeManifest::deserialize(bytes);
    REQUIRE(restored.has_value());

    CHECK(restored->rootMin == manifest.rootMin);
    CHECK_THAT(restored->rootSize, Catch::Matchers::WithinAbs(manifest.rootSize, 1e-12));
    CHECK(restored->pointCount == manifest.pointCount);
    CHECK(restored->bboxMin == manifest.bboxMin);
    CHECK(restored->bboxMax == manifest.bboxMax);
    CHECK_THAT(restored->meanSpacingXY, Catch::Matchers::WithinAbs(manifest.meanSpacingXY, kEpsilon));
    CHECK(restored->fingerprint == manifest.fingerprint);
    REQUIRE(restored->nodes.size() == manifest.nodes.size());

    for(int i = 0; i < manifest.nodes.size(); i++) {
        const cwPointOctreeNode& expected = manifest.nodes.at(i);
        const cwPointOctreeNode& actual = restored->nodes.at(i);
        CHECK(actual.level == expected.level);
        CHECK(actual.x == expected.x);
        CHECK(actual.y == expected.y);
        CHECK(actual.z == expected.z);
        CHECK(actual.pointCount == expected.pointCount);
        CHECK(actual.byteSize == expected.byteSize);
        CHECK(actual.children == expected.children);
    }

    CHECK(restored->nodeName(3) == QStringLiteral("r37"));
}

TEST_CASE("Deserialize rejects unusable bytes", "[PointOctree]") {
    const QByteArray bytes = threeLevelManifest().serialize();

    SECTION("A bumped format generation") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.set_format_generation(cw::octree::kFormatGeneration + 1);
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("A child index outside the node table") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.mutable_nodes(0)->set_children(3, 99);
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("A child that is not one level deeper than its parent") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.mutable_nodes(1)->set_level(2);
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("A root below level zero") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.mutable_nodes(0)->set_level(1);
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("An empty node table") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.clear_nodes();
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("A root cube vector of the wrong length") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.mutable_root_min()->Add(0.0);
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("A data bounds vector of the wrong length") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.mutable_bbox_max()->RemoveLast();
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("A child list that is not eight long") {
        CavewhereProto::PointOctreeManifest proto = parseManifest(bytes);
        proto.mutable_nodes(0)->add_children(-1);
        CHECK_FALSE(deserializeProto(proto).has_value());
    }

    SECTION("A level deeper than the builder can reach") {
        CavewhereProto::PointOctreeManifest accepted = parseManifest(bytes);
        addOrphan(&accepted)->set_level(cw::octree::kMaxLevel);
        CHECK(deserializeProto(accepted).has_value());

        CavewhereProto::PointOctreeManifest rejected = parseManifest(bytes);
        addOrphan(&rejected)->set_level(cw::octree::kMaxLevel + 1);
        CHECK_FALSE(deserializeProto(rejected).has_value());
    }

    SECTION("Garbage bytes") {
        CHECK_FALSE(cwPointOctreeManifest::deserialize(
                        QByteArrayLiteral("\xff\xfe\xfd\xfc not a manifest at all")).has_value());
        CHECK_FALSE(cwPointOctreeManifest::deserialize(QByteArray()).has_value());
    }
}

TEST_CASE("The source fingerprint follows the file and the CRS", "[PointOctree]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    //Larger than two fingerprint windows, so the head and tail windows are sampled
    const qint64 fileSize = 2 * cw::octree::kFingerprintWindowBytes + 1024;
    QByteArray contents(static_cast<qsizetype>(fileSize), 'a');
    contents[0] = 'h';

    const QString path = writeFile(directory.filePath(QStringLiteral("cloud.laz")), contents);

    const QString sourceCS = QStringLiteral("EPSG:26917");
    const QString frameCS = QStringLiteral("+proj=tmerc +lat_0=38");

    const QString fingerprint = cw::octree::sourceFingerprint(path, sourceCS, frameCS);
    CHECK_FALSE(fingerprint.isEmpty());

    SECTION("Stable across two reads") {
        CHECK(cw::octree::sourceFingerprint(path, sourceCS, frameCS) == fingerprint);
    }

    SECTION("Changes when the tail of the file changes") {
        contents[contents.size() - 1] = 'z';
        writeFile(path, contents);
        CHECK(cw::octree::sourceFingerprint(path, sourceCS, frameCS) != fingerprint);
    }

    SECTION("Changes when the frame CRS changes") {
        CHECK(cw::octree::sourceFingerprint(path, sourceCS, QStringLiteral("+proj=tmerc +lat_0=39"))
              != fingerprint);
    }

    SECTION("Changes when the source CRS changes") {
        CHECK(cw::octree::sourceFingerprint(path, QStringLiteral("EPSG:26918"), frameCS) != fingerprint);
    }

    SECTION("Changes when the head of the file changes") {
        contents[0] = 'H';
        writeFile(path, contents);
        CHECK(cw::octree::sourceFingerprint(path, sourceCS, frameCS) != fingerprint);
    }

    SECTION("Ignores a byte between the two windows") {
        contents[static_cast<qsizetype>(cw::octree::kFingerprintWindowBytes) + 10] = 'm';
        writeFile(path, contents);
        CHECK(cw::octree::sourceFingerprint(path, sourceCS, frameCS) == fingerprint);
    }

    SECTION("Empty for a missing file") {
        CHECK(cw::octree::sourceFingerprint(directory.filePath(QStringLiteral("gone.laz")),
                                            sourceCS, frameCS).isEmpty());
    }
}

TEST_CASE("A small file hashes size, contents, and both CRS strings", "[PointOctree]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const QByteArray contents = QByteArrayLiteral("a tiny laz file that fits in one window");
    const QString path = writeFile(directory.filePath(QStringLiteral("small.laz")), contents);

    const QString sourceCS = QStringLiteral("EPSG:26917");
    const QString frameCS = QStringLiteral("+proj=tmerc +lat_0=38");

    const quint64 littleEndianSize = qToLittleEndian(static_cast<quint64>(contents.size()));
    QByteArray hashed(reinterpret_cast<const char*>(&littleEndianSize), sizeof(littleEndianSize));
    hashed += contents;
    hashed += sourceCS.toUtf8();
    hashed += frameCS.toUtf8();

    const QString expected = QString::number(cwImageProvider::toHash(hashed), 16);
    CHECK(cw::octree::sourceFingerprint(path, sourceCS, frameCS) == expected);
}

TEST_CASE("Octree cache keys land beside the laz file", "[PointOctree]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const QDir root(directory.path());
    REQUIRE(root.mkpath(QStringLiteral("GIS Layers")));

    const QString lazPath = root.filePath(QStringLiteral("GIS Layers/cave.laz"));
    const QString fingerprint = QStringLiteral("deadbeef");

    const cwDiskCacher::Key manifestKey = cw::octree::manifestKey(lazPath, fingerprint);
    const cwDiskCacher::Key rootNodeKey = cw::octree::nodeKey(lazPath, fingerprint, QStringLiteral("r"));
    const cwDiskCacher::Key childKey = cw::octree::nodeKey(lazPath, fingerprint, QStringLiteral("r35"));

    const QString generation = QString::number(cw::octree::kFormatGeneration);
    CHECK(manifestKey.id == QStringLiteral("cave.laz-octree") + generation + QStringLiteral("-manifest"));
    CHECK(rootNodeKey.id == QStringLiteral("cave.laz-octree") + generation + QStringLiteral("-r"));
    CHECK(childKey.id == QStringLiteral("cave.laz-octree") + generation + QStringLiteral("-r35"));
    CHECK(manifestKey.checksum == fingerprint);
    CHECK(rootNodeKey.checksum == fingerprint);
    CHECK(manifestKey.path == QFileInfo(lazPath).path());
    CHECK(childKey.path == QFileInfo(lazPath).path());

    cwDiskCacher cacher(root);

    const QByteArray manifestBytes = threeLevelManifest().serialize();
    cacher.insert(manifestKey, manifestBytes);
    cacher.insert(rootNodeKey, QByteArrayLiteral("node bytes"));

    const QString expectedManifestPath =
        root.filePath(QStringLiteral(".cw_cache/GIS Layers/") + manifestKey.id);
    CHECK(QFileInfo::exists(expectedManifestPath));
    CHECK(QFileInfo::exists(root.filePath(QStringLiteral(".cw_cache/GIS Layers/") + rootNodeKey.id)));

    CHECK(cacher.entry(manifestKey) == manifestBytes);
    CHECK(cacher.entry(rootNodeKey) == QByteArrayLiteral("node bytes"));

    SECTION("A different fingerprint fails the checksum") {
        cwDiskCacher::Key stale = manifestKey;
        stale.checksum = QStringLiteral("0ddba11");
        CHECK(cacher.entry(stale).isEmpty());
    }
}

TEST_CASE("A node key base yields the same keys as the path overloads", "[PointOctree]") {
    const QString lazPath = QStringLiteral("/tmp/GIS Layers/cave.laz");
    const QString fingerprint = QStringLiteral("deadbeef");
    const cw::octree::NodeKeyBase base = cw::octree::nodeKeyBase(lazPath, fingerprint);

    const auto checkSame = [](const cwDiskCacher::Key& fromBase, const cwDiskCacher::Key& fromPath) {
        CHECK(fromBase.id == fromPath.id);
        CHECK(fromBase.path == fromPath.path);
        CHECK(fromBase.checksum == fromPath.checksum);
    };

    checkSame(cw::octree::manifestKey(base), cw::octree::manifestKey(lazPath, fingerprint));
    for(const QString& nodeName : {QStringLiteral("r"), QStringLiteral("r35"), QStringLiteral("r0123")}) {
        checkSame(cw::octree::nodeKey(base, nodeName),
                  cw::octree::nodeKey(lazPath, fingerprint, nodeName));
    }

    const QString generation = QString::number(cw::octree::kFormatGeneration);
    CHECK(cw::octree::nodeKey(base, QStringLiteral("r7")).id
          == QStringLiteral("cave.laz-octree") + generation + QStringLiteral("-r7"));
    CHECK(base.dir == QStringLiteral("/tmp/GIS Layers"));
}

TEST_CASE("A manifest round-trips each node's measured spacing", "[PointOctree]") {
    cwPointOctreeManifest manifest = threeLevelManifest();
    REQUIRE(manifest.isValid());

    //A spacing per node, none of them the grid's, and one left unmeasured
    constexpr float kFirstMeasuredSpacing = 0.37f;
    constexpr float kMeasuredSpacingStep = 0.11f;
    for(int i = 1; i < manifest.nodes.size(); i++) {
        manifest.nodes[i].measuredSpacing = kFirstMeasuredSpacing + kMeasuredSpacingStep * float(i);
    }
    REQUIRE(manifest.nodes.at(0).measuredSpacing == 0.0f);

    const auto restored = cwPointOctreeManifest::deserialize(manifest.serialize());
    REQUIRE(restored.has_value());
    REQUIRE(restored->nodes.size() == manifest.nodes.size());
    for(int i = 0; i < manifest.nodes.size(); i++) {
        INFO("node " << i);
        CHECK(restored->nodes.at(i).measuredSpacing == manifest.nodes.at(i).measuredSpacing);
    }
}

TEST_CASE("The octree format is generation 8, the one whose native spacing reaches past its pass B chunk", "[PointOctree]") {
    //Generation 4 added each node's measured spacing and the builder's minimum
    //to split; generation 5 wrote a local spacing ratio into each point's
    //reserved word; generation 6 wrote the point's absolute 3D native spacing
    //there; generation 7 wrote its horizontal native spacing under the
    //data-relative cap; generation 8 measures it over the chunk's whole xy
    //column and a halo of its neighbors, under a per-column cap, so every
    //generation 7 cache rebuilds once. The xyz payload is unchanged.
    CHECK(cw::octree::kFormatGeneration == 8);
}

TEST_CASE("A generation 3 manifest is rejected", "[PointOctree]") {
    constexpr int kGenerationWithoutMeasuredSpacing = 3;

    CavewhereProto::PointOctreeManifest proto = parseManifest(threeLevelManifest().serialize());
    proto.set_format_generation(kGenerationWithoutMeasuredSpacing);
    CHECK_FALSE(deserializeProto(proto).has_value());

    //The cache key names the generation too, so its entries are never even read
    const cwDiskCacher::Key key = cw::octree::manifestKey(QStringLiteral("/tmp/tile.laz"),
                                                          QStringLiteral("fingerprint"));
    CHECK(key.id.contains(QStringLiteral("octree%1").arg(cw::octree::kFormatGeneration)));
    CHECK_FALSE(key.id.contains(QStringLiteral("octree3")));
}

TEST_CASE("A generation 4 manifest is rejected", "[PointOctree]") {
    constexpr int kGenerationWithoutLocalSpacing = 4;

    CavewhereProto::PointOctreeManifest proto = parseManifest(threeLevelManifest().serialize());
    proto.set_format_generation(kGenerationWithoutLocalSpacing);
    CHECK_FALSE(deserializeProto(proto).has_value());

    //Its payloads hold no local spacing, so the key never reaches them either
    const cwDiskCacher::Key key = cw::octree::manifestKey(QStringLiteral("/tmp/tile.laz"),
                                                          QStringLiteral("fingerprint"));
    CHECK_FALSE(key.id.contains(QStringLiteral("octree4")));
}

TEST_CASE("A generation 5 manifest is rejected", "[PointOctree]") {
    constexpr int kGenerationWithLocalSpacingRatios = 5;

    CavewhereProto::PointOctreeManifest proto = parseManifest(threeLevelManifest().serialize());
    proto.set_format_generation(kGenerationWithLocalSpacingRatios);
    CHECK_FALSE(deserializeProto(proto).has_value());

    //Its payloads hold ratios to the grid rather than native spacings, so the
    //key never reaches them either
    const cwDiskCacher::Key key = cw::octree::manifestKey(QStringLiteral("/tmp/tile.laz"),
                                                          QStringLiteral("fingerprint"));
    CHECK_FALSE(key.id.contains(QStringLiteral("octree5")));
}

TEST_CASE("A generation 6 manifest is rejected", "[PointOctree]") {
    constexpr int kGenerationWithSpatialNativeSpacing = 6;

    CavewhereProto::PointOctreeManifest proto = parseManifest(threeLevelManifest().serialize());
    proto.set_format_generation(kGenerationWithSpatialNativeSpacing);
    CHECK_FALSE(deserializeProto(proto).has_value());

    //Its payloads hold 3D native spacings under the grid-ratio cap, so the
    //key never reaches them either
    const cwDiskCacher::Key key = cw::octree::manifestKey(QStringLiteral("/tmp/tile.laz"),
                                                          QStringLiteral("fingerprint"));
    CHECK_FALSE(key.id.contains(QStringLiteral("octree6")));
}

TEST_CASE("A generation 7 manifest is rejected", "[PointOctree]") {
    constexpr int kGenerationWithChunkBoundedNativeSpacing = 7;

    CavewhereProto::PointOctreeManifest proto = parseManifest(threeLevelManifest().serialize());
    proto.set_format_generation(kGenerationWithChunkBoundedNativeSpacing);
    CHECK_FALSE(deserializeProto(proto).has_value());

    //Its payloads hold native spacings whose neighbors stopped at the pass B
    //chunk's faces, so the key never reaches them either
    const cwDiskCacher::Key key = cw::octree::manifestKey(QStringLiteral("/tmp/tile.laz"),
                                                          QStringLiteral("fingerprint"));
    CHECK_FALSE(key.id.contains(QStringLiteral("octree7")));
}

// ---- WP13: per-point local spacing codes ------------------------------------

namespace {
    //! The octaves the native spacing codes span
    const double kLocalCodeOctaves =
        std::log2(cw::octree::kMaxLocalSpacingUnits / cw::octree::kMinLocalSpacingUnits);
    //! One code step, as a factor on the spacing: the codes from 1 to
    //! kQuantMax split kLocalCodeOctaves evenly
    const double kLocalCodeStepFactor =
        std::exp2(kLocalCodeOctaves / double(cw::octree::kQuantMax - 1));
    //! The reserved word is the fourth axis of a point
    constexpr int kReservedAxis = 3;
}

TEST_CASE("The native spacing estimate's constants are the plan's", "[PointOctree]") {
    using namespace cw::octree;

    CHECK(kLocalSpacingNeighbors == 8);
    CHECK(kLocalSpacingSurfaceFactor == std::sqrt(std::numbers::pi / 8.0));
    CHECK(kMinLocalSpacingUnits == 0.005);
    CHECK(kMaxLocalSpacingUnits == 64.0);
    CHECK(kLoneSpacingMedianRatio == 4.0);
    CHECK(kUnmeasuredLocalSpacingCode == 0);
}

TEST_CASE("The halo pass B's column index reaches into its neighbors holds the k-th neighbor of the BC tile's widest uncapped point, well inside the search's widest reach", "[PointOctree]") {
    using namespace cw::octree;

    //The BC tile's chunk ceilings run 0.38 to 0.93 m
    constexpr double kWidestTileCeiling = 0.93;
    CHECK(kLocalSpacingHaloUnits == 2.0);
    CHECK(kWidestTileCeiling / kLocalSpacingSurfaceFactor <= kLocalSpacingHaloUnits);
    CHECK(kLocalSpacingHaloUnits < kMaxLocalSpacingUnits / kLocalSpacingSurfaceFactor);
}

TEST_CASE("A native spacing round-trips through its 16-bit log2 code over the absolute range", "[PointOctree]") {
    using namespace cw::octree;

    //Log-spaced from the floor to the cap, ends included
    constexpr int kSteps = 96;
    quint16 previous = kUnmeasuredLocalSpacingCode;
    for(int step = 0; step <= kSteps; step++) {
        const double spacing = kMinLocalSpacingUnits
                               * std::exp2(kLocalCodeOctaves * double(step) / kSteps);
        INFO("spacing " << spacing << " m");
        const quint16 code = encodeLocalSpacing(spacing);
        CHECK(code != kUnmeasuredLocalSpacingCode);
        //Wider spacings take larger codes
        CHECK(code > previous);
        previous = code;
        //Rounding to the nearest code moves the spacing by at most half a step
        const double decoded = decodeLocalSpacing(code, 1.0);
        CHECK(std::abs(std::log2(decoded / spacing))
              <= 0.5 * std::log2(kLocalCodeStepFactor) * (1.0 + 1e-6));
    }

    //The ends are the first and last codes
    CHECK(encodeLocalSpacing(kMinLocalSpacingUnits) == 1);
    CHECK(encodeLocalSpacing(kMaxLocalSpacingUnits) == kQuantMax);
}

TEST_CASE("A native spacing clamps to the absolute range, decodes the same whatever the grid, and 0 decodes to the grid spacing", "[PointOctree]") {
    using namespace cw::octree;

    //A code stands for meters, not for grid spacings, so the grid the point
    //sits in never changes what it decodes to
    constexpr double kFineGrid = 0.01;
    constexpr double kCoarseGrid = 10.0;
    constexpr double kNativeSpacing = 0.27;
    const quint16 native = encodeLocalSpacing(kNativeSpacing);
    CHECK(decodeLocalSpacing(native, kFineGrid) == decodeLocalSpacing(native, kCoarseGrid));
    CHECK(decodeLocalSpacing(native, kFineGrid) == Catch::Approx(kNativeSpacing).epsilon(1e-3));

    //Past the cap codes to the cap, the widest a code goes
    constexpr double kLoneSpacing = 1000.0;
    CHECK(encodeLocalSpacing(kLoneSpacing) == kQuantMax);
    CHECK(decodeLocalSpacing(kQuantMax, kFineGrid)
          == Catch::Approx(kMaxLocalSpacingUnits).epsilon(1e-6));

    //Under the floor codes to the floor, which is never the unmeasured code
    constexpr double kStackSpacing = 0.0001;
    const quint16 stack = encodeLocalSpacing(kStackSpacing);
    CHECK(stack == encodeLocalSpacing(kMinLocalSpacingUnits));
    CHECK(stack != kUnmeasuredLocalSpacingCode);
    CHECK(encodeLocalSpacing(0.0) == stack);
    CHECK(decodeLocalSpacing(stack, kCoarseGrid)
          == Catch::Approx(kMinLocalSpacingUnits).epsilon(1e-6));

    //An unmeasured point, as an old payload holds, sizes by its grid spacing
    CHECK(decodeLocalSpacing(kUnmeasuredLocalSpacingCode, kFineGrid) == kFineGrid);
    CHECK(decodeLocalSpacing(kUnmeasuredLocalSpacingCode, kCoarseGrid) == kCoarseGrid);
}

TEST_CASE("quantizeAll carries each point's local spacing code through the Morton sort", "[PointOctree]") {
    const QBox3D node = unitNode();

    QVector<QVector3D> points;
    QVector<quint16> codes;
    constexpr int kRandomPoints = 3000;
    constexpr quint32 kFractionBits = 24;
    RandomSequence random {24680u};
    const auto nextAxis = [&random]() {
        return float(double(random.next() >> 8) / double(1u << kFractionBits) * kNodeSize);
    };

    //Every point a code of its own, so a point that lost its code or took a
    //neighbor's shows
    QHash<quint64, quint16> codeForKey;
    for(int i = 0; i < kRandomPoints; i++) {
        const QVector3D point(nextAxis(), nextAxis(), nextAxis());
        const quint16 code = quint16(1 + i);
        const quint64 key = cw::octree::mortonKey(cw::octree::quantize(point, node));
        if(codeForKey.contains(key)) {
            continue;
        }
        codeForKey.insert(key, code);
        points.append(point);
        codes.append(code);
    }

    const QByteArray bytes = cw::octree::quantizeAll(points, codes, node);
    REQUIRE(bytes.size() == points.size() * cw::octree::kBytesPerPoint);

    quint64 previous = 0;
    int matching = 0;
    for(int i = 0; i < points.size(); i++) {
        const cw::octree::QuantizedPoint written {
            readAxis(bytes, i, 0), readAxis(bytes, i, 1), readAxis(bytes, i, 2),
            readAxis(bytes, i, kReservedAxis)
        };
        const quint64 key = cw::octree::mortonKey(written);
        CHECK(key >= previous);
        previous = key;
        matching += written.reserved == codeForKey.value(key) ? 1 : 0;
    }
    CHECK(matching == points.size());

    //The positions are quantizeAll()'s own; the codes only fill the reserved word
    const QByteArray plain = cw::octree::quantizeAll(points, node);
    REQUIRE(plain.size() == bytes.size());
    for(int i = 0; i < points.size(); i++) {
        for(int axis = 0; axis < kReservedAxis; axis++) {
            CHECK(readAxis(plain, i, axis) == readAxis(bytes, i, axis));
        }
    }
}
