/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Reproducer for the keyword-visibility "stuck" bug after a LAZ clip.
//
// User-visible symptom: after rescan (e.g. post-clip), the keyword filter
// stops controlling LAZ visibility. The chain has two cooperating bugs:
//
//   1. cwLazLayerModel::rescan() used to insert new cwLazLayer rows via
//      beginInsertRows/endInsertRows BEFORE calling setSourcePath. The
//      scene node's addKeywordItemForLayer() therefore registered an item
//      with only Type+ObjectId, and the later Name/FileName updates
//      triggered a second classification pass through the filter
//      pipeline.
//
//   2. cwKeywordVisibility wired rowsInserted only — when items shuttled
//      between accepted/rejected, the last-fired model won regardless of
//      the real final state, and removals were ignored entirely.
//
// This test confirms the settled behaviour the user expects:
//   * After a rescan, every render object's final visibility is `true`
//     (default pipeline, nothing filtered out).
//   * A user-driven hide (adding a keyword item to the hide model) flips
//     the affected render objects to invisible.
//   * Removing them from the hide model restores visibility.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include "cwCavingRegion.h"
#include "cwKeywordFilterPipelineModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordVisibility.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

#include "LazFixtureHelper.h"

namespace {

struct RescanVisibilityFixture {
    QTemporaryDir tempDir;
    QDir gisLayersDir;

    cwCavingRegion region;
    cwScene scene;
    cwKeywordItemModel keywordItemModel;
    cwKeywordFilterPipelineModel pipeline;
    cwKeywordVisibility visibility;
    cwLazLayersSceneNode node;

    RescanVisibilityFixture()
    {
        REQUIRE(tempDir.isValid());
        const QString gisLayersPath = QDir(tempDir.path())
                                          .filePath(cwLazLayerModel::folderName());
        QDir().mkpath(gisLayersPath);
        gisLayersDir = QDir(gisLayersPath);
        region.lazLayers()->setGisLayersDir(gisLayersDir);

        pipeline.setKeywordModel(&keywordItemModel);
        visibility.setVisibleModel(pipeline.acceptedModel());
        visibility.setHideModel(pipeline.rejectedModel());

        node.setScene(&scene);
        node.setKeywordItemModel(&keywordItemModel);
        node.setLazLayerModel(region.lazLayers());
    }

    QString addLazFileAndRescan(const QString& tag)
    {
        const QString filePath = gisLayersDir.filePath(QStringLiteral("%1.laz").arg(tag));
        REQUIRE(!writeMinimalLaz(filePath).isEmpty());
        region.lazLayers()->rescan();
        QCoreApplication::processEvents();
        return filePath;
    }
};

} // namespace

// After a rescan (which destroys and recreates all layers), the keyword
// filter must still control LAZ visibility. The original bug left every
// render object stuck at visible=true regardless of filter state, because
// rowsRemoved was ignored and the last-fired insertion won when an item
// briefly appeared in both pipeline outputs.
TEST_CASE("cwLazLayerModel rescan + keyword filter: visibility controllable "
          "after rescan",
          "[cwLazLayerModel][cwKeywordVisibility][rescanThrash]")
{
    RescanVisibilityFixture f;

    f.addLazFileAndRescan(QStringLiteral("initial"));
    REQUIRE(f.region.lazLayers()->count() == 1);

    cwLazLayer* initialLayer = f.region.lazLayers()->layerAt(0);
    cwRenderPointCloud* initialCloud = f.node.pointCloudForLayer(initialLayer);
    REQUIRE(initialCloud != nullptr);
    REQUIRE(initialCloud->isVisible());

    // Simulate the post-clip rescan: a second file appears, and rescan()
    // destroys both layers and recreates them.
    f.addLazFileAndRescan(QStringLiteral("clip_001"));
    REQUIRE(f.region.lazLayers()->count() == 2);

    QList<cwRenderPointCloud*> liveClouds;
    for (int i = 0; i < f.region.lazLayers()->count(); ++i) {
        cwLazLayer* layer = f.region.lazLayers()->layerAt(i);
        cwRenderPointCloud* cloud = f.node.pointCloudForLayer(layer);
        REQUIRE(cloud != nullptr);
        liveClouds.append(cloud);
    }
    REQUIRE(liveClouds.size() == 2);
    for (cwRenderPointCloud* cloud : liveClouds) {
        CHECK(cloud->isVisible());
    }

    // Direct user-style hide via a dedicated hide model (bypasses the
    // pipeline so we test cwKeywordVisibility's reaction in isolation
    // from the multi-stage filter): adding a keyword item per render
    // object must drop them all to invisible.
    cwKeywordItemModel directHideModel;
    f.visibility.setHideModel(&directHideModel);
    for (cwRenderPointCloud* cloud : liveClouds) {
        auto* hideItem = new cwKeywordItem();
        hideItem->setObject(cloud);
        directHideModel.addItem(hideItem);
    }
    for (cwRenderPointCloud* cloud : liveClouds) {
        CHECK_FALSE(cloud->isVisible());
    }

    // Removing the hide entries must restore visibility.
    while (directHideModel.rowCount() > 0) {
        directHideModel.removeItem(directHideModel.item(0));
    }
    for (cwRenderPointCloud* cloud : liveClouds) {
        CHECK(cloud->isVisible());
    }
}
