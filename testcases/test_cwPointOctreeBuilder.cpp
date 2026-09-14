//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QLoggingCategory>
#include <QMutex>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QThread>
#include <QVector>
#include <QVector3D>
#include <QtEndian>

//Std includes
#include <atomic>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

//Our includes
#include "cwCoordinateTransform.h"
#include "cwDiskCacher.h"
#include "cwGeoPoint.h"
#include "cwFutureManagerModel.h"
#include "cwFutureManagerToken.h"
#include "cwPointOctree.h"
#include "cwPointOctreeBuilder.h"
#include "cwPointOctreeManifest.h"
#include "cwTask.h"

#include "LazFixtureHelper.h"

using namespace cw::octree;

namespace {
    constexpr int kPassagePointCount = 300000;

    //Enough points to fill more than one node, so pass B writes before it can fail
    constexpr int kLockedCachePointCount = 50000;

    //Small enough that 300 k points chunk at depth 2, so pass C has real work
    constexpr qint64 kTwoLevelChunkTarget = 10000;

    constexpr double kTubeLength = 120.0;
    constexpr double kTubeRadius = 3.0;
    constexpr double kTubeWaveAmplitude = 6.0;
    constexpr double kTwoPi = 6.283185307179586;

    constexpr quint32 kSeed = 20260913;

    //The match grid is finer than any node's quantization step
    constexpr float kMatchCellSize = 0.01f;
    constexpr float kMinimumMatchTolerance = 1e-4f;
    constexpr float kToleranceSteps = 1.5f;

    constexpr float kBoundsEpsilon = 1e-3f;

    //The builder's floor on a bbox edge, so mean spacing has a formula to match
    constexpr double kMinimumSpacingExtent = 1e-3;
    constexpr float kSpacingTolerance = 1e-3f;

    //Two low distortion projections a kilometer apart: a real reprojection whose
    //coordinates stay small enough on both sides that float32 keeps every digit
    const QString kSourceFrame = QStringLiteral(
        "+proj=tmerc +lat_0=37.1832 +lon_0=-84.0947 +k=1 +x_0=0 +y_0=0 "
        "+datum=WGS84 +units=m +no_defs +type=crs");
    const QString kProjectFrame = QStringLiteral(
        "+proj=tmerc +lat_0=37.1900 +lon_0=-84.1000 +k=1 +x_0=0 +y_0=0 "
        "+datum=WGS84 +units=m +no_defs +type=crs");

    constexpr int kAxisCount = 3;
    constexpr int kNeighborSpan = 1;

    //21 bits per axis, so a cell coordinate of +-1 million packs without collisions
    constexpr int kCellBits = 21;
    constexpr qint64 kCellMask = (qint64(1) << kCellBits) - 1;

    //Enough workers to split the fixture several ways, and a count that gives
    //ranges of uneven length so a part boundary lands inside a cell
    constexpr int kEvenWorkerCount = 4;
    constexpr int kUnevenWorkerCount = 7;

    //kMinPointsPerDecodeWorker and kDecodeRangesPerWorker in the builder: what
    //an automatic worker count and its range count are made of
    constexpr int kPointsPerAutomaticWorker = 262144;
    constexpr int kRangesPerWorker = 4;
    constexpr int kAutomaticWorkerFixtureCount = 2 * kPointsPerAutomaticWorker;

    //A chunk target of one point drives the depth to kMaxChunkDepth, where the
    //8^5 cells leave kMaxChunkFiles room for only two ranges
    constexpr qint64 kDeepestChunkTarget = 1;
    constexpr int kCappedRangeCount = 2;
    constexpr int kDeepChunkPointCount = 50000;

    //A patch of degrees wide enough that reprojecting it bulges past the cube
    //around its transformed corners: the frame's central meridian runs up the
    //middle of the patch, so the middle of its south edge falls below every
    //corner and the provisional root has to be measured again
    constexpr double kPatchCenterLongitude = -84.0;
    constexpr double kPatchCenterLatitude = 80.0;
    constexpr double kPatchHalfLongitude = 15.0;
    constexpr double kPatchHalfLatitude = 4.0;
    constexpr double kPatchHeight = 10.0;
    constexpr int kPatchCornerCount = 4;

    const QString kGeographicSource =
        QStringLiteral("+proj=longlat +datum=WGS84 +no_defs +type=crs");
    const QString kPatchFrame = QStringLiteral(
        "+proj=tmerc +lat_0=80 +lon_0=-84 +k=1 +x_0=0 +y_0=0 "
        "+datum=WGS84 +units=m +no_defs +type=crs");

    //Pass A owns the first half of the progress bar and pass B the second
    constexpr double kPassAProgressFraction = 0.10;
    constexpr double kPassBProgressFraction = 0.55;

    constexpr int kCancelPollMs = 1;

    //The temp directories this process's builds make, as the builder names them
    int octreeTempDirCount()
    {
        const QStringList pattern {QStringLiteral("cwPointOctree-%1-*")
                                       .arg(QCoreApplication::applicationPid())};
        return int(QDir::temp().entryList(pattern, QDir::Dirs | QDir::NoDotAndDotDot).size());
    }

