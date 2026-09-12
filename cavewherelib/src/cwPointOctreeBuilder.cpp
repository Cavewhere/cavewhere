// cwPointOctreeBuilder.cpp
#include "cwPointOctreeBuilder.h"

//Qt includes
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QPromise>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <QVector>

//Std includes
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>

//Our includes
#include "cwConcurrent.h"
#include "cwCoordinateTransform.h"
#include "cwDiskCacher.h"
#include "cwGeoPoint.h"
#include "cwLazLoader.h"
#include "cwPointOctreeSampler.h"
#include "cwProgressReporter.h"
#include "cwTask.h"

// LAStools / LASlib
#include <LASlib/lasreader.hpp>

using namespace cw::octree;

namespace {

    using Result = cwPointOctreeBuilder::Result;
    using Promise = QPromise<Result>;
    using Reporter = cwProgressReporter<Promise>;

    constexpr int kAxisCount = 3;

    constexpr int kBoxCornerCount = 8;

    //Points transformed and written per batch, so transformInPlace amortizes PROJ's per-call overhead
    constexpr qsizetype kPointChunkSize = 64 * 1024;

    //Native float32 x, y, z per point in a temp chunk file
    constexpr qsizetype kTempBytesPerPoint = kAxisCount * qsizetype(sizeof(float));

    //Pass A holds at most this much unwritten chunk data before flushing every buffer
    constexpr qint64 kMaxBufferedChunkBytes = 32 * 1024 * 1024;

    //The root cube of a cloud with no extent, so quantization still has a range
    constexpr double kMinimumRootSize = 1.0;

    //Floor on a bbox edge, so a vertical-only scan still has a mean spacing
    constexpr double kMinimumSpacingExtent = 1e-3;

    constexpr int kMapPollMs = 30;

    //A top cell packs into one key: level in the high bits, then x, y, z
    constexpr int kLevelShift = 60;
    constexpr int kXShift = 40;
    constexpr int kYShift = 20;
    constexpr quint64 kCoordinateMask = (quint64(1) << kYShift) - 1;

    quint64 cellKey(const Cell& cell)
    {
        return (quint64(cell.level) << kLevelShift)
               | (quint64(cell.x) << kXShift)
               | (quint64(cell.y) << kYShift)
               | quint64(cell.z);
    }

    Cell cellFromKey(quint64 key)
    {
        return Cell {
            int(key >> kLevelShift),
            quint32((key >> kXShift) & kCoordinateMask),
            quint32((key >> kYShift) & kCoordinateMask),
            quint32(key & kCoordinateMask)
        };
    }

    Cell parentOf(const Cell& cell)
    {
        return Cell {cell.level - 1, cell.x / 2, cell.y / 2, cell.z / 2};
    }

    //The octant a cell occupies in its parent, bit 0 is x, 1 is y, 2 is z
    int octantInParent(const Cell& cell)
    {
        return int(cell.x & 1) | int((cell.y & 1) << 1) | int((cell.z & 1) << 2);
    }

    //The Potree name of a cell: one octant digit per level, root first
    QString nodeNameForCell(const Cell& cell)
    {
        QVector<int> octantPath;
        octantPath.reserve(cell.level);

        for(int level = cell.level; level >= 1; level--) {
            const int shift = level - 1;
            const int octant = int((cell.x >> shift) & 1)
                               | int(((cell.y >> shift) & 1) << 1)
                               | int(((cell.z >> shift) & 1) << 2);
            octantPath.append(octant);
        }

        return nodeName(octantPath);
    }

    /**
     * Inserts data and reports whether the entry landed. cwDiskCacher::insert
     * is void, so a full or read-only cache shows up as a missing file.
     */
    bool writeEntry(cwDiskCacher& cacher, const cwDiskCacher::Key& key, const QByteArray& data)
    {
        cacher.insert(key, data);
        return cacher.hasEntry(key);
    }

    struct BoundsTracker {
        QVector3D minimum {std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::infinity()};
        QVector3D maximum {-std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity()};

