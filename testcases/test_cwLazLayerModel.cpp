#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVector>
#include <QVector3D>

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

// Clipping writes a new .laz into the GIS Layers folder, then calls
// rescan(). The expected behavior is purely additive: the existing layer
// object stays alive and the model emits rowsInserted for the new file
// only. Today rescan() does a full clear-and-rebuild, which destroys the
// original cwLazLayer and forces its LAZ data to reload from disk — the
// wasteful behavior this test guards against.
TEST_CASE("cwLazLayerModel: rescan preserves existing layers when a file is added",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("original")).isEmpty());
    model.rescan();
    REQUIRE(model.count() == 1);

    cwLazLayer* originalLayer = model.layerAt(0);
    REQUIRE(originalLayer != nullptr);
    const QString originalPath = originalLayer->sourcePath();
    // QPointer auto-nulls if originalLayer is destroyed — the signal that
    // rescan tore down and rebuilt the layer instead of preserving it.
    QPointer<cwLazLayer> originalGuard(originalLayer);

    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("clip_001")).isEmpty());
    model.rescan();

    REQUIRE(model.count() == 2);

    // Existing layer object must still be alive — anything else means its
    // already-loaded LAZ data has to be re-streamed from disk.
    REQUIRE(!originalGuard.isNull());
    REQUIRE(originalGuard.data() == originalLayer);
    REQUIRE(originalLayer->sourcePath() == originalPath);

    // Original must still occupy one of the rows and the model must surface
    // both layers.
    bool originalStillPresent = false;
    for (int i = 0; i < model.count(); ++i) {
        if (model.layerAt(i) == originalLayer) {
            originalStillPresent = true;
            break;
        }
    }
    REQUIRE(originalStillPresent);

    // Purely additive: one inserted row, nothing removed, no reset.
    REQUIRE(removeSpy.size() == 0);
    REQUIRE(resetSpy.size() == 0);
    REQUIRE(insertSpy.size() == 1);
    const QList<QVariant> args = insertSpy.takeFirst();
    const int first = args.at(1).toInt();
    const int last = args.at(2).toInt();
    REQUIRE(first == last); // exactly one row inserted
}

// An in-place overwrite of an existing GIS Layers file (same path, new
// content) must invalidate the cached layer so the new bytes get loaded.
// The diff-merge rescan keys on path alone for the additive case; this
// test guards the (size, mtime) fingerprint check that catches overwrites.
TEST_CASE("cwLazLayerModel: rescan invalidates a layer when its file is overwritten",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);
    const QString path = gisLayers.filePath(QStringLiteral("overwrite.laz"));

    // Initial write: 5 points (the default writeMinimalLaz size).
    REQUIRE(!writeMinimalLaz(path).isEmpty());

    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);
    model.rescan();
    REQUIRE(model.count() == 1);

    cwLazLayer* originalLayer = model.layerAt(0);
    REQUIRE(originalLayer != nullptr);
    QPointer<cwLazLayer> originalGuard(originalLayer);

    // Overwrite the file with a clearly larger payload so the (size, mtime)
    // fingerprint must differ even if the filesystem's mtime resolution
    // collapses the two writes to the same second.
    QVector<QVector3D> manyPoints;
    manyPoints.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        manyPoints.append(QVector3D(float(i), float(i) * 2.0f, float(i) * 3.0f));
    }
    REQUIRE(writeSyntheticLazFile(path, manyPoints));

    model.rescan();

    // rescan() routes the old layer through deleteLater(); flush the
    // pending DeferredDelete events so the QPointer can observe the
    // destruction.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    // Same path is still present, but the layer object backing it must
    // have been torn down — otherwise the layer's cached point data still
    // reflects the pre-overwrite 5 points.
    REQUIRE(model.count() == 1);
    REQUIRE(originalGuard.isNull());
    REQUIRE(model.layerAt(0) != nullptr);
    REQUIRE(model.layerAt(0)->sourcePath() == path);
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

TEST_CASE("cwLazLayerModel: roleNames() exposes \"enabled\" mapped to EnabledRole",
          "[cwLazLayerModel]") {
    cwLazLayerModel model;
    const auto roles = model.roleNames();
    REQUIRE(roles.value(cwLazLayerModel::EnabledRole) == QByteArray("enabled"));
}

TEST_CASE("cwLazLayerModel: data(EnabledRole) returns layer->enabled()",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);
    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("enabledread")).isEmpty());
    model.rescan();
    auto* layer = model.layerAt(0);
    REQUIRE(layer != nullptr);

    const QModelIndex idx = model.index(0, 0);
    REQUIRE(model.data(idx, cwLazLayerModel::EnabledRole).toBool() == layer->enabled());

    layer->setEnabled(false);
    REQUIRE(model.data(idx, cwLazLayerModel::EnabledRole).toBool() == false);

    layer->setEnabled(true);
    REQUIRE(model.data(idx, cwLazLayerModel::EnabledRole).toBool() == true);
}

TEST_CASE("cwLazLayerModel: setData(EnabledRole) drives layer->enabled()",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);
    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("enabledwrite")).isEmpty());
    model.rescan();
    auto* layer = model.layerAt(0);
    REQUIRE(layer != nullptr);
    REQUIRE(layer->enabled() == true);

    const QModelIndex idx = model.index(0, 0);
    REQUIRE(model.setData(idx, false, cwLazLayerModel::EnabledRole) == true);
    REQUIRE(layer->enabled() == false);

    REQUIRE(model.setData(idx, true, cwLazLayerModel::EnabledRole) == true);
    REQUIRE(layer->enabled() == true);

    // Out-of-range row returns false without crashing.
    const QModelIndex badIdx = model.index(99, 0);
    REQUIRE(model.setData(badIdx, false, cwLazLayerModel::EnabledRole) == false);
}

TEST_CASE("cwLazLayerModel: external setEnabled emits dataChanged with EnabledRole",
          "[cwLazLayerModel]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QDir gisLayers = makeGisLayersDir(tempDir);
    cwLazLayerModel model;
    model.setGisLayersDir(gisLayers);

    REQUIRE(!dropLazIntoDir(gisLayers, QStringLiteral("enabledspy")).isEmpty());
    model.rescan();
    auto* layer = model.layerAt(0);
    REQUIRE(layer != nullptr);

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    layer->setEnabled(false);

    bool sawEnabledRole = false;
    for (int i = 0; i < dataChangedSpy.size(); ++i) {
        const auto& args = dataChangedSpy.at(i);
        const QVector<int> roles = args.at(2).value<QVector<int>>();
        if (roles.contains(int(cwLazLayerModel::EnabledRole))) {
            sawEnabledRole = true;
            break;
        }
    }
    REQUIRE(sawEnabledRole);
}
