/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLoader.h"

//Qt includes
#include <QDebug>
#include <QPromise>
#include <QThread>
#include <QThreadPool>
#include <QVector>

//Std includes
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

//Our includes
#include "cwConcurrent.h"
#include "cwCoordinateTransform.h"
#include "cwTask.h"

// LAStools / LASlib
#include <LASlib/lasreader.hpp>

// LASlib stores the OGC WKT CRS in vlr_geo_ogc_wkt when the file uses the
// modern (LAS 1.4 / OGC WKT) CRS encoding. PROJ accepts WKT strings directly,
// so we pass it straight through. Older LAS files use GeoTIFF GeoKeys
// (vlr_geo_keys / vlr_geo_key_entries), which we don't decode here — those
// files load with an empty source CS and the transform short-circuits to
// identity. The user can supply an explicit override to handle that case.
static QString extractEmbeddedCS(const LASheader& header)
{
    if (header.vlr_geo_ogc_wkt != nullptr && header.vlr_geo_ogc_wkt[0] != '\0') {
        return QString::fromLatin1(header.vlr_geo_ogc_wkt);
    }
    return QString();
}

QString cwLazLoader::resolveSourceCS(const QString& override, const LASheader& header)
{
    return override.isEmpty() ? extractEmbeddedCS(header) : override;
}

