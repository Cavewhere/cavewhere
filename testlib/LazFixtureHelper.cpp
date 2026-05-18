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
