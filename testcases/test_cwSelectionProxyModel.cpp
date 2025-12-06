// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwSelectionProxyModel.h"
#include "cwKeywordGroupByKeyModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"

// Qt includes
#include <QModelIndex>

// Std includes
#include <memory>

TEST_CASE("cwSelectionProxyModel tracks selection using valueRole ids", "[cwSelectionProxyModel]")
{
    auto groupBy = std::make_unique<cwKeywordGroupByKeyModel>();

    auto component1 = new cwKeywordItem();
    auto component2 = new cwKeywordItem();
    auto component3 = new cwKeywordItem();
    auto component4 = new cwKeywordItem();

    auto keywordModel1 = component1->keywordModel();
    auto keywordModel2 = component2->keywordModel();
    auto keywordModel3 = component3->keywordModel();
    auto keywordModel4 = component4->keywordModel();

    auto entity1 = std::make_unique<QObject>();
    auto entity2 = std::make_unique<QObject>();
    auto entity3 = std::make_unique<QObject>();
    auto entity4 = std::make_unique<QObject>();

    keywordModel1->add({"type", "scrap"});
    keywordModel2->add({"type", "point"});
    keywordModel3->add({"type", "line"});
    keywordModel4->add({"type", "point"});

    keywordModel1->add({"trip", "trip1"});
    keywordModel2->add({"trip", "trip2"});
    keywordModel3->add({"trip", "trip2"});
    keywordModel4->add({"trip", "trip3"});

    component1->setObject(entity1.get());
    component2->setObject(entity2.get());
    component3->setObject(entity3.get());
    component4->setObject(entity4.get());

    auto keywordEntityModel = std::make_unique<cwKeywordItemModel>();
    keywordEntityModel->addItem(component1);
    keywordEntityModel->addItem(component2);
    keywordEntityModel->addItem(component3);
    keywordEntityModel->addItem(component4);

    groupBy->setSourceModel(keywordEntityModel.get());
    groupBy->setKey("type");

    cwSelectionProxyModel selection;
    selection.setIdRole(cwKeywordGroupByKeyModel::ValueRole);
    selection.setSourceModel(groupBy.get());

    REQUIRE(selection.selectionRole() != -1);
    REQUIRE(selection.selectionCount() == 0);

    auto indexForValue = [&selection](const QString& value) {
        for(int i = 0; i < selection.rowCount(); i++) {
            auto idx = selection.index(i, 0);
            if(idx.data(cwKeywordGroupByKeyModel::ValueRole).toString() == value) {
                return idx;
            }
        }
        return QModelIndex();
    };

    auto scrapIndex = indexForValue("scrap");
    REQUIRE(scrapIndex.isValid());

    SECTION("Set and toggle selection on a single row")
    {
        CHECK(selection.data(scrapIndex, selection.selectionRole()).toBool() == false);
        CHECK(selection.setData(scrapIndex, true, selection.selectionRole()));
        CHECK(selection.data(scrapIndex, selection.selectionRole()).toBool() == true);
        CHECK(selection.selectionCount() == 1);

        selection.toggleSelected(scrapIndex);
        CHECK(selection.data(scrapIndex, selection.selectionRole()).toBool() == false);
        CHECK(selection.selectionCount() == 0);
    }

    SECTION("Select all and clear selection")
    {
        selection.selectAll();
        CHECK(selection.selectionCount() == selection.rowCount());
        for(int i = 0; i < selection.rowCount(); i++) {
            auto idx = selection.index(i, 0);
            CHECK(selection.data(idx, selection.selectionRole()).toBool() == true);
        }

        selection.clearSelection();
        CHECK(selection.selectionCount() == 0);
        for(int i = 0; i < selection.rowCount(); i++) {
            auto idx = selection.index(i, 0);
            CHECK(selection.data(idx, selection.selectionRole()).toBool() == false);
        }
    }

    SECTION("Removing rows drops selected ids")
    {
        selection.selectAll();
        auto pointIndex = indexForValue("point");
        REQUIRE(pointIndex.isValid());

        keywordEntityModel->removeItem(component2);
        keywordEntityModel->removeItem(component4);
        CHECK(selection.selectionCount() == selection.rowCount());
        CHECK(selection.rowCount() == 3);
        for(int i = 0; i < selection.rowCount(); i++) {
            auto idx = selection.index(i, 0);
            CHECK(selection.data(idx, selection.selectionRole()).toBool() == true);
        }
    }

    SECTION("Model reset clears selection")
    {
        selection.selectAll();
        groupBy->setKey("trip");
        CHECK(selection.selectionCount() == 0);
    }

    SECTION("Inserting new rows does not alter existing selection")
    {
        selection.selectAll();
        const int previousSelectionCount = selection.selectionCount();

        auto newComponent = new cwKeywordItem();
        auto newKeywordModel = newComponent->keywordModel();
        auto newEntity = std::make_unique<QObject>();
        newKeywordModel->add({"type", "area"});
        newKeywordModel->add({"trip", "trip1"});
        newComponent->setObject(newEntity.get());
        keywordEntityModel->addItem(newComponent);

        CHECK(selection.rowCount() == previousSelectionCount + 1);
        CHECK(selection.selectionCount() == previousSelectionCount);

        // New row should not be selected
        auto areaIndex = indexForValue("area");
        REQUIRE(areaIndex.isValid());
        CHECK(selection.data(areaIndex, selection.selectionRole()).toBool() == false);

        // Existing rows should remain selected
        for(int i = 0; i < selection.rowCount(); i++) {
            auto idx = selection.index(i, 0);
            if(idx == areaIndex) {
                continue;
            }
            CHECK(selection.data(idx, selection.selectionRole()).toBool() == true);
        }
    }

    SECTION("Changing id role data removes selection for changed rows")
    {
        selection.selectAll();
        CHECK(selection.selectionCount() == selection.rowCount());

        // Capture index before change
        auto originalScrapIndex = indexForValue("scrap");
        REQUIRE(originalScrapIndex.isValid());
        CHECK(selection.data(originalScrapIndex, selection.selectionRole()).toBool() == true);

        // Change the id role data for the scrap item so its group value changes
        auto scrapComponent = keywordEntityModel->item(0);
        scrapComponent->keywordModel()->replace({"type", "scrapUpdated"});

        // Re-group to reflect updated values
        groupBy->setKey("type");

        // Old id should no longer exist or be unselected
        auto oldScrapIndex = indexForValue("scrap");
        if(oldScrapIndex.isValid()) {
            CHECK(selection.data(oldScrapIndex, selection.selectionRole()).toBool() == false);
        }

        // New id row should be unselected
        auto updatedScrapIndex = indexForValue("scrapUpdated");
        REQUIRE(updatedScrapIndex.isValid());
        CHECK(selection.data(updatedScrapIndex, selection.selectionRole()).toBool() == false);
    }
}