        void add(const QVector3D& point)
        {
            for(int axis = 0; axis < kAxisCount; axis++) {
                minimum[axis] = std::min(minimum[axis], point[axis]);
                maximum[axis] = std::max(maximum[axis], point[axis]);
            }
        }
    };

    struct RootCube {
        QVector3D minimum;
        double size = kMinimumRootSize;
    };

    //The padded cube around bounds, centered on it
    RootCube cubeAround(const QVector3D& minimum, const QVector3D& maximum)
    {
        double longest = 0.0;
        for(int axis = 0; axis < kAxisCount; axis++) {
            longest = std::max(longest, double(maximum[axis]) - double(minimum[axis]));
        }

        RootCube cube;
        cube.size = std::max(longest * (1.0 + 2.0 * kRootPaddingFraction), kMinimumRootSize);

        const QVector3D center = (minimum + maximum) * 0.5f;
        const float half = float(cube.size * 0.5);
        cube.minimum = center - QVector3D(half, half, half);
        return cube;
    }

    //The enclosing box of the header box's eight corners in the frame CS
    RootCube provisionalRoot(const cwGeoPoint& sourceMin,
                             const cwGeoPoint& sourceMax,
                             const cwCoordinateTransform& transform)
    {
        BoundsTracker bounds;

        for(int corner = 0; corner < kBoxCornerCount; corner++) {
            const cwGeoPoint source((corner & 1) ? sourceMax.x : sourceMin.x,
                                    (corner & 2) ? sourceMax.y : sourceMin.y,
                                    (corner & 4) ? sourceMax.z : sourceMin.z);
            bounds.add(transform.transform(source).toVector3D());
        }

        return cubeAround(bounds.minimum, bounds.maximum);
    }

    struct ChunkBuffer {
        QString path;
        QByteArray buffer;
        qint64 pointCount = 0;
    };

