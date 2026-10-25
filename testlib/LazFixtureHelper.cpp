#include "LazFixtureHelper.h"

#include <QAbstractItemModel>
#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>
#include <QUrl>

#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwRootData.h"

#include <LASlib/lasreader.hpp>
#include <LASlib/laswriter.hpp>
#include <LASlib/laswriter_las.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

//! How long a wait blocks before it looks at the world again.
constexpr int kSignalWaitMs = 100;
constexpr int kSpinSliceMs = 50;
constexpr int kSleepSliceMs = 5;

constexpr U16 kOgcWktRecordId = 2112;

/**
 * Center the header's offsets on @a points.
 *
 * LAS stores each coordinate as an int32 count of scale units from the
 * header's offset. With the offset left at 0 and a 0.001 scale, anything past
 * ~2.1e6 wraps silently — a UTM northing of 4194010 comes back as -100957.296,
 * with no error anywhere. Real-world coordinates need a real offset.
 */
template <typename PointRange, typename Getter>
void centerHeaderOffsets(LASheader* header, const PointRange& points, Getter get)
{
    if (points.isEmpty()) {
        return;
    }

    double minX = 0.0, minY = 0.0, minZ = 0.0;
    double maxX = 0.0, maxY = 0.0, maxZ = 0.0;
    bool first = true;
    for (const auto& point : points) {
        const cwGeoPoint position = get(point);
        const double x = position.x;
        const double y = position.y;
        const double z = position.z;
        if (first) {
            minX = maxX = x;
            minY = maxY = y;
            minZ = maxZ = z;
            first = false;
            continue;
        }
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
        minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
    }

    // Whole meters, as the spec asks for "reasonable round numbers". Decoded
    // coordinates are unaffected, so fixtures that already fit keep their
    // exact values.
    header->x_offset = std::floor((minX + maxX) * 0.5);
    header->y_offset = std::floor((minY + maxY) * 0.5);
    header->z_offset = std::floor((minZ + maxZ) * 0.5);
}

} // namespace

void setUnterminatedOgcWkt(LASheader& header, const QByteArray& wkt)
{
    // add_vlr takes ownership of the new[] buffer; LASheader::clean frees it.
    auto* data = new CHAR[wkt.size()];
    std::memcpy(data, wkt.constData(), size_t(wkt.size()));
    header.vlr_geo_ogc_wkt = data;
    header.add_vlr("LASF_Projection", kOgcWktRecordId, U16(wkt.size()),
                   reinterpret_cast<U8*>(data));
}

bool writeSyntheticLazFile(const QString& outPath,
                           const QVector<QVector3D>& points,
                           const QString& wktCS,
                           LazCompression compression,
                           WktTermination wktTermination)
{
    QVector<cwGeoPoint> geoPoints;
    geoPoints.reserve(points.size());
    for (const QVector3D& p : points) {
        geoPoints.append(cwGeoPoint::fromSceneLocal(p));
    }
    return writeSyntheticGeoLazFile(outPath, geoPoints, wktCS, compression, wktTermination);
}

