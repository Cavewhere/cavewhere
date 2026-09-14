// cwPointOctreeBuilder.cpp
#include "cwPointOctreeBuilder.h"

//Qt includes
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
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
#include "cwProfileLog.h"
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

    //One decode worker holds at most this much unwritten chunk data before flushing its buffers
    constexpr qint64 kDecodeWorkerBufferBytes = 8 * 1024 * 1024;

    //Fewer points than this for a worker to decode means fewer workers
    constexpr qint64 kMinPointsPerDecodeWorker = 262144;

    //Every range writes its own part file per cell, so this caps the ranges
    constexpr qint64 kMaxChunkFiles = 65536;

    //Four of this machine's cores are efficiency cores, so several ranges a
    //worker let the map balance the tail
    constexpr int kDecodeRangesPerWorker = 4;

    //What one point of a chunk costs pass B while its chunk runs: the chunk's
    //own 12 bytes and eleven more copies of it, which is what the sampler's
    //partitions, subtree nodes and scratch measured against the big cloud
    //(§2.11's calibration: 12.7 M points held 1.76 GB on their own)
    constexpr qint64 kPassBWorkingSetBytesPerPoint = 144;

    //The root cube of a cloud with no extent, so quantization still has a range
    constexpr double kMinimumRootSize = 1.0;

    //Floor on a bbox edge, so a vertical-only scan still has a mean spacing
    constexpr double kMinimumSpacingExtent = 1e-3;

    constexpr int kMapPollMs = 30;

    //A build log line's elapsed field, in seconds
    QString secondsSince(const QElapsedTimer& timer)
    {
        constexpr double kMillisecondsPerSecond = 1000.0;
        constexpr int kSecondsPrecision = 3;
        return QString::number(double(timer.elapsed()) / kMillisecondsPerSecond, 'f', kSecondsPrecision);
    }

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

    //One decode range's share of one cell, in a file the range alone writes
    struct ChunkPart {
        QString path;
        qint64 pointCount = 0;
    };

    //A contiguous span of the file's points, decoded by one worker
    struct DecodeRange {
        qint64 startIndex = 0;
        qint64 count = 0;
        int rangeIndex = 0;
    };

    //Read-only, shared by value with every range's worker
    struct DecodeContext {
        QString path;
        QString sourceCS;
        QString frameCS;
        RootCube root;
        int depth = 0;
        QString tempDirPath;
        std::atomic<bool>* cancel = nullptr;
        std::atomic<qint64>* pointsDone = nullptr;

        //Set only where the build thread decodes the one range itself, so a
        //lone range cancels and reports as the map's poll loop does for many
        Promise* inlinePromise = nullptr;
        Reporter* inlineProgress = nullptr;
    };

    struct DecodeResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        qint64 pointCount = 0;
        BoundsTracker bounds;
        bool escapedRoot = false;
        QHash<quint64, ChunkPart> parts;
    };

    struct PassAResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        qint64 pointCount = 0;
        QVector3D bboxMin;
        QVector3D bboxMax;
        bool escapedRoot = false;
        int rangeCount = 0;

        //Cell key to that cell's parts, in range order, which is file order
        QHash<quint64, QVector<ChunkPart>> parts;
    };

    /**
     * Pass A on one range: reads its span of the file once, transforms every
     * point into the frame CS, and appends its native float32 xyz to the part
     * file of its cell at depth. The part files are read back by the same
     * process, so native byte order never leaves this build.
     */
    DecodeResult decodeRange(const DecodeRange& range, const DecodeContext& context)
    {
        DecodeResult result;
        if(range.count <= 0) {
            return result;
        }

        if(context.cancel->load(std::memory_order_relaxed)) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
            return result;
        }

        //LASreadOpener holds the reader's state, so every range opens its own
        const QByteArray pathBytes = context.path.toUtf8();
        LASreadOpener opener;
        opener.set_file_name(pathBytes.constData(), FALSE);
        LASreader* reader = opener.open();
        if(reader == nullptr) {
            result.errorCode = cwPointOctreeBuilder::OpenFailed;
            result.errorMessage = QStringLiteral("Could not open %1 for read.").arg(context.path);
            return result;
        }

        if(range.startIndex > 0 && !reader->seek(I64(range.startIndex))) {
            reader->close();
            delete reader;
            result.errorCode = cwPointOctreeBuilder::ReadFailed;
            result.errorMessage = QStringLiteral("Could not seek to point %1 of %2.")
                                      .arg(range.startIndex)
                                      .arg(context.path);
            return result;
        }

        const cwCoordinateTransform transform(context.sourceCS, context.frameCS);
        const bool hasTransform = !transform.isIdentity();

        const QDir tempDir(context.tempDirPath);
        const quint32 grid = quint32(1) << context.depth;
        const double cellEdge = std::ldexp(context.root.size, -context.depth);
        const double inverseCellEdge = cellEdge > 0.0 ? 1.0 / cellEdge : 0.0;

        QHash<quint64, ChunkBuffer> buffers;
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
            for(ChunkBuffer& chunk : buffers) {
                flushChunk(chunk);
            }
            bufferedBytes = 0;
        };

        const auto appendPoint = [&](const QVector3D& point) {
            result.bounds.add(point);

            std::array<quint32, kAxisCount> coordinate = {};
            for(int axis = 0; axis < kAxisCount; axis++) {
                const double offset = double(point[axis]) - double(context.root.minimum[axis]);
                if(offset < 0.0 || offset > context.root.size) {
                    result.escapedRoot = true;
                }
                coordinate[axis] = quint32(std::clamp(offset * inverseCellEdge, 0.0, double(grid - 1)));
            }

            const quint64 key = cellKey(Cell {context.depth, coordinate[0], coordinate[1], coordinate[2]});
            auto found = buffers.find(key);
            if(found == buffers.end()) {
                ChunkBuffer chunk;
                chunk.path = tempDir.filePath(QStringLiteral("chunk-%1-r%2.bin")
                                                  .arg(key)
                                                  .arg(range.rangeIndex));
                found = buffers.insert(key, chunk);
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

        qint64 reported = 0;
        const auto publishProgress = [&]() {
            context.pointsDone->fetch_add(result.pointCount - reported, std::memory_order_relaxed);
            reported = result.pointCount;

            if(context.inlinePromise == nullptr) {
                return;
            }

            if(context.inlinePromise->isCanceled()) {
                context.cancel->store(true, std::memory_order_relaxed);
            }
            context.inlineProgress->report(context.pointsDone->load(std::memory_order_relaxed));
        };

        bool cancelled = false;
        qsizetype sinceCheck = 0;

        while(result.pointCount + sourceChunk.size() < range.count && reader->read_point()) {
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

            if(bufferedBytes >= kDecodeWorkerBufferBytes) {
                flushAll();
            }

            if(writeFailed) {
                break;
            }

            publishProgress();

            if(context.cancel->load(std::memory_order_relaxed)) {
                cancelled = true;
                break;
            }
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
                                      .arg(context.tempDirPath);
            return result;
        }

        publishProgress();

        result.parts.reserve(buffers.size());
        for(auto it = buffers.constBegin(); it != buffers.constEnd(); ++it) {
            result.parts.insert(it.key(), ChunkPart {it->path, it->pointCount});
        }

        return result;
    }

    /**
     * Runs a map of this pool's own tasks to completion, forwarding a cancel to
     * it and reporting @a pointsDone above @a progressBase while it runs. Both
     * passes map from a task on the pool they map onto, so both give up their
     * slot for the duration or the pool cannot scale up to run the map.
     */
    template <typename T>
    QList<T> awaitMapped(QFuture<T>& mapFuture,
                         Promise& promise,
                         Reporter& progress,
                         std::atomic<bool>& cancel,
                         const std::atomic<qint64>& pointsDone,
                         qint64 progressBase)
    {
        QThreadPool* pool = cwTask::threadPool();
        pool->releaseThread();

        while(!mapFuture.isFinished()) {
            if(promise.isCanceled() && !cancel.load(std::memory_order_relaxed)) {
                cancel.store(true, std::memory_order_relaxed);
                mapFuture.cancel();
            }
            progress.report(progressBase + pointsDone.load(std::memory_order_relaxed));
            QThread::msleep(kMapPollMs);
        }

        pool->reserveThread();

        mapFuture.waitForFinished();
        return mapFuture.results();
    }

    //Contiguous ranges covering [0, npoints)
    QVector<DecodeRange> buildRanges(qint64 npoints, int rangeCount)
    {
        QVector<DecodeRange> ranges;
        if(npoints <= 0 || rangeCount <= 0) {
            return ranges;
        }

        ranges.reserve(rangeCount);

        const qint64 base = npoints / rangeCount;
        const qint64 remainder = npoints % rangeCount;
        qint64 start = 0;

        for(int i = 0; i < rangeCount; i++) {
            const qint64 count = base + (i < remainder ? 1 : 0);
            ranges.append(DecodeRange {start, count, i});
            start += count;
        }

        return ranges;
    }

    /**
     * Seeking a point-wise compressed LAZ decodes every point before the one
     * asked for, so ranges of such a file would each re-read the whole cloud.
     * An uncompressed .las seeks with fseek and a chunked LAZ decodes at most
     * one chunk, so both take as many workers as there is work for.
     */
    bool seeksCheaply(const LASheader& header)
    {
        if(header.laszip == nullptr || header.laszip->compressor == LASZIP_COMPRESSOR_NONE) {
            return true;
        }

        return header.laszip->compressor != LASZIP_COMPRESSOR_POINTWISE
               && header.laszip->chunk_size != std::numeric_limits<quint32>::max();
    }

    //Workers pass A decodes with: what the request asks for, else one per
    //kMinPointsPerDecodeWorker points, both held to the pool, and one where
    //seeking is expensive
    int decodeWorkerCount(const cwPointOctreeBuilder::Request& request,
                          qint64 npoints,
                          bool cheapSeeks)
    {
        if(!cheapSeeks) {
            return 1;
        }

        //One slot stays with the orchestrator, which polls while the ranges run
        const qint64 byPool = std::max(1, cwTask::threadPool()->maxThreadCount() - 1);

        if(request.decodeWorkerCount > 0) {
            return int(std::min(qint64(request.decodeWorkerCount), byPool));
        }

        const qint64 byWork = std::max(qint64(1), npoints / kMinPointsPerDecodeWorker);
        return int(std::min(byPool, byWork));
    }

    //Ranges pass A splits into: several per worker so the map balances the
    //tail, capped so cells x ranges part files stay under kMaxChunkFiles
    int decodeRangeCount(int workerCount, int depth)
    {
        if(workerCount <= 1) {
            return 1;
        }

        qint64 cells = 1;
        for(int level = 0; level < depth; level++) {
            cells *= kChildCount;
        }

        const qint64 byFiles = std::max(qint64(1), kMaxChunkFiles / cells);
        const qint64 byWorkers = qint64(kDecodeRangesPerWorker) * workerCount;
        return int(std::max(qint64(1), std::min(byWorkers, byFiles)));
    }

    /**
     * Pass A: every range decodes its own span into its own part files, and
     * the results fold in range order, so a cell's points end up in the order
     * one reader would have read them in.
     */
    PassAResult runPassA(const cwPointOctreeBuilder::Request& request,
                         const QString& sourceCS,
                         const RootCube& root,
                         int depth,
                         qint64 npoints,
                         int workerCount,
                         const QDir& tempDir,
                         Promise& promise,
                         Reporter& progress)
    {
        PassAResult result;

        const QVector<DecodeRange> ranges = buildRanges(npoints, decodeRangeCount(workerCount, depth));
        result.rangeCount = int(ranges.size());
        if(ranges.isEmpty()) {
            return result;
        }

        std::atomic<bool> cancel {promise.isCanceled()};
        std::atomic<qint64> pointsDone {0};

        DecodeContext context {
            .path = request.path,
            .sourceCS = sourceCS,
            .frameCS = request.frameCS,
            .root = root,
            .depth = depth,
            .tempDirPath = tempDir.path(),
            .cancel = &cancel,
            .pointsDone = &pointsDone
        };

        QList<DecodeResult> decoded;
        if(ranges.size() == 1) {
            context.inlinePromise = &promise;
            context.inlineProgress = &progress;
            decoded.append(decodeRange(ranges.constFirst(), context));
        } else {
            const auto worker = [context](const DecodeRange& range) {
                return decodeRange(range, context);
            };

            QFuture<DecodeResult> mapFuture = cwConcurrent::mapped(ranges, worker);
            decoded = awaitMapped(mapFuture, promise, progress, cancel, pointsDone, 0);
        }

        //A cancelled map stops pending ranges from starting, so a short result
        //list is the cancel showing up as missing work
        if(decoded.size() != ranges.size()) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
            return result;
        }

        BoundsTracker bounds;

        for(const DecodeResult& decodedRange : std::as_const(decoded)) {
            if(result.errorCode == Monad::ResultBase::NoError
               && decodedRange.errorCode != Monad::ResultBase::NoError) {
                result.errorCode = decodedRange.errorCode;
                result.errorMessage = decodedRange.errorMessage;
            }

            result.pointCount += decodedRange.pointCount;
            result.escapedRoot = result.escapedRoot || decodedRange.escapedRoot;

            if(decodedRange.pointCount > 0) {
                bounds.add(decodedRange.bounds.minimum);
                bounds.add(decodedRange.bounds.maximum);
            }

            //This loop walks the ranges in order, so every cell's parts land in that order
            for(auto it = decodedRange.parts.constBegin();
                it != decodedRange.parts.constEnd();
                ++it) {
                result.parts[it.key()].append(it.value());
            }
        }

        if(result.errorCode != Monad::ResultBase::NoError) {
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
        QVector<ChunkPart> parts;   //!< in range order, so the cell reads in file order
        qint64 pointCount = 0;      //!< the parts' points together
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

        //QVector3D is three contiguous floats, the layout pass A wrote, so the
        //parts read straight into the vector rather than through a second copy
        static_assert(sizeof(QVector3D) == kTempBytesPerPoint);

        QVector<QVector3D> points(qsizetype(task.pointCount));
        char* cursor = reinterpret_cast<char*>(points.data());

        //Range order is the order one reader would have read these points in
        for(const ChunkPart& part : task.parts) {
            QFile file(part.path);
            if(!file.open(QIODevice::ReadOnly)) {
                result.errorCode = cwPointOctreeBuilder::TempDirFailed;
                result.errorMessage = QStringLiteral("Could not read the octree chunk file %1.")
                                          .arg(part.path);
                return result;
            }

            const qint64 wanted = part.pointCount * kTempBytesPerPoint;
            if(file.read(cursor, wanted) != wanted) {
                result.errorCode = cwPointOctreeBuilder::ReadFailed;
                result.errorMessage = QStringLiteral("Could not read the octree chunk file %1.")
                                          .arg(part.path);
                return result;
            }

            file.close();
            cursor += wanted;

            //Temp scratch declines through pass B rather than peaking at the manifest
            QFile::remove(part.path);
        }

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

    struct PassBResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;

        //One entry a task, in task order; nullopt where the task never ran
        QVector<std::optional<ChunkResult>> chunks;

        qint64 peakBytes = 0;
    };

    /**
     * Pass B: the chunks run under a memory budget rather than all at once.
     * The tasks are admitted biggest first, a task starts only while its
     * working set still fits beside the ones already running, and the biggest
     * chunk of all runs on its own however small the budget is.
     */
    PassBResult runPassB(const QVector<ChunkTask>& tasks,
                         const ChunkContext& context,
                         qint64 budgetBytes,
                         Promise& promise,
                         Reporter& progress,
                         qint64 progressBase)
    {
        PassBResult result;
        if(tasks.isEmpty()) {
            return result;
        }

        result.chunks.resize(tasks.size());

        struct RunningChunk {
            qsizetype taskIndex = 0;
            qint64 bytes = 0;
            QFuture<ChunkResult> future;
        };

        QThreadPool* pool = cwTask::threadPool();

        //The orchestrator polls rather than works, so it gives its slot up to
        //the chunks it runs, as pass A's map does
        pool->releaseThread();

        //Read after the release, so the slot the orchestrator gave up is one a
        //chunk may run in, as it was under the map
        const qsizetype maxRunning = std::max(qsizetype(1), qsizetype(pool->maxThreadCount()));

        QList<RunningChunk> running;
        qint64 inFlightBytes = 0;
        qsizetype nextTask = 0;
        bool admitting = true;

        //A canceled or failed pass admits nothing more, so what is running is
        //all that is left of it
        while((admitting && nextTask < tasks.size()) || !running.isEmpty()) {
            //The started chunks poll the shared flag per node, and the ones
            //still waiting are never admitted
            if(admitting && promise.isCanceled()) {
                admitting = false;
                context.cancel->store(true, std::memory_order_relaxed);
            }

            while(admitting && nextTask < tasks.size() && running.size() < maxRunning) {
                const ChunkTask& task = tasks.at(nextTask);
                const qint64 bytes = task.pointCount * kPassBWorkingSetBytesPerPoint;

                //The first chunk of a round runs whatever it costs, so a chunk
                //bigger than the whole budget still gets built, on its own
                if(!running.isEmpty() && inFlightBytes + bytes > budgetBytes) {
                    break;
                }

                running.append(RunningChunk {
                    nextTask,
                    bytes,
                    cwConcurrent::run([task, context]() { return buildChunk(task, context); })});

                inFlightBytes += bytes;
                result.peakBytes = std::max(result.peakBytes, inFlightBytes);
                nextTask++;
            }

            bool reaped = false;
            for(qsizetype i = running.size() - 1; i >= 0; i--) {
                if(!running.at(i).future.isFinished()) {
                    continue;
                }

                reaped = true;
                RunningChunk finished = running.takeAt(i);
                inFlightBytes -= finished.bytes;

                ChunkResult chunk = finished.future.takeResult();

                //A chunk that failed is the build's answer, so the running
                //chunks stop at their next node and no more are admitted
                if(chunk.errorCode != Monad::ResultBase::NoError) {
                    admitting = false;
                    context.cancel->store(true, std::memory_order_relaxed);
                }

                result.chunks[finished.taskIndex] = std::move(chunk);
            }

            progress.report(progressBase + context.pointsDone->load(std::memory_order_relaxed));

            //A slot a chunk just freed is filled on the next turn rather than
            //after a sleep, so the pool waits only while every chunk still runs
            if(!reaped && !running.isEmpty()) {
                QThread::msleep(kMapPollMs);
            }
        }

        pool->reserveThread();

        //The chunk that failed is the build's answer, whichever came first in
        //task order. Its siblings stop at their next node and report Cancelled,
        //as do the tasks admission never reached, so both stand behind it.
        bool everyChunkRan = true;
        bool anyChunkCanceled = false;
        for(const std::optional<ChunkResult>& chunk : std::as_const(result.chunks)) {
            if(!chunk.has_value()) {
                everyChunkRan = false;
                continue;
            }

            if(chunk->errorCode == Monad::ResultBase::NoError) {
                continue;
            }

            if(chunk->errorCode == cwPointOctreeBuilder::Cancelled) {
                anyChunkCanceled = true;
                continue;
            }

            result.errorCode = chunk->errorCode;
            result.errorMessage = chunk->errorMessage;
            return result;
        }

        if(anyChunkCanceled || !everyChunkRan) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
        }

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
            const bool cheapSeeks = seeksCheaply(header);
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

            const int workerCount = decodeWorkerCount(request, npoints, cheapSeeks);

            QElapsedTimer buildTimer;
            buildTimer.start();

            QElapsedTimer passTimer;
            passTimer.start();

            PassAResult passA = runPassA(request, sourceCS, root, depth, npoints,
                                         workerCount, tempPath, promise, progress);

            if(passA.errorCode == Monad::ResultBase::NoError
               && passA.escapedRoot
               && passA.pointCount > 0) {
                qInfo() << "cwPointOctreeBuilder: reprojection pushed points outside the provisional root of"
                        << request.path << "- chunking again with the measured bounds";

                //Pass A appends, so a surviving part file would be counted twice
                for(const QVector<ChunkPart>& parts : std::as_const(passA.parts)) {
                    for(const ChunkPart& part : parts) {
                        if(!QFile::remove(part.path)) {
                            fail(QStringLiteral("Could not reset the octree chunk file %1.").arg(part.path),
                                 TempDirFailed);
                            return;
                        }
                    }
                }

                root = cubeAround(passA.bboxMin, passA.bboxMax);
                passA = runPassA(request, sourceCS, root, depth, npoints,
                                 workerCount, tempPath, promise, progress);
            }

            if(passA.errorCode != Monad::ResultBase::NoError) {
                fail(passA.errorMessage, passA.errorCode);
                return;
            }

            cw::profile::write(lcProfileLoad(),
                               QStringLiteral("build passA points=%1 ranges=%2 workers=%3 seconds=%4")
                                   .arg(passA.pointCount)
                                   .arg(passA.rangeCount)
                                   .arg(workerCount)
                                   .arg(secondsSince(passTimer)));
            passTimer.restart();

            QVector<ChunkTask> tasks;
            tasks.reserve(passA.parts.size());

            for(auto it = passA.parts.constBegin(); it != passA.parts.constEnd(); ++it) {
                qint64 points = 0;
                for(const ChunkPart& part : it.value()) {
                    points += part.pointCount;
                }

                tasks.append(ChunkTask {cellFromKey(it.key()), it.value(), points});
            }
            passA.parts.clear();

            //Biggest first, so the chunk the budget has the most trouble with
            //runs while the fewest others are beside it. The cell key breaks
            //ties, so the order is the input's whatever the hash hands back.
            std::sort(tasks.begin(), tasks.end(), [](const ChunkTask& left, const ChunkTask& right) {
                if(left.pointCount != right.pointCount) {
                    return left.pointCount > right.pointCount;
                }
                return cellKey(left.cell) < cellKey(right.cell);
            });

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

            const qint64 memoryBudgetBytes = request.memoryBudgetBytes > 0
                                                 ? request.memoryBudgetBytes
                                                 : kDefaultBuildMemoryBudgetBytes;

            PassBResult passB = runPassB(tasks, context, memoryBudgetBytes,
                                         promise, progress, passA.pointCount);

            cw::profile::write(lcProfileLoad(),
                               QStringLiteral("build passB chunks=%1 budgetBytes=%2 "
                                              "inFlightPeakBytes=%3 seconds=%4")
                                   .arg(tasks.size())
                                   .arg(memoryBudgetBytes)
                                   .arg(passB.peakBytes)
                                   .arg(secondsSince(passTimer)));
            passTimer.restart();

            const QString cancelledMessage =
                QStringLiteral("The octree build of %1 was cancelled.").arg(request.path);

            if(passB.errorCode != Monad::ResultBase::NoError) {
                fail(passB.errorCode == Cancelled ? cancelledMessage : passB.errorMessage,
                     passB.errorCode);
                return;
            }

            QVector<std::optional<ChunkResult>> chunkResults = std::move(passB.chunks);

            cwDiskCacher cacher {QDir(request.cacheRootPath)};

            QHash<quint64, qsizetype> chunkIndexByCell;
            chunkIndexByCell.reserve(tasks.size());

            QHash<quint64, LevelNode> level;
            level.reserve(tasks.size());

            for(qsizetype i = 0; i < tasks.size(); i++) {
                const quint64 key = cellKey(tasks.at(i).cell);
                chunkIndexByCell.insert(key, i);
                level.insert(key,
                             LevelNode {tasks.at(i).cell, std::move(chunkResults[i]->rootPoints)});
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

                //One parent group at a time: its children leave the level,
                //feed the sample, are written, and are freed before the next
                //group is read, so a level and its parents never coexist whole
                for(auto it = childrenByParent.constBegin(); it != childrenByParent.constEnd(); ++it) {
                    const Cell parentCell = cellFromKey(it.key());

                    QVector<LevelNode> children;
                    children.reserve(it->size());
                    for(quint64 childKey : std::as_const(it.value())) {
                        const auto found = level.find(childKey);
                        children.append(LevelNode {found->cell, std::move(found->points)});
                    }

                    QVector<QVector<QVector3D>*> sources;
                    sources.reserve(children.size());
                    for(LevelNode& child : children) {
                        sources.append(&child.points);
                    }

                    parents.insert(it.key(),
                                   LevelNode {parentCell,
                                              sampleUp(cellBounds(parentCell, root.minimum, root.size), sources)});

                    for(const LevelNode& child : std::as_const(children)) {
                        if(promise.isCanceled()) {
                            fail(cancelledMessage, Cancelled);
                            return;
                        }

                        if(!writeTopNode(child)) {
                            return;
                        }
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
                        chunkResults.at(chunkIndexByCell.value(cellKey(cell)))->nodes;

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

            cw::profile::write(lcProfileLoad(),
                               QStringLiteral("build passC nodes=%1 seconds=%2")
                                   .arg(manifest.nodes.size())
                                   .arg(secondsSince(passTimer)));
            cw::profile::write(lcProfileLoad(),
                               QStringLiteral("build total seconds=%1 peakRssBytes=%2")
                                   .arg(secondsSince(buildTimer))
                                   .arg(cw::profile::peakResidentBytes()));

            promise.addResult(Result(manifest));
        });
}
