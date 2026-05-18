/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazClipOperation.h"

//Qt includes
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPromise>
#include <QRectF>
#include <QVector3D>

//Std includes
#include <atomic>
#include <cmath>
#include <limits>

//Our includes
#include "cwConcurrent.h"

// LAStools / LASlib
#include <LASlib/laswriter.hpp>

namespace {

constexpr qsizetype kCancelCheckStride = 64 * 1024;
constexpr double kOutputScale = 0.001; // mm precision for encoded coords
constexpr U8 kLasPointFormat = 0;
constexpr U16 kLasFormat0RecordLength = 20;

// Per-source validated slice into the geometry's packed Position buffer.
// Built in preflight so the hot loop doesn't have to re-validate or fall
// back on Q_ASSERTs.
struct PositionSlice {
    const QVector3D* points = nullptr;
    qsizetype count = 0;
};

const cwGeometry::VertexAttribute* packedPositionAttribute(const cwGeometry& geom)
{
    const cwGeometry::VertexAttribute* attr =
        geom.attribute(cwGeometry::Semantic::Position);
    if (attr == nullptr) {
        return nullptr;
    }
    // Same invariant as cwLazLoader: a single Vec3 Position attribute makes
    // the buffer a packed array of QVector3D so we can iterate by pointer.
    if (attr->bufferStride != int(sizeof(QVector3D))
        || attr->byteOffsetInBuffer != 0) {
        return nullptr;
    }
    return attr;
}

bool isFinite(const cwGeoPoint& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

bool isFinite(const QPolygonF& poly)
{
    for (const QPointF& p : poly) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) {
            return false;
        }
    }
    return true;
}

Monad::Result<qint64> clipOneSlice(const PositionSlice& slice,
                                   const cwLazClipOperation::Request& request,
                                   const QRectF& polygonBBox,
                                   LASwriter* writer,
                                   LASpoint& point,
                                   std::atomic<qint64>& pointsDone,
                                   QPromise<cwLazClipOperation::Result>& promise,
                                   int progressMax)
{
    const double ox = request.worldOrigin.x;
    const double oy = request.worldOrigin.y;
    const double oz = request.worldOrigin.z;
    const QPolygonF& polygon = request.polygonLocalXY;
    const bool keep = (request.mode == cwLazClipOperation::Mode::Keep);

    qint64 written = 0;
    qsizetype sinceReport = 0;

    auto publishProgress = [&](qsizetype delta) {
        if (delta <= 0) {
            return;
        }
        const qint64 done = pointsDone.fetch_add(qint64(delta),
                                                 std::memory_order_relaxed)
                            + qint64(delta);
        // Extra parens defeat the min/max macros from windows.h.
        promise.setProgressValue(int((std::min<qint64>)(done, qint64(progressMax))));
    };

    for (qsizetype i = 0; i < slice.count; ++i) {
        if (sinceReport >= kCancelCheckStride) {
            publishProgress(sinceReport);
            sinceReport = 0;
            if (promise.isCanceled()) {
                break;
            }
        }
        const QVector3D& v = slice.points[i];
        const double vx = double(v.x());
        const double vy = double(v.y());
        const double vz = double(v.z());
        const QPointF local(vx, vy);
        // BBox prefilter: containsPoint is O(V); a polygon-bbox reject is
        // 4 comparisons. Big win when the polygon covers a small fraction
        // of the point cloud extent (the common UI case).
        const bool inside = polygonBBox.contains(local)
                            && polygon.containsPoint(local, Qt::OddEvenFill);
        if (keep ? inside : !inside) {
            // Geometry vertices are worldOrigin-relative; the LAS writer
            // wants absolute coords so downstream tools see geographic
            // values. header.x_offset == worldOrigin so the encoded int
            // lands back on the same scale floor.
            point.set_x(vx + ox);
            point.set_y(vy + oy);
            point.set_z(vz + oz);
            if (!writer->write_point(&point)) {
                publishProgress(sinceReport);
                return Monad::Result<qint64>(
                    QStringLiteral("Failed to write LAZ point (disk full or I/O error)."),
                    cwLazClipOperation::WritePointFailed);
            }
            ++written;
        }
        ++sinceReport;
    }
    publishProgress(sinceReport);
    return Monad::Result<qint64>(written);
}

qint64 totalPointsFor(const QList<PositionSlice>& slices) noexcept
{
    qint64 total = 0;
    for (const PositionSlice& s : slices) {
        total += qint64(s.count);
    }
    return total;
}

} // namespace

