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

//Our includes
#include "cwConcurrent.h"
#include "cwCoordinateTransform.h"

// LAStools / LASlib
#include <LASlib/lasreader.hpp>

namespace {

// LASlib stores the OGC WKT CRS in vlr_geo_ogc_wkt when the file uses the
// modern (LAS 1.4 / OGC WKT) CRS encoding. PROJ accepts WKT strings directly,
// so we pass it straight through. Older LAS files use GeoTIFF GeoKeys
// (vlr_geo_keys / vlr_geo_key_entries), which we don't decode here — those
// files load with an empty source CS and the transform short-circuits to
// identity. The user can supply an explicit override to handle that case.
QString extractEmbeddedCS(const LASheader& header)
{
    if (header.vlr_geo_ogc_wkt != nullptr && header.vlr_geo_ogc_wkt[0] != '\0') {
        return QString::fromLatin1(header.vlr_geo_ogc_wkt);
    }
    return QString();
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

            // cwGeometry's vertex count is int-typed; clamp before allocation
            // so a corrupt or 4-billion-point header can't cause UB or a near-
            // unbounded allocation. (LAS 1.4 supports U64 npoints.)
            if (effectiveMax > qsizetype(std::numeric_limits<int>::max())) {
                qWarning() << "cwLazLoader: file" << path
                           << "reports" << totalPoints
                           << "points; clamping to INT_MAX";
                effectiveMax = std::numeric_limits<int>::max();
            }

            promise.setProgressRange(0, int(effectiveMax > 0 ? effectiveMax : 1));
            promise.setProgressValue(0);

            // Reserve once — vertexData is contiguous, incremental grows would
            // copy ~12 * N bytes on each resize.
            result.geometry.resizeVertices(int(effectiveMax));

            const cwGeometry::VertexAttribute* positionAttribute =
                result.geometry.attribute(cwGeometry::Semantic::Position);
            Q_ASSERT(positionAttribute);
            // Single Vec3 Position attribute → stride == sizeof(QVector3D), so
            // the vertex buffer is a packed array of QVector3D and we can
            // memcpy chunks directly.
            Q_ASSERT(result.geometry.vertexStride() == int(sizeof(QVector3D)));
            Q_ASSERT(positionAttribute->byteOffset == 0);

            constexpr int chunkSize = 64 * 1024;
            QVector<cwGeoPoint> sourceChunk;
            QVector<QVector3D> destChunk;
            sourceChunk.reserve(chunkSize);
            destChunk.reserve(chunkSize);

            qsizetype written = 0;
            int progressMark = 0;
            float minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;

            const auto flushChunk = [&]() {
                if (sourceChunk.isEmpty()) {
                    return;
                }
                if (!transform.isIdentity()) {
                    transform.transformInPlace(sourceChunk.data(), sourceChunk.size());
                }
                destChunk.resize(sourceChunk.size());
                for (int i = 0; i < sourceChunk.size(); ++i) {
                    const QVector3D v = sourceChunk[i].toVector3D(worldOrigin);
                    destChunk[i] = v;

                    if (written == 0 && i == 0) {
                        minX = maxX = v.x();
                        minY = maxY = v.y();
                        minZ = maxZ = v.z();
                    } else {
                        if (v.x() < minX) minX = v.x();
                        if (v.y() < minY) minY = v.y();
                        if (v.z() < minZ) minZ = v.z();
                        if (v.x() > maxX) maxX = v.x();
                        if (v.y() > maxY) maxY = v.y();
                        if (v.z() > maxZ) maxZ = v.z();
                    }
                }

                std::memcpy(result.geometry.vertexDataMutable().data()
                                + written * sizeof(QVector3D),
                            destChunk.constData(),
                            destChunk.size() * sizeof(QVector3D));

                written += sourceChunk.size();
                sourceChunk.clear();
                destChunk.clear();
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
                    if (int(written) - progressMark > chunkSize) {
                        promise.setProgressValue(int(written));
                        progressMark = int(written);
                    }
                }
            }
            flushChunk();

            if (written < effectiveMax) {
                result.geometry.resizeVertices(int(written));
            }

            if (written > 0) {
                result.bboxMin = QVector3D(minX, minY, minZ);
                result.bboxMax = QVector3D(maxX, maxY, maxZ);
            }

            promise.setProgressValue(int(written));

            reader->close();
            delete reader;

            promise.addResult(std::move(result));
        });
}
