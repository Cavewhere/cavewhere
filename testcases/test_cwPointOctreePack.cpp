//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QTemporaryDir>
#include <QtEndian>

//Std includes
#include <algorithm>
#include <memory>
#include <numeric>
#include <random>

//Our includes
#include "cavewhere.pb.h"
#include "cwConcurrent.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwPointOctreePack.h"

using namespace cw::octree;

namespace {
    //The on-disk layout the plan fixes, restated here so a test can rewrite a
    //pack without a production hook
    constexpr qsizetype kHeaderBytes = 8;
    constexpr qsizetype kFooterBytes = 56;
    constexpr qsizetype kIndexEntryBytes = 16;
    constexpr qsizetype kChecksumBytes = 8;
    constexpr int kFooterManifestOffset = 0;
    constexpr int kFooterManifestSize = 8;
    constexpr int kFooterIndexOffset = 16;
    constexpr int kFooterNodeCount = 24;
    constexpr int kFooterFileSize = 32;
    constexpr int kFooterChecksum = 40;

    constexpr quint32 kPointsPerNode = 5;
    constexpr qint64 kShortPayloadBytes = 8;
    constexpr unsigned kShuffleSeed = 1234;
    constexpr int kWideLevel = 3;
    constexpr quint32 kWideCellsPerAxis = 8;
    constexpr quint32 kWideCellsZ = 4;

    cwPointOctreeNode makeNode(int level, quint32 x, quint32 y, quint32 z, quint32 pointCount)
    {
        cwPointOctreeNode node;
        node.level = level;
        node.x = x;
        node.y = y;
        node.z = z;
        node.pointCount = pointCount;
        node.byteSize = qint64(pointCount) * kBytesPerPoint;
        node.measuredSpacing = 0.25f * float(level + 1);
        return node;
    }

    //A root with three children, one of which has a child of its own
    cwPointOctreeManifest makeManifest()
    {
        cwPointOctreeManifest manifest;
        manifest.rootMin = QVector3D(10.0f, 20.0f, 30.0f);
        manifest.rootSize = 64.0;
        manifest.bboxMin = QVector3D(11.0f, 21.0f, 31.0f);
        manifest.bboxMax = QVector3D(70.0f, 80.0f, 90.0f);
        manifest.meanSpacingXY = 0.5f;
        manifest.fingerprint = QStringLiteral("feedbeef");

        cwPointOctreeNode root = makeNode(0, 0, 0, 0, kPointsPerNode);
        root.children[0] = 1;
        root.children[3] = 3;
        root.children[7] = 4;
        cwPointOctreeNode first = makeNode(1, 0, 0, 0, kPointsPerNode + 1);
        first.children[1] = 2;
        manifest.nodes = {
            root,
            first,
            makeNode(2, 1, 0, 0, kPointsPerNode + 2),
            makeNode(1, 1, 1, 0, kPointsPerNode + 3),
            makeNode(1, 1, 1, 1, kPointsPerNode + 4)
        };

        qint64 points = 0;
        for(const cwPointOctreeNode& node : std::as_const(manifest.nodes)) {
            points += node.pointCount;
        }
        manifest.pointCount = points;
        return manifest;
    }

    //makeManifest's tree plus hundreds of cells at one deeper level, so
    //concurrent appends overlap
    cwPointOctreeManifest makeWideManifest()
    {
        cwPointOctreeManifest manifest = makeManifest();
        qint64 points = manifest.pointCount;
        for(quint32 z = 0; z < kWideCellsZ; z++) {
            for(quint32 y = 0; y < kWideCellsPerAxis; y++) {
                for(quint32 x = 0; x < kWideCellsPerAxis; x++) {
                    const quint32 pointCount = kPointsPerNode + x;
                    manifest.nodes.append(makeNode(kWideLevel, x, y, z, pointCount));
                    points += pointCount;
                }
            }
        }
        manifest.pointCount = points;
        return manifest;
    }

