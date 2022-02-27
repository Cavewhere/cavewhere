//Catch includes
#include "catch.hpp"

//Our includes
#include "cwKeywordGroupByKeyModel.h"
#include "SpyChecker.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwKeywordItemModel.h"
#include "cwAsyncFuture.h"

//Qt includes
#include <QFuture>
#include <QSignalSpy>
#include <QVector>

//Async includes
#include "asyncfuture.h"

//Std includes
#include <algorithm>

namespace test_cwKeywordGroupByKeyModelModel {

class Element {
public:
    Element() = default;
    Element(const QString& key, const QVector<QObject*>& entities, bool accepted) :
        key(key),
        entities(entities),
        accepted(accepted)
    {}

    QString key;
    QVector<QObject*> entities;
    bool accepted;
};
}

using namespace test_cwKeywordGroupByKeyModelModel;

TEST_CASE("cwKeywordGroupByKeyModel should initilize correctly", "[cwKeywordGroupByKeyModel]") {
    cwKeywordGroupByKeyModel filter;
    REQUIRE(filter.rowCount() == 1);
    auto first = filter.index(0);
    CHECK(first.data(cwKeywordGroupByKeyModel::ObjectCountRole).toInt() == 0);
    CHECK(first.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == QString("Others").toStdString());

    CHECK(filter.key().isEmpty());
    CHECK(filter.acceptedModel()->rowCount() == 0);
}

TEST_CASE("cwKeyordItemKeyFilter should categorize objects correctly", "[cwKeywordGroupByKeyModel]") {
    auto model = std::make_unique<cwKeywordGroupByKeyModel>();

    auto check = [&model](const QModelIndex& root, const QVector<Element>& list) {
        REQUIRE(list.size() == model->rowCount(root));

        for(int i = 0; i < model->rowCount(root); i++) {

            auto index = model->index(i, 0, root);
            auto listElement = list.at(i);

            INFO("Index:" << i << " " << listElement.key.toStdString());

            CHECK(index.data(cwKeywordGroupByKeyModel::ValueRole).toString().toStdString() == listElement.key.toStdString());
            CHECK(index.data(cwKeywordGroupByKeyModel::AcceptedRole).toBool() == listElement.accepted);

            auto entities = index.data(cwKeywordGroupByKeyModel::ObjectsRole).value<QVector<QObject*>>();
            int entitiesCount = index.data(cwKeywordGroupByKeyModel::ObjectCountRole).value<int>();
            CHECK(entities.size() == entitiesCount);
            CHECK(entities.size() == listElement.entities.size());

            for(auto entity : listElement.entities) {
                CHECK(entities.contains(entity));
            }

            auto hasEntity = [&model](QObject* entity) {
                for(int i = 0; i < model->acceptedModel()->rowCount(); i++) {
                    auto acceptedIndex = model->acceptedModel()->index(i);
                    if(acceptedIndex.data(cwKeywordItemModel::ObjectRole).value<QObject*>() == entity) {
                        return true;
                    }
                }
                return false;
            };

            for(auto entity : entities) {
                INFO("Entity:" << entity << " " << entity->objectName().toStdString());
                CHECK(listElement.accepted == hasEntity(entity));
            }
        }
    };

    auto spyCheck = SpyChecker::makeChecker(model.get());

    auto addSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::rowsInserted);
    auto removeSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::rowsRemoved);
    auto moveSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::rowsMoved);
    auto dataChangedSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::dataChanged);
    auto resetSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::modelReset);
    auto aboutToResetSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::modelAboutToBeReset);
    auto keySpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::keyChanged);
    auto sourceSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::sourceChanged);

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
    QVector<Element> lists {
        {cwKeywordGroupByKeyModel::otherCategory(), {}, true}
    };
    check(QModelIndex(), lists);

    model->setSourceModel(keywordEntityModel.get());
    lists = {
        //This other should be true when type isn't set
        {cwKeywordGroupByKeyModel::otherCategory(), {entity1.get(), entity2.get(), entity3.get(), entity4.get()}, true}
    };

    check(QModelIndex(), lists);

    spyCheck[resetSpy]++;
    spyCheck[aboutToResetSpy]++;
    spyCheck[sourceSpy]++;
    spyCheck.checkSpies();

    SECTION("Set key") {
        model->setKey("type");

        lists = {
            {"line", {entity3.get()}, true},
            {"point", {entity4.get()}, true},
            {"scrap", {entity1.get(), entity2.get()}, true},
            {cwKeywordGroupByKeyModel::otherCategory(), {}, false}
        };

        check(QModelIndex(), lists);

        spyCheck[resetSpy]++;
        spyCheck[aboutToResetSpy]++;
        spyCheck[keySpy]++;
        spyCheck[dataChangedSpy]++;
        spyCheck.checkSpies();

        SECTION("Set to a different key") {
            model->setKey("trip");

            lists = {
                {"trip1", {entity1.get()}, true},
                {"trip2", {entity2.get(), entity3.get()}, true},
                {"trip3", {entity4.get(),}, true},
                {cwKeywordGroupByKeyModel::otherCategory(), {}, false}
            };

            check(QModelIndex(), lists);
        }

        SECTION("Set accepted") {
            CHECK(model->setData(model->index(2), false, cwKeywordGroupByKeyModel::AcceptedRole));

            lists = {
                {"line", {entity3.get()}, true},
                {"point", {entity4.get()}, true},
                {"scrap", {entity1.get(), entity2.get()}, false},
                {cwKeywordGroupByKeyModel::otherCategory(), {}, false}
            };

            check(QModelIndex(), lists);

            SECTION("Set other to true") {
                CHECK(model->setData(model->index(3), true, cwKeywordGroupByKeyModel::AcceptedRole));

                lists = {
                    {"line", {entity3.get()}, true},
                    {"point", {entity4.get()}, true},
                    {"scrap", {entity1.get(), entity2.get()}, false},
                    {cwKeywordGroupByKeyModel::otherCategory(), {}, true}
                };

                check(QModelIndex(), lists);
            }

            SECTION("Inverting the filter should work correctly") {
                model->invert();

                lists = {
                    {"line", {entity3.get()}, false},
                    {"point", {entity4.get()}, false},
                    {"scrap", {entity1.get(), entity2.get()}, true},
                    {cwKeywordGroupByKeyModel::otherCategory(), {}, true}
                };

                check(QModelIndex(), lists);
            }
        }

        SECTION("Replace should work correctly") {
            keywordModel3->replace({"type", "scrap"});

            lists = {
                {"point", {entity4.get()}, true},
                {"scrap", {entity1.get(), entity2.get(), entity3.get()}, true},
                {cwKeywordGroupByKeyModel::otherCategory(), {}, false}
            };

            check(QModelIndex(), lists);
        }
    }
}
