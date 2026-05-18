#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwCavingRegion.h"
#include "cwKeyword.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

#include "LazFixtureHelper.h"

namespace {

// Helper: write a .laz directly into the model's GIS Layers folder and
// trigger a rescan so it surfaces as a layer row. This bypasses the
// project-based addFromFiles copy pipeline, which is sufficient for tests
// that only care about model + keyword registration behavior.
struct LazKeywordFixture {
    QTemporaryDir tempDir;
    QDir gisLayersDir;
    cwCavingRegion region;
    cwKeywordItemModel keywordItemModel;
    cwScene scene;
    cwLazLayersSceneNode node;

    LazKeywordFixture()
    {
        const QString gisLayersPath = QDir(tempDir.path())
                                          .filePath(cwLazLayerModel::folderName());
        QDir().mkpath(gisLayersPath);
        gisLayersDir = QDir(gisLayersPath);
        region.lazLayers()->setGisLayersDir(gisLayersDir);

        node.setScene(&scene);
        node.setKeywordItemModel(&keywordItemModel);
        node.setLazLayerModel(region.lazLayers());
    }

    // Drops a .laz file into the GIS Layers folder under the given tag,
    // triggers a rescan, and returns the layer at row 0 (assumes only one
    // file in the folder).
    cwLazLayer* addLazFile(const QString& tag)
    {
        const QString fileName = QStringLiteral("%1.laz").arg(tag);
        const QString filePath = gisLayersDir.filePath(fileName);
        REQUIRE(!writeMinimalLaz(filePath).isEmpty());
        region.lazLayers()->rescan();
        return region.lazLayers()->layerAt(region.lazLayers()->count() - 1);
    }
};

int findLazVisibilityRow(cwKeywordItemModel* model) {
    for (int i = 0; i < model->rowCount(); ++i) {
        cwKeywordItem* item = model->item(i);
        if (!item) continue;
        if (qobject_cast<cwRenderPointCloud*>(item->object()) != nullptr) {
            return i;
        }
    }
    return -1;
}

cwKeywordItem* findLazKeywordItem(cwKeywordItemModel* model, cwLazLayer* layer) {
    const QString shortId = layer->id().toString(QUuid::WithoutBraces).left(8);
    for (int i = 0; i < model->rowCount(); ++i) {
        cwKeywordItem* item = model->item(i);
        if (!item) continue;
        if (qobject_cast<cwRenderPointCloud*>(item->object()) == nullptr) {
            continue;
        }
        const auto keywords = item->keywordModel()->keywords();
        for (const auto& kw : keywords) {
            if (kw.key() == cwKeywordModel::ObjectIdKey && kw.value() == shortId) {
                return item;
            }
        }
    }
    return nullptr;
}

QString keywordValue(cwKeywordItem* item, const QString& key) {
    const auto keywords = item->keywordModel()->keywords();
    for (const auto& kw : keywords) {
        if (kw.key() == key) {
            return kw.value();
        }
    }
    return QString();
}

} // namespace

TEST_CASE("Adding a LAZ layer registers a keyword item with Type / FileName / ObjectId",
          "[LAZLayerKeywords]") {
    LazKeywordFixture f;
    REQUIRE(f.keywordItemModel.rowCount() == 0);

    cwLazLayer* layer = f.addLazFile(QStringLiteral("kw"));
    REQUIRE(layer != nullptr);

    cwKeywordItem* kwItem = findLazKeywordItem(&f.keywordItemModel, layer);
    REQUIRE(kwItem != nullptr);

    REQUIRE(keywordValue(kwItem, cwKeywordModel::TypeKey) == QStringLiteral("LAZ Layer"));
    REQUIRE(keywordValue(kwItem, cwKeywordModel::FileNameKey) ==
            QFileInfo(layer->sourcePath()).fileName());
    REQUIRE(keywordValue(kwItem, cwKeywordModel::ObjectIdKey) ==
            layer->id().toString(QUuid::WithoutBraces).left(8));
}

TEST_CASE("Removing a LAZ layer unregisters its keyword item", "[LAZLayerKeywords]") {
    LazKeywordFixture f;

    cwLazLayer* layer = f.addLazFile(QStringLiteral("rm"));
    REQUIRE(findLazKeywordItem(&f.keywordItemModel, layer) != nullptr);

    f.region.lazLayers()->removeAt(0);

    REQUIRE(findLazVisibilityRow(&f.keywordItemModel) == -1);
}

TEST_CASE("Changing sourcePath updates the LAZ keyword item's FileName / Name",
          "[LAZLayerKeywords]") {
    LazKeywordFixture f;

    cwLazLayer* layer = f.addLazFile(QStringLiteral("orig"));
    cwKeywordItem* kwItem = findLazKeywordItem(&f.keywordItemModel, layer);
    REQUIRE(kwItem != nullptr);
    REQUIRE(keywordValue(kwItem, cwKeywordModel::FileNameKey) ==
            QFileInfo(layer->sourcePath()).fileName());

    const QString pathB = f.gisLayersDir.filePath(QStringLiteral("renamed.laz"));
    REQUIRE(!writeMinimalLaz(pathB).isEmpty());
    layer->setSourcePath(pathB);

    REQUIRE(keywordValue(kwItem, cwKeywordModel::FileNameKey) == QFileInfo(pathB).fileName());
}

TEST_CASE("LAZ keyword item visibility round-trip toggles render-side visibility",
          "[LAZLayerKeywords]") {
    LazKeywordFixture f;

    cwLazLayer* layer = f.addLazFile(QStringLiteral("vis"));
    cwKeywordItem* kwItem = findLazKeywordItem(&f.keywordItemModel, layer);
    REQUIRE(kwItem != nullptr);

    auto* renderObject = qobject_cast<cwRenderPointCloud*>(kwItem->object());
    REQUIRE(renderObject != nullptr);
    REQUIRE(renderObject->isVisible());

    renderObject->setVisible(false);
    REQUIRE_FALSE(renderObject->isVisible());

    renderObject->setVisible(true);
    REQUIRE(renderObject->isVisible());
}