    //Distinct bytes per node, so a payload read back under the wrong index shows
    QByteArray payloadFor(const cwPointOctreeManifest& manifest, int index)
    {
        const cwPointOctreeNode& node = manifest.nodes.at(index);
        QByteArray payload(qsizetype(node.byteSize), Qt::Uninitialized);
        for(qsizetype i = 0; i < payload.size(); i++) {
            payload[i] = char((index * 31 + i) & 0xff);
        }
        return payload;
    }

    QVector<int> shuffledIndices(int count)
    {
        QVector<int> indices(count);
        std::iota(indices.begin(), indices.end(), 0);
        std::mt19937 generator(kShuffleSeed);
        std::shuffle(indices.begin(), indices.end(), generator);
        return indices;
    }

    void writePack(const QString& path, const cwPointOctreeManifest& manifest)
    {
        cwPointOctreePack::Writer writer(path);
        REQUIRE(writer.isOpen());
        for(int index : shuffledIndices(int(manifest.nodes.size()))) {
            REQUIRE(writer.append(cellKey(manifest.nodes.at(index)), payloadFor(manifest, index)));
        }
        INFO(writer.errorMessage().toStdString());
        REQUIRE(writer.finish(manifest));
    }

    QByteArray readAll(const QString& path)
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }

    void writeAll(const QString& path, const QByteArray& bytes)
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        REQUIRE(file.write(bytes) == bytes.size());
    }

    quint64 footerField(const QByteArray& pack, int fieldOffset)
    {
        return qFromLittleEndian<quint64>(pack.constData() + pack.size() - kFooterBytes + fieldOffset);
    }

    void appendU64(QByteArray& bytes, quint64 value)
    {
        char buffer[sizeof(quint64)];
        qToLittleEndian(value, buffer);
        bytes.append(buffer, sizeof(quint64));
    }

    //Rewrites the pack at path with manifestBytes in place of its manifest,
    //keeping its blobs and index entries and restamping the footer and checksum
    void replaceManifest(const QString& path, const QByteArray& manifestBytes)
    {
        const QByteArray pack = readAll(path);
        REQUIRE(pack.size() >= kHeaderBytes + kFooterBytes);
        const qsizetype manifestOffset = qsizetype(footerField(pack, kFooterManifestOffset));
        const qsizetype indexOffset = qsizetype(footerField(pack, kFooterIndexOffset));
        const quint64 nodeCount = footerField(pack, kFooterNodeCount);
        const QByteArray index = pack.mid(indexOffset, qsizetype(nodeCount) * kIndexEntryBytes);
        const QByteArray trailer = pack.right(kFooterBytes).mid(kFooterChecksum + kChecksumBytes);

        QByteArray rewritten = pack.left(manifestOffset);
        rewritten.append(manifestBytes);
        rewritten.append(index);

        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(manifestBytes);
        hash.addData(index);

        appendU64(rewritten, quint64(manifestOffset));
        appendU64(rewritten, quint64(manifestBytes.size()));
        appendU64(rewritten, quint64(manifestOffset + manifestBytes.size()));
        appendU64(rewritten, nodeCount);
        appendU64(rewritten, quint64(rewritten.size() + kFooterBytes - kFooterFileSize));
        rewritten.append(hash.result().left(kChecksumBytes));
        rewritten.append(trailer);
        REQUIRE(footerField(rewritten, kFooterFileSize) == quint64(rewritten.size()));
        writeAll(path, rewritten);
    }

    QByteArray manifestBytesWithGeneration(const cwPointOctreeManifest& manifest, int generation)
    {
        const QByteArray bytes = manifest.serialize();
        CavewhereProto::PointOctreeManifest proto;
        REQUIRE(proto.ParseFromArray(bytes.constData(), int(bytes.size())));
        proto.set_format_generation(generation);
        const std::string restamped = proto.SerializeAsString();
        return QByteArray(restamped.data(), qsizetype(restamped.size()));
    }

    bool sameNode(const cwPointOctreeNode& left, const cwPointOctreeNode& right)
    {
        return left.level == right.level
               && left.x == right.x
               && left.y == right.y
               && left.z == right.z
               && left.pointCount == right.pointCount
               && left.byteSize == right.byteSize
               && left.measuredSpacing == right.measuredSpacing
               && left.children == right.children;
    }

    QStringList filesIn(const QString& dir)
    {
        return QDir(dir).entryList(QDir::Files | QDir::Hidden);
    }
}