    //Cancels the build once its progress passes fraction of the bar. Returns
    //false where the build finished first, so a cancel that tested nothing is
    //loud rather than a silent pass.
    bool cancelAtProgress(QFuture<cwPointOctreeBuilder::Result>& future, double fraction)
    {
        bool reached = false;
        while(!future.isFinished()) {
            const int maximum = future.progressMaximum();
            if(maximum > 0 && future.progressValue() > int(double(maximum) * fraction)) {
                reached = true;
                break;
            }
            QThread::msleep(kCancelPollMs);
        }

        future.cancel();
        return reached;
    }

    //The workers a request of this many gets: pass A holds the count to the
    //pool, which keeps one slot for the orchestrator
    int expectedWorkers(int requested)
    {
        return std::min(requested, std::max(1, cwTask::threadPool()->maxThreadCount() - 1));
    }

    QMutex& profileLogMutex()
    {
        static QMutex mutex;
        return mutex;
    }

    QStringList& profileLogLines()
    {
        static QStringList lines;
        return lines;
    }

    std::atomic<QtMessageHandler>& chainedMessageHandler()
    {
        static std::atomic<QtMessageHandler> handler {nullptr};
        return handler;
    }

    void collectMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        {
            QMutexLocker locker(&profileLogMutex());
            profileLogLines().append(message);
        }

        //cw.profile.load is on only for this log, so its lines stay out of the
        //test output; everything else goes on to the handler that was there
        const bool isProfileLine =
            context.category != nullptr
            && QLatin1StringView(context.category) == QLatin1StringView("cw.profile.load");