    struct PassAResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        qint64 pointCount = 0;
        QVector3D bboxMin;
        QVector3D bboxMax;
        bool escapedRoot = false;
        QHash<quint64, ChunkBuffer> chunks;
    };

    /**
     * Pass A: streams the file once, transforms every point into the frame CS,
     * and appends its native float32 xyz to the temp file of its cell at depth.
     * The chunk files are read back by the same process, so native byte order
     * never leaves this build.
     */
    PassAResult chunkPoints(const cwPointOctreeBuilder::Request& request,
                            const QString& sourceCS,
                            const RootCube& root,
                            int depth,
                            const QDir& tempDir,
                            Promise& promise,
                            Reporter& progress)
    {
        PassAResult result;

        const QByteArray pathBytes = request.path.toUtf8();
        LASreadOpener opener;
        opener.set_file_name(pathBytes.constData(), FALSE);
        LASreader* reader = opener.open();
        if(reader == nullptr) {
            result.errorCode = cwPointOctreeBuilder::OpenFailed;
            result.errorMessage = QStringLiteral("Could not open %1 for read.").arg(request.path);
            return result;
        }

        const cwCoordinateTransform transform(sourceCS, request.frameCS);
        const bool hasTransform = !transform.isIdentity();

        const quint32 grid = quint32(1) << depth;
        const double cellEdge = std::ldexp(root.size, -depth);
        const double inverseCellEdge = cellEdge > 0.0 ? 1.0 / cellEdge : 0.0;

        BoundsTracker bounds;
        qint64 bufferedBytes = 0;
        bool writeFailed = false;

        const auto flushChunk = [&](ChunkBuffer& chunk) {
            if(chunk.buffer.isEmpty()) {
                return;
            }

            QFile file(chunk.path);
            if(!file.open(QIODevice::WriteOnly | QIODevice::Append)
               || file.write(chunk.buffer) != chunk.buffer.size()) {
                writeFailed = true;
                return;
            }

            chunk.buffer.clear();
        };

        const auto flushAll = [&]() {
            for(ChunkBuffer& chunk : result.chunks) {
                flushChunk(chunk);
            }
            bufferedBytes = 0;
        };

        const auto appendPoint = [&](const QVector3D& point) {
            bounds.add(point);

            std::array<quint32, kAxisCount> coordinate = {};
            for(int axis = 0; axis < kAxisCount; axis++) {
                const double offset = double(point[axis]) - double(root.minimum[axis]);
                if(offset < 0.0 || offset > root.size) {
                    result.escapedRoot = true;
                }
                coordinate[axis] = quint32(std::clamp(offset * inverseCellEdge, 0.0, double(grid - 1)));
            }

            const quint64 key = cellKey(Cell {depth, coordinate[0], coordinate[1], coordinate[2]});
            auto found = result.chunks.find(key);
            if(found == result.chunks.end()) {
                ChunkBuffer chunk;
                chunk.path = tempDir.filePath(QStringLiteral("chunk-%1.bin").arg(key));
                found = result.chunks.insert(key, chunk);
            }

            const float xyz[kAxisCount] = {point.x(), point.y(), point.z()};
            found->buffer.append(reinterpret_cast<const char*>(xyz), sizeof(xyz));
            found->pointCount++;

            bufferedBytes += kTempBytesPerPoint;
            result.pointCount++;
        };

        QVector<cwGeoPoint> sourceChunk;
        if(hasTransform) {
            sourceChunk.reserve(kPointChunkSize);
        }

        const auto flushPoints = [&]() {
            if(sourceChunk.isEmpty()) {
                return;
            }

            transform.transformInPlace(sourceChunk.data(), sourceChunk.size());
            for(const cwGeoPoint& geoPoint : std::as_const(sourceChunk)) {
                appendPoint(geoPoint.toVector3D());
            }
            sourceChunk.clear();
        };

        bool cancelled = false;
        qsizetype sinceCheck = 0;

        while(reader->read_point()) {
            if(hasTransform) {
                sourceChunk.append(cwGeoPoint(reader->point.get_x(),
                                              reader->point.get_y(),
                                              reader->point.get_z()));
            } else {
                appendPoint(QVector3D(float(reader->point.get_x()),
                                      float(reader->point.get_y()),
                                      float(reader->point.get_z())));
            }

            sinceCheck++;
            if(sinceCheck < kPointChunkSize) {
                continue;
            }

            flushPoints();
            sinceCheck = 0;

            if(bufferedBytes >= kMaxBufferedChunkBytes) {
                flushAll();
            }

            if(writeFailed) {
                break;
            }

            if(promise.isCanceled()) {
                cancelled = true;
                break;
            }

            progress.report(result.pointCount);
        }

        if(!cancelled && !writeFailed) {
            flushPoints();
            flushAll();
        }

        reader->close();
        delete reader;

        if(cancelled) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
            return result;
        }

        if(writeFailed) {
            result.errorCode = cwPointOctreeBuilder::TempDirFailed;
            result.errorMessage = QStringLiteral("Could not write the octree chunk files in %1.")
                                      .arg(tempDir.path());
            return result;
        }

        if(result.pointCount > 0) {
            result.bboxMin = bounds.minimum;
            result.bboxMax = bounds.maximum;
        }

        progress.report(result.pointCount);
        return result;
    }

    struct SubtreeRecord {
        Cell cell;
        quint32 pointCount = 0;
        std::array<int, kChildCount> children = {-1, -1, -1, -1, -1, -1, -1, -1};
    };

    struct ChunkTask {
        Cell cell;
        QString path;
        qint64 pointCount = 0;
    };

    struct ChunkContext {
        QString lazPath;
        QString fingerprint;
        QString cacheRootPath;
        QVector3D rootMin;
        double rootSize = 0.0;
        std::atomic<bool>* cancel = nullptr;
        std::atomic<qint64>* pointsDone = nullptr;
    };

    struct ChunkResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        QVector<QVector3D> rootPoints;
        QVector<SubtreeRecord> nodes;
    };

    /**
     * Pass B on one chunk: read it back, build its subtree, and write every
     * node but the chunk root, whose points Pass C still pulls from.
     */
    ChunkResult buildChunk(const ChunkTask& task, const ChunkContext& context)
    {
        ChunkResult result;

        if(context.cancel->load(std::memory_order_relaxed)) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
            return result;
        }

        QFile file(task.path);
        if(!file.open(QIODevice::ReadOnly)) {
            result.errorCode = cwPointOctreeBuilder::TempDirFailed;
            result.errorMessage = QStringLiteral("Could not read the octree chunk file %1.").arg(task.path);
            return result;
        }

        //QVector3D is three contiguous floats, the layout pass A wrote, so the
        //chunk reads straight into the vector rather than through a second copy
        static_assert(sizeof(QVector3D) == kTempBytesPerPoint);

        const qsizetype pointCount = file.size() / kTempBytesPerPoint;
        const qint64 wanted = qint64(pointCount) * kTempBytesPerPoint;

        QVector<QVector3D> points(pointCount);
        if(file.read(reinterpret_cast<char*>(points.data()), wanted) != wanted) {
            result.errorCode = cwPointOctreeBuilder::ReadFailed;
            result.errorMessage = QStringLiteral("Could not read the octree chunk file %1.").arg(task.path);
            return result;
        }
        file.close();

        QVector<SampledNode> subtree = buildSubtree(std::move(points),
                                                    task.cell,
                                                    context.rootMin,
                                                    context.rootSize);

        cwDiskCacher cacher {QDir(context.cacheRootPath)};

        result.nodes.reserve(subtree.size());
        for(qsizetype i = 0; i < subtree.size(); i++) {
            const SampledNode& node = subtree.at(i);
            result.nodes.append(SubtreeRecord {node.cell, quint32(node.points.size()), node.children});

            if(i == 0) {
                continue;
            }

            if(context.cancel->load(std::memory_order_relaxed)) {
                result.errorCode = cwPointOctreeBuilder::Cancelled;
                return result;
            }

            const QByteArray payload =
                quantizeAll(node.points, cellBounds(node.cell, context.rootMin, context.rootSize));
            const cwDiskCacher::Key key =
                nodeKey(context.lazPath, context.fingerprint, nodeNameForCell(node.cell));

            if(!writeEntry(cacher, key, payload)) {
                result.errorCode = cwPointOctreeBuilder::CacheWriteFailed;
                result.errorMessage = QStringLiteral("Could not write the octree node %1 into the cache.")
                                          .arg(key.id);
                return result;
            }
        }

        result.rootPoints = std::move(subtree.first().points);
        context.pointsDone->fetch_add(task.pointCount, std::memory_order_relaxed);
        return result;
    }

    struct LevelNode {
        Cell cell;
        QVector<QVector3D> points;
    };

    cwPointOctreeNode manifestNode(const Cell& cell, quint32 pointCount)
    {
        cwPointOctreeNode node;
        node.level = cell.level;
        node.x = cell.x;
        node.y = cell.y;
        node.z = cell.z;
        node.pointCount = pointCount;
        node.byteSize = qint64(pointCount) * kBytesPerPoint;
        return node;
    }
}

