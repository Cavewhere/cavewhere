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
#include <bit>
#include <atomic>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>

//Our includes
#include "cwConcurrent.h"
#include "cwTaskTrace.h"
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwLazLoader.h"
#include "cwPointOctreePack.h"
#include "cwPointOctreeSampler.h"
#include "cwProfileLog.h"
#include "cwProfilePhase.h"
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

    //What the native spacing estimate adds to that while it runs on a
    //chunk's column: every chunk point's horizontal position copied into the
    //column's neighbor index with its key (16 bytes), the key and index each
    //worker sorts a node's points by (16), the 4 byte copy of each spacing
    //the column's median reads, and each point's 4 byte spacing and 2 byte
    //code, which stay until the payloads are written.
    constexpr qint64 kPassBLocalSpacingBytesPerPoint = 48;

    //One point's entry in a column's neighbor index: its key and its
    //horizontal position
    constexpr qint64 kPassBIndexEntryBytes = 16;

    //What one point a column's neighbors lend it costs while the column runs:
    //its bytes read back from the halo file and its entry in the column's
    //neighbor index. It is never coded there, so it carries none of a chunk
    //point's spacing state.
    constexpr qint64 kPassBHaloBytesPerPoint = kTempBytesPerPoint + kPassBIndexEntryBytes;

    //Pass B's cost per point over pass A's, which sizes its share of the
    //progress bar: on a 76 M point tile pass A took 2.4 s and pass B 17-19 s
    constexpr qint64 kPassBPointWeight = 8;

    //The pass B stages that each credit every chunk point once: building the
    //subtree, measuring native spacing and writing the nodes, which took
    //roughly equal time per column
    constexpr qint64 kPassBStageCount = 3;

    //Widens the cells a halo point's columns are looked for in, so a point a
    //rounding error from a column's widened box still reaches the exact test
    constexpr double kHaloCellSlack = 1e-6;

    //The root cube of a cloud with no extent, so quantization still has a range
    constexpr double kMinimumRootSize = 1.0;

    //Floor on a bbox edge, so a vertical-only scan still has a mean spacing
    constexpr double kMinimumSpacingExtent = 1e-3;

    constexpr int kMapPollMs = 30;

    //One pass of a build, as a "build <name> ..." line on cw.profile.load
    cwProfilePhase buildProfilePhase(const QString& name)
    {
        return cwProfilePhase(lcProfileLoad(), QStringLiteral("build"), name);
    }

    quint64 cellKey(const Cell& cell)
    {
        return cw::octree::cellKey(cell.level, cell.x, cell.y, cell.z);
    }

    Cell cellFromKey(quint64 key)
    {
        return Cell {
            int(key >> kCellKeyLevelShift),
            quint32((key >> kCellKeyXShift) & kCellKeyCoordinateMask),
            quint32((key >> kCellKeyYShift) & kCellKeyCoordinateMask),
            quint32(key & kCellKeyCoordinateMask)
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

    qint64 pointsIn(const QVector<ChunkPart>& parts)
    {
        qint64 points = 0;
        for(const ChunkPart& part : parts) {
            points += part.pointCount;
        }
        return points;
    }

    //! Appends @a chunk's buffered points to its file and empties the buffer;
    //! false where the file would not take them
    bool flushChunkBuffer(ChunkBuffer& chunk)
    {
        if(chunk.buffer.isEmpty()) {
            return true;
        }

        QFile file(chunk.path);
        if(!file.open(QIODevice::WriteOnly | QIODevice::Append)
           || file.write(chunk.buffer) != chunk.buffer.size()) {
            return false;
        }

        chunk.buffer.clear();
        return true;
    }

    //! Appends @a point to @a chunk's buffer and adds its bytes to @a bufferedBytes
    void bufferPoint(ChunkBuffer& chunk, const QVector3D& point, qint64& bufferedBytes)
    {
        chunk.buffer.append(reinterpret_cast<const char*>(&point), kTempBytesPerPoint);
        chunk.pointCount++;
        bufferedBytes += kTempBytesPerPoint;
    }

    template<typename Result>
    void setChunkReadError(Result& result, int errorCode, const QString& path)
    {
        result.errorCode = errorCode;
        result.errorMessage = QStringLiteral("Could not read the octree chunk file %1.").arg(path);
    }

    /**
     * Reads every point of @a parts, in order, a batch of at most
     * kPointChunkSize points at a time, and hands each batch to
     * @a visitBatch(points, count), which returns false to stop. Returns false
     * where a part would not read, with the error in @a result, or where
     * @a visitBatch stopped.
     */
    template<typename Result, typename VisitBatch>
    bool streamParts(const QVector<ChunkPart>& parts, Result& result, const VisitBatch& visitBatch)
    {
        //QVector3D is three contiguous floats, the layout pass A wrote, so a
        //batch reads straight into the vector rather than through a second copy
        static_assert(sizeof(QVector3D) == kTempBytesPerPoint);

        QVector<QVector3D> batch(kPointChunkSize);
        for(const ChunkPart& part : parts) {
            QFile file(part.path);
            if(!file.open(QIODevice::ReadOnly)) {
                setChunkReadError(result, cwPointOctreeBuilder::TempDirFailed, part.path);
                return false;
            }

            for(qint64 remaining = part.pointCount; remaining > 0; ) {
                const qsizetype count = qsizetype(std::min<qint64>(remaining, kPointChunkSize));
                const qint64 wanted = count * kTempBytesPerPoint;
                if(file.read(reinterpret_cast<char*>(batch.data()), wanted) != wanted) {
                    setChunkReadError(result, cwPointOctreeBuilder::ReadFailed, part.path);
                    return false;
                }
                remaining -= count;

                if(!visitBatch(std::as_const(batch).constData(), count)) {
                    return false;
                }
            }
        }
        return true;
    }

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
            if(!flushChunkBuffer(chunk)) {
                writeFailed = true;
            }
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

    struct SplitResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        qint64 cellsSplit = 0;
    };

    /**
     * Splits one cell a level deeper: its parts are read once, in range order,
     * into one part file per occupied child, so a child holds its points in
     * the order one reader would have read them. The parts it read are gone
     * when it returns, and the cell is replaced in @a parts by its children.
     */
    SplitResult splitCell(quint64 key,
                          QHash<quint64, QVector<ChunkPart>>& parts,
                          const RootCube& root,
                          const QDir& tempDir)
    {
        SplitResult result;

        const Cell cell = cellFromKey(key);
        const int childLevel = cell.level + 1;
        const double childEdge = std::ldexp(root.size, -childLevel);
        const double inverseChildEdge = childEdge > 0.0 ? 1.0 / childEdge : 0.0;
        const quint32 childGrid = quint32(1) << childLevel;

        std::array<ChunkBuffer, kChildCount> children;
        for(int octant = 0; octant < kChildCount; octant++) {
            children[octant].path = tempDir.filePath(
                QStringLiteral("chunk-%1-split.bin").arg(cellKey(childCell(cell, octant))));
        }

        qint64 bufferedBytes = 0;

        const auto flushAll = [&]() {
            for(ChunkBuffer& child : children) {
                if(!flushChunkBuffer(child)) {
                    return false;
                }
            }

            bufferedBytes = 0;
            return true;
        };

        const auto writeFailure = [&]() {
            SplitResult failed;
            failed.errorCode = cwPointOctreeBuilder::TempDirFailed;
            failed.errorMessage =
                QStringLiteral("Could not write the octree chunk files in %1.").arg(tempDir.path());
            return failed;
        };

        const auto appendPoint = [&](const QVector3D& point) {
            std::array<quint32, kAxisCount> coordinate = {};
            for(int axis = 0; axis < kAxisCount; axis++) {
                const double offset = double(point[axis]) - double(root.minimum[axis]);
                coordinate[axis] =
                    quint32(std::clamp(offset * inverseChildEdge, 0.0, double(childGrid - 1)));
            }

            //The octant the point's own cell at childLevel sits in, so the
            //child it lands in is the one pass A would have written it to
            const Cell pointCell {childLevel, coordinate[0], coordinate[1], coordinate[2]};
            bufferPoint(children[octantInParent(pointCell)], point, bufferedBytes);
        };

        const QVector<ChunkPart> sourceParts = parts.value(key);

        bool flushFailed = false;
        const bool streamed = streamParts(sourceParts, result,
                                          [&](const QVector3D* points, qsizetype count) {
            for(qsizetype i = 0; i < count; i++) {
                appendPoint(points[i]);
            }
            flushFailed = bufferedBytes >= kDecodeWorkerBufferBytes && !flushAll();
            return !flushFailed;
        });

        if(flushFailed || (streamed && !flushAll())) {
            return writeFailure();
        }
        if(!streamed) {
            return result;
        }

        for(const ChunkPart& part : sourceParts) {
            if(!QFile::remove(part.path)) {
                result.errorCode = cwPointOctreeBuilder::TempDirFailed;
                result.errorMessage =
                    QStringLiteral("Could not remove the octree chunk file %1.").arg(part.path);
                return result;
            }
        }

        parts.remove(key);

        for(int octant = 0; octant < kChildCount; octant++) {
            const ChunkBuffer& child = children.at(octant);
            if(child.pointCount == 0) {
                continue;
            }

            parts.insert(cellKey(childCell(cell, octant)),
                         QVector<ChunkPart> {ChunkPart {child.path, child.pointCount}});
        }

        result.cellsSplit = 1;
        return result;
    }

    /**
     * Splits every cell holding more than @a maxPoints, a level at a time,
     * until no cell is over the cap or the cells that are sit at
     * kMaxChunkDepth. The cap reads point counts alone, so the shape it gives
     * an input never depends on how pass A was scheduled.
     */
    SplitResult splitOversizedCells(QHash<quint64, QVector<ChunkPart>>& parts,
                                    qint64 maxPoints,
                                    const RootCube& root,
                                    const QDir& tempDir,
                                    const Promise& promise)
    {
        SplitResult result;
        if(maxPoints <= 0) {
            return result;
        }

        for(;;) {
            QVector<quint64> oversized;
            for(auto it = parts.constBegin(); it != parts.constEnd(); ++it) {
                if(cellFromKey(it.key()).level < kMaxChunkDepth
                   && pointsIn(it.value()) > maxPoints) {
                    oversized.append(it.key());
                }
            }

            if(oversized.isEmpty()) {
                return result;
            }

            for(quint64 key : std::as_const(oversized)) {
                if(promise.isCanceled()) {
                    result.errorCode = cwPointOctreeBuilder::Cancelled;
                    return result;
                }

                const SplitResult cellResult = splitCell(key, parts, root, tempDir);
                if(cellResult.errorCode != Monad::ResultBase::NoError) {
                    result.errorCode = cellResult.errorCode;
                    result.errorMessage = cellResult.errorMessage;
                    return result;
                }

                result.cellsSplit += cellResult.cellsSplit;
            }
        }
    }

    struct SubtreeRecord {
        Cell cell;
        quint32 pointCount = 0;
        float measuredSpacing = 0.0f;
        std::array<int, kChildCount> children = {-1, -1, -1, -1, -1, -1, -1, -1};
    };

    struct ChunkTask {
        Cell cell;
        QVector<ChunkPart> parts;   //!< in range order, so the cell reads in file order
        qint64 pointCount = 0;      //!< the parts' points together
    };

    struct ChunkContext {
        //Owned by build(), which outlives every column
        cwPointOctreePack::Writer* writer = nullptr;
        QVector3D rootMin;
        double rootSize = 0.0;
        std::atomic<bool>* cancel = nullptr;
        std::atomic<qint64>* pointsDone = nullptr;
    };

    struct ChunkResult {
        QVector<QVector3D> rootPoints;
        //Each root point's native spacing under its column's ceiling, which
        //pass C carries to whatever level the point ends up in
        QVector<float> rootSpacings;
        QVector<SubtreeRecord> nodes;
    };

    //! One xy column of pass B: every chunk whose cell shares a level, an x
    //! and a y, which measure their native spacings against one index
    struct ColumnTask {
        Cell column;                //!< the chunks' level, x and y, z 0
        QVector<qsizetype> chunks;  //!< indices into the chunk tasks, in task order
        QVector<ChunkPart> halo;    //!< what the other columns lend it, in chunk task order
        qint64 haloPoints = 0;      //!< the halo's points together
        qint64 bytes = 0;           //!< passBColumnBytes() of the column
    };

    quint64 columnKey(const Cell& cell)
    {
        return cellKey(Cell {cell.level, cell.x, cell.y, 0});
    }

    struct ColumnResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        //One entry a chunk, in ColumnTask::chunks order
        QVector<ChunkResult> chunks;
        //What measureNativeSpacing() took on this column
        qint64 localSpacingMilliseconds = 0;
        //The column's points that took its ceiling, and the ceiling itself
        qint64 localSpacingCappedPoints = 0;
        double localSpacingCeiling = 0.0;
    };

    //! Reads every part of @a parts, in order, onto the end of @a points;
    //! false, with the error in @a result, where a part would not read
    template<typename Result>
    bool readParts(const QVector<ChunkPart>& parts, QVector<QVector3D>& points, Result& result)
    {
        //QVector3D is three contiguous floats, the layout pass A wrote, so the
        //parts read straight into the vector rather than through a second copy
        static_assert(sizeof(QVector3D) == kTempBytesPerPoint);

        const qsizetype first = points.size();
        points.resize(first + qsizetype(pointsIn(parts)));
        char* cursor = reinterpret_cast<char*>(points.data() + first);

        //Range order is the order one reader would have read these points in
        for(const ChunkPart& part : parts) {
            QFile file(part.path);
            if(!file.open(QIODevice::ReadOnly)) {
                setChunkReadError(result, cwPointOctreeBuilder::TempDirFailed, part.path);
                return false;
            }

            const qint64 wanted = part.pointCount * kTempBytesPerPoint;
            if(file.read(cursor, wanted) != wanted) {
                setChunkReadError(result, cwPointOctreeBuilder::ReadFailed, part.path);
                return false;
            }

            file.close();
            cursor += wanted;

            //Each part has one reader, so temp scratch declines through pass B
            //rather than peaking at the manifest
            QFile::remove(part.path);
        }
        return true;
    }

    /**
     * Pass B on one xy column: read its chunks back and build each one's
     * subtree, measure every point of the column against the column and the
     * halo its neighbors lend it, and write every node but the chunk roots,
     * whose points pass C still pulls from.
     */
    ColumnResult buildColumn(const ColumnTask& column, const QVector<ChunkTask>& chunkTasks,
                             const ChunkContext& context)
    {
        ColumnResult result;

        if(context.cancel->load(std::memory_order_relaxed)) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
            return result;
        }

        QVector<QVector3D> halo;
        if(!readParts(column.halo, halo, result)) {
            return result;
        }

        const NodeProgress creditNode = [pointsDone = context.pointsDone](qsizetype pointCount) {
            pointsDone->fetch_add(pointCount, std::memory_order_relaxed);
        };

        QVector<QVector<SampledNode>> subtrees;
        subtrees.reserve(chunkTasks.size());
        for(const ChunkTask& task : chunkTasks) {
            QVector<QVector3D> points;
            if(!readParts(task.parts, points, result)) {
                return result;
            }
            subtrees.append(buildSubtree(std::move(points), task.cell, context.rootMin,
                                         context.rootSize, kLeafMaxPoints, kMinSplitPoints,
                                         creditNode));
        }

        //Every point of the column, its chunk roots' included, measures its
        //native spacing against the column's raw data, which its subtrees'
        //nodes hold together, and the halo, so the estimate's neighbors stop
        //only kLocalSpacingHaloUnits past the column's xy faces and nowhere
        //along z. The column's own median sets the ceiling of every point
        //measured here, the roots' too, which carry it up into pass C's top
        //tree.
        QElapsedTimer localSpacingTimer;
        localSpacingTimer.start();
        QVector<const QVector<SampledNode>*> measured;
        measured.reserve(subtrees.size());
        for(const QVector<SampledNode>& subtree : std::as_const(subtrees)) {
            measured.append(&subtree);
        }
        NativeSpacingEstimate estimate =
            measureNativeSpacing(measured, halo, context.rootMin, context.rootSize, creditNode);
        halo = QVector<QVector3D>();
        result.localSpacingMilliseconds = localSpacingTimer.elapsed();
        result.localSpacingCappedPoints = estimate.cappedPoints;
        result.localSpacingCeiling = estimate.ceiling;

        result.chunks.reserve(subtrees.size());
        for(qsizetype chunk = 0; chunk < subtrees.size(); chunk++) {
            QVector<SampledNode>& subtree = subtrees[chunk];
            QVector<QVector<float>>& nativeSpacings = estimate.spacings[chunk];
            ChunkResult chunkResult;

            chunkResult.nodes.reserve(subtree.size());
            for(qsizetype i = 0; i < subtree.size(); i++) {
                const SampledNode& node = subtree.at(i);

                //The chunk root still gives points up to pass C, which measures it
                //once they are final
                if(i == 0) {
                    chunkResult.nodes.append(SubtreeRecord {node.cell, quint32(node.points.size()),
                                                            0.0f, node.children});
                    creditNode(node.points.size());
                    continue;
                }

                const QVector<quint16> codes = encodeLocalSpacings(nativeSpacings.at(i));
                chunkResult.nodes.append(SubtreeRecord {node.cell, quint32(node.points.size()),
                                                        measureNodeSpacing(node.points),
                                                        node.children});

                if(context.cancel->load(std::memory_order_relaxed)) {
                    result.errorCode = cwPointOctreeBuilder::Cancelled;
                    return result;
                }

                const QByteArray payload =
                    quantizeAll(node.points, codes,
                                cellBounds(node.cell, context.rootMin, context.rootSize));

                if(!context.writer->append(cellKey(node.cell), payload)) {
                    result.errorCode = cwPointOctreeBuilder::CacheWriteFailed;
                    result.errorMessage = context.writer->errorMessage();
                    return result;
                }
                creditNode(node.points.size());
            }

            chunkResult.rootPoints = std::move(subtree.first().points);
            chunkResult.rootSpacings = std::move(nativeSpacings.first());
            //The chunk's points are written, so it frees them before the next
            subtree = QVector<SampledNode>();
            nativeSpacings = QVector<QVector<float>>();
            result.chunks.append(std::move(chunkResult));
        }

        return result;
    }

    //Read-only, shared by value with every chunk's halo worker
    struct HaloContext {
        RootCube root;
        QString tempDirPath;
        //Column key (columnKey()) to its index among the columns
        QHash<quint64, int> columnByKey;
        //The levels the columns sit at, each once
        QVector<int> columnLevels;
        std::atomic<bool>* cancel = nullptr;
    };

    struct HaloResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        //Column index to what this chunk lends it
        QHash<int, ChunkPart> parts;
    };

    //! Whether @a point lies in the xy box of the column @a column widened by
    //! kLocalSpacingHaloUnits on every side, the halo box: from its low
    //! faces, inclusive, to its high faces, exclusive
    bool inHaloBox(const QVector3D& point, const Cell& column, const RootCube& root)
    {
        const double size = std::ldexp(root.size, -column.level);
        const std::array<quint32, 2> cell {column.x, column.y};
        for(int axis = 0; axis < 2; axis++) {
            const double low = double(root.minimum[axis]) + double(cell.at(size_t(axis))) * size;
            const double value = double(point[axis]);
            if(value < low - kLocalSpacingHaloUnits || value >= low + size + kLocalSpacingHaloUnits) {
                return false;
            }
        }
        return true;
    }

    //! Calls @a visit with the index of every column but @a ownColumn, the
    //! column of the chunk cell @a ownCell, whose halo box holds @a point
    template<typename Visit>
    void forEachHaloColumn(const QVector3D& point, const Cell& ownCell, int ownColumn,
                           const HaloContext& context, const Visit& visit)
    {
        for(const int level : context.columnLevels) {
            const double size = std::ldexp(context.root.size, -level);
            const qint64 lastCell = (qint64(1) << level) - 1;
            std::array<qint64, 2> low = {};
            std::array<qint64, 2> high = {};
            for(int axis = 0; axis < 2; axis++) {
                const double offset = double(point[axis]) - double(context.root.minimum[axis]);
                low[size_t(axis)] = std::max(
                    qint64(0),
                    qint64(std::floor((offset - kLocalSpacingHaloUnits) / size - kHaloCellSlack)));
                high[size_t(axis)] = std::min(
                    lastCell,
                    qint64(std::floor((offset + kLocalSpacingHaloUnits) / size + kHaloCellSlack)));
            }

            //Clear of every face of its own column: nothing to lend at this level
            if(level == ownCell.level && low == high
               && low == std::array<qint64, 2> {qint64(ownCell.x), qint64(ownCell.y)}) {
                continue;
            }

            for(qint64 y = low.at(1); y <= high.at(1); y++) {
                for(qint64 x = low.at(0); x <= high.at(0); x++) {
                    const Cell column {level, quint32(x), quint32(y), 0};
                    const auto found = context.columnByKey.constFind(columnKey(column));
                    if(found == context.columnByKey.constEnd() || found.value() == ownColumn
                       || !inHaloBox(point, column, context.root)) {
                        continue;
                    }
                    visit(found.value());
                }
            }
        }
    }

    /**
     * The halo pass on one chunk: reads its parts once, in batches, and
     * appends every point that lies in another column's halo box to a halo
     * file of that column's that this chunk alone writes. The parts stay for
     * the chunk's own column to read.
     */
    HaloResult writeHalo(const ChunkTask& task, int ownColumn, const HaloContext& context)
    {
        HaloResult result;
        if(context.cancel->load(std::memory_order_relaxed)) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
            return result;
        }

        const QDir tempDir(context.tempDirPath);
        QHash<int, ChunkBuffer> buffers;
        qint64 bufferedBytes = 0;

        const auto flushAll = [&]() {
            for(ChunkBuffer& buffer : buffers) {
                if(!flushChunkBuffer(buffer)) {
                    result.errorCode = cwPointOctreeBuilder::TempDirFailed;
                    result.errorMessage =
                        QStringLiteral("Could not write the octree chunk files in %1.")
                            .arg(context.tempDirPath);
                    return false;
                }
            }
            bufferedBytes = 0;
            return true;
        };

        const auto lend = [&](const QVector3D& point, int column) {
            auto found = buffers.find(column);
            if(found == buffers.end()) {
                ChunkBuffer buffer;
                buffer.path = tempDir.filePath(QStringLiteral("halo-%1-from-%2.bin")
                                                   .arg(column)
                                                   .arg(cellKey(task.cell)));
                found = buffers.insert(column, buffer);
            }
            bufferPoint(*found, point, bufferedBytes);
        };

        const bool streamed = streamParts(task.parts, result,
                                          [&](const QVector3D* points, qsizetype count) {
            for(qsizetype i = 0; i < count; i++) {
                const QVector3D& point = points[i];
                forEachHaloColumn(point, task.cell, ownColumn, context,
                                  [&](int column) { lend(point, column); });
            }

            if(bufferedBytes >= kDecodeWorkerBufferBytes && !flushAll()) {
                return false;
            }

            if(context.cancel->load(std::memory_order_relaxed)) {
                result.errorCode = cwPointOctreeBuilder::Cancelled;
                return false;
            }
            return true;
        });

        if(!streamed || !flushAll()) {
            return result;
        }

        for(auto it = buffers.constBegin(); it != buffers.constEnd(); ++it) {
            result.parts.insert(it.key(), ChunkPart {it->path, it->pointCount});
        }
        return result;
    }

    struct ColumnsResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;
        QVector<ColumnTask> columns;
    };

    /**
     * Groups @a tasks into their xy columns and runs the halo pass, so every
     * column holds the halo files its neighbors lend it and what it costs
     * pass B, the columns biggest first. The halo pass reads every chunk's
     * parts once more than pass B does, and writes the points beside a
     * column's xy faces once for each column they lend to. Where columns of
     * two levels overlap (one z cell of an xy footprint split, its sibling
     * kept) each lends the other its whole overlap, which the measurement
     * needs and which the column's budget charge counts.
     */
    ColumnsResult planColumns(const QVector<ChunkTask>& tasks, const RootCube& root,
                              const QDir& tempDir, Promise& promise, Reporter& progress,
                              qint64 progressBase)
    {
        ColumnsResult result;

        HaloContext context {.root = root, .tempDirPath = tempDir.path()};
        QVector<int> columnOfTask(tasks.size(), -1);
        for(qsizetype i = 0; i < tasks.size(); i++) {
            const quint64 key = columnKey(tasks.at(i).cell);
            auto found = context.columnByKey.constFind(key);
            if(found == context.columnByKey.constEnd()) {
                found = context.columnByKey.insert(key, int(result.columns.size()));
                Cell column = tasks.at(i).cell;
                column.z = 0;
                result.columns.append(ColumnTask {column, {}, {}, 0, 0});
                if(!context.columnLevels.contains(column.level)) {
                    context.columnLevels.append(column.level);
                }
            }
            columnOfTask[i] = found.value();
            result.columns[found.value()].chunks.append(i);
        }
        std::sort(context.columnLevels.begin(), context.columnLevels.end());

        //A lone column has no neighbor to lend it anything
        if(result.columns.size() > 1) {
            std::atomic<bool> cancel {promise.isCanceled()};
            const std::atomic<qint64> pointsDone {0};
            context.cancel = &cancel;

            QVector<qsizetype> taskIndices(tasks.size());
            std::iota(taskIndices.begin(), taskIndices.end(), qsizetype(0));
            const auto worker = [&tasks, &columnOfTask, context](qsizetype task) {
                return writeHalo(tasks.at(task), columnOfTask.at(task), context);
            };
            QFuture<HaloResult> mapFuture = cwConcurrent::mapped(taskIndices, worker);
            const QList<HaloResult> halos =
                awaitMapped(mapFuture, promise, progress, cancel, pointsDone, progressBase);

            if(halos.size() != tasks.size()) {
                result.errorCode = cwPointOctreeBuilder::Cancelled;
                return result;
            }

            //Task order, so a column's halo reads the same whatever order the
            //workers finished in
            for(const HaloResult& halo : halos) {
                if(halo.errorCode != Monad::ResultBase::NoError) {
                    result.errorCode = halo.errorCode;
                    result.errorMessage = halo.errorMessage;
                    return result;
                }

                QList<int> lentTo = halo.parts.keys();
                std::sort(lentTo.begin(), lentTo.end());
                for(const int column : std::as_const(lentTo)) {
                    const ChunkPart& part = halo.parts.value(column);
                    result.columns[column].halo.append(part);
                    result.columns[column].haloPoints += part.pointCount;
                }
            }
        }

        for(ColumnTask& column : result.columns) {
            QVector<qint64> chunkPoints;
            chunkPoints.reserve(column.chunks.size());
            for(const qsizetype chunk : std::as_const(column.chunks)) {
                chunkPoints.append(tasks.at(chunk).pointCount);
            }
            column.bytes = passBColumnBytes(chunkPoints, column.haloPoints);
        }

        //Biggest first, so the column the budget has the most trouble with
        //runs while the fewest others are beside it. The column key breaks
        //ties, so the order is the input's whatever the hash hands back.
        std::sort(result.columns.begin(), result.columns.end(),
                  [](const ColumnTask& left, const ColumnTask& right) {
                      if(left.bytes != right.bytes) {
                          return left.bytes > right.bytes;
                      }
                      return columnKey(left.column) < columnKey(right.column);
                  });
        return result;
    }

    struct PassBResult {
        int errorCode = Monad::ResultBase::NoError;
        QString errorMessage;

        //One entry a chunk task, in task order; nullopt where its column never ran
        QVector<std::optional<ChunkResult>> chunks;
        //One entry a column, in column order; nullopt where it never ran
        QVector<std::optional<ColumnResult>> columns;

        qint64 peakBytes = 0;
    };

    /**
     * Pass B: the columns run under a memory budget rather than all at once.
     * The columns are admitted biggest first, a column starts only while its
     * working set (ColumnTask::bytes) still fits beside the ones already
     * running, and the biggest column of all runs on its own however small
     * the budget is.
     */
    PassBResult runPassB(const QVector<ChunkTask>& tasks,
                         const QVector<ColumnTask>& columns,
                         const ChunkContext& context,
                         qint64 budgetBytes,
                         Promise& promise,
                         Reporter& progress,
                         qint64 progressBase)
    {
        PassBResult result;
        if(columns.isEmpty()) {
            return result;
        }

        result.chunks.resize(tasks.size());
        result.columns.resize(columns.size());

        struct RunningColumn {
            qsizetype columnIndex = 0;
            qint64 bytes = 0;
            QFuture<ColumnResult> future;
        };

        QThreadPool* pool = cwTask::threadPool();

        //The orchestrator polls rather than works, so it gives its slot up to
        //the columns it runs, as pass A's map does
        pool->releaseThread();

        //Read after the release, so the slot the orchestrator gave up is one a
        //column may run in, as it was under the map
        const qsizetype maxRunning = std::max(qsizetype(1), qsizetype(pool->maxThreadCount()));

        QList<RunningColumn> running;
        qint64 inFlightBytes = 0;
        qsizetype nextColumn = 0;
        bool admitting = true;

        //A canceled or failed pass admits nothing more, so what is running is
        //all that is left of it
        while((admitting && nextColumn < columns.size()) || !running.isEmpty()) {
            //The started columns poll the shared flag per node, and the ones
            //still waiting are never admitted
            if(admitting && promise.isCanceled()) {
                admitting = false;
                context.cancel->store(true, std::memory_order_relaxed);
            }

            while(admitting && nextColumn < columns.size() && running.size() < maxRunning) {
                const ColumnTask& column = columns.at(nextColumn);
                const qint64 bytes = column.bytes;

                //The first column of a round runs whatever it costs, so a
                //column bigger than the whole budget still gets built, on its own
                if(!running.isEmpty() && inFlightBytes + bytes > budgetBytes) {
                    break;
                }

                QVector<ChunkTask> chunkTasks;
                chunkTasks.reserve(column.chunks.size());
                for(const qsizetype chunk : column.chunks) {
                    chunkTasks.append(tasks.at(chunk));
                }

                running.append(RunningColumn {
                    nextColumn,
                    bytes,
                    cwConcurrent::run([column, chunkTasks, context]() {
                        return buildColumn(column, chunkTasks, context);
                    })});

                inFlightBytes += bytes;
                result.peakBytes = std::max(result.peakBytes, inFlightBytes);
                nextColumn++;
            }

            bool reaped = false;
            for(qsizetype i = running.size() - 1; i >= 0; i--) {
                if(!running.at(i).future.isFinished()) {
                    continue;
                }

                reaped = true;
                RunningColumn finished = running.takeAt(i);
                inFlightBytes -= finished.bytes;

                ColumnResult column = finished.future.takeResult();

                //A column that failed is the build's answer, so the running
                //columns stop at their next node and no more are admitted
                if(column.errorCode != Monad::ResultBase::NoError) {
                    admitting = false;
                    context.cancel->store(true, std::memory_order_relaxed);
                } else {
                    const QVector<qsizetype>& chunks = columns.at(finished.columnIndex).chunks;
                    for(qsizetype c = 0; c < chunks.size(); c++) {
                        result.chunks[chunks.at(c)] = std::move(column.chunks[c]);
                    }
                    column.chunks.clear();
                }

                result.columns[finished.columnIndex] = std::move(column);
            }

            //Each stage credits every point once, and the stages share pass B's
            //weighted span of the bar
            progress.report(progressBase
                            + context.pointsDone->load(std::memory_order_relaxed)
                                  * kPassBPointWeight / kPassBStageCount);

            //A slot a column just freed is filled on the next turn rather than
            //after a sleep, so the pool waits only while every column still runs
            if(!reaped && !running.isEmpty()) {
                QThread::msleep(kMapPollMs);
            }
        }

        pool->reserveThread();

        //The column that failed is the build's answer, whichever came first in
        //column order. Its siblings stop at their next node and report
        //Cancelled, as do the columns admission never reached, so both stand
        //behind it.
        bool everyColumnRan = true;
        bool anyColumnCanceled = false;
        for(const std::optional<ColumnResult>& column : std::as_const(result.columns)) {
            if(!column.has_value()) {
                everyColumnRan = false;
                continue;
            }

            if(column->errorCode == Monad::ResultBase::NoError) {
                continue;
            }

            if(column->errorCode == cwPointOctreeBuilder::Cancelled) {
                anyColumnCanceled = true;
                continue;
            }

            result.errorCode = column->errorCode;
            result.errorMessage = column->errorMessage;
            return result;
        }

        if(anyColumnCanceled || !everyColumnRan) {
            result.errorCode = cwPointOctreeBuilder::Cancelled;
        }

        return result;
    }

    struct LevelNode {
        Cell cell;
        QVector<QVector3D> points;
        //Each point's native spacing from pass B, in points' order
        QVector<float> spacings;
    };

    //A point's exact position, which bit-identical duplicates share
    struct PositionKey {
        quint32 x = 0;
        quint32 y = 0;
        quint32 z = 0;

        explicit PositionKey(const QVector3D& point) :
            x(std::bit_cast<quint32>(point.x())),
            y(std::bit_cast<quint32>(point.y())),
            z(std::bit_cast<quint32>(point.z()))
        {
        }

        bool operator==(const PositionKey& other) const = default;
    };

    size_t qHash(const PositionKey& key, size_t seed = 0)
    {
        return qHashMulti(seed, key.x, key.y, key.z);
    }

    /**
     * Rebuilds the spacings of @a nodes after sampleUp() moved points among
     * their point lists, from what each position carried before. Duplicates
     * share a position and saw the same distinct neighbors in pass B, so
     * they carry the same spacing and the lookup is exact.
     */
    class SpacingByPosition {
    public:
        explicit SpacingByPosition(const QVector<LevelNode>& nodes)
        {
            for(const LevelNode& node : nodes) {
                Q_ASSERT(node.spacings.size() == node.points.size());
                for(qsizetype i = 0; i < node.points.size(); i++) {
                    m_spacings.insert(PositionKey(node.points.at(i)), node.spacings.at(i));
                }
            }
        }

        QVector<float> spacingsOf(const QVector<QVector3D>& points) const
        {
            QVector<float> spacings;
            spacings.reserve(points.size());
            for(const QVector3D& point : points) {
                //sampleUp only moves points, so every position is held; one
                //that is not draws at the cap rather than at the floor
                const auto found = m_spacings.constFind(PositionKey(point));
                Q_ASSERT(found != m_spacings.constEnd());
                spacings.append(found != m_spacings.constEnd()
                                    ? found.value()
                                    : std::numeric_limits<float>::infinity());
            }
            return spacings;
        }

    private:
        QHash<PositionKey, float> m_spacings;
    };

    //What pass C knows of a node it wrote
    struct TopNodeRecord {
        quint32 pointCount = 0;
        float measuredSpacing = 0.0f;
    };

    cwPointOctreeNode manifestNode(const Cell& cell, quint32 pointCount, float measuredSpacing)
    {
        cwPointOctreeNode node;
        node.level = cell.level;
        node.x = cell.x;
        node.y = cell.y;
        node.z = cell.z;
        node.pointCount = pointCount;
        node.byteSize = qint64(pointCount) * kBytesPerPoint;
        node.measuredSpacing = measuredSpacing;
        return node;
    }
}