        QtMessageHandler chained = chainedMessageHandler().load(std::memory_order_relaxed);
        if(!isProfileLine && chained != nullptr) {
            chained(type, context, message);
        }
    }

    //Turns cw.profile.load on and collects every message a build writes, from
    //whatever thread writes it, then puts the rules and the handler back
    class BuildLog
    {
    public:
        BuildLog()
        {
            {
                QMutexLocker locker(&profileLogMutex());
                profileLogLines().clear();
            }

            QLoggingCategory::setFilterRules(QStringLiteral("cw.profile.load.debug=true"));
            m_previous = qInstallMessageHandler(collectMessage);
            chainedMessageHandler().store(m_previous, std::memory_order_relaxed);
        }

        ~BuildLog()
        {
            qInstallMessageHandler(m_previous);
            chainedMessageHandler().store(nullptr, std::memory_order_relaxed);

            //Empty rules are the manual rules the constructor replaced
            QLoggingCategory::setFilterRules(QString());
        }

        BuildLog(const BuildLog&) = delete;
        BuildLog& operator=(const BuildLog&) = delete;

        bool hasLineContaining(const QString& text) const
        {
            QMutexLocker locker(&profileLogMutex());

            for(const QString& line : std::as_const(profileLogLines())) {
                if(line.contains(text)) {
                    return true;
                }
            }
            return false;
        }

        QStringList linesStartingWith(const QString& prefix) const
        {
            QMutexLocker locker(&profileLogMutex());

            QStringList found;
            for(const QString& line : std::as_const(profileLogLines())) {
                if(line.startsWith(prefix)) {
                    found.append(line);
                }
            }
            return found;
        }

    private:
        QtMessageHandler m_previous = nullptr;
    };

    //Two builds of the same file agree byte for byte: same manifest, same nodes
    void requireSameCache(const cwPointOctreeBuilder::Request& leftRequest,
                          const cwPointOctreeManifest& left,
                          const cwPointOctreeBuilder::Request& rightRequest,
                          const cwPointOctreeManifest& right)
    {
        REQUIRE(left.serialize() == right.serialize());

        const cwDiskCacher leftCacher {QDir(leftRequest.cacheRootPath)};
        const cwDiskCacher rightCacher {QDir(rightRequest.cacheRootPath)};

        for(int i = 0; i < left.nodes.size(); i++) {
            const QByteArray leftPayload =
                leftCacher.entry(nodeKey(leftRequest.path, left.fingerprint, left.nodeName(i)));
            const QByteArray rightPayload =
                rightCacher.entry(nodeKey(rightRequest.path, right.fingerprint, right.nodeName(i)));

            REQUIRE_FALSE(leftPayload.isEmpty());
            if(leftPayload != rightPayload) {
                FAIL("Node " << left.nodeName(i).toStdString()
                             << " differs between the two worker counts");
            }
        }
    }

    //A deterministic tube of points, the shape a survey passage scan has
    QVector<QVector3D> passagePoints(int count)
    {
        QVector<QVector3D> points;
        points.reserve(count);

        quint32 state = kSeed;
        const auto nextUnit = [&state]() {
            state = state * 1664525u + 1013904223u;
            return double(state >> 8) / double(1 << 24);
        };

        for(int i = 0; i < count; i++) {
            const double along = nextUnit() * kTubeLength;
            const double angle = nextUnit() * kTwoPi;
            const double radius = kTubeRadius * std::sqrt(nextUnit());

            const double centerY = kTubeWaveAmplitude * std::sin(along / kTubeLength * kTwoPi);
            points.append(QVector3D(float(along),
                                    float(centerY + radius * std::cos(angle)),
                                    float(radius * std::sin(angle))));
        }

        return points;
    }

    //Degrees over the patch, corners first so the header box is the patch itself
    //and the point that escapes the provisional root is in the cloud
    QVector<QVector3D> geographicPatchPoints(int count)
    {
        QVector<QVector3D> points;
        points.reserve(count);

        const double minimumLongitude = kPatchCenterLongitude - kPatchHalfLongitude;
        const double minimumLatitude = kPatchCenterLatitude - kPatchHalfLatitude;

        for(int corner = 0; corner < kPatchCornerCount; corner++) {
            points.append(QVector3D(
                float(minimumLongitude + ((corner & 1) ? 2.0 * kPatchHalfLongitude : 0.0)),
                float(minimumLatitude + ((corner & 2) ? 2.0 * kPatchHalfLatitude : 0.0)),
                0.0f));
        }

        //The middle of the south edge, on the frame's central meridian
        points.append(QVector3D(float(kPatchCenterLongitude), float(minimumLatitude), 0.0f));

        quint32 state = kSeed;
        const auto nextUnit = [&state]() {
            state = state * 1664525u + 1013904223u;
            return double(state >> 8) / double(1 << 24);
        };

        while(points.size() < count) {
            points.append(QVector3D(
                float(minimumLongitude + nextUnit() * 2.0 * kPatchHalfLongitude),
                float(minimumLatitude + nextUnit() * 2.0 * kPatchHalfLatitude),
                float(nextUnit() * kPatchHeight)));
        }

        return points;
    }

    qint64 matchCellKey(int x, int y, int z)
    {
        return ((qint64(x) & kCellMask) << (2 * kCellBits))
               | ((qint64(y) & kCellMask) << kCellBits)
               | (qint64(z) & kCellMask);
    }

    //A uniform grid over the source points, so matching a node point is a handful of lookups
    class PointGrid
    {
    public:
        explicit PointGrid(const QVector<QVector3D>& points) :
            m_points(points)
        {
            for(qsizetype i = 0; i < points.size(); i++) {
                m_cells[keyOf(points.at(i))].append(i);
            }
        }

        bool hasPointNear(const QVector3D& point, float tolerance) const
        {
            for(int x = -kNeighborSpan; x <= kNeighborSpan; x++) {
                for(int y = -kNeighborSpan; y <= kNeighborSpan; y++) {
                    for(int z = -kNeighborSpan; z <= kNeighborSpan; z++) {
                        const qint64 key = matchCellKey(cellOf(point.x()) + x,
                                                        cellOf(point.y()) + y,
                                                        cellOf(point.z()) + z);
                        const auto found = m_cells.constFind(key);
                        if(found == m_cells.constEnd()) {
                            continue;
                        }

                        for(qsizetype index : found.value()) {
                            if(isWithin(m_points.at(index), point, tolerance)) {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

    private:
        static int cellOf(float value)
        {
            return int(std::floor(value / kMatchCellSize));
        }

        static qint64 keyOf(const QVector3D& point)
        {
            return matchCellKey(cellOf(point.x()), cellOf(point.y()), cellOf(point.z()));
        }

        static bool isWithin(const QVector3D& a, const QVector3D& b, float tolerance)
        {
            for(int axis = 0; axis < kAxisCount; axis++) {
                if(std::abs(a[axis] - b[axis]) > tolerance) {
                    return false;
                }
            }
            return true;
        }

        QVector<QVector3D> m_points;
        QHash<qint64, QVector<qsizetype>> m_cells;
    };

    QVector<QVector3D> readPoints(const QString& path)
    {
        QVector<QVector3D> points;
        const LazFileContents contents = readLazFile(path);
        points.reserve(contents.points.size());
        for(const LazAttributePoint& point : contents.points) {
            points.append(point.position);
        }
        return points;
    }

    //The same reprojection the builder runs, so the comparison is against the
    //points the octree is actually made of
    QVector<QVector3D> reprojected(const QVector<QVector3D>& points,
                                   const QString& sourceCS,
                                   const QString& frameCS)
    {
        const cwCoordinateTransform transform(sourceCS, frameCS);
        REQUIRE(transform.isValid());
        REQUIRE_FALSE(transform.isIdentity());

        QVector<cwGeoPoint> geoPoints;
        geoPoints.reserve(points.size());
        for(const QVector3D& point : points) {
            geoPoints.append(cwGeoPoint(point.x(), point.y(), point.z()));
        }

        transform.transformInPlace(geoPoints.data(), geoPoints.size());

        QVector<QVector3D> framePoints;
        framePoints.reserve(geoPoints.size());
        for(const cwGeoPoint& geoPoint : std::as_const(geoPoints)) {
            framePoints.append(geoPoint.toVector3D());
        }
        return framePoints;
    }

    cwPointOctreeBuilder::Result waitForBuild(QFuture<cwPointOctreeBuilder::Result> future)
    {
        future.waitForFinished();
        REQUIRE(future.resultCount() == 1);
        return future.result();
    }

    //Every invariant the on-disk octree has to hold, whatever the chunk depth
    void verifyOctree(const cwPointOctreeManifest& manifest,
                      const cwPointOctreeBuilder::Request& request,
                      const QVector<QVector3D>& sourcePoints)
    {
        REQUIRE(manifest.isValid());
        REQUIRE(manifest.pointCount == sourcePoints.size());
        REQUIRE(manifest.fingerprint == cwPointOctreeBuilder::fingerprintFor(request));
        REQUIRE(manifest.meanSpacingXY > 0.0f);

        QVector3D minimum(std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::infinity());
        QVector3D maximum = -minimum;
        for(const QVector3D& point : sourcePoints) {
            for(int axis = 0; axis < kAxisCount; axis++) {
                minimum[axis] = std::min(minimum[axis], point[axis]);
                maximum[axis] = std::max(maximum[axis], point[axis]);
            }
        }

        for(int axis = 0; axis < kAxisCount; axis++) {
            REQUIRE(std::abs(manifest.bboxMin[axis] - minimum[axis]) < kBoundsEpsilon);
            REQUIRE(std::abs(manifest.bboxMax[axis] - maximum[axis]) < kBoundsEpsilon);
        }

        const double dx = std::max(double(maximum.x() - minimum.x()), kMinimumSpacingExtent);
        const double dy = std::max(double(maximum.y() - minimum.y()), kMinimumSpacingExtent);
        const float expectedSpacing = float(std::sqrt(dx * dy / double(manifest.pointCount)));
        REQUIRE(std::abs(manifest.meanSpacingXY - expectedSpacing) < kSpacingTolerance * expectedSpacing);

        const cwDiskCacher cacher {QDir(request.cacheRootPath)};
        const PointGrid grid(sourcePoints);

        qint64 totalPoints = 0;
        for(int i = 0; i < manifest.nodes.size(); i++) {
            const cwPointOctreeNode& node = manifest.nodes.at(i);
            totalPoints += node.pointCount;

            const cwDiskCacher::Key key =
                nodeKey(request.path, manifest.fingerprint, manifest.nodeName(i));
            REQUIRE(cacher.hasEntry(key));

            const QByteArray payload = cacher.entry(key);
            REQUIRE(payload.size() == qsizetype(node.pointCount) * kBytesPerPoint);
            REQUIRE(node.byteSize == qint64(node.pointCount) * kBytesPerPoint);

            //Depth first with the root at 0: a node's children follow it in
            //octant order and the first of them sits immediately after it
            int firstChild = -1;
            int previousChild = -1;
            for(int octant = 0; octant < kChildCount; octant++) {
                const int child = node.children.at(octant);
                if(child < 0) {
                    continue;
                }

                REQUIRE(child > i);
                REQUIRE(child > previousChild);
                REQUIRE(manifest.nodes.at(child).level == node.level + 1);
                previousChild = child;

                if(firstChild < 0) {
                    firstChild = child;
                }
            }

            if(firstChild >= 0) {
                REQUIRE(firstChild == i + 1);
            }

            const QBox3D bounds = manifest.nodeBounds(i);
            const float step = float(bounds.size().x() / kQuantMax);
            const float tolerance = std::max(kToleranceSteps * step, kMinimumMatchTolerance);

            for(quint32 pointIndex = 0; pointIndex < node.pointCount; pointIndex++) {
                const char* raw = payload.constData() + qsizetype(pointIndex) * kBytesPerPoint;
                QuantizedPoint quantized;
                quantized.x = qFromLittleEndian<quint16>(raw);
                quantized.y = qFromLittleEndian<quint16>(raw + sizeof(quint16));
                quantized.z = qFromLittleEndian<quint16>(raw + 2 * sizeof(quint16));

                if(!grid.hasPointNear(dequantize(quantized, bounds), tolerance)) {
                    FAIL("Node " << manifest.nodeName(i).toStdString()
                                 << " holds a point that is in no source point's neighborhood");
                }
            }
        }

        REQUIRE(totalPoints == manifest.pointCount);
    }
}

TEST_CASE("cwPointOctreeBuilder: chunkDepthFor spreads the cloud over chunk sized cells",
          "[PointOctree][PointOctreeBuilder]") {
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(0) == 0);
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(1) == 0);
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(kChunkTargetPoints) == 0);
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(kChunkTargetPoints * 8) == 1);
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(100000000) == 2);
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(kChunkTargetPoints * 64 + 1) == 3);
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(std::numeric_limits<qint64>::max()) == kMaxChunkDepth);

    //The target is a real parameter, so a small one deepens the tree
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(kPassagePointCount, kTwoLevelChunkTarget) == 2);
}

