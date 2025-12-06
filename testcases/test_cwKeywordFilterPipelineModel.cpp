//Our includes
#include "cwKeywordFilterPipelineModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordGroupByKeyModel.h"
#include "cwUniqueValueFilterModel.h"
#include "cwKeywordFilterGroupProxyModel.h"
#include "cwKeywordFilterOrGroupProxyModel.h"
#include "SignalSpyChecker.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Std includes
#include <vector>

TEST_CASE("cwKeywordFilterPipelineModel should initilize correctly", "[cwKeywordFilterPipelineModel]") {
    cwKeywordFilterPipelineModel model;
    REQUIRE(model.rowCount() == 1);
    CHECK(model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
    CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);

    REQUIRE(model.acceptedModel() != nullptr);
    CHECK(model.acceptedModel()->rowCount() == 0);
}

TEST_CASE("cwKeywordFilterPipelineModel clear resets to defaults", "[cwKeywordFilterPipelineModel]") {
    cwKeywordFilterPipelineModel model;
    auto keywordModel = std::make_unique<cwKeywordItemModel>();

    auto item = new cwKeywordItem();
    item->keywordModel()->add({"type", "scrap"});
    item->setObject(new QObject(&model));
    keywordModel->addItem(item);

    model.setKeywordModel(keywordModel.get());
    model.addRow();
    REQUIRE(model.rowCount() == 2);
    REQUIRE(model.acceptedModel()->rowCount() == 1);

    model.clear();

    CHECK(model.rowCount() == 1);
    auto filterModel = model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>();
    CHECK(filterModel != nullptr);
    CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);

    //I'm not sure if this should be 0 or 1
    CHECK(model.acceptedModel()->rowCount() == 1);
}