bool writeSyntheticGeoLazFile(const QString& outPath,
                              const QVector<cwGeoPoint>& points,
                              const QString& wktCS,
                              LazCompression compression,
                              WktTermination wktTermination)
{
    LASheader header;
    header.clean_las_header();
    header.x_scale_factor = 0.001;
    header.y_scale_factor = 0.001;
    header.z_scale_factor = 0.001;
    header.point_data_format = 0;
    header.point_data_record_length = 20;
    centerHeaderOffsets(&header, points, [](const cwGeoPoint& p) { return p; });

    QByteArray wktBytes;
    if (!wktCS.isEmpty()) {
        wktBytes = wktCS.toLatin1();
        if (wktTermination == WktTermination::Unterminated) {
            setUnterminatedOgcWkt(header, wktBytes);
        } else {
            // LASheader copies the string into its own buffer; length excludes
            // the implicit null terminator (matches set_geo_ogc_wkt's convention).
            header.set_geo_ogc_wkt(wktBytes.size(), wktBytes.constData());
        }
    }

    LASpoint pointTemplate;
    pointTemplate.init(&header, header.point_data_format,
                       header.point_data_record_length, &header);

    const QByteArray pathBytes = outPath.toUtf8();

    // LASwriteOpener has no compressor argument, so the point-wise fixture
    // opens the LAS writer itself.
    LASwriter* writer = nullptr;
    if (compression == LazCompression::PointWise) {
        auto* lasWriter = new LASwriterLAS();
        if (lasWriter->open(pathBytes.constData(), &header, LASZIP_COMPRESSOR_POINTWISE)) {
            writer = lasWriter;
        } else {
            delete lasWriter;
        }
    } else {
        LASwriteOpener opener;
        opener.set_file_name(pathBytes.constData());
        writer = opener.open(&header);
    }

    if (writer == nullptr) {
        return false;
    }

    for (const cwGeoPoint& p : points) {
        pointTemplate.set_x(p.x);
        pointTemplate.set_y(p.y);
        pointTemplate.set_z(p.z);
        writer->write_point(&pointTemplate);
        writer->update_inventory(&pointTemplate);
    }

    writer->update_header(&header, TRUE);
    writer->close();
    delete writer;
    return true;
}

bool writeAttributedLazFile(const QString& outPath,
                            const QVector<LazAttributePoint>& points,
                            quint8 pointDataFormat,
                            const QString& wktCS)
{
    // Standard record lengths for the formats this helper supports.
    U16 recordLength = 0;
    switch (pointDataFormat) {
    case 0: recordLength = 20; break;
    case 1: recordLength = 28; break;
    case 2: recordLength = 26; break;
    case 3: recordLength = 34; break;
    case 6: recordLength = 30; break;
    default: return false;
    }

    LASheader header;
    header.clean_las_header();
    header.x_scale_factor = 0.001;
    header.y_scale_factor = 0.001;
    header.z_scale_factor = 0.001;
    header.point_data_format = pointDataFormat;
    header.point_data_record_length = recordLength;
    centerHeaderOffsets(&header, points,
                        [](const LazAttributePoint& p) {
                            return cwGeoPoint::fromSceneLocal(p.position);
                        });
    // Formats 6+ are LAS 1.4 only; a 1.2 header can't store their point count.
    if (pointDataFormat >= 6) {
        header.version_minor = 4;
        header.header_size = 375;
        header.offset_to_point_data = 375;
    }

    QByteArray wktBytes;
    if (!wktCS.isEmpty()) {
        wktBytes = wktCS.toLatin1();
        header.set_geo_ogc_wkt(wktBytes.size(), wktBytes.constData());
    }

    LASpoint pointTemplate;
    pointTemplate.init(&header, header.point_data_format,
                       header.point_data_record_length, &header);

    const QByteArray pathBytes = outPath.toUtf8();
    LASwriteOpener opener;
    opener.set_file_name(pathBytes.constData());
    LASwriter* writer = opener.open(&header);
    if (writer == nullptr) {
        return false;
    }

    const bool hasGps = (pointDataFormat == 1 || pointDataFormat == 3 || pointDataFormat == 6);
    const bool hasRgb = (pointDataFormat == 2 || pointDataFormat == 3);

    for (const LazAttributePoint& p : points) {
        pointTemplate.set_x(double(p.position.x()));
        pointTemplate.set_y(double(p.position.y()));
        pointTemplate.set_z(double(p.position.z()));
        pointTemplate.intensity = p.intensity;
        pointTemplate.set_classification(p.classification);
        pointTemplate.point_source_ID = p.pointSourceId;
        if (hasGps) {
            pointTemplate.gps_time = p.gpsTime;
        }
        if (hasRgb) {
            pointTemplate.rgb[0] = p.red;
            pointTemplate.rgb[1] = p.green;
            pointTemplate.rgb[2] = p.blue;
        }
        writer->write_point(&pointTemplate);
        writer->update_inventory(&pointTemplate);
    }

    writer->update_header(&header, TRUE);
    writer->close();
    delete writer;
    return true;
}