TEST_CASE("cwPointOctreeBuilder: builds a one chunk octree and caches every node",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-one-chunk"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path()
    };

    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(kPassagePointCount) == 0);
    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());

    const QVector<QVector3D> sourcePoints = readPoints(path);
    REQUIRE(sourcePoints.size() == kPassagePointCount);

    const cwPointOctreeManifest manifest = result.value();
    verifyOctree(manifest, request, sourcePoints);

    const std::optional<cwPointOctreeManifest> cached =
        cwPointOctreeBuilder::cachedManifest(request);
    REQUIRE(cached.has_value());
    REQUIRE(cached->fingerprint == manifest.fingerprint);
    REQUIRE(cached->pointCount == manifest.pointCount);
    REQUIRE(cached->rootSize == manifest.rootSize);
    REQUIRE(cached->rootMin == manifest.rootMin);
    REQUIRE(cached->meanSpacingXY == manifest.meanSpacingXY);
    REQUIRE(cached->bboxMin == manifest.bboxMin);
    REQUIRE(cached->bboxMax == manifest.bboxMax);

    //The whole node table survives the round trip, field by field
    REQUIRE(cached->nodes.size() == manifest.nodes.size());
    for(int i = 0; i < manifest.nodes.size(); i++) {
        const cwPointOctreeNode& built = manifest.nodes.at(i);
        const cwPointOctreeNode& read = cached->nodes.at(i);
        REQUIRE(read.level == built.level);
        REQUIRE(read.x == built.x);
        REQUIRE(read.y == built.y);
        REQUIRE(read.z == built.z);
        REQUIRE(read.pointCount == built.pointCount);
        REQUIRE(read.byteSize == built.byteSize);
        REQUIRE(read.children == built.children);
    }

    verifyOctree(*cached, request, sourcePoints);
}

