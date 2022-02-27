//Our includes
#include "cwKeywordFilterPipelineModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordGroupByKeyModel.h"
#include "SpyChecker.h"

//Catch includes
#include "catch.hpp"

TEST_CASE("cwKeywordFilterPipelineModel should initilize correctly", "[cwKeywordFilterPipelineModel]") {
    cwKeywordFilterPipelineModel model;
    REQUIRE(model.rowCount() == 1);
    CHECK(model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
    CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);

    REQUIRE(model.acceptedModel() != nullptr);
    CHECK(model.acceptedModel()->rowCount() == 0);
}

TEST_CASE("cwKeywordFilterPipelineModel filter correctly", "[cwKeywordFilterPipelineModel]") {
    qDebug() << "Start test";

    cwKeywordFilterPipelineModel model;

    auto keywordEntityModel = std::make_unique<cwKeywordItemModel>();

    auto component1 = new cwKeywordItem();
    auto component2 = new cwKeywordItem();
    auto component3 = new cwKeywordItem();
    auto component4 = new cwKeywordItem();

    component1->setObjectName("component1");
    component2->setObjectName("component2");
    component3->setObjectName("component3");
    component4->setObjectName("component4");

    cwKeywordModel* keywordModel1 = component1->keywordModel();
    cwKeywordModel* keywordModel2 = component2->keywordModel();
    cwKeywordModel* keywordModel3 = component3->keywordModel();
    cwKeywordModel* keywordModel4 = component4->keywordModel();

    keywordModel1->setObjectName("keywordModel1");
    keywordModel2->setObjectName("keywordModel2");
    keywordModel3->setObjectName("keywordModel3");
    keywordModel4->setObjectName("keywordModel4");

    auto entity1 = std::make_unique<QObject>();
    auto entity2 = std::make_unique<QObject>();
    auto entity3 = std::make_unique<QObject>();
    auto entity4 = std::make_unique<QObject>();

    entity1->setObjectName("entity1");
    entity2->setObjectName("entity2");
    entity3->setObjectName("entity3");
    entity4->setObjectName("entity4");

    keywordModel1->add({"type", "scrap"});
    keywordModel1->add({"trip", "trip1"});

    keywordModel2->add({"type", "scrap"});
    keywordModel2->add({"trip", "trip2"});
    keywordModel2->add({"cave", "cave1"});

    keywordModel3->add({"type", "line"});
    keywordModel3->add({"trip", "trip2"});
    keywordModel3->add({"cave", "cave1"});

    keywordModel4->add({"type", "point"});
    keywordModel4->add({"trip", "trip3"});
    keywordModel4->add({"cave", "cave2"});

    component1->setObject(entity1.get());
    component2->setObject(entity2.get());
    component3->setObject(entity3.get());
    component4->setObject(entity4.get());

    keywordEntityModel->addItem(component1);
    keywordEntityModel->addItem(component2);
    keywordEntityModel->addItem(component3);
    keywordEntityModel->addItem(component4);

    auto pipelineSpy = SpyChecker::makeChecker(&model);
    auto acceptedSpy = SpyChecker::makeChecker(model.acceptedModel());

    model.setKeywordModel(keywordEntityModel.get());

    auto keywordModelSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::keywordModelChanged);
    auto rowsInsertedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsInserted);
    auto rowsAboutToBeInsertedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeInserted);
    auto rowsRemovedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeRemoved);
    auto rowsAboutToBeRemovedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeRemoved);

    auto acceptedRowsInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsInserted);
    auto acceptedRowsAboutToBeInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeInserted);
    auto acceptedRowsRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsRemoved);
    auto acceptedRowsAboutToBeRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeRemoved);
    auto accetpedColumnInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsInserted);
    auto accetpedColumnAboutToBeInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsAboutToBeInserted);
    auto accetpedColumnRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsRemoved);
    auto accetpedColumnAboutToBeRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsAboutToBeRemoved);

    pipelineSpy[keywordModelSpy]++;
    acceptedSpy[acceptedRowsInsertedSpy] = 4;
    acceptedSpy[acceptedRowsAboutToBeInsertedSpy] = 4;

    pipelineSpy.checkSpies();
    acceptedSpy.checkSpies();

    QVector<QObject*> acceptedObjects;
    QVector<QObject*> rejectedObjects;
    auto checkAcceptedRejected = [&acceptedObjects, &rejectedObjects, &model]() {
        CHECK(model.acceptedModel()->rowCount() == acceptedObjects.size());

        auto findInModel = [](QObject* obj, QAbstractItemModel* model) {
            for(int i = 0; i < model->rowCount(); i++) {
                if(model->index(i, 0).data(cwKeywordItemModel::ObjectRole).value<QObject*>() == obj) {
                    return true;
                }
            }
            return false;
        };

        for(auto obj : acceptedObjects) {
            INFO("Obj:" << obj->objectName().toStdString());
            CHECK(findInModel(obj, model.acceptedModel()));
        }

        for(auto obj : rejectedObjects) {
            INFO("Obj:" << obj->objectName().toStdString());
            CHECK(findInModel(obj, model.rejectedModel()));
        }
    };

    acceptedObjects = {
        entity1.get(),
        entity2.get(),
        entity3.get(),
        entity4.get()
    };

    REQUIRE(model.rowCount() == 1);
    CHECK(model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
    CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);
    CHECK(model.acceptedModel()->rowCount() == 4);
    checkAcceptedRejected();

    SECTION("Set the first key") {
        auto groupBy = model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<cwKeywordGroupByKeyModel*>();
        REQUIRE(groupBy);

        groupBy->setKey("type");
        CHECK(groupBy->rowCount() == 4);

        auto groupByFirstIndex = groupBy->index(0);
        CHECK(groupByFirstIndex.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "line");
        CHECK(groupByFirstIndex.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);

        pipelineSpy.checkSpies();
        acceptedSpy.checkSpies();

        acceptedObjects = {
            entity1.get(),
            entity2.get(),
            entity3.get(),
            entity4.get()
        };

        checkAcceptedRejected();

        SECTION("Reject scrap") {
            auto scrapIndex = groupBy->index(2);
            CHECK(scrapIndex.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "scrap");
            CHECK(scrapIndex.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);
            CHECK(groupBy->setData(scrapIndex, false, cwKeywordGroupByKeyModel::AcceptedRole));
            CHECK(scrapIndex.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);

            acceptedObjects = {
                entity3.get(),
                entity4.get()
            };

            rejectedObjects = {
                entity1.get(),
                entity2.get()
            };

            acceptedSpy[acceptedRowsAboutToBeRemovedSpy] += 2;
            acceptedSpy[acceptedRowsRemovedSpy] += 2;

            acceptedSpy.checkSpies();
            pipelineSpy.checkSpies();

            checkAcceptedRejected();

            SECTION("Add another KeywordItem rejected") {
                auto component5 = new cwKeywordItem();
                component5->setObjectName("component5");
                cwKeywordModel* keywordModel5 = component5->keywordModel();
                auto entity5 = std::make_unique<QObject>();
                entity5->setObjectName("entity5");
                component5->setObject(entity5.get());

                keywordModel5->add({"type", "scrap"});
                keywordModel5->add({"trip", "trip3"});
                keywordModel5->add({"cave", "cave2"});

                keywordEntityModel->addItem(component5);

                rejectedObjects.append(entity5.get());

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Add KeywordItem accepted") {
                auto component5 = new cwKeywordItem();
                component5->setObjectName("component5");
                cwKeywordModel* keywordModel5 = component5->keywordModel();
                auto entity5 = std::make_unique<QObject>();
                entity5->setObjectName("entity5");
                component5->setObject(entity5.get());

                keywordModel5->add({"type", "line"});
                keywordModel5->add({"trip", "trip3"});
                keywordModel5->add({"cave", "cave2"});

                keywordEntityModel->addItem(component5);

                acceptedObjects.append(entity5.get());

                acceptedSpy[acceptedRowsAboutToBeInsertedSpy] += 1;
                acceptedSpy[acceptedRowsInsertedSpy] += 1;

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Change keywordModel from accepted to rejected") {
                keywordModel4->replace({"type", "scrap"});

                acceptedObjects = {entity3.get()};
                rejectedObjects = {entity1.get(),
                                   entity2.get(),
                                   entity4.get()};

                acceptedSpy[acceptedRowsAboutToBeRemovedSpy] += 1;
                acceptedSpy[acceptedRowsRemovedSpy] += 1;

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Change keywordModel from rejected to accepted") {
                keywordModel1->replace({"type", "line"});

                acceptedObjects = {
                    entity3.get(),
                    entity4.get(),
                    entity1.get(),
                };

                rejectedObjects = {
                    entity2.get()
                };

                acceptedSpy[acceptedRowsAboutToBeInsertedSpy] += 1;
                acceptedSpy[acceptedRowsInsertedSpy] += 1;

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Remove accepted keywordItem") {
                keywordEntityModel->removeItem(component3);

                acceptedObjects = {
                    entity4.get()
                };

                rejectedObjects = {
                    entity1.get(),
                    entity2.get()
                };

                acceptedSpy[acceptedRowsAboutToBeRemovedSpy] += 1;
                acceptedSpy[acceptedRowsRemovedSpy] += 1;

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Remove rejected keywordItem") {
                keywordEntityModel->removeItem(component2);

                acceptedObjects = {
                    entity4.get(),
                    entity3.get()
                };

                rejectedObjects = {
                    entity1.get()
                };

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Add row") {
                //Make sure we're and'ing the new row
                qDebug() << "Add row";
                model.addRow();

                REQUIRE(model.rowCount() == 2);
                CHECK(model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);
                CHECK(model.index(1).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                CHECK(model.index(1).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);

                acceptedSpy[acceptedRowsInsertedSpy] += 2;
                acceptedSpy[acceptedRowsAboutToBeInsertedSpy] += 2;
                acceptedSpy[acceptedRowsAboutToBeRemovedSpy] += 2;
                acceptedSpy[acceptedRowsRemovedSpy] += 2;
//                acceptedSpy[accetpedColumnAboutToBeInsertedSpy]++;
//                acceptedSpy[accetpedColumnInsertedSpy]++;
//                acceptedSpy[accetpedColumnAboutToBeRemovedSpy]++;
//                acceptedSpy[accetpedColumnRemovedSpy]++;

                pipelineSpy[rowsInsertedSpy]++;
                pipelineSpy[rowsAboutToBeInsertedSpy]++;

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();
                checkAcceptedRejected();

                SECTION("Update second model") {
                    //Set the type
                    auto groupBy1 = model.index(1).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<cwKeywordGroupByKeyModel*>();
                    REQUIRE(groupBy1);

                    groupBy1->setKey("trip");
                    CHECK(groupBy1->rowCount() == 3);

                    acceptedSpy.checkSpies();
                    pipelineSpy.checkSpies();
                    checkAcceptedRejected();

                    auto groupByFirstIndex = groupBy1->index(0);
                    CHECK(groupByFirstIndex.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "trip2");
                    CHECK(groupByFirstIndex.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);
                    auto groupBy2Index = groupBy1->index(1);
                    CHECK(groupBy2Index.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "trip3");
                    CHECK(groupBy2Index.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);

                    SECTION("Accept only trip2") {
                        CHECK(groupBy1->setData(groupBy2Index, false, cwKeywordGroupByKeyModel::AcceptedRole));
                        CHECK(groupByFirstIndex.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);
                        CHECK(groupBy2Index.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);

                        acceptedObjects = {
                            entity3.get()
                        };

                        rejectedObjects = {
                            entity1.get(),
                            entity2.get(),
                            entity4.get()
                        };

                        acceptedSpy[acceptedRowsAboutToBeRemovedSpy]++;
                        acceptedSpy[acceptedRowsRemovedSpy]++;

                        acceptedSpy.checkSpies();
                        pipelineSpy.checkSpies();
                        checkAcceptedRejected();

                        SECTION("Set OR operator") {
                            qDebug() << "Or";
                            CHECK(model.setData(model.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole));

                            REQUIRE(model.rowCount() == 2);
                            CHECK(model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                            CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);
                            CHECK(model.index(1).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                            CHECK(model.index(1).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::Or);

                            acceptedObjects = {
                                entity1.get(),
                                entity2.get(),
                                entity3.get()
                            };

                            rejectedObjects = {
                                entity4.get()
                            };

                            //Reject type=scraps or accept trip==(trip1 or trip2)

                            checkAcceptedRejected();

                            for(int i = 0; i < model.acceptedModel()->rowCount(); i++) {
                                auto index = model.acceptedModel()->index(i, 0);
                                qDebug() << "Accepted:" << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                            }
                        }
                    }
                }

//                SECTION("Insert row") {

//                }
            }
        }
    }
}
