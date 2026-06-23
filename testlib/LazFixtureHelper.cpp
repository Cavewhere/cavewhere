#include "LazFixtureHelper.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QUrl>

#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwRootData.h"

#include <LASlib/lasreader.hpp>
#include <LASlib/laswriter.hpp>

bool writeSyntheticLazFile(const QString& outPath,
                           const QVector<QVector3D>& points,
                           const QString& wktCS)
{
    LASheader header;
    header.clean_las_header();
    header.x_scale_factor = 0.001;
    header.y_scale_factor = 0.001;
    header.z_scale_factor = 0.001;
    header.point_data_format = 0;
    header.point_data_record_length = 20;

    QByteArray wktBytes;
    if (!wktCS.isEmpty()) {
        wktBytes = wktCS.toLatin1();
        // LASheader copies the string into its own buffer; length excludes
        // the implicit null terminator (matches set_geo_ogc_wkt's convention).
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

    for (const QVector3D& p : points) {
        pointTemplate.set_x(double(p.x()));
        pointTemplate.set_y(double(p.y()));
        pointTemplate.set_z(double(p.z()));
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
    default: return false;
    }

    LASheader header;
    header.clean_las_header();
    header.x_scale_factor = 0.001;
    header.y_scale_factor = 0.001;
    header.z_scale_factor = 0.001;
    header.point_data_format = pointDataFormat;
    header.point_data_record_length = recordLength;

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

    const bool hasGps = (pointDataFormat == 1 || pointDataFormat == 3);
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

void addLazAndWait(cwRootData* root, const QStringList& externalPaths)
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
}