TEST_CASE("cwPointOctreeBuilder: a small chunk target builds the same octree through pass C",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-deep"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget
    };

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());

    const cwPointOctreeManifest manifest = result.value();

    //Pass C ran: the tree reaches past the chunk depth, and the root came from the chunk roots
    int deepestLevel = 0;
    for(const cwPointOctreeNode& node : manifest.nodes) {
        deepestLevel = std::max(deepestLevel, node.level);
    }
    REQUIRE(deepestLevel > 2);
    REQUIRE(manifest.nodes.constFirst().pointCount > 0);

    verifyOctree(manifest, request, readPoints(path));
    REQUIRE(cwPointOctreeBuilder::cachedManifest(request).has_value());
}

TEST_CASE("cwPointOctreeBuilder: every cached node is written in Morton order",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-morton"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget
    };

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());

    const cwPointOctreeManifest manifest = result.value();
    REQUIRE(manifest.nodes.size() > 1);

    //The generation is part of every id, so an older cache is never read back
    REQUIRE(manifestKey(path, manifest.fingerprint).id.contains(QStringLiteral("octree2")));

    const cwDiskCacher cacher {QDir(request.cacheRootPath)};
    for(int i = 0; i < manifest.nodes.size(); i++) {
        const QByteArray payload =
            cacher.entry(nodeKey(path, manifest.fingerprint, manifest.nodeName(i)));
        REQUIRE(payload.size() == manifest.nodes.at(i).byteSize);

        quint64 previous = 0;
        const qsizetype count = payload.size() / kBytesPerPoint;
        for(qsizetype point = 0; point < count; point++) {
            const char* axes = payload.constData() + point * kBytesPerPoint;
            constexpr int kAxisBytes = int(sizeof(quint16));
            const QuantizedPoint quantized {
                qFromLittleEndian<quint16>(axes),
                qFromLittleEndian<quint16>(axes + kAxisBytes),
                qFromLittleEndian<quint16>(axes + 2 * kAxisBytes),
                0
            };

            const quint64 key = mortonKey(quantized);
            REQUIRE(key >= previous);
            previous = key;
        }
    }
}

TEST_CASE("cwPointOctreeBuilder: a cache from an older format generation rebuilds",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-generation"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kLockedCachePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path()
    };

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());
    REQUIRE(cwPointOctreeBuilder::cachedManifest(request).has_value());

    const cwPointOctreeManifest manifest = result.value();
    cwDiskCacher cacher {QDir(request.cacheRootPath)};

    //Move the whole cache back a generation: the same bytes under the ids the
    //previous format wrote them with
    const QString current = QStringLiteral("-octree")
                            + QString::number(kFormatGeneration)
                            + QStringLiteral("-");
    const QString previous = QStringLiteral("-octree")
                             + QString::number(kFormatGeneration - 1)
                             + QStringLiteral("-");

    const auto moveBack = [&](const cwDiskCacher::Key& key) {
        cwDiskCacher::Key older = key;
        older.id = QString(key.id).replace(current, previous);
        cacher.insert(older, cacher.entry(key));
        REQUIRE(QFile::remove(cacher.filePath(key)));
    };

    for(int i = 0; i < manifest.nodes.size(); i++) {
        moveBack(nodeKey(path, manifest.fingerprint, manifest.nodeName(i)));
    }
    moveBack(manifestKey(path, manifest.fingerprint));

    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());
}