TEST_CASE("cwKeywordFilterPipelineModel filter correctly", "[cwKeywordFilterPipelineModel]") {
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

    auto pipelineSpy = SignalSpyChecker::Constant::makeChecker(&model);
    auto acceptedSpy = SignalSpyChecker::Constant::makeChecker(model.acceptedModel());

    model.setKeywordModel(keywordEntityModel.get());

    auto keywordModelSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::keywordModelChanged);
    auto rowsInsertedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsInserted);
    auto rowsAboutToBeInsertedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeInserted);
    auto rowsRemovedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeRemoved);
    auto rowsAboutToBeRemovedSpy = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeRemoved);
    auto possibleKeysChanged = pipelineSpy.findSpy(&cwKeywordFilterPipelineModel::possibleKeysChanged);

    auto acceptedRowsInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsInserted);
    auto acceptedRowsAboutToBeInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeInserted);
    auto acceptedRowsRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsRemoved);
    auto acceptedRowsAboutToBeRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::rowsAboutToBeRemoved);
    auto accetpedColumnInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsInserted);
    auto accetpedColumnAboutToBeInsertedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsAboutToBeInserted);
    auto accetpedColumnRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsRemoved);
    auto accetpedColumnAboutToBeRemovedSpy = acceptedSpy.findSpy(&cwKeywordFilterPipelineModel::columnsAboutToBeRemoved);

    pipelineSpy[keywordModelSpy]++;
    pipelineSpy[possibleKeysChanged]++;
    acceptedSpy[acceptedRowsInsertedSpy] = 4;
    acceptedSpy[acceptedRowsAboutToBeInsertedSpy] = 4;

    pipelineSpy.checkSpies();
    acceptedSpy.checkSpies();

    QVector<QObject*> acceptedObjects;
    QVector<QObject*> rejectedObjects;
    auto checkAcceptedRejected = [&acceptedObjects, &rejectedObjects, &model]() {

        auto findInModel = [](QObject* obj, QAbstractItemModel* model) {
            for(int i = 0; i < model->rowCount(); i++) {
                // qDebug() << "findInModel:" << (model->index(i, 0).data(cwKeywordItemModel::ObjectRole).value<QObject*>() == obj) << model->index(i, 0).data(cwKeywordItemModel::ObjectRole).value<QObject*>() << obj;
                if(model->index(i, 0).data(cwKeywordItemModel::ObjectRole).value<QObject*>() == obj) {
                    return true;
                }
            }
            return false;
        };

        CHECK(model.acceptedModel()->rowCount() == acceptedObjects.size());
        // qDebug() << "Check accepted:";
        for(auto obj : acceptedObjects) {
            INFO("Obj:" << obj->objectName().toStdString());
            CHECK(findInModel(obj, model.acceptedModel()));
        }

        // qDebug() << "Check rejected:";
        CHECK(model.rejectedModel()->rowCount() == rejectedObjects.size());
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

    SECTION("Possible keys") {
        CHECK(model.possibleKeys() == QStringList({"cave", "trip", "type"}));
    }

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

                // qDebug() << "Adding rejected" << component5;
                keywordEntityModel->addItem(component5);

                rejectedObjects.append(entity5.get());

                pipelineSpy[possibleKeysChanged]++;

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
                pipelineSpy[possibleKeysChanged]++;

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Change keywordModel from accepted to rejected") {
                // qDebug() << "To scrap, reject";
                keywordModel4->replace({"type", "scrap"});
                // qDebug() << "Done";

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
                // qDebug() << "Removing component3";
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
                pipelineSpy[possibleKeysChanged]++;

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

                pipelineSpy[possibleKeysChanged]++;

                acceptedSpy.checkSpies();
                pipelineSpy.checkSpies();

                checkAcceptedRejected();
            }

            SECTION("Add row") {
                //Make sure we're and'ing the new row
                // qDebug() << "Add row";
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
                        //By !Scrap && !Trip2
                        //keywordModel1->add({"type", "scrap"}); //false
                        //keywordModel1->add({"trip", "trip1"}); //true
                        //
                        //keywordModel2->add({"type", "scrap"}); //false
                        //keywordModel2->add({"trip", "trip2"}); //true
                        //keywordModel2->add({"cave", "cave1"}); //-
                        //
                        //keywordModel3->add({"type", "line"}); //true
                        //keywordModel3->add({"trip", "trip2"}); //true
                        //keywordModel3->add({"cave", "cave1"}); //-
                        //
                        //keywordModel4->add({"type", "point"}); //true
                        //keywordModel4->add({"trip", "trip3"}); //false
                        //keywordModel4->add({"cave", "cave2"});


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
                            //By !Scrap || !Trip2
                            //keywordModel1->add({"type", "scrap"}); //false
                            //keywordModel1->add({"trip", "trip1"}); //true
                            //
                            //keywordModel2->add({"type", "scrap"}); //false
                            //keywordModel2->add({"trip", "trip2"}); //true
                            //keywordModel2->add({"cave", "cave1"}); //-
                            //
                            //keywordModel3->add({"type", "line"}); //true
                            //keywordModel3->add({"trip", "trip2"}); //true
                            //keywordModel3->add({"cave", "cave1"}); //-
                            //
                            //keywordModel4->add({"type", "point"}); //true
                            //keywordModel4->add({"trip", "trip3"}); //false
                            //keywordModel4->add({"cave", "cave2"});

                            // for(int i = 0; i < model.acceptedModel()->rowCount(); i++) {
                            //     auto index = model.acceptedModel()->index(i, 0);
                            //     qDebug() << "Accepted:" << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                            // }
                            // for(int i = 0; i < model.rejectedModel()->rowCount(); i++) {
                            //     auto index = model.rejectedModel()->index(i, 0);
                            //     qDebug() << "Rejected:" << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                            // }

                            // dynamic_cast<cwUniqueValueFilterModel*>(model.acceptedModel())->printRows();

                            // qDebug() << "Or";
                            CHECK(model.setData(model.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole));

                            // dynamic_cast<cwUniqueValueFilterModel*>(model.acceptedModel())->printRows();

                            REQUIRE(model.rowCount() == 2);
                            CHECK(model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                            CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);
                            CHECK(model.index(1).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                            CHECK(model.index(1).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::Or);

                            acceptedObjects = {
                                entity3.get(),
                                entity4.get()
                            };

                            rejectedObjects = {
                                entity1.get(),
                                entity2.get(),
                            };

                            checkAcceptedRejected();

                            auto typeGroupBy0 = groupBy->index(0);
                            CHECK(typeGroupBy0.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "line");
                            CHECK(typeGroupBy0.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);
                            auto typeGroupBy1 = groupBy->index(1);
                            CHECK(typeGroupBy1.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "point");
                            CHECK(typeGroupBy1.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);
                            auto typeGroupBy2 = groupBy->index(2);
                            CHECK(typeGroupBy2.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "scrap");
                            CHECK(typeGroupBy2.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);


                            auto tripGroupBy0 = groupBy1->index(0);
                            CHECK(tripGroupBy0.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "trip1");
                            CHECK(tripGroupBy0.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);
                            auto tripGroupBy1 = groupBy1->index(1);
                            CHECK(tripGroupBy1.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "trip2");
                            CHECK(tripGroupBy1.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);
                            auto tripGroupBy2 = groupBy1->index(2);
                            CHECK(tripGroupBy2.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == "trip3");
                            CHECK(tripGroupBy2.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);


                            // for(int i = 0; i < model.acceptedModel()->rowCount(); i++) {
                            //     auto index = model.acceptedModel()->index(i, 0);
                            //     qDebug() << "Accepted:" << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                            // }
                            // for(int i = 0; i < model.rejectedModel()->rowCount(); i++) {
                            //     auto index = model.rejectedModel()->index(i, 0);
                            //     qDebug() << "Rejected:" << index.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
                            // }

                            SECTION("Enabled entry1 and entry 2") {
                                // Accept trip1 and trip2 in the OR group to bring scrap rows back in.
                                CHECK(groupBy1->setData(tripGroupBy0, true, cwKeywordGroupByKeyModel::AcceptedRole));
                                CHECK(groupBy1->setData(tripGroupBy1, true, cwKeywordGroupByKeyModel::AcceptedRole));

                                CHECK(tripGroupBy0.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);
                                CHECK(tripGroupBy1.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == true);
                                CHECK(tripGroupBy2.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);

                                acceptedObjects = {
                                    entity1.get(),
                                    entity2.get(),
                                    entity3.get(),
                                    entity4.get()
                                };
                                rejectedObjects.clear();

                                checkAcceptedRejected();
                            }

                            SECTION("Set Or operator a second time") {
                                CHECK(model.setData(model.index(1), cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole) == false);
                            }

                            SECTION("Set back to AND opertar") {
                                //By !Scrap && !Trip2
                                //keywordModel1->add({"type", "scrap"}); //false
                                //keywordModel1->add({"trip", "trip1"}); //true
                                //
                                //keywordModel2->add({"type", "scrap"}); //false
                                //keywordModel2->add({"trip", "trip2"}); //true
                                //keywordModel2->add({"cave", "cave1"}); //-
                                //
                                //keywordModel3->add({"type", "line"}); //true
                                //keywordModel3->add({"trip", "trip2"}); //true
                                //keywordModel3->add({"cave", "cave1"}); //-
                                //
                                //keywordModel4->add({"type", "point"}); //true
                                //keywordModel4->add({"trip", "trip3"}); //true
                                //keywordModel4->add({"cave", "cave2"});

                                CHECK(model.setData(model.index(1), cwKeywordFilterPipelineModel::And, cwKeywordFilterPipelineModel::OperatorRole));

                                REQUIRE(model.rowCount() == 2);
                                CHECK(model.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                                CHECK(model.index(0).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);
                                CHECK(model.index(1).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<QAbstractItemModel*>() != nullptr);
                                CHECK(model.index(1).data(cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::And);

                                acceptedObjects = {
                                    entity3.get(),
                                    entity4.get()
                                };

                                rejectedObjects = {
                                    entity1.get(),
                                    entity2.get()
                                };

                                checkAcceptedRejected();
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("cwKeywordFilterPipelineModel OR rows start unchecked", "[cwKeywordFilterPipelineModel]") {
    cwKeywordFilterPipelineModel pipeline;
    auto keywordModel = std::make_unique<cwKeywordItemModel>();

    std::vector<std::unique_ptr<QObject>> objects;

    auto addItem = [&](const QString& type, const QString& trip) {
        auto item = new cwKeywordItem();
        auto obj = std::make_unique<QObject>();
        obj->setObjectName(type + "_" + trip);

        item->keywordModel()->add({"type", type});
        item->keywordModel()->add({"trip", trip});
        item->setObject(obj.get());

        keywordModel->addItem(item);
        objects.push_back(std::move(obj));
    };

    addItem("scrap", "trip1");
    addItem("line", "trip2");

    pipeline.setKeywordModel(keywordModel.get());

    auto firstGroup = pipeline.index(0).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<cwKeywordGroupByKeyModel*>();
    REQUIRE(firstGroup);
    firstGroup->setKey("type");

    pipeline.addRow();

    auto secondGroup = pipeline.index(1).data(cwKeywordFilterPipelineModel::FilterModelObjectRole).value<cwKeywordGroupByKeyModel*>();
    REQUIRE(secondGroup);
    secondGroup->setKey("type");

    // Switch the second row to OR and ensure everything becomes unchecked
    auto orIndex = pipeline.index(1);
    REQUIRE(orIndex.isValid());
    REQUIRE(pipeline.setData(orIndex, cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole));

    // Re-apply the key to rebuild rows with the new default (simulates user having a key selected)
    secondGroup->setKey("type");

    CHECK(secondGroup->acceptByDefault() == false);
    //ignore other
    for(int i = 0; i < secondGroup->rowCount() - 1; i++) {
        INFO("i:" << i << secondGroup->index(i, 0).data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString());
        CHECK(secondGroup->index(i, 0).data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == false);
    }
}

TEST_CASE("cwKeywordFilterPipelineModel removing first row of middle group keeps or groups", "[cwKeywordFilterPipelineModel]") {
    // Mirrors test-qml/tst_KeywordTab.qml::test_hideRemoveSecondOr without UI clicks.
    cwKeywordFilterPipelineModel pipeline;
    auto keywordModel = std::make_unique<cwKeywordItemModel>();

    auto item = new cwKeywordItem();
    item->keywordModel()->add({"type", "scrap"});
    auto object = std::make_unique<QObject>();
    item->setObject(object.get());
    keywordModel->addItem(item);

    pipeline.setKeywordModel(keywordModel.get());

    cwKeywordFilterOrGroupProxyModel orProxy;
    orProxy.setSourceModel(&pipeline);

    REQUIRE(pipeline.rowCount() == 1);
    CHECK(orProxy.rowCount() == 1);

    auto setOrOperator = [&pipeline](int row) {
        auto idx = pipeline.index(row);
        REQUIRE(idx.isValid());
        REQUIRE(pipeline.setData(idx, cwKeywordFilterPipelineModel::Or, cwKeywordFilterPipelineModel::OperatorRole));
        REQUIRE(pipeline.data(idx, cwKeywordFilterPipelineModel::OperatorRole).toInt() == cwKeywordFilterPipelineModel::Or);
    };

    // Create two OR boundaries (three groups)
    pipeline.addRow();
    setOrOperator(1);
    pipeline.addRow();
    setOrOperator(2);

    REQUIRE(pipeline.rowCount() == 3);
    CHECK(orProxy.rowCount() == 3);

    // Add another row inside the second group (not the last)
    pipeline.insertRow(2);
    REQUIRE(pipeline.rowCount() == 4);
    CHECK(orProxy.rowCount() == 3);

    // Remove the first row in the second group
    pipeline.removeRow(1);
    REQUIRE(pipeline.rowCount() == 3);

    // The OR-group list should remain at three groups
    CHECK(orProxy.rowCount() == 3);

    // Each group should still expose at least one row
    cwKeywordFilterGroupProxyModel group0;
    group0.setSourceModel(&pipeline);
    group0.setGroupIndex(0);

    cwKeywordFilterGroupProxyModel group1;
    group1.setSourceModel(&pipeline);
    group1.setGroupIndex(1);

    cwKeywordFilterGroupProxyModel group2;
    group2.setSourceModel(&pipeline);
    group2.setGroupIndex(2);

    CHECK(group0.rowCount() >= 1);
    CHECK(group1.rowCount() >= 1);
    CHECK(group2.rowCount() >= 1);
}
