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
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QPromise>
#include <QRectF>
#include <QVector3D>

//Std includes
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

//Our includes
#include "cwConcurrent.h"
#include "cwCoordinateTransform.h"
#include "cwLazLoader.h"
#include "cwProgressReporter.h"

// LAStools / LASlib
#include <LASlib/lasreader.hpp>
#include <LASlib/laswriter.hpp>

namespace {

constexpr qsizetype kChunkSize = 64 * 1024;
constexpr double kOutputScale = 0.001; // mm precision for encoded coords

// Point formats 6–10 are LAS 1.4 only. A 1.2 header has no 64-bit
// extended_number_of_point_records field and a format-6+ writer leaves the
// legacy 32-bit count at 0, so the on-disk point count is lost (readers see an
// empty cloud). Stamp the header as 1.4 with its larger size whenever the
// chosen output format is extended.
constexpr U8 kLas14VersionMinor = 4;
constexpr U16 kLas14HeaderSize = 375;

// Standard LAS point-record byte lengths by point_data_format. Used when
// several sources of differing formats merge into one output: the chosen
// merge format carries only its standard fields (no source extra bytes), so
// the output record is exactly the standard length. A single source (or
// uniform-format set) instead reuses the source's own record length so any
// extra bytes pass straight through.
U16 standardRecordLength(U8 format) noexcept
{
    switch (format) {
    case 0: return 20;
    case 1: return 28;
    case 2: return 26;
    case 3: return 34;
    case 4: return 57;
    case 5: return 63;
    case 6: return 30;
    case 7: return 36;
    case 8: return 38;
    case 9: return 59;
    case 10: return 67;
    default: return 0;
    }
}

bool formatHasGpsTime(U8 f) noexcept
{
    return f == 1 || f == 3 || f == 4 || f == 5 || f >= 6;
}

bool formatHasRgb(U8 f) noexcept
{
    return f == 2 || f == 3 || f == 5 || f == 7 || f == 8 || f == 10;
}

bool formatHasNir(U8 f) noexcept
{
    return f == 8 || f == 10;
}

bool formatIsExtended(U8 f) noexcept
{
    return f >= 6;
}

struct OutputFormat {
    U8 format = 0;
    U16 recordLength = standardRecordLength(0);
};

// Pick one output point format that is a superset of every source's standard
// attributes, so each source upconverts into it via LASpoint::operator= with
// no loss. A uniform-format set keeps the exact source record length (extra
// bytes survive); a mixed set falls back to the richest *standard* format.
// Wavepacket-only fields (formats 4/5/9/10) are not specifically merged —
// their gps/rgb content is preserved, the waveform payload is dropped on a
// mixed merge (it survives the uniform case unchanged).
OutputFormat chooseOutputFormat(const QList<U8>& formats,
                                const QList<U16>& recordLengths) noexcept
{
    OutputFormat out;
    if (formats.isEmpty()) {
        return out;
    }

    bool uniform = true;
    for (int i = 1; i < formats.size(); ++i) {
        if (formats.at(i) != formats.at(0)
            || recordLengths.at(i) != recordLengths.at(0)) {
            uniform = false;
            break;
        }
    }
    if (uniform) {
        out.format = formats.at(0);
        out.recordLength = recordLengths.at(0);
        return out;
    }

    bool anyExtended = false;
    bool needGps = false;
    bool needRgb = false;
    bool needNir = false;
    for (U8 f : formats) {
        anyExtended = anyExtended || formatIsExtended(f);
        needGps = needGps || formatHasGpsTime(f);
        needRgb = needRgb || formatHasRgb(f);
        needNir = needNir || formatHasNir(f);
    }

    if (anyExtended) {
        out.format = needRgb ? (needNir ? U8(8) : U8(7)) : U8(6);
    } else if (needGps && needRgb) {
        out.format = 3;
    } else if (needRgb) {
        out.format = 2;
    } else if (needGps) {
        out.format = 1;
    } else {
        out.format = 0;
    }
    out.recordLength = standardRecordLength(out.format);
    return out;
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

// Streams one source LAZ from disk, reprojects to the output CS, tests each
// point in eye XY, and writes the kept points with their full attribute record
// intact. Returns the number of points written, or a Result-with-ErrorCode on
// open/write failure.
Monad::Result<qint64> clipOneSource(const cwLazClipSource& src,
                                    int sourceIndex,
                                    int sourceCount,
                                    const cwLazClipOperation::Request& request,
                                    const QPolygonF& polygonEyeXY,
                                    const QRectF& polygonBBox,
                                    LASwriter* writer,
                                    LASpoint& outPoint,
                                    std::atomic<qint64>& pointsDone,
                                    QPromise<cwLazClipOperation::Result>& promise,
                                    cwProgressReporter<QPromise<cwLazClipOperation::Result>>& progress)
{
    const QByteArray pathBytes = src.sourcePath.toUtf8();
    LASreadOpener opener;
    opener.set_file_name(pathBytes.constData(), FALSE);
    LASreader* reader = opener.open();
    if (reader == nullptr) {
        return Monad::Result<qint64>(
            QStringLiteral("Could not open source %1 of %2 for read: %3")
                .arg(sourceIndex + 1).arg(sourceCount).arg(src.sourcePath),
            cwLazClipOperation::SourceOpenFailed);
    }

    const QString sourceCS =
        cwLazLoader::resolveSourceCS(src.sourceCSOverride, reader->header);
    cwCoordinateTransform transform(sourceCS, request.outputWktCS);
    const bool hasTransform = !transform.isIdentity();

    const double ox = request.worldOrigin.x;
    const double oy = request.worldOrigin.y;
    const double oz = request.worldOrigin.z;
    const QMatrix4x4& view = request.viewMatrix;
    const bool keep = (request.mode == cwLazClipOperation::Mode::Keep);

    qint64 written = 0;
    qsizetype sinceReport = 0;
    bool writeFailed = false;

    auto publishProgress = [&](qsizetype delta) {
        if (delta <= 0) {
            return;
        }
        progress.report(pointsDone.fetch_add(qint64(delta), std::memory_order_relaxed)
                        + qint64(delta));
    };

    // global == output-CS coords (post-transform); writes the kept point.
    // srcPoint carries the full source attribute record into outPoint;
    // LASpoint::operator= bridges legacy↔extended formats, then only XYZ is
    // re-encoded against the output header (reprojected + worldOrigin-anchored).
    auto testAndWrite = [&](double gx, double gy, double gz, const LASpoint& srcPoint) {
        const QVector3D v(float(gx - ox), float(gy - oy), float(gz - oz));
        const QVector3D eye = view.map(v);
        const QPointF eyeXY(double(eye.x()), double(eye.y()));
        // BBox prefilter — 4 comparisons vs O(V) containsPoint. Big win
        // when the polygon covers a small fraction of the cloud extent.
        const bool inside = polygonBBox.contains(eyeXY)
                            && polygonEyeXY.containsPoint(eyeXY, Qt::OddEvenFill);
        if (keep ? inside : !inside) {
            outPoint = srcPoint;
            // operator= copies the optional attributes only when the source
            // carries them, and outPoint is reused across points and sources.
            // When a richer merged output format carries an attribute this
            // source lacks, clear it so the point doesn't inherit the previous
            // point's value (e.g. a format-0 point merged after a format-3 one).
            if (!srcPoint.have_gps_time) {
                outPoint.gps_time = 0.0;
            }
            if (!srcPoint.have_rgb) {
                outPoint.rgb[0] = 0;
                outPoint.rgb[1] = 0;
                outPoint.rgb[2] = 0;
                outPoint.rgb[3] = 0;
            } else if (!srcPoint.have_nir) {
                outPoint.rgb[3] = 0;
            }
            outPoint.set_x(gx);
            outPoint.set_y(gy);
            outPoint.set_z(gz);
            if (!writer->write_point(&outPoint)) {
                writeFailed = true;
                return;
            }
            // Feed the inventory so update_header(TRUE) at finalize stamps a
            // correct point count and bounding box into the output header.
            writer->update_inventory(&outPoint);
            ++written;
        }
    };

    if (hasTransform) {
        // Batch a chunk through transformInPlace to amortize PROJ's per-call
        // overhead (same pattern as cwLazLoader). Each point's full record is
        // serialized into recordBytes so its attributes can be paired with the
        // transformed coordinate after the batch — reader->point is clobbered
        // on the next read.
        const U16 recordLen = reader->header.point_data_record_length;
        LASpoint scratchSource;
        scratchSource.init(&reader->header, reader->header.point_data_format,
                           recordLen, &reader->header);

        QVector<cwGeoPoint> coords;
        coords.reserve(int(kChunkSize));
        std::vector<U8> recordBytes;
        recordBytes.resize(size_t(kChunkSize) * recordLen);

        auto flushChunk = [&]() {
            if (coords.isEmpty()) {
                return;
            }
            transform.transformInPlace(coords.data(), coords.size());
            for (qsizetype k = 0; k < coords.size() && !writeFailed; ++k) {
                scratchSource.copy_from(recordBytes.data() + size_t(k) * recordLen);
                const cwGeoPoint& gp = coords.at(k);
                testAndWrite(gp.x, gp.y, gp.z, scratchSource);
            }
            publishProgress(coords.size());
            coords.clear();
        };

        while (reader->read_point()) {
            if (coords.isEmpty() && promise.isCanceled()) {
                break;
            }
            reader->point.copy_to(recordBytes.data()
                                  + size_t(coords.size()) * recordLen);
            coords.append(cwGeoPoint(reader->point.get_x(),
                                     reader->point.get_y(),
                                     reader->point.get_z()));
            if (coords.size() >= kChunkSize) {
                flushChunk();
                if (writeFailed) {
                    break;
                }
            }
        }
        if (!writeFailed) {
            flushChunk();
        }
    } else {
        // Identity CS: source coords already match the output CS, so each
        // point is tested and written in the read loop with no batching.
        while (reader->read_point()) {
            if (sinceReport == 0 && promise.isCanceled()) {
                break;
            }
            testAndWrite(reader->point.get_x(),
                         reader->point.get_y(),
                         reader->point.get_z(),
                         reader->point);
            if (writeFailed) {
                break;
            }
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

    if (writeFailed) {
        return Monad::Result<qint64>(
            QStringLiteral("Failed to write LAZ point (disk full or I/O error)."),
            cwLazClipOperation::WritePointFailed);
    }
    return Monad::Result<qint64>(written);
}

// Opens each source header to sum point counts (for the progress range) and
// collect point formats (to choose the output format). Reopening for the data
// pass is intentional — the same independent-reader pattern cwLazLoader uses.
struct SourceProbe {
    qint64 totalPoints = 0;
    QList<U8> formats;
    QList<U16> recordLengths;
};

Monad::Result<SourceProbe> probeSources(const QList<cwLazClipSource>& sources)
{
    SourceProbe probe;
    probe.formats.reserve(sources.size());
    probe.recordLengths.reserve(sources.size());
    for (int i = 0; i < sources.size(); ++i) {
        const QByteArray pathBytes = sources.at(i).sourcePath.toUtf8();
        LASreadOpener opener;
        opener.set_file_name(pathBytes.constData(), FALSE);
        LASreader* reader = opener.open();
        if (reader == nullptr) {
            return Monad::Result<SourceProbe>(
                QStringLiteral("Could not open source %1 of %2 for read: %3")
                    .arg(i + 1).arg(sources.size()).arg(sources.at(i).sourcePath),
                cwLazClipOperation::SourceOpenFailed);
        }
        probe.totalPoints += qint64(reader->npoints);
        probe.formats.append(reader->header.point_data_format);
        probe.recordLengths.append(reader->header.point_data_record_length);
        reader->close();
        delete reader;
    }
    return Monad::Result<SourceProbe>(probe);
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

            const Monad::Result<SourceProbe> probeResult =
                probeSources(request.sources);
            if (probeResult.hasError()) {
                promise.addResult(Result(probeResult.errorMessage(),
                                         probeResult.errorCode()));
                return;
            }
            const SourceProbe& probe = probeResult.value();

            const qint64 totalPoints = probe.totalPoints;
            // The reporter sizes the range to the real point count, scaling it
            // down (never clamping to INT_MAX) so the bar keeps advancing even
            // for sources past ~1B points.
            cwProgressReporter<QPromise<Result>> progress(promise, totalPoints);

            const OutputFormat outputFormat =
                chooseOutputFormat(probe.formats, probe.recordLengths);

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
            header.point_data_format = outputFormat.format;
            header.point_data_record_length = outputFormat.recordLength;
            // Must precede set_geo_ogc_wkt below: that call grows
            // offset_to_point_data by the WKT VLR size, so the 1.4 header size
            // has to be in place first or the point data offset lands short.
            if (formatIsExtended(outputFormat.format)) {
                header.version_minor = kLas14VersionMinor;
                header.header_size = kLas14HeaderSize;
                header.offset_to_point_data = kLas14HeaderSize;
            }

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
                progress.dismiss();
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

            for (int i = 0; i < request.sources.size(); ++i) {
                if (promise.isCanceled()) {
                    break;
                }
                Monad::Result<qint64> sliceResult =
                    clipOneSource(request.sources.at(i), i, request.sources.size(),
                                  request, polygonEyeXY, polygonBBox, writer,
                                  pointTemplate, pointsDone, promise, progress);
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
                progress.dismiss();
                QFile::remove(request.outputPath);
                promise.addResult(Result(failureMessage, failureCode));
                return;
            }
            if (finalizeFailed) {
                progress.dismiss();
                QFile::remove(request.outputPath);
                promise.addResult(Result(
                    QStringLiteral("Could not finalize output LAZ (disk full or I/O error)."),
                    FinalizeFailed));
                return;
            }

            // progress completes structurally: the reporter's destructor snaps
            // the bar to full on scope exit (the promise isn't cancelled here).
            promise.addResult(Result(success));
        });
}