TEST_CASE("cwPointOctreeBuilder: a missing node makes the cached manifest a rebuild",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-evicted"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget
    };

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());
    REQUIRE(cwPointOctreeBuilder::cachedManifest(request).has_value());

    const cwPointOctreeManifest manifest = result.value();
    REQUIRE(manifest.nodes.size() > 1);

    const cwDiskCacher cacher {QDir(request.cacheRootPath)};
    const int lastNode = manifest.nodes.size() - 1;
    REQUIRE(QFile::remove(cacher.filePath(
        nodeKey(path, manifest.fingerprint, manifest.nodeName(lastNode)))));

    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());
}

TEST_CASE("cwPointOctreeBuilder: the frame CS is part of the fingerprint",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-frame"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(1000), utmZoneWkt(17, -81)));

    const cwPointOctreeBuilder::Request local {
        .path = path,
        .cacheRootPath = tempDir.path()
    };

    cwPointOctreeBuilder::Request reframed = local;
    reframed.frameCS = utmZoneWkt(16, -87);

    const QString localFingerprint = cwPointOctreeBuilder::fingerprintFor(local);
    REQUIRE_FALSE(localFingerprint.isEmpty());
    REQUIRE(localFingerprint != cwPointOctreeBuilder::fingerprintFor(reframed));
}

TEST_CASE("cwPointOctreeBuilder: an overriding source CS builds in the frame CS",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    //No embedded CS, so the override is the only thing that can name the source
    const QString path = tempLazPath(tempDir, QStringLiteral("octree-reprojected"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .sourceCSOverride = kSourceFrame,
        .frameCS = kProjectFrame,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget
    };

    cwPointOctreeBuilder::Request embedded = request;
    embedded.sourceCSOverride.clear();
    REQUIRE(cwPointOctreeBuilder::fingerprintFor(request)
            != cwPointOctreeBuilder::fingerprintFor(embedded));

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());

    const QVector<QVector3D> framePoints =
        reprojected(readPoints(path), kSourceFrame, kProjectFrame);

    const cwPointOctreeManifest manifest = result.value();
    verifyOctree(manifest, request, framePoints);

    const std::optional<cwPointOctreeManifest> cached =
        cwPointOctreeBuilder::cachedManifest(request);
    REQUIRE(cached.has_value());
    verifyOctree(*cached, request, framePoints);

    //The octree is stored in the frame CS, so another frame is another octree
    cwPointOctreeBuilder::Request reframed = request;
    reframed.frameCS = kSourceFrame;
    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(reframed).has_value());
}

TEST_CASE("cwPointOctreeBuilder: a cache it cannot write fails with CacheWriteFailed",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString cacheRoot = tempDir.filePath(QStringLiteral("read-only-cache"));
    REQUIRE(QDir().mkpath(cacheRoot));

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-locked-cache"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kLockedCachePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = cacheRoot
    };

    REQUIRE(QFile::setPermissions(cacheRoot,
                                  QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    //Running as a user who writes anywhere (root in some containers) has nothing to prove
    QFile probe(QDir(cacheRoot).filePath(QStringLiteral("probe")));
    const bool cacheIsWritable = probe.open(QIODevice::WriteOnly);
    probe.close();

    if(!cacheIsWritable) {
        const cwPointOctreeBuilder::Result result =
            waitForBuild(cwPointOctreeBuilder::build(request));
        REQUIRE(result.hasError());
        REQUIRE(result.errorCode() == cwPointOctreeBuilder::CacheWriteFailed);
        REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());
    }

    REQUIRE(QFile::setPermissions(cacheRoot,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));
}

TEST_CASE("cwPointOctreeBuilder: a missing file fails with OpenFailed",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const cwPointOctreeBuilder::Request request {
        .path = tempDir.filePath(QStringLiteral("does-not-exist.laz")),
        .cacheRootPath = tempDir.path()
    };

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE(result.hasError());
    REQUIRE(result.errorCode() == cwPointOctreeBuilder::OpenFailed);
    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());
}

TEST_CASE("cwPointOctreeBuilder: a cancelled build writes no manifest",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-cancel"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget
    };

    QFuture<cwPointOctreeBuilder::Result> future = cwPointOctreeBuilder::build(request);
    future.cancel();
    future.waitForFinished();

    REQUIRE(future.isCanceled());

    //QPromise drops a result added after the cancel, so a delivered one is the only one to check
    const QList<cwPointOctreeBuilder::Result> results = future.results();
    if(!results.isEmpty()) {
        REQUIRE(results.constFirst().errorCode() == cwPointOctreeBuilder::Cancelled);
    }

    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());
}

TEST_CASE("cwPointOctreeBuilder: the build shows up as a job and finishes its progress",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-progress"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget
    };

    cwFutureManagerModel manager;
    cwFutureManagerToken token(&manager);

    QSignalSpy rowsInsertedSpy(&manager, &QAbstractItemModel::rowsInserted);

    QFuture<cwPointOctreeBuilder::Result> future = cwPointOctreeBuilder::build(request);
    token.addJob(future, QStringLiteral("Building the octree of the test passage"));

    REQUIRE(rowsInsertedSpy.size() >= 1);

    while(!future.isFinished()) {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    QCoreApplication::processEvents();

    REQUIRE(future.progressMaximum() > 0);
    REQUIRE(future.progressValue() == future.progressMaximum());
    REQUIRE(manager.rowCount() == 0);
}

