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
#include <QFileInfo>
#include <QPromise>

//Std includes
#include <atomic>
#include <cmath>
#include <limits>

//Our includes
#include "cwConcurrent.h"
#include "cwCoordinateTransform.h"
#include "cwLazLoader.h"

// LAStools / LASlib
#include <LASlib/lasreader.hpp>
#include <LASlib/laswriter.hpp>

namespace {

constexpr int kChunkSize = 64 * 1024;
constexpr double kOutputScale = 0.001; // mm precision for encoded coords

struct SourcePassResult {
    bool ok = false;
    qint64 written = 0;
};

SourcePassResult clipOneSource(const cwLazClipSource& src,
                               const cwLazClipOperation::Request& request,
                               LASwriter* writer,
                               LASpoint& point,
                               std::atomic<qint64>& pointsDone,
                               QPromise<cwLazClipOperation::Result>& promise,
                               int progressMax)
{
    SourcePassResult res;

    const QByteArray pathBytes = src.sourcePath.toUtf8();
    LASreadOpener opener;
    opener.set_file_name(pathBytes.constData(), FALSE);
    LASreader* reader = opener.open();
    if (reader == nullptr) {
        qWarning() << "cwLazClipOperation: failed to open" << src.sourcePath;
        return res;
    }

    const QString sourceCS =
        cwLazLoader::resolveSourceCS(src.sourceCSOverride, reader->header);
    cwCoordinateTransform transform(sourceCS, request.globalCS);
    const bool hasTransform = !transform.isIdentity();

    const double ox = request.worldOrigin.x;
    const double oy = request.worldOrigin.y;
    const QPolygonF& polygon = request.polygonLocalXY;
    const bool keep = (request.mode == cwLazClipOperation::Mode::Keep);

    auto publishProgress = [&](qint64 delta) {
        if (delta <= 0) {
            return;
        }
        const qint64 done = pointsDone.fetch_add(delta, std::memory_order_relaxed) + delta;
        const qint64 clamped = std::min<qint64>(done, qint64(progressMax));
        promise.setProgressValue(int(clamped));
    };

    auto testAndWrite = [&](double sx, double sy, double sz) {
        const QPointF local(sx - ox, sy - oy);
        const bool inside = polygon.containsPoint(local, Qt::OddEvenFill);
        if (keep ? inside : !inside) {
            point.set_x(sx);
            point.set_y(sy);
            point.set_z(sz);
            writer->write_point(&point);
            ++res.written;
        }
    };

    if (hasTransform) {
        // Batch into a per-chunk cwGeoPoint buffer so transformInPlace
        // amortizes PROJ's per-call overhead — same pattern as cwLazLoader.
        QVector<cwGeoPoint> sourceChunk;
        sourceChunk.reserve(kChunkSize);

        auto flushChunk = [&]() {
            if (sourceChunk.isEmpty()) {
                return;
            }
            transform.transformInPlace(sourceChunk.data(), sourceChunk.size());
            for (const cwGeoPoint& gp : std::as_const(sourceChunk)) {
                testAndWrite(gp.x, gp.y, gp.z);
            }
            publishProgress(sourceChunk.size());
            sourceChunk.clear();
        };

        while (reader->read_point()) {
            if (sourceChunk.isEmpty() && promise.isCanceled()) {
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
        qint64 sinceReport = 0;
        while (reader->read_point()) {
            if (sinceReport == 0 && promise.isCanceled()) {
                break;
            }
            testAndWrite(reader->point.get_x(),
                         reader->point.get_y(),
                         reader->point.get_z());
            ++sinceReport;
            if (sinceReport >= kChunkSize) {
                publishProgress(sinceReport);
                sinceReport = 0;
            }
        }
        publishProgress(sinceReport);
    }

    reader->close();
    delete reader;

    res.ok = true;
    return res;
}

qint64 totalPointsFor(const QList<cwLazClipSource>& sources)
{
    qint64 total = 0;
    for (const cwLazClipSource& src : sources) {
        const QByteArray pathBytes = src.sourcePath.toUtf8();
        LASreadOpener opener;
        opener.set_file_name(pathBytes.constData(), FALSE);
        LASreader* reader = opener.open();
        if (reader == nullptr) {
            continue;
        }
        total += qint64(reader->npoints);
        reader->close();
        delete reader;
    }
    return total;
}

} // namespace

QFuture<cwLazClipOperation::Result> cwLazClipOperation::run(const Request& request)
{
    return cwConcurrent::run(
        [request](QPromise<Result>& promise) {
            Result result;
            result.outputPath = request.outputPath;

            if (request.sources.isEmpty()) {
                result.errorMessage = QStringLiteral("No source LAZ layers to clip.");
                promise.addResult(std::move(result));
                return;
            }
            if (request.polygonLocalXY.size() < 3) {
                result.errorMessage =
                    QStringLiteral("Clip polygon needs at least 3 vertices.");
                promise.addResult(std::move(result));
                return;
            }
            if (request.outputPath.isEmpty()) {
                result.errorMessage = QStringLiteral("Output path is empty.");
                promise.addResult(std::move(result));
                return;
            }

            const qint64 totalPoints = totalPointsFor(request.sources);
            // Progress range is int. For files >INT_MAX the bar caps at full
            // before the run actually finishes; the clip itself still
            // completes.
            const int progressMax = totalPoints > qint64(std::numeric_limits<int>::max())
                                        ? std::numeric_limits<int>::max()
                                        : int(totalPoints > 0 ? totalPoints : 1);
            promise.setProgressRange(0, progressMax);
            promise.setProgressValue(0);

            LASheader header;
            header.clean_las_header();
            header.x_scale_factor = kOutputScale;
            header.y_scale_factor = kOutputScale;
            header.z_scale_factor = kOutputScale;
            // Anchor encoded coords on worldOrigin so the int32 range stays
            // centered on relevant data even when regionGlobalCS coords are
            // large (e.g. UTM eastings).
            header.x_offset = request.worldOrigin.x;
            header.y_offset = request.worldOrigin.y;
            header.z_offset = request.worldOrigin.z;
            header.point_data_format = 0;
            header.point_data_record_length = 20;

            QByteArray wktBytes;
            if (!request.globalCS.isEmpty()) {
                wktBytes = request.globalCS.toLatin1();
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
                result.errorMessage =
                    QStringLiteral("Could not open output for write: %1")
                        .arg(request.outputPath);
                promise.addResult(std::move(result));
                return;
            }

            std::atomic<qint64> pointsDone{0};
            bool anySourceFailed = false;

            for (const cwLazClipSource& src : request.sources) {
                if (promise.isCanceled()) {
                    break;
                }
                SourcePassResult srcRes =
                    clipOneSource(src, request, writer, pointTemplate,
                                  pointsDone, promise, progressMax);
                if (!srcRes.ok) {
                    anySourceFailed = true;
                }
                result.pointsWritten += srcRes.written;
            }

            writer->update_header(&header, TRUE);
            writer->close();
            delete writer;

            if (promise.isCanceled()) {
                result.errorMessage = QStringLiteral("Clip cancelled.");
                promise.addResult(std::move(result));
                return;
            }
            if (anySourceFailed) {
                result.errorMessage =
                    QStringLiteral("One or more source LAZ files could not be read.");
                // Leave the partial output on disk — caller decides whether
                // to publish it or unlink it.
            }
            result.success = !anySourceFailed;
            promise.setProgressValue(progressMax);
            promise.addResult(std::move(result));
        });
}