TEST_CASE("cellKey packs level, x, y, z the way the builder always has", "[PointOctree][PointOctreePack]")
{
    constexpr int kLevel = 3;
    constexpr quint32 kX = 5;
    constexpr quint32 kY = 6;
    constexpr quint32 kZ = 7;
    constexpr quint64 kExpected = (quint64(kLevel) << 60) | (quint64(kX) << 40) | (quint64(kY) << 20) | quint64(kZ);
    CHECK(cellKey(kLevel, kX, kY, kZ) == kExpected);
    CHECK(cellKey(makeNode(kLevel, kX, kY, kZ, kPointsPerNode)) == kExpected);
    CHECK(cellKey(0, 0, 0, 0) != cellKey(1, 0, 0, 0));
}

TEST_CASE("cwPointOctreePack round trips every node", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("nested/deeper/tile.cwpack"));
    const cwPointOctreeManifest manifest = makeManifest();

    writePack(path, manifest);
    CHECK(QFileInfo::exists(path));
    CHECK_FALSE(QFileInfo::exists(path + QStringLiteral(".tmp")));

    const std::shared_ptr<const cwPointOctreePack> pack = cwPointOctreePack::open(path);
    REQUIRE(pack);
    CHECK(pack->path() == path);
    CHECK(pack->nodeCount() == manifest.nodes.size());
    for(int i = 0; i < manifest.nodes.size(); i++) {
        INFO("node " << i);
        CHECK(pack->node(i) == payloadFor(manifest, i));
    }

    const cwPointOctreeManifest& read = pack->manifest();
    CHECK(read.rootMin == manifest.rootMin);
    CHECK(read.rootSize == manifest.rootSize);
    CHECK(read.pointCount == manifest.pointCount);
    CHECK(read.bboxMin == manifest.bboxMin);
    CHECK(read.bboxMax == manifest.bboxMax);
    CHECK(read.meanSpacingXY == manifest.meanSpacingXY);
    CHECK(read.fingerprint == manifest.fingerprint);
    REQUIRE(read.nodes.size() == manifest.nodes.size());
    for(int i = 0; i < manifest.nodes.size(); i++) {
        INFO("node " << i);
        CHECK(sameNode(read.nodes.at(i), manifest.nodes.at(i)));
    }

    SECTION("a payload outlives its pack")
    {
        std::shared_ptr<const cwPointOctreePack> transient = cwPointOctreePack::open(path);
        REQUIRE(transient);
        const QByteArray payload = transient->node(0);
        transient.reset();
        CHECK(payload == payloadFor(manifest, 0));
    }

    SECTION("a second write replaces the pack")
    {
        cwPointOctreeManifest changed = manifest;
        changed.fingerprint = QStringLiteral("cafe");
        writePack(path, changed);
        const std::shared_ptr<const cwPointOctreePack> reopened = cwPointOctreePack::open(path);
        REQUIRE(reopened);
        CHECK(reopened->manifest().fingerprint == changed.fingerprint);
    }
}

