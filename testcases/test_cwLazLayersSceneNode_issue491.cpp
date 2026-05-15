// Reproducer for issue #491: deleting a LAZ layer before any render sync had
// drained cwScene's queues left a freed cwRenderPointCloud pointer in them,
// and the next cwRhiScene::synchroize() pass dereferenced it.

#include <catch2/catch_test_macros.hpp>

#include <QPointer>
#include <QTemporaryDir>

#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

#include "LazFixtureHelper.h"

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

    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("issue491")));
    cwLazLayer* layer = model.addLayer(path);
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

    for (int i = 0; i < 5; ++i) {
        const QString tag = QStringLiteral("issue491-%1").arg(i);
        const QString path = writeMinimalLaz(tempLazPath(tempDir, tag));
        cwLazLayer* layer = model.addLayer(path);
        REQUIRE(layer != nullptr);
        REQUIRE(node.pointCloudForLayer(layer) != nullptr);

        model.removeAt(0);
        REQUIRE(model.rowCount() == 0);
    }

    REQUIRE(scene.pendingItemCount() == 0);
}
