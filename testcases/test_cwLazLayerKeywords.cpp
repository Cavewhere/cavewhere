#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
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

struct LazKeywordFixture {
    cwCavingRegion region;
    cwKeywordItemModel keywordItemModel;
    cwScene scene;
    cwLazLayersSceneNode node;

    LazKeywordFixture() {
        node.setScene(&scene);
        node.setKeywordItemModel(&keywordItemModel);
        node.setLazLayerModel(region.lazLayers());
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
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    LazKeywordFixture f;

    REQUIRE(f.keywordItemModel.rowCount() == 0);

    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("kw")));
    cwLazLayer* layer = f.region.lazLayers()->addLayer(path);
    REQUIRE(layer != nullptr);

    cwKeywordItem* kwItem = findLazKeywordItem(&f.keywordItemModel, layer);
    REQUIRE(kwItem != nullptr);

    REQUIRE(keywordValue(kwItem, cwKeywordModel::TypeKey) == QStringLiteral("LAZ Layer"));
    REQUIRE(keywordValue(kwItem, cwKeywordModel::FileNameKey) == QFileInfo(path).fileName());
    REQUIRE(keywordValue(kwItem, cwKeywordModel::ObjectIdKey) ==
            layer->id().toString(QUuid::WithoutBraces).left(8));
}

TEST_CASE("Removing a LAZ layer unregisters its keyword item", "[LAZLayerKeywords]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    LazKeywordFixture f;

    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("rm")));
    cwLazLayer* layer = f.region.lazLayers()->addLayer(path);
    REQUIRE(findLazKeywordItem(&f.keywordItemModel, layer) != nullptr);

    f.region.lazLayers()->removeAt(0);

    REQUIRE(findLazVisibilityRow(&f.keywordItemModel) == -1);
}

TEST_CASE("Changing sourcePath updates the LAZ keyword item's FileName / Name",
          "[LAZLayerKeywords]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    LazKeywordFixture f;

    const QString pathA = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("orig")));
    const QString pathB = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("renamed")));

    cwLazLayer* layer = f.region.lazLayers()->addLayer(pathA);
    cwKeywordItem* kwItem = findLazKeywordItem(&f.keywordItemModel, layer);
    REQUIRE(kwItem != nullptr);
    REQUIRE(keywordValue(kwItem, cwKeywordModel::FileNameKey) == QFileInfo(pathA).fileName());

    layer->setSourcePath(pathB);

    REQUIRE(keywordValue(kwItem, cwKeywordModel::FileNameKey) == QFileInfo(pathB).fileName());
}

TEST_CASE("LAZ keyword item visibility round-trip toggles render-side visibility",
          "[LAZLayerKeywords]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    LazKeywordFixture f;

    const QString path = writeMinimalLaz(tempLazPath(tempDir, QStringLiteral("vis")));
    cwLazLayer* layer = f.region.lazLayers()->addLayer(path);
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
