//Catch includes
#include "catch.hpp"

//Our includes
#include "cwKeywordModel.h"
#include "TestHelper.h"

//Qt includes
#include <QSignalSpy>

TEST_CASE("cwKeywordModel should initilize correctly", "[cwKeywordModel]") {
    cwKeywordModel model;
    CHECK(model.rowCount() == 0);

    auto roles = model.roleNames();
    CHECK(roles.size() == 3);
    CHECK(roles[cwKeywordModel::KeyRole] == "keyRole");
    CHECK(roles[cwKeywordModel::ValueRole] == "valueRole");
    CHECK(roles[cwKeywordModel::KeywordRole] == "keywordRole");
}

TEST_CASE("cwKeywordModel should add and remove keywords correctly", "[cwKeywordModel]") {
    cwKeywordModel model;

    QSignalSpy aboutinsertSpy(&model, &cwKeywordModel::rowsAboutToBeInserted);
    QSignalSpy insertSpy(&model, &cwKeywordModel::rowsInserted);
    QSignalSpy aboutRemovedSpy(&model, &cwKeywordModel::rowsAboutToBeRemoved);
    QSignalSpy removeSpy(&model, &cwKeywordModel::rowsRemoved);

    SECTION("Basic add and remove") {
        model.add({"key1", "value1"});
        CHECK(model.rowCount() == 1);

        auto first = model.index(0);
        CHECK(model.data(first, cwKeywordModel::KeyRole).toString().toStdString() == QString("key1").toStdString());
        CHECK(model.data(first, cwKeywordModel::ValueRole).toString().toStdString() == QString("value1").toStdString());

        REQUIRE(aboutinsertSpy.size() == 1);
        CHECK(aboutinsertSpy.first().at(0) == QModelIndex());
        CHECK(aboutinsertSpy.first().at(1) == 0);
        CHECK(aboutinsertSpy.first().at(2) == 0);

        REQUIRE(insertSpy.size() == 1);
        CHECK(insertSpy.first().at(0) == QModelIndex());
        CHECK(insertSpy.first().at(1) == 0);
        CHECK(insertSpy.first().at(2) == 0);

        CHECK(aboutRemovedSpy.size() == 0);
        CHECK(removeSpy.size() == 0);

        model.remove({"key1", "value1"});
        CHECK(model.rowCount() == 0);
        CHECK(aboutRemovedSpy.size() == 1);
        CHECK(aboutinsertSpy.size() == 1);
        CHECK(insertSpy.size() == 1);
        CHECK(removeSpy.size() == 1);
    }

    SECTION("Add many values") {

        QVector<cwKeyword> keywords {
            {"key1", "value1"},
            {"key1", "value1"}, //Duplicate
            {"key1", "value1"}, //Duplicate
            {"key3", "value3"},
            {"key2", "value1"},
            {"key2", "value3"},
            {"key2", "value2"},
            {"key3", "value1"},
            {"key2", "value1"}, //Duplicate
            {"key4", "value2"},
        };

        for(auto keyword : keywords) {
            model.add(keyword);
        }

        auto modelKeywords = keywords;
        modelKeywords.removeAt(8); //Remove duplicates
        modelKeywords.removeAt(2);
        modelKeywords.removeAt(1);

        auto checkRows = [&model, &modelKeywords]() {
            REQUIRE(model.rowCount() == modelKeywords.size());

            for(int i = 0; i < model.rowCount(); i++) {
                auto index = model.index(i);
                auto modelKeyword = modelKeywords.at(i);
                INFO("Comparing:" << model.data(index, cwKeywordModel::KeywordRole).value<cwKeyword>() << " to " << modelKeyword);
                CHECK(model.data(index, cwKeywordModel::KeyRole).toString().toStdString() == modelKeyword.key().toStdString());
                CHECK(model.data(index, cwKeywordModel::ValueRole).toString().toStdString() == modelKeyword.value().toStdString());
                CHECK(model.data(index, cwKeywordModel::KeywordRole).value<cwKeyword>() == modelKeyword);
            }
        };

        CHECK(insertSpy.size() == model.rowCount());
        CHECK(aboutinsertSpy.size() == model.rowCount());
        CHECK(removeSpy.size() == 0);
        CHECK(aboutRemovedSpy.size() == 0);

        checkRows();

        insertSpy.clear();
        aboutinsertSpy.clear();

        SECTION("Remove keywords with key") {

            model.removeAll("key2");

            modelKeywords.remove(2, 3);

            CHECK(insertSpy.size() == 0);
            CHECK(aboutinsertSpy.size() == 0);
            CHECK(removeSpy.size() == 3);
            CHECK(aboutRemovedSpy.size() == 3);

            checkRows();
        }

        SECTION("Remove a specific keyword") {
            model.remove({"key4", "value2"});

            modelKeywords.removeLast();

            CHECK(insertSpy.size() == 0);
            CHECK(aboutinsertSpy.size() == 0);
            CHECK(removeSpy.size() == 1);
            CHECK(aboutRemovedSpy.size() == 1);

            checkRows();
        }

        SECTION("Test setData") {
            QSignalSpy dataChangedSpy(&model, &cwKeywordModel::dataChanged);

            auto index = model.index(1);
            CHECK(model.setData(index, "value2", cwKeywordModel::ValueRole) == true);
            modelKeywords[1].setValue("value2");
            checkRows();
            CHECK(dataChangedSpy.size() == 1);

            //Changing the value to an existing key pair
            CHECK(model.setData(index, "value1", cwKeywordModel::ValueRole) == false);
            CHECK(dataChangedSpy.size() == 1);
            checkRows();

            //Change the key
            CHECK(model.setData(index, "key5", cwKeywordModel::KeyRole) == true);
            CHECK(dataChangedSpy.size() == 2);
            modelKeywords[1].setKey("key5");
            checkRows();

            //Changing the value to an existing key pair
            CHECK(model.setData(index, "key2", cwKeywordModel::KeyRole) == false);
            CHECK(dataChangedSpy.size() == 2);
            checkRows();

            //Change the keyword
            auto newKeyword = cwKeyword("sauce", "test");
            modelKeywords[1] = newKeyword;
            CHECK(model.setData(index, QVariant::fromValue(newKeyword), cwKeywordModel::KeywordRole));
            CHECK(dataChangedSpy.size() == 3);
            checkRows();
        }

        SECTION("Adding invalid keywords") {
            model.add(cwKeyword());
            model.add(cwKeyword(QString(), "test"));
            model.add(cwKeyword("test", QString()));

            CHECK(insertSpy.size() == 0);
            CHECK(removeSpy.size() == 0);

            checkRows();
        }
    }
}
