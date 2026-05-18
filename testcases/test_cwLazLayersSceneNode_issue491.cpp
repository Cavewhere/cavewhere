// Reproducer for issue #491: deleting a LAZ layer before any render sync had
// drained cwScene's queues left a freed cwRenderPointCloud pointer in them,
// and the next cwRhiScene::synchroize() pass dereferenced it.

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QPointer>
#include <QTemporaryDir>

#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

#include "LazFixtureHelper.h"

namespace {

// Local helper: drop a .laz file into a GIS Layers subdir of @a tempDir
// and trigger a rescan() on @a model, returning the freshly added layer.
QDir prepareGisLayersDir(const QTemporaryDir& tempDir)
{
    const QString path = QDir(tempDir.path()).filePath(cwLazLayerModel::folderName());
    QDir().mkpath(path);
    return QDir(path);
}

cwLazLayer* addLazViaRescan(cwLazLayerModel& model,
                            const QDir& gisLayersDir,
                            const QString& tag)
{
    const QString filePath = gisLayersDir.filePath(QStringLiteral("%1.laz").arg(tag));
    if (writeMinimalLaz(filePath).isEmpty()) {
        return nullptr;
    }
    model.setGisLayersDir(gisLayersDir);
    model.rescan();
    return model.layerAt(model.count() - 1);
}

} // namespace

TEST_CASE("issue #491: deleting a never-loaded LAZ layer must not leave a "
          "dangling pointer in cwScene",
          "[cwLazLayersSceneNode][issue491]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwLazLayersSceneNode node;
    node.setScene(&scene);

    cwLazLayerModel model;
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    cwLazLayer* layer = addLazViaRescan(model, gisLayersDir, QStringLiteral("issue491"));
    REQUIRE(layer != nullptr);

    cwRenderPointCloud* renderObject = node.pointCloudForLayer(layer);
    REQUIRE(renderObject != nullptr);
    REQUIRE(scene.pendingItemCount() > 0);

    QPointer<cwRenderPointCloud> guard(renderObject);
    model.removeAt(0);

    REQUIRE(guard.isNull());
    REQUIRE(scene.pendingItemCount() == 0);
}

TEST_CASE("issue #491: repeated add/remove of never-loaded LAZ layers stays clean",
          "[cwLazLayersSceneNode][issue491]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwLazLayersSceneNode node;
    node.setScene(&scene);

    cwLazLayerModel model;
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    for (int i = 0; i < 5; ++i) {
        const QString tag = QStringLiteral("issue491-%1").arg(i);
        cwLazLayer* layer = addLazViaRescan(model, gisLayersDir, tag);
        REQUIRE(layer != nullptr);
        REQUIRE(node.pointCloudForLayer(layer) != nullptr);

        model.removeAt(0);
        REQUIRE(model.rowCount() == 0);

        // The fixture file is reused each iteration; remove it so the next
        // rescan-with-different-tag actually produces a single new row.
        QFile::remove(gisLayersDir.filePath(QStringLiteral("%1.laz").arg(tag)));
    }

    REQUIRE(scene.pendingItemCount() == 0);
}
