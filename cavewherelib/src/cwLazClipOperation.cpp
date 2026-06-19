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
#include <QPointF>
#include <QPolygonF>
#include <QPromise>
#include <QRectF>
#include <QVector3D>

//Std includes
#include <atomic>
#include <cmath>
#include <cstring>
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

// Per-source validated cursors into the geometry's vertex buffers. Built in
// preflight so the hot loop reads each attribute by stride without
// re-validating. Position and Classification may be packed (each in its own
// buffer, stride == element size) or interleaved (a shared PointRecord
// stride): cwLazLoader emits the interleaved {Position, Classification,
// SinkDepth} layout, the unit tests the packed one. Both reduce to
// base + i * stride here, so the clip is layout-agnostic.
struct PointSlice {
    const char* positionBase = nullptr;
    const char* classificationBase = nullptr; // null when the source has no class
    int positionStride = 0;
    int classificationStride = 0;
    qsizetype count = 0;
};

const char* attributeBase(const cwGeometry& geom,
                          const cwGeometry::VertexAttribute* attr)
{
    const QByteArray* buf = geom.vertexBuffer(attr->bufferIndex);
    return buf->constData() + attr->byteOffsetInBuffer;
}

bool isFinite(const cwGeoPoint& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

bool isFinite(const QList<QVector3D>& poly)
{
    for (const QVector3D& p : poly) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || !std::isfinite(p.z())) {
            return false;
        }
    }
    return true;
}

bool isFinite(const QMatrix4x4& m)
{
    const float* data = m.constData();
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(data[i])) {
            return false;
        }
    }
    return true;
}

QPolygonF projectPolygonToEyeXY(const QList<QVector3D>& worldXYZ,
                                const QMatrix4x4& view)
{
    QPolygonF result;
    result.reserve(worldXYZ.size());
    for (const QVector3D& v : worldXYZ) {
        const QVector3D eye = view.map(v);
        result.append(QPointF(double(eye.x()), double(eye.y())));
    }
    return result;
}

Monad::Result<qint64> clipOneSlice(const PointSlice& slice,
                                   const cwLazClipOperation::Request& request,
                                   const QPolygonF& polygonEyeXY,
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
    const QMatrix4x4& view = request.viewMatrix;
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
        QVector3D v;
        std::memcpy(&v, slice.positionBase + i * slice.positionStride, sizeof(QVector3D));
        const double vx = double(v.x());
        const double vy = double(v.y());
        const double vz = double(v.z());
        const QVector3D eye = view.map(v);
        const QPointF eyeXY(double(eye.x()), double(eye.y()));
        // BBox prefilter — 4 comparisons vs O(V) containsPoint. Big win
        // when the polygon covers a small fraction of the cloud extent.
        const bool inside = polygonBBox.contains(eyeXY)
                            && polygonEyeXY.containsPoint(eyeXY, Qt::OddEvenFill);
        if (keep ? inside : !inside) {
            // Geometry vertices are worldOrigin-relative; the LAS writer
            // wants absolute coords so downstream tools see geographic
            // values. header.x_offset == worldOrigin so the encoded int
            // lands back on the same scale floor.
            point.set_x(vx + ox);
            point.set_y(vy + oy);
            point.set_z(vz + oz);
            // Preserve the source LAS classification (ground=2, etc.) so the
            // clipped cloud keeps its ground/feature classes. Sources without
            // a Classification attribute write 0 (unclassified), the LAS
            // format-0 default.
            U8 classification = 0;
            if (slice.classificationBase != nullptr) {
                quint32 raw = 0;
                std::memcpy(&raw,
                            slice.classificationBase + i * slice.classificationStride,
                            sizeof(quint32));
                classification = U8(raw & 0xFFu);
            }
            point.set_classification(classification);
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

qint64 totalPointsFor(const QList<PointSlice>& slices) noexcept
{
    qint64 total = 0;
    for (const PointSlice& s : slices) {
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
            if (request.polygonWorldXYZ.size() < 3) {
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
            if (!isFinite(request.worldOrigin)
                || !isFinite(request.polygonWorldXYZ)
                || !isFinite(request.viewMatrix)) {
                promise.addResult(Result(
                    QStringLiteral("Polygon, worldOrigin, or viewMatrix contains non-finite values."),
                    NonFiniteInput));
                return;
            }

            // Build validated cursors once. clipOneSlice can then read each
            // attribute by stride without re-resolving attributes per point.
            QList<PointSlice> slices;
            slices.reserve(request.sources.size());
            for (int i = 0; i < request.sources.size(); ++i) {
                const cwGeometry& geom = request.sources.at(i);
                const cwGeometry::VertexAttribute* posAttr =
                    geom.attribute(cwGeometry::Semantic::Position);
                if (posAttr == nullptr
                    || posAttr->format != cwGeometry::AttributeFormat::Vec3) {
                    promise.addResult(Result(
                        QStringLiteral("Source %1 of %2 has no Vec3 Position attribute.")
                            .arg(i + 1).arg(request.sources.size()),
                        MissingPosition));
                    return;
                }
                PointSlice slice;
                slice.count = geom.vertexCount();
                slice.positionBase = attributeBase(geom, posAttr);
                slice.positionStride = posAttr->bufferStride;
                // Classification is optional: real LAZ sources carry it
                // (ground=2, etc.) and the clip preserves it; synthetic
                // Position-only geometry simply writes class 0.
                const cwGeometry::VertexAttribute* classAttr =
                    geom.attribute(cwGeometry::Semantic::Classification);
                if (classAttr != nullptr
                    && classAttr->format == cwGeometry::AttributeFormat::UInt) {
                    slice.classificationBase = attributeBase(geom, classAttr);
                    slice.classificationStride = classAttr->bufferStride;
                }
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

            const QPolygonF polygonEyeXY = projectPolygonToEyeXY(
                request.polygonWorldXYZ, request.viewMatrix);
            const QRectF polygonBBox = polygonEyeXY.boundingRect();
            std::atomic<qint64> pointsDone{0};
            SuccessValue success;
            success.outputPath = request.outputPath;

            QString failureMessage;
            int failureCode = Monad::ResultBase::NoError;

            for (const PointSlice& slice : slices) {
                if (promise.isCanceled()) {
                    break;
                }
                Monad::Result<qint64> sliceResult =
                    clipOneSlice(slice, request, polygonEyeXY, polygonBBox, writer,
                                 pointTemplate, pointsDone, promise, progressMax);
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
