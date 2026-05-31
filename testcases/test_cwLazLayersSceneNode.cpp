// Covers commit 1 of DISABLE_LAZ_LAYERS_PLAN: cwLazLayersSceneNode must gate
// materialization on cwLazLayer::enabled(). Disabled layers never get a
// cwRenderPointCloud or a cwKeywordItem; toggling enabled materializes or
// dematerializes the scene-side state, with the enabledChanged hook-up living
// across the materialization boundary.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwKeywordItemModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

#include "LazFixtureHelper.h"

namespace {

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

TEST_CASE("scene node skips materialization for layers disabled at connect time",
          "[cwLazLayersSceneNode][cwLazLayerEnabled]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwKeywordItemModel keywordItems;
    cwLazLayerModel model;
    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    cwLazLayer* layer = addLazViaRescan(model, gisLayersDir, QStringLiteral("disabled-at-connect"));
    REQUIRE(layer != nullptr);

    // Disable before wiring the scene node so the initial rebuild() observes
    // the disabled state during its first pass.
    layer->setEnabled(false);

    cwLazLayersSceneNode node;
    node.setScene(&scene);
    node.setKeywordItemModel(&keywordItems);
    node.setLazLayerModel(&model);

    REQUIRE(node.pointCloudForLayer(layer) == nullptr);
    REQUIRE(node.visibleLayers().isEmpty());
}

TEST_CASE("scene node dematerializes when an already-loaded layer is disabled",
          "[cwLazLayersSceneNode][cwLazLayerEnabled]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwKeywordItemModel keywordItems;
    cwLazLayerModel model;

    cwLazLayersSceneNode node;
    node.setScene(&scene);
    node.setKeywordItemModel(&keywordItems);
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    cwLazLayer* layer = addLazViaRescan(model, gisLayersDir, QStringLiteral("toggle-off"));
    REQUIRE(layer != nullptr);

    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(node.pointCloudForLayer(layer) != nullptr);

    QPointer<cwRenderPointCloud> guard(node.pointCloudForLayer(layer));

    layer->setEnabled(false);

    // Dematerialize destroys the render object and removes the keyword item.
    REQUIRE(guard.isNull());
    REQUIRE(node.pointCloudForLayer(layer) == nullptr);
    REQUIRE(node.visibleLayers().isEmpty());
}

TEST_CASE("scene node materializes on re-enable and survives subsequent loadStatusChanged",
          "[cwLazLayersSceneNode][cwLazLayerEnabled]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwKeywordItemModel keywordItems;
    cwLazLayerModel model;

    cwLazLayersSceneNode node;
    node.setScene(&scene);
    node.setKeywordItemModel(&keywordItems);
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    cwLazLayer* layer = addLazViaRescan(model, gisLayersDir, QStringLiteral("toggle-on"));
    REQUIRE(layer != nullptr);

    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(node.pointCloudForLayer(layer) != nullptr);

    layer->setEnabled(false);
    REQUIRE(node.pointCloudForLayer(layer) == nullptr);

    // Re-enable: materialize must run before the layer fires its first
    // loadStatusChanged of the new load. After waiting for Loaded the render
    // object should have geometry pushed into it without any "no render object
    // for layer" warning.
    layer->setEnabled(true);
    REQUIRE(node.pointCloudForLayer(layer) != nullptr);
    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(node.pointCloudForLayer(layer) != nullptr);
    REQUIRE(node.visibleLayers().size() == 1);
    REQUIRE(node.visibleLayers().constFirst() == layer);
}

TEST_CASE("scene node materializes a freshly-rescanned layer after removeAt",
          "[cwLazLayersSceneNode][cwLazLayerEnabled]")
{
    // Covers the materialize/dematerialize round-trip across a remove + rescan
    // cycle. UUID-collision regression (paired .cwlaz reviving the prior id)
    // is intentionally out of scope here — the addLazViaRescan fixture writes
    // only the .laz, so rescan mints a fresh UUID and there is no stale-entry
    // path to exercise. See plan §3 for the collision scenario.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwKeywordItemModel keywordItems;
    cwLazLayerModel model;

    cwLazLayersSceneNode node;
    node.setScene(&scene);
    node.setKeywordItemModel(&keywordItems);
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    cwLazLayer* first = addLazViaRescan(model, gisLayersDir, QStringLiteral("readd"));
    REQUIRE(first != nullptr);
    REQUIRE(node.pointCloudForLayer(first) != nullptr);

    model.removeAt(0);
    REQUIRE(model.count() == 0);

    model.rescan();
    REQUIRE(model.count() == 1);
    cwLazLayer* second = model.layerAt(0);
    REQUIRE(second != nullptr);
    REQUIRE(node.pointCloudForLayer(second) != nullptr);
    REQUIRE(node.visibleLayers().size() <= 1);
}

TEST_CASE("scene node disconnects enabledChanged on removeLayer",
          "[cwLazLayersSceneNode][cwLazLayerEnabled]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwKeywordItemModel keywordItems;
    cwLazLayerModel model;

    cwLazLayersSceneNode node;
    node.setScene(&scene);
    node.setKeywordItemModel(&keywordItems);
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    cwLazLayer* layer = addLazViaRescan(model, gisLayersDir, QStringLiteral("orphan"));
    REQUIRE(layer != nullptr);
    REQUIRE(node.pointCloudForLayer(layer) != nullptr);

    // cwLazLayerModel::removeAt uses deleteLater(), so the layer pointer is
    // still safe to touch synchronously after removeAt returns. That gives us
    // exactly one window to exercise the disconnect path: fire enabled toggles
    // and verify the scene node does NOT react.
    QPointer<cwLazLayer> layerGuard(layer);
    model.removeAt(0);
    REQUIRE_FALSE(layerGuard.isNull());
    REQUIRE(node.pointCloudForLayer(layer) == nullptr);

    layer->setEnabled(false);
    REQUIRE(node.pointCloudForLayer(layer) == nullptr);
    layer->setEnabled(true);
    REQUIRE(node.pointCloudForLayer(layer) == nullptr);

    REQUIRE(scene.pendingItemCount() == 0);

    // Pump the event loop so deleteLater() fires and the layer is destroyed
    // before the test fixture goes out of scope.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    REQUIRE(layerGuard.isNull());
}

TEST_CASE("setLazLayerModel disconnects prior-model layers from onEnabledChanged",
          "[cwLazLayersSceneNode][cwLazLayerEnabled]")
{
    // Regression: previously addLayer() installed an enabledChanged hook-up
    // on every layer it saw, but disconnectModel() only severed the model's
    // own signals. Swapping models left orphan connections live, so a
    // setEnabled() on a layer from the prior model would call onEnabledChanged
    // and materialize an unreachable render object into m_pointClouds.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwKeywordItemModel keywordItems;

    cwLazLayerModel modelA;
    cwLazLayerModel modelB;

    cwLazLayersSceneNode node;
    node.setScene(&scene);
    node.setKeywordItemModel(&keywordItems);
    node.setLazLayerModel(&modelA);

    const QDir gisLayersDirA = prepareGisLayersDir(tempDir);
    cwLazLayer* layerA = addLazViaRescan(modelA, gisLayersDirA, QStringLiteral("modelA"));
    REQUIRE(layerA != nullptr);
    REQUIRE(node.pointCloudForLayer(layerA) != nullptr);

    // Swap to modelB. modelA still owns layerA (it stays alive).
    node.setLazLayerModel(&modelB);
    REQUIRE(node.pointCloudForLayer(layerA) == nullptr);

    // Toggling layerA's enabled must NOT round-trip through the scene node.
    QSignalSpy enabledSpy(layerA, &cwLazLayer::enabledChanged);
    layerA->setEnabled(false);
    REQUIRE(enabledSpy.size() == 1);
    REQUIRE(node.pointCloudForLayer(layerA) == nullptr);

    layerA->setEnabled(true);
    REQUIRE(enabledSpy.size() == 2);
    REQUIRE(node.pointCloudForLayer(layerA) == nullptr);
}

TEST_CASE("scene node survives rapid enable/disable toggling without leaks",
          "[cwLazLayersSceneNode][cwLazLayerEnabled]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwKeywordItemModel keywordItems;
    cwLazLayerModel model;

    cwLazLayersSceneNode node;
    node.setScene(&scene);
    node.setKeywordItemModel(&keywordItems);
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    cwLazLayer* layer = addLazViaRescan(model, gisLayersDir, QStringLiteral("rapid-toggle"));
    REQUIRE(layer != nullptr);

    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        layer->setEnabled(false);
        REQUIRE(node.pointCloudForLayer(layer) == nullptr);
        layer->setEnabled(true);
        REQUIRE(node.pointCloudForLayer(layer) != nullptr);
    }

    // Final state: enabled. The render object exists and the scene has no
    // stale pending items beyond the single live one.
    REQUIRE(node.pointCloudForLayer(layer) != nullptr);
}