qint64 cw::octree::passBColumnBytes(const QVector<qint64>& chunkPointCounts, qint64 haloPoints)
{
    qint64 bytes = haloPoints * kPassBHaloBytesPerPoint;
    for(const qint64 points : chunkPointCounts) {
        bytes += points * (kPassBWorkingSetBytesPerPoint + kPassBLocalSpacingBytesPerPoint);
    }
    return bytes;
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

std::shared_ptr<const cwPointOctreePack> cwPointOctreeBuilder::cachedPack(const Request& request)
{
    const QString fingerprint = fingerprintFor(request);
    if(fingerprint.isEmpty()) {
        return nullptr;
    }

    std::shared_ptr<const cwPointOctreePack> pack =
        cwPointOctreePack::open(packPath(request.cacheRootPath, request.path));
    if(pack == nullptr || pack->manifest().fingerprint != fingerprint) {
        return nullptr;
    }

    return pack;
}

std::optional<cwPointOctreeManifest> cwPointOctreeBuilder::cachedManifest(const Request& request)
{
    const std::shared_ptr<const cwPointOctreePack> pack = cachedPack(request);
    if(pack == nullptr) {
        return std::nullopt;
    }

    return pack->manifest();
}

QFuture<cwPointOctreeBuilder::Result> cwPointOctreeBuilder::build(const Request& request)
{
    cwTaskContextScope traceScope(QStringLiteral("octree"));
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

            //The occupied chunk count is a Pass A answer, so the estimate uses
            //every cell at depth. It is a floor: the chunk cap can push chunks
            //a level or more deeper, and the reporter clamps what runs past it.
            qint64 nodeCountEstimate = kChildCount;
            for(int level = 0; level < depth; level++) {
                nodeCountEstimate *= kChildCount;
            }
            Reporter progress(promise,
                              npoints + kPassBPointWeight * npoints + nodeCountEstimate);

            const auto fail = [&promise, &progress](const QString& message, int code) {
                progress.dismiss();
                promise.addResult(Result(message, code));
            };

            const QString cancelledMessage =
                QStringLiteral("The octree build of %1 was cancelled.").arg(request.path);

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

            cwProfilePhase buildPhase = buildProfilePhase(QStringLiteral("total"));
            cwProfilePhase passPhase = buildProfilePhase(QStringLiteral("passA"));

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

            passPhase.addField(QStringLiteral("points"), passA.pointCount);
            passPhase.addField(QStringLiteral("ranges"), passA.rangeCount);
            passPhase.addField(QStringLiteral("workers"), workerCount);
            passPhase.finish();
            passPhase = buildProfilePhase(QStringLiteral("split"));

            //A cell over the cap is a pass B task no budget can admit beside
            //another, so it becomes eight cells a level deeper
            const qint64 chunkMaxPoints = request.chunkMaxPoints > 0
                                              ? request.chunkMaxPoints
                                              : kChunkMaxPointsMultiple * request.chunkTargetPoints;

            const SplitResult split =
                splitOversizedCells(passA.parts, chunkMaxPoints, root, tempPath, promise);

            if(split.errorCode != Monad::ResultBase::NoError) {
                fail(split.errorCode == Cancelled ? cancelledMessage : split.errorMessage,
                     split.errorCode);
                return;
            }

            passPhase.addField(QStringLiteral("cells"), split.cellsSplit);
            passPhase.addField(QStringLiteral("maxPoints"), chunkMaxPoints);
            passPhase.finish();
            passPhase = buildProfilePhase(QStringLiteral("halo"));

            QVector<ChunkTask> tasks;
            tasks.reserve(passA.parts.size());

            for(auto it = passA.parts.constBegin(); it != passA.parts.constEnd(); ++it) {
                tasks.append(ChunkTask {cellFromKey(it.key()), it.value(), pointsIn(it.value())});
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

            //A split cell's chunks sit deeper than an unsplit one's, so pass C
            //starts at the deepest chunk level rather than at the chunk depth
            int deepestChunkLevel = 0;
            for(const ChunkTask& task : std::as_const(tasks)) {
                deepestChunkLevel = std::max(deepestChunkLevel, task.cell.level);
            }

            std::atomic<bool> cancelFlag {false};
            std::atomic<qint64> chunkPointsDone {0};

            const qint64 memoryBudgetBytes = request.memoryBudgetBytes > 0
                                                 ? request.memoryBudgetBytes
                                                 : kDefaultBuildMemoryBudgetBytes;

            ColumnsResult columns =
                planColumns(tasks, root, tempPath, promise, progress, passA.pointCount);
            if(columns.errorCode != Monad::ResultBase::NoError) {
                fail(columns.errorCode == Cancelled ? cancelledMessage : columns.errorMessage,
                     columns.errorCode);
                return;
            }

            //A canceled or failed build destroys the writer before finish(),
            //which removes its temp file
            cwPointOctreePack::Writer writer(packPath(request.cacheRootPath, request.path));
            if(!writer.isOpen()) {
                fail(writer.errorMessage(), CacheWriteFailed);
                return;
            }

            const ChunkContext context {
                .writer = &writer,
                .rootMin = root.minimum,
                .rootSize = root.size,
                .cancel = &cancelFlag,
                .pointsDone = &chunkPointsDone
            };

            qint64 haloPoints = 0;
            for(const ColumnTask& column : std::as_const(columns.columns)) {
                haloPoints += column.haloPoints;
            }
            passPhase.addField(QStringLiteral("columns"), columns.columns.size());
            passPhase.addField(QStringLiteral("haloPoints"), haloPoints);
            passPhase.finish();
            passPhase = buildProfilePhase(QStringLiteral("passB"));

            PassBResult passB = runPassB(tasks, columns.columns, context, memoryBudgetBytes,
                                         promise, progress, passA.pointCount);

            qint64 localSpacingMilliseconds = 0;
            qint64 localSpacingCappedPoints = 0;
            std::optional<std::pair<double, double>> ceilingRange;
            for(const std::optional<ColumnResult>& column : std::as_const(passB.columns)) {
                if(!column || column->errorCode != Monad::ResultBase::NoError) {
                    continue;
                }
                localSpacingMilliseconds += column->localSpacingMilliseconds;
                localSpacingCappedPoints += column->localSpacingCappedPoints;
                const double ceiling = column->localSpacingCeiling;
                if(ceilingRange) {
                    ceilingRange->first = std::min(ceilingRange->first, ceiling);
                    ceilingRange->second = std::max(ceilingRange->second, ceiling);
                } else {
                    ceilingRange = std::pair {ceiling, ceiling};
                }
            }
            const QString ceilings = ceilingRange
                                         ? QStringLiteral("%1..%2")
                                               .arg(ceilingRange->first)
                                               .arg(ceilingRange->second)
                                         : QStringLiteral("none");
            passPhase.addField(QStringLiteral("chunks"), tasks.size());
            passPhase.addField(QStringLiteral("columns"), columns.columns.size());
            passPhase.addField(QStringLiteral("budgetBytes"), memoryBudgetBytes);
            passPhase.addField(QStringLiteral("inFlightPeakBytes"), passB.peakBytes);
            passPhase.addField(QStringLiteral("localSpacingChunkMs"), localSpacingMilliseconds);
            passPhase.addField(QStringLiteral("localSpacingCappedPoints"), localSpacingCappedPoints);
            passPhase.addField(QStringLiteral("localSpacingCeilings"), ceilings);
            passPhase.finish();
            passPhase = buildProfilePhase(QStringLiteral("passC"));

            if(passB.errorCode != Monad::ResultBase::NoError) {
                fail(passB.errorCode == Cancelled ? cancelledMessage : passB.errorMessage,
                     passB.errorCode);
                return;
            }

            QVector<std::optional<ChunkResult>> chunkResults = std::move(passB.chunks);

            QHash<quint64, qsizetype> chunkIndexByCell;
            chunkIndexByCell.reserve(tasks.size());

            for(qsizetype i = 0; i < tasks.size(); i++) {
                chunkIndexByCell.insert(cellKey(tasks.at(i).cell), i);
            }

            //A chunk root joins pass C at the level its own cell sits at. A
            //split cell's children are disjoint from every unsplit cell, so a
            //chunk root never lands on a parent the sampler already made.
            const auto insertChunkRootsAt = [&](int chunkLevel, QHash<quint64, LevelNode>& into) {
                for(qsizetype i = 0; i < tasks.size(); i++) {
                    const Cell& cell = tasks.at(i).cell;
                    if(cell.level != chunkLevel) {
                        continue;
                    }

                    into.insert(cellKey(cell),
                                LevelNode {cell, std::move(chunkResults[i]->rootPoints),
                                           std::move(chunkResults[i]->rootSpacings)});
                }
            };

            QHash<quint64, LevelNode> level;
            level.reserve(tasks.size());
            insertChunkRootsAt(deepestChunkLevel, level);

            //Pass C: every node the sampler pulls from is final only once its parent has sampled it
            QHash<quint64, TopNodeRecord> topNodes;
            qint64 nodesWritten = 0;
            const qint64 passCProgressBase =
                passA.pointCount + kPassBPointWeight * passA.pointCount;

            //What carrying pass B's native spacings through the top tree took
            QElapsedTimer topLocalSpacingTimer;
            qint64 topLocalSpacingMilliseconds = 0;

            const auto writeTopNode = [&](const LevelNode& node) {
                const QVector<quint16> codes = encodeLocalSpacings(node.spacings);
                const QByteArray payload =
                    quantizeAll(node.points, codes, cellBounds(node.cell, root.minimum, root.size));

                if(!writer.append(cellKey(node.cell), payload)) {
                    fail(writer.errorMessage(), CacheWriteFailed);
                    return false;
                }

                topNodes.insert(cellKey(node.cell),
                                TopNodeRecord {quint32(node.points.size()),
                                               measureNodeSpacing(node.points)});
                nodesWritten++;
                progress.report(passCProgressBase + nodesWritten);
                return true;
            };

            for(int parentLevel = deepestChunkLevel - 1; parentLevel >= 0; parentLevel--) {
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
                        children.append(LevelNode {found->cell, std::move(found->points),
                                                   std::move(found->spacings)});
                    }

                    topLocalSpacingTimer.start();
                    const SpacingByPosition spacingByPosition(children);
                    topLocalSpacingMilliseconds += topLocalSpacingTimer.elapsed();

                    QVector<QVector<QVector3D>*> sources;
                    sources.reserve(children.size());
                    for(LevelNode& child : children) {
                        sources.append(&child.points);
                    }

                    QVector<QVector3D> parentPoints =
                        sampleUp(cellBounds(parentCell, root.minimum, root.size), sources);

                    //sampleUp moved points out of the children, so their
                    //spacings follow by position
                    topLocalSpacingTimer.start();
                    QVector<float> parentSpacings = spacingByPosition.spacingsOf(parentPoints);
                    for(LevelNode& child : children) {
                        child.spacings = spacingByPosition.spacingsOf(child.points);
                    }
                    topLocalSpacingMilliseconds += topLocalSpacingTimer.elapsed();

                    parents.insert(it.key(), LevelNode {parentCell, std::move(parentPoints),
                                                        std::move(parentSpacings)});

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

                insertChunkRootsAt(parentLevel, parents);
                level = std::move(parents);
            }

            const Cell rootCell {0, 0, 0, 0};
            const LevelNode rootNode = level.value(cellKey(rootCell), LevelNode {rootCell, {}, {}});
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
                const TopNodeRecord top = topNodes.value(cellKey(cell));
                manifest.nodes.append(manifestNode(cell, top.pointCount, top.measuredSpacing));

                if(chunkIndexByCell.contains(cellKey(cell))) {
                    const QVector<SubtreeRecord>& records =
                        chunkResults.at(chunkIndexByCell.value(cellKey(cell)))->nodes;

                    for(qsizetype i = 1; i < records.size(); i++) {
                        const SubtreeRecord& record = records.at(i);

                        cwPointOctreeNode child =
                            manifestNode(record.cell, record.pointCount, record.measuredSpacing);
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
                    if(!topNodes.contains(cellKey(child))) {
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

            if(!writer.finish(manifest)) {
                fail(writer.errorMessage(), CacheWriteFailed);
                return;
            }

            passPhase.addField(QStringLiteral("nodes"), manifest.nodes.size());
            passPhase.addField(QStringLiteral("localSpacingMs"), topLocalSpacingMilliseconds);
            passPhase.finish();
            if(buildPhase.logging()) {
                buildPhase.addField(QStringLiteral("peakRssBytes"), cw::profile::peakResidentBytes());
            }
            buildPhase.finish();

            promise.addResult(Result(manifest));
        });
}