int cwPointOctreeBuilder::chunkDepthFor(qint64 npoints, qint64 chunkTargetPoints)
{
    if(npoints <= 0 || chunkTargetPoints <= 0) {
        return 0;
    }

    const double chunks = double(npoints) / double(chunkTargetPoints);
    if(chunks <= 1.0) {
        return 0;
    }

    const double depth = std::ceil(std::log(chunks) / std::log(double(kChildCount)));
    return std::clamp(int(depth), 0, kMaxChunkDepth);
}

QString cwPointOctreeBuilder::fingerprintFor(const Request& request)
{
    const cwLazLoader::ProbeResult probe = cwLazLoader::probeHeader(request.path);
    if(!probe.valid) {
        return QString();
    }

    const QString sourceCS = request.sourceCSOverride.isEmpty()
                                 ? probe.sourceCS
                                 : request.sourceCSOverride;

    return sourceFingerprint(request.path, sourceCS, request.frameCS);
}

std::optional<cwPointOctreeManifest> cwPointOctreeBuilder::cachedManifest(const Request& request)
{
    const QString fingerprint = fingerprintFor(request);
    if(fingerprint.isEmpty()) {
        return std::nullopt;
    }

    const cwDiskCacher cacher {QDir(request.cacheRootPath)};

    const std::optional<cwPointOctreeManifest> manifest =
        cwPointOctreeManifest::deserialize(cacher.entry(manifestKey(request.path, fingerprint)));
    if(!manifest.has_value() || manifest->fingerprint != fingerprint) {
        return std::nullopt;
    }

    for(int i = 0; i < manifest->nodes.size(); i++) {
        if(!cacher.hasEntry(nodeKey(request.path, fingerprint, manifest->nodeName(i)))) {
            return std::nullopt;
        }
    }

    return manifest;
}