namespace {

struct WorkerRange {
    qsizetype startIndex = 0; //!< first point index this worker handles
    qsizetype count = 0;      //!< number of points to decode
    QVector3D* dst = nullptr; //!< write target inside the shared vertex buffer
};

struct WorkerResult {
    qsizetype written = 0;
    QVector3D bboxMin{std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::infinity()};
    QVector3D bboxMax{-std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity()};
};

struct WorkerContext {
    QString path;
    QString sourceCS;
    QString globalCS;
    cwGeoPoint worldOrigin;
    std::atomic<qsizetype>* pointsDone;
    std::atomic<bool>* cancel;
};

constexpr int kChunkSize = 64 * 1024;
// Below this, the partition + per-worker LASreader open overhead eats the
// parallel decode win. Tests run on 4–200 point files; those should stay
// single-threaded.
constexpr qsizetype kMinPointsPerWorker = 256 * 1024;

static int chooseWorkerCount(qsizetype effectiveMax)
{
    if (effectiveMax <= 0) {
        return 1;
    }
    // Leave one pool slot for the orchestrator that's polling progress; any
    // additional workers beyond this would just queue on the pool.
    const int byPool = std::max(1, QThread::idealThreadCount() - 1);
    const qsizetype byWork = std::max<qsizetype>(1, effectiveMax / kMinPointsPerWorker);
    return int(std::min<qsizetype>(byPool, byWork));
}

static QVector<WorkerRange> buildRanges(qsizetype effectiveMax,
                                        int workerCount,
                                        QVector3D* dstBase)
{
    QVector<WorkerRange> ranges;
    if (workerCount <= 0 || effectiveMax <= 0) {
        return ranges;
    }
    ranges.reserve(workerCount);
    const qsizetype base = effectiveMax / workerCount;
    const qsizetype remainder = effectiveMax % workerCount;
    qsizetype offset = 0;
    for (int i = 0; i < workerCount; ++i) {
        WorkerRange r;
        r.startIndex = offset;
        r.count = base + (i < remainder ? 1 : 0);
        r.dst = dstBase + offset;
        ranges.append(r);
        offset += r.count;
    }
    return ranges;
}

static WorkerResult decodeRange(const WorkerRange& range,
                                const WorkerContext& ctx)
{
    WorkerResult res;
    if (range.count <= 0) {
        return res;
    }
    if (ctx.cancel->load(std::memory_order_relaxed)) {
        return res;
    }

    const QByteArray pathBytes = ctx.path.toUtf8();
    LASreadOpener opener;
    opener.set_file_name(pathBytes.constData(), FALSE);
    LASreader* reader = opener.open();
    if (reader == nullptr) {
        qWarning() << "cwLazLoader: worker failed to open" << ctx.path;
        return res;
    }

    if (range.startIndex > 0) {
        if (!reader->seek(I64(range.startIndex))) {
            qWarning() << "cwLazLoader: seek to" << range.startIndex << "failed in" << ctx.path;
            reader->close();
            delete reader;
            return res;
        }
    }

    cwCoordinateTransform transform(ctx.sourceCS, ctx.globalCS);
    const bool hasTransform = !transform.isIdentity();

    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();

    QVector3D* const dst = range.dst;
    qsizetype written = 0;

    auto writeOne = [&](const QVector3D& v) {
        dst[written] = v;
        if (v.x() < minX) { minX = v.x(); }
        if (v.y() < minY) { minY = v.y(); }
        if (v.z() < minZ) { minZ = v.z(); }
        if (v.x() > maxX) { maxX = v.x(); }
        if (v.y() > maxY) { maxY = v.y(); }
        if (v.z() > maxZ) { maxZ = v.z(); }
        ++written;
    };

    if (hasTransform) {
        // Non-identity path: batch into a per-worker cwGeoPoint chunk so
        // transformInPlace amortizes PROJ call overhead.
        QVector<cwGeoPoint> sourceChunk;
        sourceChunk.reserve(kChunkSize);

        auto flushChunk = [&]() {
            if (sourceChunk.isEmpty()) {
                return;
            }
            transform.transformInPlace(sourceChunk.data(), sourceChunk.size());
            for (const cwGeoPoint& gp : std::as_const(sourceChunk)) {
                writeOne(gp.toVector3D(ctx.worldOrigin));
            }
            ctx.pointsDone->fetch_add(sourceChunk.size(), std::memory_order_relaxed);
            sourceChunk.clear();
        };

        while (written + qsizetype(sourceChunk.size()) < range.count
               && reader->read_point()) {
            if (ctx.cancel->load(std::memory_order_relaxed)) {
                break;
            }
            sourceChunk.append(cwGeoPoint(reader->point.get_x(),
                                          reader->point.get_y(),
                                          reader->point.get_z()));
            if (sourceChunk.size() >= kChunkSize) {
                flushChunk();
            }
        }
        flushChunk();
    } else {
        // Identity fast path: write straight into dst, no intermediate
        // cwGeoPoint buffer. This is the common case (most LAZ tiles already
        // sit in the project's working CS) and removes one full pass over the
        // chunk plus a per-worker heap allocation.
        const double ox = ctx.worldOrigin.x;
        const double oy = ctx.worldOrigin.y;
        const double oz = ctx.worldOrigin.z;
        qsizetype sinceReport = 0;
        while (written < range.count && reader->read_point()) {
            // Cancel check every chunkSize points — keeps the inner loop
            // tight while still bounding cancel latency to ~one chunk.
            if (sinceReport == 0
                && ctx.cancel->load(std::memory_order_relaxed)) {
                break;
            }
            writeOne(QVector3D(float(reader->point.get_x() - ox),
                               float(reader->point.get_y() - oy),
                               float(reader->point.get_z() - oz)));
            ++sinceReport;
            if (sinceReport >= kChunkSize) {
                ctx.pointsDone->fetch_add(sinceReport, std::memory_order_relaxed);
                sinceReport = 0;
            }
        }
        if (sinceReport > 0) {
            ctx.pointsDone->fetch_add(sinceReport, std::memory_order_relaxed);
        }
    }

    reader->close();
    delete reader;

    res.written = written;
    res.bboxMin = QVector3D(minX, minY, minZ);
    res.bboxMax = QVector3D(maxX, maxY, maxZ);
    return res;
}

} // namespace

