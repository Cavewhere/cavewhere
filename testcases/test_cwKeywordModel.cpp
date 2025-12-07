//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwKeywordModel.h"
#include "cwNoteLiDAR.h"
#include "cwTrip.h"
#include "TestHelper.h"
#include "SignalSpyChecker.h"

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

TEST_CASE("cwKeywordModel exposes object type keywords", "[cwKeywordModel]") {
    CHECK(cwKeywordModel::TypeKey == QStringLiteral("Type"));
}

TEST_CASE("cwNoteLiDAR keywords include object type and trip metadata", "[cwKeywordModel][cwNoteLiDAR]") {
    cwTrip trip;
    trip.setName("LiDAR Trip");

    cwNoteLiDAR note;
    note.setParentTrip(&trip);

    const auto keywords = note.keywordModel()->keywords();
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("LiDAR"))));
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::TripNameKey, QStringLiteral("LiDAR Trip"))));
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

        auto checkRows = [&modelKeywords](cwKeywordModel* model) {
            REQUIRE(model->rowCount() == modelKeywords.size());

            for(int i = 0; i < model->rowCount(); i++) {
                auto index = model->index(i);
                auto modelKeyword = modelKeywords.at(i);
                INFO("Comparing:" << model->data(index, cwKeywordModel::KeywordRole).value<cwKeyword>() << " to " << modelKeyword);
                CHECK(model->data(index, cwKeywordModel::KeyRole).toString().toStdString() == modelKeyword.key().toStdString());
                CHECK(model->data(index, cwKeywordModel::ValueRole).toString().toStdString() == modelKeyword.value().toStdString());
                CHECK(model->data(index, cwKeywordModel::KeywordRole).value<cwKeyword>() == modelKeyword);
            }
        };

        CHECK(insertSpy.size() == model.rowCount());
        CHECK(aboutinsertSpy.size() == model.rowCount());
        CHECK(removeSpy.size() == 0);
        CHECK(aboutRemovedSpy.size() == 0);

        checkRows(&model);

        insertSpy.clear();
        aboutinsertSpy.clear();

        SECTION("Remove keywords with key") {

            model.removeAll("key2");

            modelKeywords.remove(2, 3);

            CHECK(insertSpy.size() == 0);
            CHECK(aboutinsertSpy.size() == 0);
            CHECK(removeSpy.size() == 3);
            CHECK(aboutRemovedSpy.size() == 3);

            checkRows(&model);
        }

        SECTION("Remove a specific keyword") {
            model.remove({"key4", "value2"});

            modelKeywords.removeLast();

            CHECK(insertSpy.size() == 0);
            CHECK(aboutinsertSpy.size() == 0);
            CHECK(removeSpy.size() == 1);
            CHECK(aboutRemovedSpy.size() == 1);

            checkRows(&model);
        }

        SECTION("Clear the model") {
            model.clear();
            modelKeywords.clear();

            CHECK(insertSpy.size() == 0);
            CHECK(aboutinsertSpy.size() == 0);
            CHECK(removeSpy.size() == 1);
            CHECK(aboutRemovedSpy.size() == 1);

            checkRows(&model);
        }

        SECTION("Test replacing keywords with new list of keywords") {
            QVector<cwKeyword> newKeywords {
                {"key4", "value3"},
                {"key5", "value4"},
                {"key6", "value3"},
            };

            model.setKeywords(newKeywords);
            modelKeywords = newKeywords;

            CHECK(insertSpy.size() == 1);
            CHECK(aboutinsertSpy.size() == 1);
            CHECK(removeSpy.size() == 1);
            CHECK(aboutRemovedSpy.size() == 1);

            checkRows(&model);
        }

        SECTION("Test setData") {
            QSignalSpy dataChangedSpy(&model, &cwKeywordModel::dataChanged);

            auto index = model.index(1);
            CHECK(model.setData(index, "value2", cwKeywordModel::ValueRole) == true);
            modelKeywords[1].setValue("value2");
            checkRows(&model);
            CHECK(dataChangedSpy.size() == 1);

            //Changing the value to an existing key pair
            CHECK(model.setData(index, "value1", cwKeywordModel::ValueRole) == false);
            CHECK(dataChangedSpy.size() == 1);
            checkRows(&model);

            //Change the key
            CHECK(model.setData(index, "key5", cwKeywordModel::KeyRole) == true);
            CHECK(dataChangedSpy.size() == 2);
            modelKeywords[1].setKey("key5");
            checkRows(&model);

            //Changing the value to an existing key pair
            CHECK(model.setData(index, "key2", cwKeywordModel::KeyRole) == false);
            CHECK(dataChangedSpy.size() == 2);
            checkRows(&model);

            //Change the keyword
            auto newKeyword = cwKeyword("sauce", "test");
            modelKeywords[1] = newKeyword;
            CHECK(model.setData(index, QVariant::fromValue(newKeyword), cwKeywordModel::KeywordRole));
            CHECK(dataChangedSpy.size() == 3);
            checkRows(&model);
        }

        SECTION("Adding invalid keywords") {
            model.add(cwKeyword());
            model.add(cwKeyword(QString(), "test"));
            model.add(cwKeyword("test", QString()));

            CHECK(insertSpy.size() == 0);
            CHECK(removeSpy.size() == 0);

            checkRows(&model);
        }

        SECTION("cwKeywordModel should handle cwKeywordModel extentions propertly") {
            auto keywordModelCave = std::make_unique<cwKeywordModel>();
            auto keywordModelTrip = std::make_unique<cwKeywordModel>();
            auto keywordModelScrap = std::make_unique<cwKeywordModel>();

            keywordModelCave->setObjectName("keywordModelCave");
            keywordModelTrip->setObjectName("keywordModelTrip");
            keywordModelScrap->setObjectName("keywordModelScrap");

            keywordModelCave->addKeywords({
                                      {"Cave", "Sauce cave"},
                                      {"Country", "USA"}
                                  });

            keywordModelTrip->addKeywords({
                                      {"Trip", "Trip1"},
                                      {"Surveyor", "Philip Schuchardt"},
                                      {"Surveyor", "Sara"},
                                      {"Date", "2019-08-22"}
                                  });

            keywordModelScrap->addKeywords({
                                       {"Type", "scrap"}
                                   });

            keywordModelTrip->addExtension(keywordModelCave.get());
            keywordModelScrap->addExtension(keywordModelTrip.get());

            modelKeywords.clear();
            modelKeywords.append({
                                     {"Cave", "Sauce cave"},
                                     {"Country", "USA"}
                                 });

            checkRows(keywordModelCave.get());

            modelKeywords.clear();
            modelKeywords.append({
                                     {"Trip", "Trip1"},
                                     {"Surveyor", "Philip Schuchardt"},
                                     {"Surveyor", "Sara"},
                                     {"Date", "2019-08-22"},
                                     {"Cave", "Sauce cave"},
                                     {"Country", "USA"}
                                 });

            checkRows(keywordModelTrip.get());

            modelKeywords.clear();
            modelKeywords.append({
                                     {"Type", "scrap"},
                                     {"Trip", "Trip1"},
                                     {"Surveyor", "Philip Schuchardt"},
                                     {"Surveyor", "Sara"},
                                     {"Date", "2019-08-22"},
                                     {"Cave", "Sauce cave"},
                                     {"Country", "USA"}
                                 });

            checkRows(keywordModelScrap.get());

            SignalSpyChecker::SignalSpy aboutinsertSpy(keywordModelScrap.get(), &cwKeywordModel::rowsAboutToBeInserted);
            SignalSpyChecker::SignalSpy insertSpy(keywordModelScrap.get(), &cwKeywordModel::rowsInserted);
            SignalSpyChecker::SignalSpy aboutRemovedSpy(keywordModelScrap.get(), &cwKeywordModel::rowsAboutToBeRemoved);
            SignalSpyChecker::SignalSpy removeSpy(keywordModelScrap.get(), &cwKeywordModel::rowsRemoved);
            SignalSpyChecker::SignalSpy dataChanged(keywordModelScrap.get(), &cwKeywordModel::dataChanged);

            SignalSpyChecker::Constant spyChecker {
                {&aboutinsertSpy, 0},
                {&insertSpy, 0},
                {&aboutRemovedSpy, 0},
                {&removeSpy, 0},
                {&dataChanged, 0}
            };

            spyChecker.checkSpies();

            SECTION("Check single add") {
                keywordModelCave->add({"test", "sauce"});

                modelKeywords.clear();
                modelKeywords.append({
                                         {"Type", "scrap"},
                                         {"Trip", "Trip1"},
                                         {"Surveyor", "Philip Schuchardt"},
                                         {"Surveyor", "Sara"},
                                         {"Date", "2019-08-22"},
                                         {"Cave", "Sauce cave"},
                                         {"Country", "USA"},
                                         {"test", "sauce"}
                                     });

                checkRows(keywordModelScrap.get());
                spyChecker[&aboutinsertSpy] = 1;
                spyChecker[&insertSpy] = 1;
                spyChecker.requireSpies();

                REQUIRE(aboutinsertSpy.at(0).size() == 3);
                CHECK(aboutinsertSpy.at(0).at(1).toInt() == 7);
                CHECK(aboutinsertSpy.at(0).at(2).toInt() == 7);

                REQUIRE(insertSpy.at(0).size() == 3);
                CHECK(insertSpy.at(0).at(1).toInt() == 7);
                CHECK(insertSpy.at(0).at(2).toInt() == 7);
            }

            SECTION("Check multi add") {
                keywordModelTrip->addKeywords({
                                          {"test1", "sauce1"},
                                          {"test2", "sauce2"}
                                      });

                modelKeywords.clear();
                modelKeywords.append({
                                         {"Type", "scrap"},
                                         {"Trip", "Trip1"},
                                         {"Surveyor", "Philip Schuchardt"},
                                         {"Surveyor", "Sara"},
                                         {"Date", "2019-08-22"},
                                         {"test1", "sauce1"},
                                         {"test2", "sauce2"},
                                         {"Cave", "Sauce cave"},
                                         {"Country", "USA"},
                                     });

                checkRows(keywordModelScrap.get());
                spyChecker[&aboutinsertSpy] = 1;
                spyChecker[&insertSpy] = 1;
                spyChecker.requireSpies();

                REQUIRE(insertSpy.at(0).size() == 3);
                CHECK(insertSpy.at(0).at(1).toInt() == 5);
                CHECK(insertSpy.at(0).at(2).toInt() == 6);

                REQUIRE(aboutinsertSpy.at(0).size() == 3);
                CHECK(aboutinsertSpy.at(0).at(1).toInt() == 5);
                CHECK(aboutinsertSpy.at(0).at(2).toInt() == 6);
            }

            SECTION("Check remove") {
                keywordModelCave->removeAll("Cave");
                modelKeywords.clear();
                modelKeywords.append({
                                         {"Type", "scrap"},
                                         {"Trip", "Trip1"},
                                         {"Surveyor", "Philip Schuchardt"},
                                         {"Surveyor", "Sara"},
                                         {"Date", "2019-08-22"},
                                         {"Country", "USA"},
                                     });

                checkRows(keywordModelScrap.get());
                spyChecker[&aboutRemovedSpy] = 1;
                spyChecker[&removeSpy] = 1;
                spyChecker.requireSpies();

                REQUIRE(removeSpy.at(0).size() == 3);
                CHECK(removeSpy.at(0).at(1).toInt() == 5);
                CHECK(removeSpy.at(0).at(2).toInt() == 5);

                REQUIRE(aboutRemovedSpy.at(0).size() == 3);
                CHECK(aboutRemovedSpy.at(0).at(1).toInt() == 5);
                CHECK(aboutRemovedSpy.at(0).at(2).toInt() == 5);
            }

            SECTION("Check setData") {
                keywordModelCave->setData(keywordModelCave->index(1), "Greenland", cwKeywordModel::ValueRole);

                modelKeywords.clear();
                modelKeywords.append({
                                         {"Type", "scrap"},
                                         {"Trip", "Trip1"},
                                         {"Surveyor", "Philip Schuchardt"},
                                         {"Surveyor", "Sara"},
                                         {"Date", "2019-08-22"},
                                         {"Cave", "Sauce cave"},
                                         {"Country", "Greenland"}
                                     });

                checkRows(keywordModelScrap.get());

                spyChecker[&dataChanged] = 1;
                spyChecker.requireSpies();

                REQUIRE(dataChanged.first().size() == 3);
                CHECK(dataChanged.first().at(0).value<QModelIndex>().row() == 6);
                CHECK(dataChanged.first().at(1).value<QModelIndex>().row() == 6);
                REQUIRE(dataChanged.first().at(2).value<QVector<int>>().size() == 2);
                CHECK(dataChanged.first().at(2).value<QVector<int>>().contains(cwKeywordModel::ValueRole) == true);
                CHECK(dataChanged.first().at(2).value<QVector<int>>().contains(cwKeywordModel::KeywordRole) == true);
            }

            SECTION("Check deletion 1") {
                keywordModelTrip.reset(nullptr);

                modelKeywords.clear();
                modelKeywords.append({
                                         {"Type", "scrap"},
                                     });

                checkRows(keywordModelScrap.get());
                spyChecker[&aboutRemovedSpy] = 1;
                spyChecker[&removeSpy] = 1;
                spyChecker.requireSpies();

                REQUIRE(removeSpy.at(0).size() == 3);
                CHECK(removeSpy.at(0).at(1).toInt() == 1);
                CHECK(removeSpy.at(0).at(2).toInt() == 6);

                REQUIRE(aboutRemovedSpy.at(0).size() == 3);
                CHECK(aboutRemovedSpy.at(0).at(1).toInt() == 1);
                CHECK(aboutRemovedSpy.at(0).at(2).toInt() == 6);
            }

            SECTION("Check deletion 2") {
                keywordModelCave.reset(nullptr);

                modelKeywords.clear();
                modelKeywords.append({
                                         {"Type", "scrap"},
                                         {"Trip", "Trip1"},
                                         {"Surveyor", "Philip Schuchardt"},
                                         {"Surveyor", "Sara"},
                                         {"Date", "2019-08-22"}
                                     });

                checkRows(keywordModelScrap.get());
                spyChecker[&aboutRemovedSpy] = 1;
                spyChecker[&removeSpy] = 1;
                spyChecker.requireSpies();

                REQUIRE(removeSpy.at(0).size() == 3);
                CHECK(removeSpy.at(0).at(1).toInt() == 5);
                CHECK(removeSpy.at(0).at(2).toInt() == 6);

                REQUIRE(aboutRemovedSpy.at(0).size() == 3);
                CHECK(aboutRemovedSpy.at(0).at(1).toInt() == 5);
                CHECK(aboutRemovedSpy.at(0).at(2).toInt() == 6);
            }
        }
    }
}