TEST_CASE("cwPointOctreeBuilder: the decode worker count never changes the bytes it writes",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-workers"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    //One cell holding every part, then cells whose points straddle range bounds
    const qint64 chunkTarget =
        GENERATE(cw::octree::kChunkTargetPoints, kTwoLevelChunkTarget);

    const auto buildWith = [&](int workers) {
        const QString cacheRoot =
            tempDir.filePath(QStringLiteral("cache-%1-%2").arg(chunkTarget).arg(workers));
        REQUIRE(QDir().mkpath(cacheRoot));

        const cwPointOctreeBuilder::Request request {
            .path = path,
            .cacheRootPath = cacheRoot,
            .chunkTargetPoints = chunkTarget,
            .decodeWorkerCount = workers
        };

        BuildLog log;

        const cwPointOctreeBuilder::Result result =
            waitForBuild(cwPointOctreeBuilder::build(request));
        REQUIRE_FALSE(result.hasError());

        //The request's worker count, held to the pool, is what pass A decoded with
        const QStringList passA = log.linesStartingWith(QStringLiteral("build passA"));
        REQUIRE(passA.size() == 1);
        REQUIRE(passA.constFirst().contains(
            QStringLiteral("workers=%1").arg(expectedWorkers(workers))));

        return std::make_pair(request, result.value());
    };

    const auto single = buildWith(1);
    verifyOctree(single.second, single.first, readPoints(path));

    //Seven workers give ranges of uneven length, so a part boundary lands mid cell
    for(int workers : {kEvenWorkerCount, kUnevenWorkerCount}) {
        const auto many = buildWith(workers);
        requireSameCache(single.first, single.second, many.first, many.second);
    }
}

TEST_CASE("cwPointOctreeBuilder: a cancel in either pass leaves no manifest and no temp directory",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-parallel-cancel"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget,
        .decodeWorkerCount = kEvenWorkerCount
    };

    //Pass A owns the first half of the bar, so these cancel in one pass each
    const double fraction = GENERATE(kPassAProgressFraction, kPassBProgressFraction);

    QFuture<cwPointOctreeBuilder::Result> future = cwPointOctreeBuilder::build(request);
    const bool cancelled = cancelAtProgress(future, fraction);
    future.waitForFinished();

    //A build that beat the cancel proves nothing about cancelling, so say so
    REQUIRE(cancelled);

    //QPromise drops a result added after the cancel, so a delivered one is the only one to check
    const QList<cwPointOctreeBuilder::Result> results = future.results();
    if(!results.isEmpty()) {
        REQUIRE(results.constFirst().errorCode() == cwPointOctreeBuilder::Cancelled);
    }

    REQUIRE_FALSE(cwPointOctreeBuilder::cachedManifest(request).has_value());
    REQUIRE(octreeTempDirCount() == 0);
}

TEST_CASE("cwPointOctreeBuilder: a point wise compressed file decodes on one worker",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    //Seeking a point wise LAZ decodes from the start, so ranges would reread the cloud
    const QString path = tempLazPath(tempDir, QStringLiteral("octree-pointwise"));
    REQUIRE(writeSyntheticLazFile(path,
                                  passagePoints(kLockedCachePointCount),
                                  QString(),
                                  LazCompression::PointWise));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .decodeWorkerCount = kEvenWorkerCount
    };

    BuildLog log;

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());

    verifyOctree(result.value(), request, readPoints(path));

    const QStringList passA = log.linesStartingWith(QStringLiteral("build passA"));
    REQUIRE(passA.size() == 1);
    REQUIRE(passA.constFirst().contains(QStringLiteral("workers=1")));
    REQUIRE(passA.constFirst().contains(QStringLiteral("ranges=1")));

    //The other three lines of a build are there too, so a run reports every pass
    REQUIRE(log.linesStartingWith(QStringLiteral("build passB")).size() == 1);
    REQUIRE(log.linesStartingWith(QStringLiteral("build passC")).size() == 1);
    REQUIRE(log.linesStartingWith(QStringLiteral("build total")).size() == 1);
}