TEST_CASE("cwPointOctreePack::Writer sets a replaced pack aside", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString packName = QStringLiteral("tile.cwpack");
    const QString path = dir.filePath(packName);
    const QString stalePrefix = packName + QStringLiteral(".stale");
    const cwPointOctreeManifest manifest = makeManifest();
    cwPointOctreeManifest changed = manifest;
    changed.fingerprint = QStringLiteral("cafe");

    SECTION("a second write replaces a pack a reader still holds")
    {
        writePack(path, manifest);
        std::shared_ptr<const cwPointOctreePack> held = cwPointOctreePack::open(path);
        REQUIRE(held);

        writePack(path, changed);
        CHECK(held->manifest().fingerprint == manifest.fingerprint);
        CHECK(held->node(0) == payloadFor(manifest, 0));

        const std::shared_ptr<const cwPointOctreePack> reopened = cwPointOctreePack::open(path);
        REQUIRE(reopened);
        CHECK(reopened->manifest().fingerprint == changed.fingerprint);

        //On Windows the stale copy survives while held maps it
        for(const QString& name : filesIn(dir.path())) {
            INFO(name.toStdString());
            CHECK((name == packName || name.startsWith(stalePrefix)));
        }

        held.reset();
        writePack(path, manifest);
        CHECK(filesIn(dir.path()) == QStringList {packName});
    }

    SECTION("finish sweeps stale copies of this pack and leaves other files alone")
    {
        const QString other = QStringLiteral("other.cwpack.stale0");
        writePack(path, manifest);
        writeAll(dir.filePath(stalePrefix + QStringLiteral("0")), QByteArray());
        writeAll(dir.filePath(stalePrefix + QStringLiteral("1")), QByteArray());
        writeAll(dir.filePath(other), QByteArray());

        writePack(path, changed);
        CHECK(filesIn(dir.path()) == QStringList {other, packName});
    }

    SECTION("the stale name skips a name that is taken")
    {
        //A directory can be neither renamed over nor removed as a file, so it
        //stands in for a stale copy Windows still holds mapped
        const QString taken = dir.filePath(stalePrefix + QStringLiteral("0"));
        REQUIRE(QDir().mkpath(taken));
        const QString inside = QDir(taken).filePath(QStringLiteral("kept"));
        writeAll(inside, QByteArray());

        writePack(path, manifest);
        writePack(path, changed);

        const std::shared_ptr<const cwPointOctreePack> reopened = cwPointOctreePack::open(path);
        REQUIRE(reopened);
        CHECK(reopened->manifest().fingerprint == changed.fingerprint);
        CHECK(QFileInfo(taken).isDir());
        CHECK(QFileInfo::exists(inside));
        CHECK(filesIn(dir.path()) == QStringList {packName});
    }
}

TEST_CASE("cwPointOctreePack::Writer takes concurrent appends", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("tile.cwpack"));
    const cwPointOctreeManifest manifest = makeWideManifest();

    cwPointOctreePack::Writer writer(path);
    REQUIRE(writer.isOpen());

    const QVector<int> indices = shuffledIndices(int(manifest.nodes.size()));
    QFuture<bool> appended = cwConcurrent::mapped(indices, [&](int index) {
        return writer.append(cellKey(manifest.nodes.at(index)), payloadFor(manifest, index));
    });
    appended.waitForFinished();
    const QList<bool> results = appended.results();
    CHECK(results.size() == indices.size());
    CHECK(std::all_of(results.cbegin(), results.cend(), [](bool ok) { return ok; }));

    INFO(writer.errorMessage().toStdString());
    REQUIRE(writer.finish(manifest));

    const std::shared_ptr<const cwPointOctreePack> pack = cwPointOctreePack::open(path);
    REQUIRE(pack);
    REQUIRE(pack->nodeCount() == manifest.nodes.size());
    for(int i = 0; i < manifest.nodes.size(); i++) {
        INFO("node " << i);
        CHECK(pack->node(i) == payloadFor(manifest, i));
    }
}

