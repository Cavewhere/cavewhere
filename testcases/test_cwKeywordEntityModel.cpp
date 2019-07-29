//Catch includes
#include "catch.hpp"

//Our includes
#include "SpyChecker.h"
#include "TestHelper.h"

//Cavewhwere includes
#include "cwKeywordEntityModel.h"
#include "cwKeywordComponent.h"
#include "cwKeywordModel.h"

//Std includes
#include <memory>

//Qt includes
#include <Qt3DCore/QEntity>
#include <QVector>
#include <QSignalSpy>

using namespace Qt3DCore;

TEST_CASE("cwKeywordEntityModel must initilize correctly", "[cwKeywordEntityModel]") {
    cwKeywordEntityModel model;

    CHECK(model.rowCount() == 0);
}

TEST_CASE("cwKeywordEntityModel should add / remove and update component correctly", "[cwKeywordEntityModel]") {
    auto model = std::make_unique<cwKeywordEntityModel>();

    QSignalSpy addSpy(model.get(), &cwKeywordEntityModel::rowsInserted);
    QSignalSpy removeSpy(model.get(), &cwKeywordEntityModel::rowsRemoved);
    QSignalSpy moveSpy(model.get(), &cwKeywordEntityModel::rowsMoved);
    QSignalSpy dataChangedSpy(model.get(), &cwKeywordEntityModel::dataChanged);

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

    component1->setObjectName("component1");
    component2->setObjectName("component2");

    cwKeywordModel* keywordModel1 = component1->keywordModel();
    cwKeywordModel* keywordModel2 = component2->keywordModel();

    keywordModel1->setObjectName("keywordModel1");
    keywordModel2->setObjectName("keywordModel2");

    auto entity1 = std::make_unique<QEntity>();
    auto entity2 = std::make_unique<QEntity>();

    entity1->setObjectName("entity1");
    entity2->setObjectName("entity2");

    keywordModel1->add({"name", "name1"});
    keywordModel1->add({"date", "date1"});

    keywordModel2->add({"name", "name2"});
    keywordModel2->add({"date", "date2"});
    keywordModel2->add({"keyword", "keyword2"});

    entity1->addComponent(component1);
    entity2->addComponent(component2);

    model->addComponent(component1);
    model->addComponent(component2);

    checker[&addSpy] = 2;
    checker.checkSpies();

    QVector<QEntity*> entities;

    auto add = [&entities](QEntity* entity) {
        entities.append(entity);
        std::sort(entities.begin(), entities.end());
    };

    auto remove = [&entities](QEntity* entity) {
        entities.removeAll(entity);
        std::sort(entities.begin(), entities.end());
    };

    add(entity1.get());
    add(entity2.get());

    //Model sort entites by pointer

    CHECK(model->rowCount() == 2);
    REQUIRE(model->rowCount() == entities.size());

    auto checkEntity = [&model](QEntity* entity, const QModelIndex& parentIndex) {

        CHECK(parentIndex.data(cwKeywordEntityModel::EntityRole).value<QEntity*>() == entity);

        REQUIRE(entity->components().size() == 1);
        auto keywordComponent = dynamic_cast<cwKeywordComponent*>(entity->components().first());

        CHECK(keywordComponent);
        if(keywordComponent->keywordModel()) {
            CHECK(model->rowCount(parentIndex) == keywordComponent->keywordModel()->rowCount());
            for(int i = 0; i < model->rowCount(); i++) {
                auto keywordEntityIndex = model->index(i, 0, parentIndex);
                auto keywordIndex = keywordComponent->keywordModel()->index(i);

                CHECK(keywordEntityIndex.data(cwKeywordEntityModel::KeyRole).toString().toStdString() == keywordIndex.data(cwKeywordModel::KeyRole).toString().toStdString());
                CHECK(keywordEntityIndex.data(cwKeywordEntityModel::ValueRole).toString().toStdString() == keywordIndex.data(cwKeywordModel::ValueRole).toString().toStdString());
            }
        } else {
            CHECK(model->rowCount(parentIndex) == 0);
        }
    };

    auto checkModel = [&model, checkEntity, &entities]() {
        for(int i = 0; i < model->rowCount(); i++) {
            QModelIndex entityIndex = model->index(i, 0, QModelIndex());
            auto entity = entities.at(i);
            checkEntity(entity, entityIndex);
        }
    };

    checkModel();

    SECTION("Add empty component") {
        auto componentEmpty = new cwKeywordComponent();

        checker.clearSpyCounts();

        model->addComponent(componentEmpty);

        checker.checkSpies();

        checkModel();

        SECTION("Add Entity to empty component") {
            auto entity3 = std::make_unique<QEntity>();

            add(entity3.get());

            entity3->addComponent(componentEmpty);

            checker[&addSpy]++;
            checker.checkSpies();

            checkModel();
        }
    }

    SECTION("Add component to multiple entities") {
        auto entity3 = std::make_unique<QEntity>();
        add(entity3.get());

        entity3->addComponent(component1);

        checker[&addSpy]++;
        checker.checkSpies();

        checkModel();

        SECTION("Update data with multiple entities") {
            SECTION("Set data") {
                checker.clearSpyCounts();

                auto keywordIndex = component1->keywordModel()->index(0);
                component1->keywordModel()->setData(keywordIndex, "newKey", cwKeywordModel::KeyRole);

                checker[&dataChangedSpy] = 2; //Called twice because connected to two seperate entities
                checker.requireSpies();

                auto entity1Index = model->indexOf(entity1.get());
                auto entity3Index = model->indexOf(entity3.get());

                for(int i = 0; i < dataChangedSpy.size(); i++) {
                    auto spyAtIndex = dataChangedSpy.at(i);
                    REQUIRE(spyAtIndex.size() == 3);
                    auto entityIndex = spyAtIndex.at(0).value<QModelIndex>().parent() == entity1Index ? entity1Index : entity3Index;
                    auto index = model->index(0, 0, entityIndex);
                    INFO("Comparing:" << spyAtIndex.at(0).value<QModelIndex>() << " to " << index);
                    CHECK(spyAtIndex.at(0).value<QModelIndex>() == index);
                    CHECK(spyAtIndex.at(1).value<QModelIndex>() == index);
                    CHECK(spyAtIndex.at(2).value<QVector<int>>() == QVector<int>({cwKeywordEntityModel::KeyRole}));
                }

                checker.clearSpyCounts();

                checkModel();

                component1->keywordModel()->setData(keywordIndex, "newValue", cwKeywordModel::ValueRole);

                checker[&dataChangedSpy] = 2; //Called twice because connected to two seperate entities
                checker.requireSpies();

                for(int i = 0; i < dataChangedSpy.size(); i++) {
                    auto spyAtIndex = dataChangedSpy.at(i);
                    REQUIRE(spyAtIndex.size() == 3);
                    auto entityIndex = spyAtIndex.at(0).value<QModelIndex>().parent() == entity1Index ? entity1Index : entity3Index;
                    auto index = model->index(0, 0, entityIndex);
                    INFO("Comparing:" << spyAtIndex.at(0).value<QModelIndex>() << " to " << index);
                    CHECK(spyAtIndex.at(0).value<QModelIndex>() == index);
                    CHECK(spyAtIndex.at(1).value<QModelIndex>() == index);
                    CHECK(spyAtIndex.at(2).value<QVector<int>>() == QVector<int>({cwKeywordEntityModel::ValueRole}));
                }

                checkModel();
            }

            SECTION("Add data") {
                checker.clearSpyCounts();

                component1->keywordModel()->add({"newKey1", "newValue1"});

                checker[&addSpy] = 2; //Called twice because connected to two seperate entities
                checker.requireSpies();

                for(int i = 0; i < addSpy.size(); i++) {
                    auto spyAtIndex = addSpy.at(i);
                    REQUIRE(spyAtIndex.size() == 3);
                    CHECK(spyAtIndex.at(0).value<QModelIndex>().parent() == QModelIndex());
                    CHECK(spyAtIndex.at(1).value<int>() == component1->keywordModel()->rowCount() - 1);
                    CHECK(spyAtIndex.at(2).value<int>() == component1->keywordModel()->rowCount() - 1);
                }

                checkModel();
            }

            SECTION("Remove data") {
                checker.clearSpyCounts();

                component1->keywordModel()->removeAll("name");

                checker[&removeSpy] = 2; //Called twice because connected to two seperate entities
                checker.requireSpies();

                for(int i = 0; i < removeSpy.size(); i++) {
                    auto spyAtIndex = removeSpy.at(i);
                    REQUIRE(spyAtIndex.size() == 3);
                    CHECK(spyAtIndex.at(0).value<QModelIndex>().parent() == QModelIndex());
                    CHECK(spyAtIndex.at(1).value<int>() == 0);
                    CHECK(spyAtIndex.at(2).value<int>() == 0);
                }

                checkModel();
            }

            SECTION("Remove invalid keyword") {
                checker.clearSpyCounts();

                component1->keywordModel()->removeAll("notAKeyword");
                checker.checkSpies();
                checkModel();

                component1->keywordModel()->remove(cwKeyword());
                checker.checkSpies();
                checkModel();
            }
        }
    }

    SECTION("Remove from the model") {
        checker.clearSpyCounts();

        int indexToRemove = model->indexOf(entity2.get()).row();
        REQUIRE(indexToRemove >= 0);

        auto checkRemove = [&]() {
            checker[&removeSpy] = 1;
            checker.requireSpies();

            CHECK(removeSpy.at(0).at(0).value<QModelIndex>() == QModelIndex());
            CHECK(removeSpy.at(0).at(1).value<int>() == indexToRemove);
            CHECK(removeSpy.at(0).at(2).value<int>() == indexToRemove);

            checkModel();

            //This checks to make sure the connections have been removed
            checker.clearSpyCounts();

            component2->keywordModel()->add({"my", "face"});
            component2->keywordModel()->removeAll("date");
            auto indexToModify = component2->keywordModel()->index(0);
            component2->keywordModel()->setData(indexToModify, "sauce", cwKeywordModel::KeyRole);

            checker.checkSpies(); //There should be no signals emitted

            checkModel();
        };

        SECTION("Remove component") {
            model->removeComponent(component2);
            remove(entity2.get());
            checkRemove();
        }

        SECTION("Remove entity") {
            entity2->removeComponent(component2);
            remove(entity2.get());
            checkRemove();
        }
    }
}