QFuture<cwLazClipOperation::Result> cwLazClipOperation::run(const Request& request)
{
    return cwConcurrent::run(
        [request](QPromise<Result>& promise) {
            // Preflight — every failure here is a Result-with-ErrorCode.
            if (request.sources.isEmpty()) {
                promise.addResult(Result(
                    QStringLiteral("No source point clouds to clip."),
                    NoSources));
                return;
            }
            if (request.polygonLocalXY.size() < 3) {
                promise.addResult(Result(
                    QStringLiteral("Clip polygon needs at least 3 vertices."),
                    BadPolygon));
                return;
            }
            if (request.outputPath.isEmpty()) {
                promise.addResult(Result(
                    QStringLiteral("Output path is empty."),
                    EmptyOutputPath));
                return;
            }
            if (!isFinite(request.worldOrigin) || !isFinite(request.polygonLocalXY)) {
                promise.addResult(Result(
                    QStringLiteral("Polygon or worldOrigin contains non-finite values."),
                    NonFiniteInput));
                return;
            }

            // Build validated slices once. clipOneSlice can then read by
            // pointer without re-running positionAttribute / bounds checks.
            QList<PositionSlice> slices;
            slices.reserve(request.sources.size());
            for (int i = 0; i < request.sources.size(); ++i) {
                const cwGeometry& geom = request.sources.at(i);
                const cwGeometry::VertexAttribute* attr = packedPositionAttribute(geom);
                if (attr == nullptr) {
                    promise.addResult(Result(
                        QStringLiteral("Source %1 of %2 has no packed Vec3 Position attribute.")
                            .arg(i + 1).arg(request.sources.size()),
                        MissingPosition));
                    return;
                }
                const QByteArray* buf = geom.vertexBuffer(attr->bufferIndex);
                PositionSlice slice;
                slice.points = reinterpret_cast<const QVector3D*>(buf->constData());
                slice.count = geom.vertexCount();
                slices.append(slice);
            }

            const qint64 totalPoints = totalPointsFor(slices);
            // Progress range is int. For sources >INT_MAX the bar caps at full
            // before the run actually finishes; the clip itself still
            // completes.
            const int progressMax =
                totalPoints > qint64((std::numeric_limits<int>::max)())
                    ? (std::numeric_limits<int>::max)()
                    : int(totalPoints > 0 ? totalPoints : 1);
            promise.setProgressRange(0, progressMax);
            promise.setProgressValue(0);

            LASheader header;
            header.clean_las_header();
            header.x_scale_factor = kOutputScale;
            header.y_scale_factor = kOutputScale;
            header.z_scale_factor = kOutputScale;
            // Anchor encoded coords on worldOrigin so the int32 range stays
            // centered on relevant data even when CS coords are large (e.g.
            // UTM eastings).
            header.x_offset = request.worldOrigin.x;
            header.y_offset = request.worldOrigin.y;
            header.z_offset = request.worldOrigin.z;
            header.point_data_format = kLasPointFormat;
            header.point_data_record_length = kLasFormat0RecordLength;

            QByteArray wktBytes;
            if (!request.outputWktCS.isEmpty()) {
                wktBytes = request.outputWktCS.toLatin1();
                header.set_geo_ogc_wkt(wktBytes.size(), wktBytes.constData());
            }

            LASpoint pointTemplate;
            pointTemplate.init(&header, header.point_data_format,
                               header.point_data_record_length, &header);

            const QByteArray outPathBytes = request.outputPath.toUtf8();
            LASwriteOpener writeOpener;
            writeOpener.set_file_name(outPathBytes.constData());
            LASwriter* writer = writeOpener.open(&header);
            if (writer == nullptr) {
                promise.addResult(Result(
                    QStringLiteral("Could not open output for write: %1")
                        .arg(request.outputPath),
                    WriterOpenFailed));
                return;
            }

            const QRectF polygonBBox = request.polygonLocalXY.boundingRect();
            std::atomic<qint64> pointsDone{0};
            SuccessValue success;
            success.outputPath = request.outputPath;

            QString failureMessage;
            int failureCode = Monad::ResultBase::NoError;

            for (const PositionSlice& slice : slices) {
                if (promise.isCanceled()) {
                    break;
                }
                Monad::Result<qint64> sliceResult =
                    clipOneSlice(slice, request, polygonBBox, writer, pointTemplate,
                                 pointsDone, promise, progressMax);
                if (sliceResult.hasError()) {
                    failureMessage = sliceResult.errorMessage();
                    failureCode = sliceResult.errorCode();
                    break;
                }
                success.pointsWritten += sliceResult.value();
            }

            // Skip the header rewrite when we're about to delete the file —
            // update_header(...) on a large LAZ does a non-trivial flush.
            const bool keepingOutput =
                failureCode == Monad::ResultBase::NoError && !promise.isCanceled();
            bool finalizeFailed = false;
            if (keepingOutput) {
                if (!writer->update_header(&header, TRUE)) {
                    finalizeFailed = true;
                }
            }
            // close() flushes deferred LASzip chunks; check it too — a
            // disk-full at flush would otherwise silently produce a corrupt
            // .laz.
            if (writer->close() < 0 && keepingOutput) {
                finalizeFailed = true;
            }
            delete writer;

            if (promise.isCanceled()) {
                QFile::remove(request.outputPath);
                promise.addResult(Result(
                    QStringLiteral("Clip cancelled."),
                    Cancelled));
                return;
            }
            if (failureCode != Monad::ResultBase::NoError) {
                QFile::remove(request.outputPath);
                promise.addResult(Result(failureMessage, failureCode));
                return;
            }
            if (finalizeFailed) {
                QFile::remove(request.outputPath);
                promise.addResult(Result(
                    QStringLiteral("Could not finalize output LAZ (disk full or I/O error)."),
                    FinalizeFailed));
                return;
            }

            promise.setProgressValue(progressMax);
            promise.addResult(Result(success));
        });
}
