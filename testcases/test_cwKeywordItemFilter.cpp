//Catch includes
#include "catch.hpp"

//Our includes
#include "cwKeywordItemFilter.h"
#include "SpyChecker.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwKeywordItemModel.h"
#include "cwAsyncFuture.h"

//Qt includes
#include <QFuture>
#include <QSignalSpy>

//Async includes
#include "asyncfuture.h"

//Std includes
#include <algorithm>

class ListElement {
public:
    ListElement() {}
    ListElement(const QString& value, const QVector<QObject*> entities, bool accepted) :
        value(value),
        entities(entities),
        accepted(accepted)
    {}

    QString value;
    QVector<QObject*> entities;
    bool accepted;
};

TEST_CASE("cwKeywordItemFilter should initilize correctly", "[cwKeywordItemModel]") {
    cwKeywordItemFilter filter;
    REQUIRE(filter.rowCount() == 1);
    auto first = filter.index(0);
    CHECK(first.data(cwKeywordItemFilter::ObjectCountRole).toInt() == 0);
    CHECK(first.data(cwKeywordItemFilter::ValueRole).toString().toStdString() == QString("Others").toStdString());

    CHECK(filter.key().isEmpty());
    CHECK(filter.entities().isEmpty());
    CHECK(filter.acceptedEntites().isEmpty());
}

TEST_CASE("cwKeyordItemFilter should categorize objects correctly", "[cwKeywordItemFilter]") {
    auto model = std::make_unique<cwKeywordItemFilter>();

    auto check = [&model](const QModelIndex& root, const QVector<ListElement>& list) {
        REQUIRE(list.size() == model->rowCount(root));
        auto filteredEntities = model->acceptedEntites();

        for(int i = 0; i < model->rowCount(root); i++) {
            INFO("Index:" << i);

            auto index = model->index(i, 0, root);
            auto listElement = list.at(i);
            CHECK(index.data(cwKeywordItemFilter::ValueRole).toString().toStdString() == listElement.value.toStdString());
            CHECK(index.data(cwKeywordItemFilter::AcceptedRole).toBool() == listElement.accepted);

            auto entities = index.data(cwKeywordItemFilter::ObjectsRole).value<QVector<QObject*>>();
            int entitiesCount = index.data(cwKeywordItemFilter::ObjectCountRole).value<int>();
            CHECK(entities.size() == entitiesCount);
            CHECK(entities.size() == listElement.entities.size());

            for(auto entity : listElement.entities) {
                CHECK(entities.contains(entity));
            }

            if(listElement.accepted) {
                //Make sure accepted entites are in the filtered list
                for(auto entity : entities) {
                    auto iter = std::find_if(filteredEntities.begin(), filteredEntities.end(),
                                 [entity](const cwEntityAndKeywords& entityKeywords)
                    {
                        return entity == entityKeywords.entity();
                    });

                    CHECK(iter != filteredEntities.end());
                    if(iter != filteredEntities.end()) {
                        filteredEntities.erase(iter);
                    }
                }
            }
        }

        CHECK(filteredEntities.isEmpty());
    };

    QSignalSpy addSpy(model.get(), &cwKeywordItemFilter::rowsInserted);
    QSignalSpy removeSpy(model.get(), &cwKeywordItemFilter::rowsRemoved);
    QSignalSpy moveSpy(model.get(), &cwKeywordItemFilter::rowsMoved);
    QSignalSpy dataChangedSpy(model.get(), &cwKeywordItemFilter::dataChanged);
    QSignalSpy resetSpy(model.get(), &cwKeywordItemFilter::modelReset);
    QSignalSpy entitiesSpy(model.get(), &cwKeywordItemFilter::entitiesChanged);
    QSignalSpy acceptedSpy(model.get(), &cwKeywordItemFilter::acceptedEntitesChanged);
    QSignalSpy keySpy(model.get(), &cwKeywordItemFilter::keyChanged);

    addSpy.setObjectName("addSpy");
    removeSpy.setObjectName("removeSpy");
    moveSpy.setObjectName("moveSpy");
    dataChangedSpy.setObjectName("dataChangedSpy");
    resetSpy.setObjectName("resetSpy");
    entitiesSpy.setObjectName("entitiesSpy");
    acceptedSpy.setObjectName("accetpedSpy");

    SpyChecker spyCheck {
        {&addSpy, 0},
        {&removeSpy, 0},
        {&moveSpy, 0},
        {&dataChangedSpy, 0},
        {&resetSpy, 0},
        {&entitiesSpy, 0},
        {&acceptedSpy, 0}
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

    auto keywordEntityModel = std::make_unique<cwKeywordItemModel>();
    keywordEntityModel->addItem(component1);
    keywordEntityModel->addItem(component2);
    keywordEntityModel->addItem(component3);
    keywordEntityModel->addItem(component4);

    //CHECK default model
    QVector<ListElement> lists {
        {cwKeywordItemFilter::otherCategory(), {}, false}
    };
    check(QModelIndex(), lists);

    model->setEntities(keywordEntityModel->entityAndKeywords());
    lists = {
        {cwKeywordItemFilter::otherCategory(), {entity1.get(), entity2.get(), entity3.get(), entity4.get()}, false}
    };

    check(QModelIndex(), lists);

    spyCheck[&entitiesSpy]++;
    spyCheck[&dataChangedSpy]++;
    spyCheck.checkSpies();

    SECTION("Set key") {
        model->setKey("type");


        lists = {
            {"line", {entity3.get()}, true},
            {"point", {entity4.get()}, true},
            {"scrap", {entity1.get(), entity2.get()}, true},
            {cwKeywordItemFilter::otherCategory(), {}, false}
        };

        check(QModelIndex(), lists);

        spyCheck[&addSpy] += 3;
        spyCheck[&dataChangedSpy] += 5;
        spyCheck[&acceptedSpy]++;
        spyCheck.checkSpies();

        SECTION("Set to a different key") {
            model->setKey("trip");

            lists = {
                {"trip1", {entity1.get()}, true},
                {"trip2", {entity2.get(), entity3.get()}, true},
                {"trip3", {entity4.get(),}, true},
                {cwKeywordItemFilter::otherCategory(), {}, false}
            };

            check(QModelIndex(), lists);
        }

        SECTION("Set accepted") {
            CHECK(model->setData(model->index(2), false, cwKeywordItemFilter::AcceptedRole));

            lists = {
                {"line", {entity3.get()}, true},
                {"point", {entity4.get()}, true},
                {"scrap", {entity1.get(), entity2.get()}, false},
                {cwKeywordItemFilter::otherCategory(), {}, false}
            };

            check(QModelIndex(), lists);

            SECTION("Set other to true") {
                CHECK(model->setData(model->index(3), true, cwKeywordItemFilter::AcceptedRole));

                lists = {
                    {"line", {entity3.get()}, true},
                    {"point", {entity4.get()}, true},
                    {"scrap", {entity1.get(), entity2.get()}, false},
                    {cwKeywordItemFilter::otherCategory(), {}, true}
                };

                check(QModelIndex(), lists);
            }

            SECTION("Inverting the filter should work correctly") {
                model->invert();

                lists = {
                    {"line", {entity3.get()}, false},
                    {"point", {entity4.get()}, false},
                    {"scrap", {entity1.get(), entity2.get()}, true},
                    {cwKeywordItemFilter::otherCategory(), {}, true}
                };

                check(QModelIndex(), lists);
            }
        }
    }
}