LazFileContents readLazFile(const QString& path)
{
    LazFileContents contents;

    const QByteArray pathBytes = path.toUtf8();
    LASreadOpener opener;
    opener.set_file_name(pathBytes.constData(), FALSE);
    LASreader* reader = opener.open();
    if (reader == nullptr) {
        return contents;
    }

    contents.pointDataFormat = reader->header.point_data_format;
    contents.headerPointCount = qint64(reader->npoints);
    contents.headerBboxMin = QVector3D(float(reader->header.min_x),
                                       float(reader->header.min_y),
                                       float(reader->header.min_z));
    contents.headerBboxMax = QVector3D(float(reader->header.max_x),
                                       float(reader->header.max_y),
                                       float(reader->header.max_z));
    while (reader->read_point()) {
        LazAttributePoint p;
        p.position = QVector3D(float(reader->point.get_x()),
                               float(reader->point.get_y()),
                               float(reader->point.get_z()));
        p.intensity = reader->point.get_intensity();
        p.classification = reader->point.get_classification();
        p.red = reader->point.rgb[0];
        p.green = reader->point.rgb[1];
        p.blue = reader->point.rgb[2];
        p.gpsTime = reader->point.get_gps_time();
        p.pointSourceId = reader->point.get_point_source_ID();
        contents.points.append(p);
    }

    reader->close();
    delete reader;
    return contents;
}

QString tempLazPath(QTemporaryDir& dir, const QString& tag)
{
    return dir.filePath(QStringLiteral("%1-%2.laz")
                            .arg(tag)
                            .arg(QCoreApplication::applicationPid()));
}

QString kentuckyFtUsCompoundWkt()
{
    return QStringLiteral(
        "COMPD_CS[\"NAD83 / Kentucky Single Zone (ftUS) + NAVD88 height (ftUS) - Geoid12B (ftUS)\","
        "PROJCS[\"NAD83 / Kentucky Single Zone (ftUS)\",GEOGCS[\"NAD83\","
        "DATUM[\"North_American_Datum_1983\",SPHEROID[\"GRS 1980\",6378137,298.257222101,"
        "AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6269\"]],"
        "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
        "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
        "AUTHORITY[\"EPSG\",\"4269\"]],"
        "PROJECTION[\"Lambert_Conformal_Conic_2SP\"],"
        "PARAMETER[\"standard_parallel_1\",37.0833333333333],"
        "PARAMETER[\"standard_parallel_2\",38.6666666666667],"
        "PARAMETER[\"latitude_of_origin\",36.3333333333333],"
        "PARAMETER[\"central_meridian\",-85.75],"
        "PARAMETER[\"false_easting\",4921250],"
        "PARAMETER[\"false_northing\",3280833.333],"
        "UNIT[\"US survey foot\",0.304800609601219,AUTHORITY[\"EPSG\",\"9003\"]],"
        "AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],AUTHORITY[\"EPSG\",\"3089\"]],"
        "VERT_CS[\"NAVD88 height (ftUS)\","
        "VERT_DATUM[\"North American Vertical Datum 1988\",2005,AUTHORITY[\"EPSG\",\"5103\"]],"
        "UNIT[\"US survey foot\",0.304800609601219,AUTHORITY[\"EPSG\",\"9003\"]],"
        "AXIS[\"Gravity-related height\",UP],AUTHORITY[\"EPSG\",\"6360\"]]]");
}

QString utmZoneWkt(int zone, int centralMeridian)
{
    return QStringLiteral(
        "PROJCS[\"WGS 84 / UTM zone %1N\",GEOGCS[\"WGS 84\","
        "DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],"
        "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
        "PROJECTION[\"Transverse_Mercator\"],"
        "PARAMETER[\"latitude_of_origin\",0],"
        "PARAMETER[\"central_meridian\",%2],"
        "PARAMETER[\"scale_factor\",0.9996],"
        "PARAMETER[\"false_easting\",500000],"
        "PARAMETER[\"false_northing\",0],UNIT[\"metre\",1]]")
        .arg(zone)
        .arg(centralMeridian);
}

