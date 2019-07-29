//Catch includes
#include "catch.hpp"

//Our includes
#include "SpyChecker.h"
#include "cwKeywordEntityFilterModel.h"
#include "cwKeywordEntityModel.h"
#include "cwKeywordComponent.h"
#include "cwKeywordModel.h"

//Qt includes
#include <QSignalSpy>
#include <QEntity>

using namespace Qt3DCore;

TEST_CASE("cwKeywordEntityFilterModel should initilize correctly", "[cwKeywordEntityFilterModel]") {
    cwKeywordEntityFilterModel model;
    CHECK(model.rowCount(QModelIndex()) == 0);
    CHECK(model.keywordModel() == nullptr);
    CHECK(model.keys().isEmpty());
}

TEST_CASE("cwKeywordEntityFilterModel should initilize correctly with keys", "[cwKeywordEntityFilterModel]") {

    auto model = std::make_unique<cwKeywordEntityFilterModel>();

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

    auto check = [&model](const QModelIndex& root, const QVector<ListElement>& list) {
        REQUIRE(list.size() == model->rowCount(root));
        for(int i = 0; i < model->rowCount(root); i++) {
            auto index = model->index(i, 0, root);
            auto listElement = list.at(i);
            INFO("Index:" << i);
            CHECK(index.data(cwKeywordEntityFilterModel::ValueRole).toString().toStdString() == listElement.value.toStdString());

            auto entities = index.data(cwKeywordEntityFilterModel::EntitiesRole).value<QVector<QEntity*>>();
            int entitiesCount = index.data(cwKeywordEntityFilterModel::EntitiesCountRole).value<int>();
            CHECK(entities.size() == entitiesCount);
            CHECK(entities.size() == listElement.entities.size());

            for(auto entity : listElement.entities) {
                CHECK(entities.contains(entity));
            }
        }
    };

    auto keywordEntityModel = std::make_unique<cwKeywordEntityModel>();

    QSignalSpy addSpy(model.get(), &cwKeywordEntityFilterModel::rowsInserted);
    QSignalSpy removeSpy(model.get(), &cwKeywordEntityFilterModel::rowsRemoved);
    QSignalSpy moveSpy(model.get(), &cwKeywordEntityFilterModel::rowsMoved);
    QSignalSpy dataChangedSpy(model.get(), &cwKeywordEntityFilterModel::dataChanged);
    QSignalSpy resetSpy(model.get(), &cwKeywordEntityFilterModel::modelReset);

    addSpy.setObjectName("addSpy");
    removeSpy.setObjectName("removeSpy");
    moveSpy.setObjectName("moveSpy");
    dataChangedSpy.setObjectName("dataChangedSpy");

    SpyChecker checker {
        {&addSpy, 0},
        {&removeSpy, 0},
        {&moveSpy, 0},
        {&dataChangedSpy, 0},
    };

    auto component1 = new cwKeywordComponent();
    auto component2 = new cwKeywordComponent();
    auto component3 = new cwKeywordComponent();
    auto component4 = new cwKeywordComponent();

    component1->setObjectName("component1");
    component2->setObjectName("component2");
    component3->setObjectName("component3");
    component4->setObjectName("component3");

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

    entity1->addComponent(component1);
    entity2->addComponent(component2);
    entity3->addComponent(component3);
    entity4->addComponent(component4);

    keywordEntityModel->addComponent(component1);
    keywordEntityModel->addComponent(component2);
    keywordEntityModel->addComponent(component3);
    keywordEntityModel->addComponent(component4);

    model->setKeywordModel(keywordEntityModel.get());

    CHECK(model->rowCount(QModelIndex()) == 1);

    QVector<ListElement> lists {
        {"Everything Else", {entity1.get(), entity2.get(), entity3.get(), entity4.get()}}
    };

    check(QModelIndex(), lists);

    SECTION("Add single key") {
        model->addKey("type");

        QVector<ListElement> firstDepth {
            {"Everything Else", {}},
            {"line", {entity3.get()}},
            {"point", {entity4.get()}},
            {"scrap", {entity1.get(), entity2.get()}},
        };

        check(QModelIndex(), firstDepth);

        SECTION("Second level") {
            model->addKey("cave");

            QVector<ListElement> secondDepth {
                {"Everything Else", {}},
                {"line", {entity3.get()}},
                {"point", {entity4.get()}},
                {"scrap", {entity1.get(), entity2.get()}},
            };

            QVector<ListElement> firstDepth {
                {"Everything Else", {}},
                {"line", {entity3.get()}},
                {"point", {entity4.get()}},
                {"scrap", {entity1.get(), entity2.get()}},
            };

            QVector<ListElement> firstDepth {
                {"Everything Else", {}},
                {"line", {entity3.get()}},
                {"point", {entity4.get()}},
                {"scrap", {entity1.get(), entity2.get()}},
            };

            QVector<ListElement> firstDepth {
                {"Everything Else", {}},
                {"line", {entity3.get()}},
                {"point", {entity4.get()}},
                {"scrap", {entity1.get(), entity2.get()}},
            };
        }
    }

}


