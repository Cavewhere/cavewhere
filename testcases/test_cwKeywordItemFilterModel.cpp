//Catch includes
#include "catch.hpp"

//Our includes
#include "SpyChecker.h"
#include "cwKeywordItemFilterModel.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"

//Qt includes
#include <QSignalSpy>
#include <QEntity>

using namespace Qt3DCore;

class ListElement {
public:
    ListElement() {}
    ListElement(const QString& value, const QVector<QEntity*> entities) :
        value(value),
        entities(entities)
    {}

    QString value;
    QVector<QEntity*> entities;
};



TEST_CASE("cwKeywordItemFilterModel should initilize correctly", "[cwKeywordItemFilterModel]") {
    cwKeywordItemFilterModel model;
    CHECK(model.rowCount(QModelIndex()) == 1);
    CHECK(model.keywordModel() == nullptr);
    CHECK(model.keywords().size() == 0);
    CHECK(model.lastKey().isEmpty());
    CHECK(model.otherCategory().isEmpty() == false);
}

TEST_CASE("cwKeywordItemFilterModel should initilize correctly with keys", "[cwKeywordItemFilterModel]") {

    auto model = std::make_unique<cwKeywordItemFilterModel>();

    auto check = [&model](const QModelIndex& root, const QVector<ListElement>& list) {        
        REQUIRE(list.size() == model->rowCount(root));
        for(int i = 0; i < model->rowCount(root); i++) {
            INFO("Index:" << i);

            auto index = model->index(i, 0, root);
            auto listElement = list.at(i);
            CHECK(index.data(cwKeywordItemFilterModel::ValueRole).toString().toStdString() == listElement.value.toStdString());

            auto entities = index.data(cwKeywordItemFilterModel::ObjectsRole).value<QList<QObject*>>();
            int entitiesCount = index.data(cwKeywordItemFilterModel::ObjectCountRole).value<int>();
            CHECK(entities.size() == entitiesCount);
            CHECK(entities.size() == listElement.entities.size());

            for(auto entity : listElement.entities) {
                CHECK(entities.contains(entity));
            }
        }
    };

    auto keywordEntityModel = std::make_unique<cwKeywordItemModel>();

    QSignalSpy addSpy(model.get(), &cwKeywordItemFilterModel::rowsInserted);
    QSignalSpy removeSpy(model.get(), &cwKeywordItemFilterModel::rowsRemoved);
    QSignalSpy moveSpy(model.get(), &cwKeywordItemFilterModel::rowsMoved);
    QSignalSpy dataChangedSpy(model.get(), &cwKeywordItemFilterModel::dataChanged);
    QSignalSpy resetSpy(model.get(), &cwKeywordItemFilterModel::modelReset);
    QSignalSpy possibleKeysSpy(model.get(), &cwKeywordItemFilterModel::possibleKeysChanged);

    addSpy.setObjectName("addSpy");
    removeSpy.setObjectName("removeSpy");
    moveSpy.setObjectName("moveSpy");
    dataChangedSpy.setObjectName("dataChangedSpy");
    resetSpy.setObjectName("resetSpy");
    possibleKeysSpy.setObjectName("possibleKeysSpy");

    SpyChecker spyCheck {
        {&addSpy, 0},
        {&removeSpy, 0},
        {&moveSpy, 0},
        {&dataChangedSpy, 0},
        {&resetSpy, 0},
        {&possibleKeysSpy, 0}
    };

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

    auto entity1 = std::make_unique<QEntity>();
    auto entity2 = std::make_unique<QEntity>();
    auto entity3 = std::make_unique<QEntity>();
    auto entity4 = std::make_unique<QEntity>();

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

    //CHECK default model
    QVector<ListElement> lists {
        {cwKeywordItemFilterModel::otherCategory(), {}}
    };
    check(QModelIndex(), lists);

    model->setKeywordModel(keywordEntityModel.get());
    model->waitForFinished();

    CHECK(model->possibleKeys() == QStringList({"cave", "trip", "type"}));

    CHECK(model->rowCount(QModelIndex()) == 1);

    lists = {
        {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity2.get(), entity3.get(), entity4.get()}}
    };

    check(QModelIndex(), lists);

    spyCheck[&resetSpy] = 1;
    spyCheck[&possibleKeysSpy] = 1;
    spyCheck.checkSpies();
    spyCheck.clearSpyCounts();

    SECTION("Add single key") {
        model->setLastKey("type");
        model->waitForFinished();

        QVector<ListElement> modelData {
            {"line", {entity3.get()}},
            {"point", {entity4.get()}},
            {"scrap", {entity1.get(), entity2.get()}},
            {cwKeywordItemFilterModel::otherCategory(), {}},
        };

        check(QModelIndex(), modelData);

        spyCheck[&resetSpy] = 1;
        spyCheck.checkSpies();
        spyCheck.clearSpyCounts();


        SECTION("Same last key") {
            model->setLastKey("type");
            model->waitForFinished();

            check(QModelIndex(), modelData);

            //Should have no emit data, since model hasn't changed
            spyCheck.checkSpies();
            spyCheck.clearSpyCounts();
        }

        SECTION("Switch last key multiple times") {
            //This testcase depends on this being fixed: https://github.com/benlau/asyncfuture/issues/13
            model->setLastKey("cave");
            model->setLastKey("unknown");
            model->setLastKey("trip");

            QVector<ListElement> modelData {
                {"trip1", {entity1.get()}},
                {"trip2", {entity2.get(), entity3.get()}},
                {"trip3", {entity4.get()}},
                {cwKeywordItemFilterModel::otherCategory(), {}},
            };

            model->waitForFinished();

            check(QModelIndex(), modelData);
        }

        SECTION("Switch to an unknown key") {
            model->setLastKey("unknownKey");

            model->waitForFinished();

            QVector<ListElement> modelData {
                {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity2.get(), entity3.get(), entity4.get()}},
            };

            check(QModelIndex(), modelData);

            spyCheck[&resetSpy] = 1;
            spyCheck.checkSpies();
            spyCheck.clearSpyCounts();
        }

        SECTION("Second level") {
            model->setKeywords({
                                  {"cave", "cave1"}
                               });

            modelData = {
                {"line", {entity3.get()}},
                {"scrap", {entity2.get()}},
                {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get()}},
            };

            //Spies should be zero
            spyCheck.checkSpies();

            model->waitForFinished();

            CHECK(model->possibleKeys() == QStringList({"trip", "type"}));

            check(QModelIndex(), modelData);

            spyCheck[&resetSpy] = 1;
            spyCheck[&possibleKeysSpy] = 1;
            spyCheck.checkSpies();
            spyCheck.clearSpyCounts();

            SECTION("Add keyword") {
                keywordModel1->add(cwKeyword({"cave", "cave1"}));

                modelData = {
                    {"line", {entity3.get()}},
                    {"scrap", {entity2.get(), entity1.get()}},
                    {cwKeywordItemFilterModel::otherCategory(), {entity4.get()}},
                };

                check(QModelIndex(), modelData);

                spyCheck[&dataChangedSpy] = 2;
                spyCheck.requireSpies();

                auto index1 = dataChangedSpy.at(0).at(0).value<QModelIndex>();
                auto index2 = dataChangedSpy.at(0).at(1).value<QModelIndex>();
                auto roles = dataChangedSpy.at(0).at(2).value<QVector<int>>();

                CHECK(index1 == index2);
                CHECK(index1.isValid() == true);
                CHECK(index2.isValid() == true);

                CHECK(roles.size() == 2);
                CHECK(roles.contains(cwKeywordItemFilterModel::ObjectsRole) == true);
                CHECK(roles.contains(cwKeywordItemFilterModel::ObjectCountRole) == true);

                auto otherIndex = dataChangedSpy.at(1).at(0).value<QModelIndex>();
                CHECK(otherIndex.row() == model->rowCount() - 1);

                spyCheck.clearSpyCounts();
            }

            SECTION("Add multikey") {
                keywordModel2->add(cwKeyword({"type", "line"}));

                modelData = {
                    {"line", {entity3.get(), entity2.get()}},
                    {"scrap", {entity2.get()}},
                    {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get()}},
                };

                check(QModelIndex(), modelData);

                spyCheck[&dataChangedSpy] = 1;
                spyCheck.requireSpies();
                spyCheck.clearSpyCounts();

                SECTION("Removed keyword") {
                    keywordModel2->remove(cwKeyword({"type", "line"}));

                    modelData = {
                        {"line", {entity3.get()}},
                        {"scrap", {entity2.get()}},
                        {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get()}},
                    };

                    check(QModelIndex(), modelData);

                    spyCheck[&dataChangedSpy] = 1;
                    spyCheck.checkSpies();
                    spyCheck.clearSpyCounts();

                    SECTION("Remove last keyword") {
                        keywordModel2->remove(cwKeyword({"type", "scrap"}));

                        modelData = {
                            {"line", {entity3.get()}},
                            {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get(), entity2.get()}},
                        };

                        check(QModelIndex(), modelData);

                        spyCheck[&dataChangedSpy] = 1; //Other row
                        spyCheck[&removeSpy] = 1;
                        spyCheck.checkSpies();
                        spyCheck.clearSpyCounts();
                    }
                }
            }

            SECTION("Change value in keyword") {
                auto firstIndex = keywordModel2->index(0);
                keywordModel2->setData(firstIndex, "line", cwKeywordModel::ValueRole);

                modelData = {
                    {"line", {entity3.get(), entity2.get()}},
                    {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get()}},
                };

                check(QModelIndex(), modelData);

                spyCheck[&dataChangedSpy] = 1;
                spyCheck[&removeSpy] = 1;
                spyCheck.requireSpies();
                spyCheck.clearSpyCounts();
            }

            SECTION("Change key in keyword") {
                auto firstIndex = keywordModel2->index(0);
                keywordModel2->setData(firstIndex, "type2", cwKeywordModel::KeyRole);

                modelData = {
                    {"line", {entity3.get()}},
                    {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get(), entity2.get()}},
                };

                check(QModelIndex(), modelData);

                spyCheck[&dataChangedSpy] = 1;
                spyCheck[&removeSpy] = 1;
                spyCheck.requireSpies();
                spyCheck.clearSpyCounts();
            }

            SECTION("Add entity") {
                auto component5 = new cwKeywordItem();
                component5->setObjectName("component5");

                cwKeywordModel* keywordModel5 = component5->keywordModel();
                keywordModel5->setObjectName("keywordModel5");

                keywordModel5->add({"type", "point"});
                keywordModel5->add({"trip", "trip3"});
                keywordModel5->add({"cave", "cave1"});

                auto entity5 = std::make_unique<QEntity>();
                entity5->setObjectName("entity5");

                component5->setObject(entity5.get());

                keywordEntityModel->addItem(component5);

                modelData = {
                    {"line", {entity3.get()}},
                    {"point", {entity5.get()}},
                    {"scrap", {entity2.get()}},
                    {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get()}},
                };

                check(QModelIndex(), modelData);

                spyCheck[&addSpy] = 1;
                spyCheck.checkSpies();
                spyCheck.clearSpyCounts();
            }

            SECTION("Removed Entity") {
                keywordEntityModel->removeItem(component2);

                modelData = {
                    {"line", {entity3.get()}},
                    {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity4.get()}},
                };

                spyCheck[&removeSpy] = 1;
                spyCheck.checkSpies();
                spyCheck.clearSpyCounts();
            }
        }

        SECTION("Third level") {
            model->setKeywords({
                                  {"cave", "cave2"},
                                  {"trip", "trip3"}
                               });

            modelData = {
                {"point", {entity4.get()}},
                {cwKeywordItemFilterModel::otherCategory(), {entity1.get(), entity2.get(), entity3.get()}},
            };

            //Spies should be zero
            spyCheck.checkSpies();

            model->waitForFinished();

            check(QModelIndex(), modelData);

            spyCheck[&resetSpy] = 1;
            spyCheck[&possibleKeysSpy] = 1;
            spyCheck.checkSpies();
            spyCheck.clearSpyCounts();
        }
    }
}