QDir makeGisLayersDir(const QTemporaryDir& tempDir)
{
    const QString gisLayersPath = QDir(tempDir.path()).filePath(cwLazLayerModel::folderName());
    QDir().mkpath(gisLayersPath);
    return QDir(gisLayersPath);
}

QString writeMinimalLaz(const QString& path, const QString& wktCS)
{
    const QVector<QVector3D> points = {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f },
        { 2.0f, 2.0f, 2.0f },
        { 3.0f, 3.0f, 3.0f },
        { 4.0f, 4.0f, 4.0f }
    };
    if (!writeSyntheticLazFile(path, points, wktCS)) {
        return QString();
    }
    return path;
}

bool waitForLazLayerLoaded(cwLazLayer* layer, int timeoutMs)
{
    QSignalSpy spy(layer, &cwLazLayer::loadStatusChanged);
    int waited = 0;
    while (waited < timeoutMs) {
        if (layer->loadStatus() == cwLazLayer::LoadStatus::Loaded
            || layer->loadStatus() == cwLazLayer::LoadStatus::Error) {
            return true;
        }
        spy.wait(100);
        waited += 100;
    }
    return false;
}

bool waitForLazLayerHeader(cwLazLayer* layer, int timeoutMs)
{
    // The layer publishes the header before it reports the probe finished, so
    // the falling edge of this signal is the moment the header is readable.
    QSignalSpy spy(layer, &cwLazLayer::headerProbeInFlightChanged);
    QElapsedTimer elapsed;
    elapsed.start();
    while (!layer->hasReadHeader() && elapsed.elapsed() < timeoutMs) {
        spy.wait(kSignalWaitMs);
    }
    return layer->hasReadHeader();
}

void spinEventLoopSlice()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, kSpinSliceMs);
    QThread::msleep(kSleepSliceMs);
}

bool waitForLazLayerModelSettled(cwLazLayerModel* layers, int timeoutMs)
{
    const auto settling = [layers]() {
        if (layers->anyHeaderProbeInFlight()) {
            return true;
        }
        const QList<cwLazLayer*>& all = layers->layers();
        return std::any_of(all.begin(), all.end(), [](const cwLazLayer* layer) {
            return layer->loadStatus() == cwLazLayer::LoadStatus::Loading;
        });
    };

    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeoutMs && settling()) {
        spinEventLoopSlice();
    }
    return !settling();
}

LazJobRecorder::LazJobRecorder(cwFutureManagerModel* manager)
{
    m_connection = QObject::connect(manager, &QAbstractItemModel::rowsInserted, manager,
        [this, manager](const QModelIndex&, int first, int last) {
            for (int row = first; row <= last; ++row) {
                m_names.append(manager->index(row, 0)
                                   .data(cwFutureManagerModel::NameRole)
                                   .toString());
            }
        });
}

LazJobRecorder::~LazJobRecorder()
{
    QObject::disconnect(m_connection);
}

bool LazJobRecorder::waitForName(const QString& name, int timeoutMs)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeoutMs) {
        if (m_names.contains(name)) {
            return true;
        }
        spinEventLoopSlice();
    }
    return m_names.contains(name);
}

bool addLazAndWait(cwRootData* root, const QStringList& externalPaths)
{
    auto* project = root->project();
    auto* region = project->cavingRegion();
    QList<QUrl> urls;
    urls.reserve(externalPaths.size());
    for (const QString& p : externalPaths) {
        urls.append(QUrl::fromLocalFile(p));
    }
    region->lazLayers()->addFromFiles(urls);
    project->waitSaveToFinish();
    root->futureManagerModel()->waitForFinished();

    // The frame is derived from the layers' headers, and the point decodes wait
    // on the frame, so neither has necessarily happened yet: returning here
    // would hand the test a model whose rows are all still Loading.
    const bool settled = waitForLazLayerModelSettled(region->lazLayers());

    root->futureManagerModel()->waitForFinished();
    return settled;
}