TEST_CASE("cwPointOctreePack::Writer leaves no pack behind when finish fails", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("tile.cwpack"));
    const cwPointOctreeManifest manifest = makeManifest();

    SECTION("a manifest node was never appended")
    {
        cwPointOctreePack::Writer writer(path);
        REQUIRE(writer.isOpen());
        for(int i = 1; i < manifest.nodes.size(); i++) {
            REQUIRE(writer.append(cellKey(manifest.nodes.at(i)), payloadFor(manifest, i)));
        }
        CHECK_FALSE(writer.finish(manifest));
        CHECK_FALSE(writer.errorMessage().isEmpty());
        CHECK(filesIn(dir.path()).isEmpty());
    }

    SECTION("a cell was appended twice")
    {
        cwPointOctreePack::Writer writer(path);
        REQUIRE(writer.isOpen());
        for(int i = 0; i < manifest.nodes.size(); i++) {
            REQUIRE(writer.append(cellKey(manifest.nodes.at(i)), payloadFor(manifest, i)));
        }
        CHECK_FALSE(writer.append(cellKey(manifest.nodes.at(0)), payloadFor(manifest, 0)));
        CHECK_FALSE(writer.finish(manifest));
        CHECK_FALSE(writer.errorMessage().isEmpty());
        CHECK(filesIn(dir.path()).isEmpty());
    }

    SECTION("a cell was appended that the manifest lacks")
    {
        constexpr int kStrayLevel = 4;
        cwPointOctreePack::Writer writer(path);
        REQUIRE(writer.isOpen());
        for(int i = 0; i < manifest.nodes.size(); i++) {
            REQUIRE(writer.append(cellKey(manifest.nodes.at(i)), payloadFor(manifest, i)));
        }
        REQUIRE(writer.append(cellKey(kStrayLevel, 0, 0, 0), payloadFor(manifest, 0)));
        CHECK_FALSE(writer.finish(manifest));
        CHECK_FALSE(writer.errorMessage().isEmpty());
        CHECK(filesIn(dir.path()).isEmpty());
    }

    SECTION("an earlier pack at the path survives a failed rewrite")
    {
        writePack(path, manifest);
        {
            cwPointOctreePack::Writer writer(path);
            REQUIRE(writer.isOpen());
            CHECK_FALSE(writer.finish(manifest));
        }
        CHECK(filesIn(dir.path()) == QStringList {QStringLiteral("tile.cwpack")});
        CHECK(cwPointOctreePack::open(path));
    }
}

TEST_CASE("cwPointOctreePack::Writer removes its temp file when never finished", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("tile.cwpack"));
    const cwPointOctreeManifest manifest = makeManifest();

    {
        cwPointOctreePack::Writer writer(path);
        REQUIRE(writer.isOpen());
        REQUIRE(writer.append(cellKey(manifest.nodes.at(0)), payloadFor(manifest, 0)));
        CHECK(QFileInfo::exists(path + QStringLiteral(".tmp")));
    }
    CHECK(filesIn(dir.path()).isEmpty());
}