TEST_CASE("cwPointOctreeBuilder: a parallel build removes its temp directory however it ends",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-parallel-temp"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kPassagePointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kTwoLevelChunkTarget,
        .decodeWorkerCount = kEvenWorkerCount
    };

    SECTION("a build that succeeds") {
        const cwPointOctreeBuilder::Result result =
            waitForBuild(cwPointOctreeBuilder::build(request));
        REQUIRE_FALSE(result.hasError());
    }

    SECTION("a file it cannot open") {
        cwPointOctreeBuilder::Request missing = request;
        missing.path = tempDir.filePath(QStringLiteral("does-not-exist.laz"));

        const cwPointOctreeBuilder::Result result =
            waitForBuild(cwPointOctreeBuilder::build(missing));
        REQUIRE(result.errorCode() == cwPointOctreeBuilder::OpenFailed);
    }

    SECTION("a cache it cannot write") {
        const QString cacheRoot = tempDir.filePath(QStringLiteral("read-only-cache"));
        REQUIRE(QDir().mkpath(cacheRoot));

        cwPointOctreeBuilder::Request locked = request;
        locked.cacheRootPath = cacheRoot;

        REQUIRE(QFile::setPermissions(cacheRoot,
                                      QFileDevice::ReadOwner | QFileDevice::ExeOwner));

        //Running as a user who writes anywhere (root in some containers) has nothing to prove
        QFile probe(QDir(cacheRoot).filePath(QStringLiteral("probe")));
        const bool cacheIsWritable = probe.open(QIODevice::WriteOnly);
        probe.close();

        if(!cacheIsWritable) {
            const cwPointOctreeBuilder::Result result =
                waitForBuild(cwPointOctreeBuilder::build(locked));
            REQUIRE(result.errorCode() == cwPointOctreeBuilder::CacheWriteFailed);
        }

        REQUIRE(QFile::setPermissions(cacheRoot,
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner));
    }

    SECTION("a build that is cancelled") {
        QFuture<cwPointOctreeBuilder::Result> future = cwPointOctreeBuilder::build(request);
        REQUIRE(cancelAtProgress(future, kPassAProgressFraction));
        future.waitForFinished();
    }

    REQUIRE(octreeTempDirCount() == 0);
}

TEST_CASE("cwPointOctreeBuilder: a reprojection that escapes the provisional root chunks again",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-escaped-root"));
    REQUIRE(writeSyntheticLazFile(path, geographicPatchPoints(kPassagePointCount)));

    const auto buildWith = [&](int workers) {
        const QString cacheRoot = tempDir.filePath(QStringLiteral("cache-escaped-%1").arg(workers));
        REQUIRE(QDir().mkpath(cacheRoot));

        const cwPointOctreeBuilder::Request request {
            .path = path,
            .sourceCSOverride = kGeographicSource,
            .frameCS = kPatchFrame,
            .cacheRootPath = cacheRoot,
            .chunkTargetPoints = kTwoLevelChunkTarget,
            .decodeWorkerCount = workers
        };

        BuildLog log;

        const cwPointOctreeBuilder::Result result =
            waitForBuild(cwPointOctreeBuilder::build(request));
        REQUIRE_FALSE(result.hasError());

        //The retry is what the fixture is for: without it there is nothing here to test
        REQUIRE(log.hasLineContaining(QStringLiteral("reprojection pushed points outside")));

        //Every point is chunked once, so a part file the retry left behind shows up here
        REQUIRE(result.value().pointCount == kPassagePointCount);

        return std::make_pair(request, result.value());
    };

    const auto single = buildWith(1);
    const auto many = buildWith(kEvenWorkerCount);
    requireSameCache(single.first, single.second, many.first, many.second);
}

TEST_CASE("cwPointOctreeBuilder: the automatic worker count follows the point count",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-auto-workers"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kAutomaticWorkerFixtureCount)));

    //No decodeWorkerCount, so pass A picks the count itself
    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path()
    };

    BuildLog log;

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());

    const int workers = expectedWorkers(kAutomaticWorkerFixtureCount / kPointsPerAutomaticWorker);
    const int ranges = workers > 1 ? kRangesPerWorker * workers : 1;

    const QStringList passA = log.linesStartingWith(QStringLiteral("build passA"));
    REQUIRE(passA.size() == 1);
    REQUIRE(passA.constFirst().contains(QStringLiteral("workers=%1").arg(workers)));
    REQUIRE(passA.constFirst().contains(QStringLiteral("ranges=%1").arg(ranges)));
}

TEST_CASE("cwPointOctreeBuilder: the chunk file cap bounds the decode ranges",
          "[PointOctree][PointOctreeBuilder]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    //A cell per point asks for more cells than there are, so the depth clamps
    REQUIRE(cwPointOctreeBuilder::chunkDepthFor(kDeepChunkPointCount, kDeepestChunkTarget)
            == kMaxChunkDepth);

    const QString path = tempLazPath(tempDir, QStringLiteral("octree-range-cap"));
    REQUIRE(writeSyntheticLazFile(path, passagePoints(kDeepChunkPointCount)));

    const cwPointOctreeBuilder::Request request {
        .path = path,
        .cacheRootPath = tempDir.path(),
        .chunkTargetPoints = kDeepestChunkTarget,
        .decodeWorkerCount = kEvenWorkerCount
    };

    BuildLog log;

    const cwPointOctreeBuilder::Result result =
        waitForBuild(cwPointOctreeBuilder::build(request));
    REQUIRE_FALSE(result.hasError());

    const int workers = expectedWorkers(kEvenWorkerCount);
    const int ranges = workers > 1 ? kCappedRangeCount : 1;

    const QStringList passA = log.linesStartingWith(QStringLiteral("build passA"));
    REQUIRE(passA.size() == 1);
    REQUIRE(passA.constFirst().contains(QStringLiteral("workers=%1").arg(workers)));
    REQUIRE(passA.constFirst().contains(QStringLiteral("ranges=%1").arg(ranges)));
}