QFuture<cwLazLoadResult> cwLazLoader::load(const Request& request)
{
    return cwConcurrent::run(
        [request](QPromise<cwLazLoadResult>& promise)
        {
            const QString& path = request.path;
            const QString& sourceCSOverride = request.sourceCSOverride;
            const QString& globalCS = request.globalCS;
            const cwGeoPoint& worldOrigin = request.worldOrigin;
            const qsizetype maxPoints = request.maxPoints;

            cwLazLoadResult result;
            result.geometry = cwGeometry({
                {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
            });
            result.geometry.setType(cwGeometry::Type::Points);

            const QByteArray pathBytes = path.toUtf8();

            // Header pass: extract CS + npoints, then close. Workers reopen
            // the file independently so each has its own LASreader instance.
            LASreadOpener headerOpener;
            headerOpener.set_file_name(pathBytes.constData(), FALSE);
            LASreader* headerReader = headerOpener.open();
            if (headerReader == nullptr) {
                qWarning() << "cwLazLoader: failed to open" << path;
                promise.addResult(result);
                return;
            }

            const QString embeddedCS = extractEmbeddedCS(headerReader->header);
            result.sourceCS = !sourceCSOverride.isEmpty() ? sourceCSOverride : embeddedCS;
            const I64 totalPoints = headerReader->npoints;
            headerReader->close();
            delete headerReader;

            qsizetype effectiveMax = (maxPoints < 0)
                                         ? qsizetype(totalPoints)
                                         : std::min(qsizetype(totalPoints), maxPoints);

            // QProgressRange takes int. Clamp the *progress reporting* (not
            // the load itself) to INT_MAX — for files larger than that the
            // bar just looks "stuck" near full but the load still completes.
            const int progressMax = effectiveMax > qsizetype(std::numeric_limits<int>::max())
                                        ? std::numeric_limits<int>::max()
                                        : int(effectiveMax > 0 ? effectiveMax : 1);
            promise.setProgressRange(0, progressMax);
            promise.setProgressValue(0);

            if (effectiveMax <= 0) {
                promise.addResult(std::move(result));
                return;
            }

            // Reserve once — the buffer is contiguous; incremental grows would
            // copy ~12 * N bytes on each resize.
            result.geometry.resizeVertices(effectiveMax);

            const cwGeometry::VertexAttribute* positionAttribute =
                result.geometry.attribute(cwGeometry::Semantic::Position);
            Q_ASSERT(positionAttribute);
            // Single Vec3 Position attribute → stride == sizeof(QVector3D), so
            // the vertex buffer is a packed array of QVector3D and we write
            // converted points directly into it (no intermediate buffer).
            Q_ASSERT(positionAttribute->bufferStride == int(sizeof(QVector3D)));
            Q_ASSERT(positionAttribute->byteOffsetInBuffer == 0);

            QByteArray* const positionBuffer =
                result.geometry.mutableVertexBuffer(positionAttribute->bufferIndex);
            Q_ASSERT(positionBuffer);
            QVector3D* const dstBase = reinterpret_cast<QVector3D*>(positionBuffer->data());

            const int workerCount = chooseWorkerCount(effectiveMax);
            QVector<WorkerRange> ranges = buildRanges(effectiveMax, workerCount, dstBase);

            std::atomic<qsizetype> pointsDone{0};
            std::atomic<bool> cancelFlag{false};

            const WorkerContext ctx{
                .path = path,
                .sourceCS = result.sourceCS,
                .globalCS = globalCS,
                .worldOrigin = worldOrigin,
                .pointsDone = &pointsDone,
                .cancel = &cancelFlag
            };

            QList<WorkerResult> workerResults;
            workerResults.reserve(ranges.size());

            if (ranges.size() == 1) {
                // Single-worker fast path — skip the QtConcurrent::mapped
                // dispatch and the orchestrator's polling loop. Just decode
                // inline and publish progress from the worker via the same
                // atomic (so cancellation still works).
                WorkerResult r = decodeRange(ranges[0], ctx);
                workerResults.append(r);
                if (promise.isCanceled()) {
                    cancelFlag.store(true);
                }
            } else {
                auto workerFn = [ctx](const WorkerRange& r) -> WorkerResult {
                    return decodeRange(r, ctx);
                };

                QFuture<WorkerResult> mapFuture =
                    cwConcurrent::mapped(ranges, workerFn);

                // We're a task on cwTask::threadPool() that's about to block
                // waiting for other tasks on the same pool. Release our slot
                // so the pool can scale up and actually run all workers; we
                // reclaim before returning.
                QThreadPool* pool = cwTask::threadPool();
                pool->releaseThread();

                while (!mapFuture.isFinished()) {
                    if (promise.isCanceled() && !cancelFlag.load(std::memory_order_relaxed)) {
                        cancelFlag.store(true, std::memory_order_relaxed);
                        mapFuture.cancel();
                    }
                    const qsizetype done = pointsDone.load(std::memory_order_relaxed);
                    const qsizetype scaled = std::min<qsizetype>(done, progressMax);
                    promise.setProgressValue(int(scaled));
                    QThread::msleep(30);
                }

                pool->reserveThread();

                mapFuture.waitForFinished();
                workerResults = mapFuture.results();
            }

            // Fold per-worker bboxes and counts. If any worker stopped short
            // (cancel or seek failure), compact the buffer so the prefix
            // contains exactly the points that were actually written.
            qsizetype totalWritten = 0;
            float minX = std::numeric_limits<float>::infinity();
            float minY = std::numeric_limits<float>::infinity();
            float minZ = std::numeric_limits<float>::infinity();
            float maxX = -std::numeric_limits<float>::infinity();
            float maxY = -std::numeric_limits<float>::infinity();
            float maxZ = -std::numeric_limits<float>::infinity();

            for (int i = 0; i < ranges.size(); ++i) {
                const WorkerRange& rng = ranges[i];
                const WorkerResult& wr = workerResults[i];

                if (wr.written > 0) {
                    if (wr.bboxMin.x() < minX) { minX = wr.bboxMin.x(); }
                    if (wr.bboxMin.y() < minY) { minY = wr.bboxMin.y(); }
                    if (wr.bboxMin.z() < minZ) { minZ = wr.bboxMin.z(); }
                    if (wr.bboxMax.x() > maxX) { maxX = wr.bboxMax.x(); }
                    if (wr.bboxMax.y() > maxY) { maxY = wr.bboxMax.y(); }
                    if (wr.bboxMax.z() > maxZ) { maxZ = wr.bboxMax.z(); }

                    QVector3D* writeHead = dstBase + totalWritten;
                    if (writeHead != rng.dst) {
                        std::memmove(writeHead, rng.dst, size_t(wr.written) * sizeof(QVector3D));
                    }
                    totalWritten += wr.written;
                }
            }

            if (totalWritten < effectiveMax) {
                result.geometry.resizeVertices(totalWritten);
            }

            if (totalWritten > 0) {
                result.bboxMin = QVector3D(minX, minY, minZ);
                result.bboxMax = QVector3D(maxX, maxY, maxZ);

                // Mean planar spacing — treats the cloud as a roughly 2D surface
                // (terrestrial/airborne LAZ). The 1e-3 floor on dx,dy protects
                // against a degenerate bbox (vertical-only scan) producing
                // spacing = 0 and a sub-pixel point size downstream.
                const double dx = std::max(double(maxX - minX), 1e-3);
                const double dy = std::max(double(maxY - minY), 1e-3);
                result.meanSpacingXY = float(std::sqrt(dx * dy / double(totalWritten)));
            }

            promise.setProgressValue(progressMax);

            promise.addResult(std::move(result));
        });
}

cwLazLoader::ProbeResult cwLazLoader::probeHeader(const QString& path)
{
    ProbeResult result;

    const QByteArray pathBytes = path.toUtf8();

    LASreadOpener opener;
    opener.set_file_name(pathBytes.constData(), FALSE);
    LASreader* reader = opener.open();
    if (reader == nullptr) {
        return result;
    }

    const LASheader& header = reader->header;
    result.sourceCS = extractEmbeddedCS(header);
    result.bboxMin = cwGeoPoint(header.min_x, header.min_y, header.min_z);
    result.bboxMax = cwGeoPoint(header.max_x, header.max_y, header.max_z);
    result.bboxCenter = cwGeoPoint(0.5 * (header.min_x + header.max_x),
                                   0.5 * (header.min_y + header.max_y),
                                   0.5 * (header.min_z + header.max_z));
    result.valid = true;

    reader->close();
    delete reader;
    return result;
}
