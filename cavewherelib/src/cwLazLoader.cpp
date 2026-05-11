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
#include <QVector>

//Std includes
#include <cstring>
#include <limits>

//Our includes
#include "cwConcurrent.h"
#include "cwCoordinateTransform.h"

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

            LASreadOpener opener;
            opener.set_file_name(pathBytes.constData(), FALSE);
            LASreader* reader = opener.open();
            if (reader == nullptr) {
                qWarning() << "cwLazLoader: failed to open" << path;
                promise.addResult(result);
                return;
            }

            const QString embeddedCS = extractEmbeddedCS(reader->header);
            result.sourceCS = !sourceCSOverride.isEmpty() ? sourceCSOverride : embeddedCS;

            cwCoordinateTransform transform(result.sourceCS, globalCS);

            const I64 totalPoints = reader->npoints;
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

            // Reserve once — vertexData is contiguous, incremental grows would
            // copy ~12 * N bytes on each resize.
            result.geometry.resizeVertices(effectiveMax);

            const cwGeometry::VertexAttribute* positionAttribute =
                result.geometry.attribute(cwGeometry::Semantic::Position);
            Q_ASSERT(positionAttribute);
            // Single Vec3 Position attribute → stride == sizeof(QVector3D), so
            // the vertex buffer is a packed array of QVector3D and we write
            // converted points directly into it (no intermediate buffer).
            Q_ASSERT(result.geometry.vertexStride() == int(sizeof(QVector3D)));
            Q_ASSERT(positionAttribute->byteOffset == 0);

            constexpr int chunkSize = 64 * 1024;
            QVector<cwGeoPoint> sourceChunk;
            sourceChunk.reserve(chunkSize);

            QVector3D* const dstBase = reinterpret_cast<QVector3D*>(
                result.geometry.vertexDataMutable().data());
            const bool hasTransform = !transform.isIdentity();

            qsizetype written = 0;
            qsizetype progressMark = 0;
            // Sentinel init — first real point pulls them in, no per-point
            // "is this the first?" branch needed in the inner loop.
            float minX = std::numeric_limits<float>::infinity();
            float minY = std::numeric_limits<float>::infinity();
            float minZ = std::numeric_limits<float>::infinity();
            float maxX = -std::numeric_limits<float>::infinity();
            float maxY = -std::numeric_limits<float>::infinity();
            float maxZ = -std::numeric_limits<float>::infinity();

            const auto flushChunk = [&]() {
                if (sourceChunk.isEmpty()) {
                    return;
                }
                if (hasTransform) {
                    transform.transformInPlace(sourceChunk.data(), sourceChunk.size());
                }
                QVector3D* dst = dstBase + written;
                for (int i = 0; i < sourceChunk.size(); ++i) {
                    const QVector3D v = sourceChunk[i].toVector3D(worldOrigin);
                    dst[i] = v;

                    if (v.x() < minX) minX = v.x();
                    if (v.y() < minY) minY = v.y();
                    if (v.z() < minZ) minZ = v.z();
                    if (v.x() > maxX) maxX = v.x();
                    if (v.y() > maxY) maxY = v.y();
                    if (v.z() > maxZ) maxZ = v.z();
                }

                written += sourceChunk.size();
                sourceChunk.clear();
            };

            const auto reportProgress = [&]() {
                const qsizetype scaled = std::min<qsizetype>(written, progressMax);
                promise.setProgressValue(int(scaled));
            };

            while (written + qsizetype(sourceChunk.size()) < effectiveMax
                   && reader->read_point()) {
                if (promise.isCanceled()) {
                    break;
                }

                sourceChunk.append(cwGeoPoint(reader->point.get_x(),
                                              reader->point.get_y(),
                                              reader->point.get_z()));

                if (sourceChunk.size() >= chunkSize) {
                    flushChunk();
                    if (written - progressMark > chunkSize) {
                        reportProgress();
                        progressMark = written;
                    }
                }
            }
            flushChunk();

            if (written < effectiveMax) {
                result.geometry.resizeVertices(written);
            }

            if (written > 0) {
                result.bboxMin = QVector3D(minX, minY, minZ);
                result.bboxMax = QVector3D(maxX, maxY, maxZ);
            }

            reportProgress();

            reader->close();
            delete reader;

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