QFuture<cwPointOctreeBuilder::Result> cwPointOctreeBuilder::build(const Request& request)
{
    return cwConcurrent::run(
        [request](QPromise<Result>& promise)
        {
            const QByteArray pathBytes = request.path.toUtf8();
            LASreadOpener headerOpener;
            headerOpener.set_file_name(pathBytes.constData(), FALSE);
            LASreader* headerReader = headerOpener.open();
            if(headerReader == nullptr) {
                promise.addResult(Result(QStringLiteral("Could not open %1 for read.").arg(request.path),
                                         OpenFailed));
                return;
            }

            const LASheader& header = headerReader->header;
            const QString sourceCS = cwLazLoader::resolveSourceCS(request.sourceCSOverride, header);
            const cwGeoPoint sourceMin(header.min_x, header.min_y, header.min_z);
            const cwGeoPoint sourceMax(header.max_x, header.max_y, header.max_z);
            const qint64 npoints = qint64(headerReader->npoints);
            headerReader->close();
            delete headerReader;

            const QString fingerprint = sourceFingerprint(request.path, sourceCS, request.frameCS);
            if(fingerprint.isEmpty()) {
                promise.addResult(Result(QStringLiteral("Could not fingerprint %1.").arg(request.path),
                                         OpenFailed));
                return;
            }

            const int depth = chunkDepthFor(npoints, request.chunkTargetPoints);

            //The occupied chunk count is a Pass A answer, so the estimate uses every cell at depth
            qint64 nodeCountEstimate = kChildCount;
            for(int level = 0; level < depth; level++) {
                nodeCountEstimate *= kChildCount;
            }
            Reporter progress(promise, 2 * npoints + nodeCountEstimate);

            const auto fail = [&promise, &progress](const QString& message, int code) {
                progress.dismiss();
                promise.addResult(Result(message, code));
            };

            QTemporaryDir tempDir(QDir::temp().filePath(
                QStringLiteral("cwPointOctree-%1-XXXXXX").arg(QCoreApplication::applicationPid())));
            if(!tempDir.isValid()) {
                fail(QStringLiteral("Could not make a temporary directory for the octree build."),
                     TempDirFailed);
                return;
            }

            const QDir tempPath(tempDir.path());

            RootCube root = provisionalRoot(sourceMin,
                                            sourceMax,
                                            cwCoordinateTransform(sourceCS, request.frameCS));

            PassAResult passA = chunkPoints(request, sourceCS, root, depth, tempPath, promise, progress);

            if(passA.errorCode == Monad::ResultBase::NoError
               && passA.escapedRoot
               && passA.pointCount > 0) {
                qInfo() << "cwPointOctreeBuilder: reprojection pushed points outside the provisional root of"
                        << request.path << "- chunking again with the measured bounds";

                //Pass A appends, so a surviving chunk file would be counted twice
                for(const ChunkBuffer& chunk : std::as_const(passA.chunks)) {
                    if(!QFile::remove(chunk.path)) {
                        fail(QStringLiteral("Could not reset the octree chunk file %1.").arg(chunk.path),
                             TempDirFailed);
                        return;
                    }
                }

                root = cubeAround(passA.bboxMin, passA.bboxMax);
                passA = chunkPoints(request, sourceCS, root, depth, tempPath, promise, progress);
            }

            if(passA.errorCode != Monad::ResultBase::NoError) {
                fail(passA.errorMessage, passA.errorCode);
                return;
            }

            QVector<ChunkTask> tasks;
            tasks.reserve(passA.chunks.size());
            for(auto it = passA.chunks.constBegin(); it != passA.chunks.constEnd(); ++it) {
                tasks.append(ChunkTask {cellFromKey(it.key()), it->path, it->pointCount});
            }
            passA.chunks.clear();

            std::atomic<bool> cancelFlag {false};
            std::atomic<qint64> chunkPointsDone {0};

            const ChunkContext context {
                .lazPath = request.path,
                .fingerprint = fingerprint,
                .cacheRootPath = request.cacheRootPath,
                .rootMin = root.minimum,
                .rootSize = root.size,
                .cancel = &cancelFlag,
                .pointsDone = &chunkPointsDone
            };

            QList<ChunkResult> chunkResults;
            if(tasks.size() == 1) {
                cancelFlag.store(promise.isCanceled(), std::memory_order_relaxed);
                chunkResults.append(buildChunk(tasks.constFirst(), context));
                progress.report(passA.pointCount + chunkPointsDone.load(std::memory_order_relaxed));
            } else if(!tasks.isEmpty()) {
                const auto worker = [context](const ChunkTask& task) { return buildChunk(task, context); };

                QFuture<ChunkResult> mapFuture = cwConcurrent::mapped(tasks, worker);

                //This task waits on tasks from the same pool, so give up its slot until they're done
                QThreadPool* pool = cwTask::threadPool();
                pool->releaseThread();

                while(!mapFuture.isFinished()) {
                    if(promise.isCanceled() && !cancelFlag.load(std::memory_order_relaxed)) {
                        cancelFlag.store(true, std::memory_order_relaxed);
                        mapFuture.cancel();
                    }
                    progress.report(passA.pointCount + chunkPointsDone.load(std::memory_order_relaxed));
                    QThread::msleep(kMapPollMs);
                }

                pool->reserveThread();

                mapFuture.waitForFinished();
                chunkResults = mapFuture.results();
            }

            const QString cancelledMessage =
                QStringLiteral("The octree build of %1 was cancelled.").arg(request.path);

            if(chunkResults.size() != tasks.size()) {
                fail(cancelledMessage, Cancelled);
                return;
            }

            for(const ChunkResult& chunkResult : std::as_const(chunkResults)) {
                if(chunkResult.errorCode != Monad::ResultBase::NoError) {
                    fail(chunkResult.errorCode == Cancelled ? cancelledMessage : chunkResult.errorMessage,
                         chunkResult.errorCode);
                    return;
                }
            }

            cwDiskCacher cacher {QDir(request.cacheRootPath)};

            QHash<quint64, qsizetype> chunkIndexByCell;
            chunkIndexByCell.reserve(tasks.size());

            QHash<quint64, LevelNode> level;
            level.reserve(tasks.size());

            for(qsizetype i = 0; i < tasks.size(); i++) {
                const quint64 key = cellKey(tasks.at(i).cell);
                chunkIndexByCell.insert(key, i);
                level.insert(key, LevelNode {tasks.at(i).cell, chunkResults.at(i).rootPoints});
            }

            //Pass C: every node the sampler pulls from is final only once its parent has sampled it
            QHash<quint64, quint32> topPointCounts;
            qint64 nodesWritten = 0;

            const auto writeTopNode = [&](const LevelNode& node) {
                const QByteArray payload =
                    quantizeAll(node.points, cellBounds(node.cell, root.minimum, root.size));
                const cwDiskCacher::Key key =
                    nodeKey(request.path, fingerprint, nodeNameForCell(node.cell));

                if(!writeEntry(cacher, key, payload)) {
                    fail(QStringLiteral("Could not write the octree node %1 into the cache.").arg(key.id),
                         CacheWriteFailed);
                    return false;
                }

                topPointCounts.insert(cellKey(node.cell), quint32(node.points.size()));
                nodesWritten++;
                progress.report(2 * passA.pointCount + nodesWritten);
                return true;
            };

            for(int parentLevel = depth - 1; parentLevel >= 0; parentLevel--) {
                QHash<quint64, QVector<quint64>> childrenByParent;
                for(auto it = level.constBegin(); it != level.constEnd(); ++it) {
                    childrenByParent[cellKey(parentOf(it->cell))].append(it.key());
                }

                //sampleUp breaks ties by source order, so octant order keeps a
                //build reproducible whatever order the hash hands the level back
                for(auto it = childrenByParent.begin(); it != childrenByParent.end(); ++it) {
                    std::sort(it->begin(), it->end(), [](quint64 left, quint64 right) {
                        return octantInParent(cellFromKey(left)) < octantInParent(cellFromKey(right));
                    });
                }

                QHash<quint64, LevelNode> parents;
                parents.reserve(childrenByParent.size());

                for(auto it = childrenByParent.constBegin(); it != childrenByParent.constEnd(); ++it) {
                    const Cell parentCell = cellFromKey(it.key());

                    QVector<QVector<QVector3D>*> sources;
                    sources.reserve(it->size());
                    for(quint64 childKey : std::as_const(it.value())) {
                        sources.append(&level.find(childKey)->points);
                    }

                    parents.insert(it.key(),
                                   LevelNode {parentCell,
                                              sampleUp(cellBounds(parentCell, root.minimum, root.size), sources)});
                }

                for(auto it = level.constBegin(); it != level.constEnd(); ++it) {
                    if(promise.isCanceled()) {
                        fail(cancelledMessage, Cancelled);
                        return;
                    }

                    if(!writeTopNode(it.value())) {
                        return;
                    }
                }

                level = std::move(parents);
            }

            const Cell rootCell {0, 0, 0, 0};
            const LevelNode rootNode = level.value(cellKey(rootCell), LevelNode {rootCell, {}});
            if(!writeTopNode(rootNode)) {
                return;
            }

            //Depth first from the root, children in octant order
            cwPointOctreeManifest manifest;
            manifest.rootMin = root.minimum;
            manifest.rootSize = root.size;
            manifest.pointCount = passA.pointCount;
            manifest.bboxMin = passA.bboxMin;
            manifest.bboxMax = passA.bboxMax;
            manifest.fingerprint = fingerprint;

            const auto appendNode = [&](const Cell& cell, const auto& self) -> int {
                const int index = manifest.nodes.size();
                manifest.nodes.append(manifestNode(cell, topPointCounts.value(cellKey(cell), 0)));

                if(cell.level == depth && chunkIndexByCell.contains(cellKey(cell))) {
                    const QVector<SubtreeRecord>& records =
                        chunkResults.at(chunkIndexByCell.value(cellKey(cell))).nodes;

                    for(qsizetype i = 1; i < records.size(); i++) {
                        const SubtreeRecord& record = records.at(i);

                        cwPointOctreeNode child = manifestNode(record.cell, record.pointCount);
                        for(int octant = 0; octant < kChildCount; octant++) {
                            const int local = record.children.at(octant);
                            child.children[octant] = local >= 0 ? local + index : -1;
                        }

                        manifest.nodes.append(child);
                    }

                    for(int octant = 0; octant < kChildCount; octant++) {
                        const int local = records.constFirst().children.at(octant);
                        manifest.nodes[index].children[octant] = local >= 0 ? local + index : -1;
                    }

                    return index;
                }

                for(int octant = 0; octant < kChildCount; octant++) {
                    const Cell child = childCell(cell, octant);
                    if(!topPointCounts.contains(cellKey(child))) {
                        continue;
                    }

                    manifest.nodes[index].children[octant] = self(child, self);
                }

                return index;
            };

            appendNode(rootCell, appendNode);

            if(manifest.pointCount > 0) {
                const double dx = std::max(double(manifest.bboxMax.x() - manifest.bboxMin.x()),
                                           kMinimumSpacingExtent);
                const double dy = std::max(double(manifest.bboxMax.y() - manifest.bboxMin.y()),
                                           kMinimumSpacingExtent);
                manifest.meanSpacingXY = float(std::sqrt(dx * dy / double(manifest.pointCount)));
            }

            if(!writeEntry(cacher, manifestKey(request.path, fingerprint), manifest.serialize())) {
                fail(QStringLiteral("Could not write the octree manifest of %1 into the cache.").arg(request.path),
                     CacheWriteFailed);
                return;
            }

            promise.addResult(Result(manifest));
        });
}