TEST_CASE("cwPointOctreePack::open rejects a damaged pack", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("tile.cwpack"));
    const cwPointOctreeManifest manifest = makeManifest();

    SECTION("missing file")
    {
        CHECK_FALSE(cwPointOctreePack::open(path));
    }

    writePack(path, manifest);
    REQUIRE(cwPointOctreePack::open(path));
    const QByteArray pack = readAll(path);

    SECTION("truncated by one byte")
    {
        writeAll(path, pack.left(pack.size() - 1));
        CHECK_FALSE(cwPointOctreePack::open(path));
    }

    SECTION("shorter than a header and footer")
    {
        writeAll(path, pack.left(kHeaderBytes));
        CHECK_FALSE(cwPointOctreePack::open(path));
    }

    SECTION("empty file")
    {
        writeAll(path, QByteArray());
        CHECK_FALSE(cwPointOctreePack::open(path));
    }

    SECTION("a byte flipped inside the manifest")
    {
        const qsizetype manifestOffset = qsizetype(footerField(pack, kFooterManifestOffset));
        const qsizetype manifestSize = qsizetype(footerField(pack, kFooterManifestSize));
        REQUIRE(manifestSize > 0);
        QByteArray flipped = pack;
        const qsizetype at = manifestOffset + manifestSize / 2;
        flipped[at] = char(flipped.at(at) ^ 0x01);
        writeAll(path, flipped);
        CHECK_FALSE(cwPointOctreePack::open(path));
    }

    SECTION("a trailing byte the footer's fileSize does not count")
    {
        writeAll(path, pack + QByteArray(1, '\0'));
        CHECK_FALSE(cwPointOctreePack::open(path));
    }

    SECTION("a bad leading magic")
    {
        QByteArray damaged = pack;
        damaged[0] = 'X';
        writeAll(path, damaged);
        CHECK_FALSE(cwPointOctreePack::open(path));
    }

    SECTION("a manifest from another format generation")
    {
        //The rewrite itself keeps a pack readable,
        replaceManifest(path, manifest.serialize());
        REQUIRE(cwPointOctreePack::open(path));

        //so only the generation rejects it
        replaceManifest(path, manifestBytesWithGeneration(manifest, kFormatGeneration - 1));
        CHECK_FALSE(cwPointOctreePack::open(path));
    }
}

TEST_CASE("cwPointOctreePack::node is empty out of range", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("tile.cwpack"));
    writePack(path, makeManifest());

    const std::shared_ptr<const cwPointOctreePack> pack = cwPointOctreePack::open(path);
    REQUIRE(pack);
    CHECK(pack->node(-1).isEmpty());
    CHECK(pack->node(pack->nodeCount()).isEmpty());
    CHECK_FALSE(pack->node(pack->nodeCount() - 1).isEmpty());
}

TEST_CASE("cwPointOctreePack keeps a short payload short", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("tile.cwpack"));
    const cwPointOctreeManifest manifest = makeManifest();
    constexpr int kShortIndex = 2;

    cwPointOctreePack::Writer writer(path);
    REQUIRE(writer.isOpen());
    for(int i = 0; i < manifest.nodes.size(); i++) {
        QByteArray payload = payloadFor(manifest, i);
        if(i == kShortIndex) {
            payload.chop(kShortPayloadBytes);
        }
        REQUIRE(writer.append(cellKey(manifest.nodes.at(i)), payload));
    }
    REQUIRE(writer.finish(manifest));

    const std::shared_ptr<const cwPointOctreePack> pack = cwPointOctreePack::open(path);
    REQUIRE(pack);
    CHECK(pack->node(kShortIndex).size() == manifest.nodes.at(kShortIndex).byteSize - kShortPayloadBytes);
    CHECK(pack->node(kShortIndex) == payloadFor(manifest, kShortIndex).chopped(kShortPayloadBytes));
}

TEST_CASE("packPath mirrors the LAZ directory under .cw_cache", "[PointOctree][PointOctreePack]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString name = QStringLiteral("tile.laz-octree%1.cwpack").arg(kFormatGeneration);
    const QString cacheDir = root.path() + QStringLiteral("/.cw_cache");

    SECTION("relative LAZ directory")
    {
        CHECK(packPath(root.path(), QStringLiteral("lidar/tiles/tile.laz"))
              == cacheDir + QStringLiteral("/lidar/tiles/") + name);
    }

    SECTION("relative LAZ file in the root")
    {
        CHECK(packPath(root.path(), QStringLiteral("tile.laz")) == cacheDir + QStringLiteral("/") + name);
    }

    SECTION("absolute LAZ directory inside the root")
    {
        CHECK(packPath(root.path(), root.path() + QStringLiteral("/lidar/tile.laz"))
              == cacheDir + QStringLiteral("/lidar/") + name);
    }

    SECTION("absolute LAZ directory outside the root")
    {
        CHECK(packPath(root.path(), QStringLiteral("/elsewhere/scans/tile.laz"))
              == cacheDir + QStringLiteral("/elsewhere/scans/") + name);
    }
}
