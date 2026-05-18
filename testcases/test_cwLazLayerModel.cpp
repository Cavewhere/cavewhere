#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwFutureManagerModel.h"
#include "cwFutureManagerToken.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"

#include "LazFixtureHelper.h"

namespace {

// Set up a model bound to a fresh GIS Layers subdir under @a tempDir, so
// model tests can drop .laz files into the folder and call rescan() to
// observe model changes without spinning up cwRootData/cwProject.
QDir makeGisLayersDir(const QTemporaryDir& tempDir)
{
    const QString gisLayersPath = QDir(tempDir.path())
                                      .filePath(cwLazLayerModel::folderName());
    QDir().mkpath(gisLayersPath);
    return QDir(gisLayersPath);
}

QString dropLazIntoDir(const QDir& dir, const QString& tag)
{
    const QString filePath = dir.filePath(QStringLiteral("%1.laz").arg(tag));
    return writeMinimalLaz(filePath);
}

} // namespace

TEST_CASE("cwLazLayerModel: rescan / removeAt / count", "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwLazLayerModel model;
    QSignalSpy countSpy(&model, &cwLazLayerModel::countChanged);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);

    model.setGisLayersDir(gisLayers);

    REQUIRE(model.rowCount() == 0);
    REQUIRE(model.count() == 0);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("a")).isEmpty());
    model.rescan();
    REQUIRE(model.count() == 1);
    REQUIRE(insertSpy.size() >= 1);
    REQUIRE(countSpy.size() >= 1);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("b")).isEmpty());
    model.rescan();
    REQUIRE(model.count() == 2);

    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeAt(0);
    REQUIRE(model.count() == 1);
    REQUIRE(removeSpy.size() == 1);

    // Out-of-range remove is a no-op.
    model.removeAt(99);
    REQUIRE(model.count() == 1);
    model.removeAt(-1);
    REQUIRE(model.count() == 1);
}

TEST_CASE("cwLazLayerModel: data() roles round-trip", "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);
    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);

    const QString path = dropLazIntoDir(gisLayers, QStringLiteral("roles"));
    REQUIRE(!path.isEmpty());
    model.rescan();
    auto* layer = model.layerAt(0);
    REQUIRE(layer != nullptr);

    const QModelIndex idx = model.index(0, 0);
    REQUIRE(model.data(idx, cwLazLayerModel::SourcePathRole).toString() == layer->sourcePath());
    REQUIRE(model.data(idx, cwLazLayerModel::NameRole).toString() == layer->name());

    layer->setPointSize(7.5);
    REQUIRE(model.data(idx, cwLazLayerModel::PointSizeRole).toDouble() == 7.5);
}

TEST_CASE("cwLazLayerModel: dataChanged emitted on layer property change",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);
    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("changed")).isEmpty());
    model.rescan();
    auto* layer = model.layerAt(0);
    REQUIRE(layer != nullptr);

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    layer->setPointSize(3.0);
    REQUIRE(dataChangedSpy.size() >= 1);
}

TEST_CASE("cwLazLayerModel: folder scan picks up non-recursive *.laz / *.las",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    // Files that should be picked up.
    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("alpha")).isEmpty());
    REQUIRE(!writeMinimalLaz(gisLayers.filePath(QStringLiteral("beta.las"))).isEmpty());

    // Files that should be ignored — wrong extension or hidden.
    QFile junk(gisLayers.filePath(QStringLiteral("notes.txt")));
    REQUIRE(junk.open(QIODevice::WriteOnly));
    junk.write("ignored");
    junk.close();

    QFile ds(gisLayers.filePath(QStringLiteral(".DS_Store")));
    REQUIRE(ds.open(QIODevice::WriteOnly));
    ds.write("\0\0", 2);
    ds.close();

    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);
    model.rescan();

    REQUIRE(model.count() == 2);
}

TEST_CASE("cwLazLayerModel: empty / nonexistent GIS Layers dir → empty model",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwLazLayerModel model;
    // Point at a path that does not exist; rescan must not crash.
    model.setGisLayersDir(QDir(QDir(tempDir.path())
                                  .filePath(QStringLiteral("does-not-exist"))));
    model.rescan();
    REQUIRE(model.count() == 0);
}

TEST_CASE("cwLazLayerModel: missing file → loadStatus == Error",
          "[cwLazLayerModel]") {
    cwLazLayerModel model;
    cwLazLayer* layer = new cwLazLayer(&model);
    const QString missingPath = QStringLiteral("/nonexistent-laz-%1.laz")
                                    .arg(QCoreApplication::applicationPid());
    layer->setSourcePath(missingPath);
    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Error);
    REQUIRE(layer->sourcePath() == missingPath);
    delete layer;
}
