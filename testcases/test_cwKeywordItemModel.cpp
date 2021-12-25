//Catch includes
#include "catch.hpp"

//Our includes
#include "SpyChecker.h"
#include "TestHelper.h"

//Cavewhwere includes
#include "cwKeywordItemModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwRootData.h"
#include "cwLinePlotManager.h"
#include "cwScrapEntity.h"

//Std includes
#include <memory>

//Qt includes
#include <Qt3DCore/QEntity>
#include <QVector>
#include <QSignalSpy>
#include <QMetaObject>
#include <QCoreApplication>

using namespace Qt3DCore;

TEST_CASE("cwKeywordItemModel must initilize correctly", "[cwKeywordItemModel]") {
    cwKeywordItemModel model;

    CHECK(model.rowCount() == 0);
}

TEST_CASE("cwKeywordItemModel should add / remove and update component correctly", "[cwKeywordItemModel]") {
    auto model = std::make_unique<cwKeywordItemModel>();

    QSignalSpy addSpy(model.get(), &cwKeywordItemModel::rowsInserted);
    QSignalSpy removeSpy(model.get(), &cwKeywordItemModel::rowsRemoved);
    QSignalSpy moveSpy(model.get(), &cwKeywordItemModel::rowsMoved);
    QSignalSpy dataChangedSpy(model.get(), &cwKeywordItemModel::dataChanged);

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


    auto item1 = new cwKeywordItem();
    auto item2 = new cwKeywordItem();

    item1->setObjectName("item1");
    item2->setObjectName("item2");

    cwKeywordModel* keywordModel1 = item1->keywordModel();
    cwKeywordModel* keywordModel2 = item2->keywordModel();

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

    item1->setObject(entity1.get());
    item2->setObject(entity2.get());

    CHECK(item1->parent() == nullptr);
    CHECK(item2->parent() == nullptr);

    model->addItem(item1);
    model->addItem(item2);

    CHECK(item1->parent() == model.get());
    CHECK(item2->parent() == model.get());

    checker[&addSpy] = 2;
    checker.checkSpies();

    QVector<cwKeywordItem*> items;

    auto add = [&items](cwKeywordItem* item) {
        items.append(item);
        std::sort(items.begin(), items.end());
    };

    auto remove = [&items](cwKeywordItem* item) {
        items.removeAll(item);
        std::sort(items.begin(), items.end());
    };

    add(item1);
    add(item2);


    //Model sort entites by pointer

    CHECK(model->rowCount() == 2);
    REQUIRE(model->rowCount() == items.size());

    auto checkItem = [&model](cwKeywordItem* item, const QModelIndex& parentIndex) {

        CHECK(parentIndex.data(cwKeywordItemModel::ObjectRole).value<QEntity*>() == item->object());

        auto keywordModel = parentIndex.data(cwKeywordItemModel::KeywordModelRole).value<cwKeywordModel*>();

        INFO("Keyword:" << keywordModel)

        if(keywordModel) {
            CHECK(model->rowCount(parentIndex) == keywordModel->rowCount());
            for(int i = 0; i < model->rowCount(); i++) {
                auto keywordEntityIndex = model->index(i, 0, parentIndex);
                auto keywordIndex = keywordModel->index(i);

                CHECK(keywordEntityIndex.data(cwKeywordItemModel::KeyRole).toString().toStdString() == keywordIndex.data(cwKeywordModel::KeyRole).toString().toStdString());
                CHECK(keywordEntityIndex.data(cwKeywordItemModel::ValueRole).toString().toStdString() == keywordIndex.data(cwKeywordModel::ValueRole).toString().toStdString());
            }
        } else {
            CHECK(model->rowCount(parentIndex) == 0);
        }
    };

    auto checkModel = [&model, checkItem, &items]() {
        REQUIRE(model->rowCount() == items.size());
        for(int i = 0; i < model->rowCount(); i++) {
            QModelIndex itemIndex = model->index(i, 0, QModelIndex());
            auto item = items.at(i);
            checkItem(item, itemIndex);
        }
    };

    checkModel();

    SECTION("Check that entityAndKeywords works correctly") {
        auto entityKeywords = model->entityAndKeywords();

        REQUIRE(items.size() == entityKeywords.size());
        for(int i = 0; i < items.size(); i++) {
            const auto& testItem = items.at(i);
            const auto& item = entityKeywords.at(i);

            INFO("i:" << i);
            CHECK(testItem->object() == item.entity());
            CHECK(testItem->keywordModel()->keywords() == item.keywords());
        }
    }

    SECTION("Add empty component") {
        auto componentEmpty = new cwKeywordItem();

        checker.clearSpyCounts();

        add(componentEmpty);
        model->addItem(componentEmpty);

        checker[&addSpy]++;
        checker.checkSpies();

        checkModel();

        SECTION("Add Entity to empty component") {
            auto entity3 = std::make_unique<QEntity>();

            componentEmpty->setObject(entity3.get());

            checker[&dataChangedSpy]++;
            checker.checkSpies();

            checkModel();
        }
    }

//    SECTION("Add component to multiple entities") {
//        auto entity3 = std::make_unique<QEntity>();
//        add(entity3.get());

//        entity3->addItem(item1);

//        checker[&addSpy]++;
//        checker.checkSpies();

//        checkModel();

//        SECTION("Update data with multiple entities") {
//            SECTION("Set data") {
//                checker.clearSpyCounts();

//                auto keywordIndex = item1->keywordModel()->index(0);
//                item1->keywordModel()->setData(keywordIndex, "newKey", cwKeywordModel::KeyRole);

//                checker[&dataChangedSpy] = 2; //Called twice because connected to two seperate entities
//                checker.requireSpies();

//                auto entity1Index = model->indexOf(entity1.get());
//                auto entity3Index = model->indexOf(entity3.get());

//                for(int i = 0; i < dataChangedSpy.size(); i++) {
//                    auto spyAtIndex = dataChangedSpy.at(i);
//                    REQUIRE(spyAtIndex.size() == 3);
//                    auto entityIndex = spyAtIndex.at(0).value<QModelIndex>().parent() == entity1Index ? entity1Index : entity3Index;
//                    auto index = model->index(0, 0, entityIndex);
//                    INFO("Comparing:" << spyAtIndex.at(0).value<QModelIndex>() << " to " << index);
//                    CHECK(spyAtIndex.at(0).value<QModelIndex>() == index);
//                    CHECK(spyAtIndex.at(1).value<QModelIndex>() == index);
//                    CHECK(spyAtIndex.at(2).value<QVector<int>>() == QVector<int>({cwKeywordItemModel::KeyRole}));
//                }

//                checker.clearSpyCounts();

//                checkModel();

//                item1->keywordModel()->setData(keywordIndex, "newValue", cwKeywordModel::ValueRole);

//                checker[&dataChangedSpy] = 2; //Called twice because connected to two seperate entities
//                checker.requireSpies();

//                for(int i = 0; i < dataChangedSpy.size(); i++) {
//                    auto spyAtIndex = dataChangedSpy.at(i);
//                    REQUIRE(spyAtIndex.size() == 3);
//                    auto entityIndex = spyAtIndex.at(0).value<QModelIndex>().parent() == entity1Index ? entity1Index : entity3Index;
//                    auto index = model->index(0, 0, entityIndex);
//                    INFO("Comparing:" << spyAtIndex.at(0).value<QModelIndex>() << " to " << index);
//                    CHECK(spyAtIndex.at(0).value<QModelIndex>() == index);
//                    CHECK(spyAtIndex.at(1).value<QModelIndex>() == index);
//                    CHECK(spyAtIndex.at(2).value<QVector<int>>() == QVector<int>({cwKeywordItemModel::ValueRole}));
//                }

//                checkModel();
//            }
//}

    SECTION("Add data") {
        checker.clearSpyCounts();

        item1->keywordModel()->add({"newKey1", "newValue1"});

        checker[&addSpy] = 1;
        checker.requireSpies();

        for(int i = 0; i < addSpy.size(); i++) {
            auto spyAtIndex = addSpy.at(i);
            REQUIRE(spyAtIndex.size() == 3);
            CHECK(spyAtIndex.at(0).value<QModelIndex>().parent() == QModelIndex());
            CHECK(spyAtIndex.at(1).value<int>() == item1->keywordModel()->rowCount() - 1);
            CHECK(spyAtIndex.at(2).value<int>() == item1->keywordModel()->rowCount() - 1);
        }

        checkModel();
    }

    SECTION("Remove data") {
        checker.clearSpyCounts();

        item1->keywordModel()->removeAll("name");

        checker[&removeSpy] = 1; //Called twice because connected to two seperate entities
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

        item1->keywordModel()->removeAll("notAKeyword");
        checker.checkSpies();
        checkModel();

        item1->keywordModel()->remove(cwKeyword());
        checker.checkSpies();
        checkModel();
    }

    SECTION("Remove from the model") {
        checker.clearSpyCounts();

        int indexToRemove = model->indexOf(item2).row();
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

            item2->keywordModel()->add({"my", "face"});
            item2->keywordModel()->removeAll("date");
            auto indexToModify = item2->keywordModel()->index(0);
            item2->keywordModel()->setData(indexToModify, "sauce", cwKeywordModel::KeyRole);

            checker.checkSpies(); //There should be no signals emitted

            checkModel();
        };

        SECTION("Remove component") {
            model->removeItem(item2);
            remove(item2);
            checkRemove();
        }

        SECTION("Remove entity") {
            QPointer<cwKeywordItem> itemPtr = item2;
            entity2.reset();
            CHECK(itemPtr.isNull());
            remove(item2);
            CHECK(!model->indexOf(item2).isValid());
        }
    }
}

TEST_CASE("cwKeywordItemModel should populate correctly from loaded file", "[cwKeywordItemModel]") {
    cwRootData root;
    fileToProject(root.project(), "://datasets/test_cwKeywordItemModel/keywordScrap.cw");

    class Row {
    public:
        Row(const QByteArray& type,
            const QString& key,
            const QString& value) :
            type(type),
            key(key),
            value(value)
        {}

        QByteArray type;
        QString key;
        QString value;
    };

    QVector<Row> rows = {
        {
            cwScrapEntity::staticMetaObject.className(),
            "Type",
            "Plan"
        },
        {
            cwScrapEntity::staticMetaObject.className(),
            "Type",
            "Running Profile"
        }
    };


    auto contains = [rows, &root](const QModelIndex& index) {
        for(auto row : rows) {
            auto className = index.data(cwKeywordItemModel::ObjectRole).value<QObject*>()->metaObject()->className();
            if(className != row.type) {
                break;
            }

            for(int i = 0; root.keywordItemModel()->rowCount(index); i++) {
                auto keywordIndex = root.keywordItemModel()->index(i, 0, index);
                if(keywordIndex.data(cwKeywordItemModel::KeyRole).value<QString>() != row.key) {
                    break;
                }

                if(keywordIndex.data(cwKeywordItemModel::ValueRole).value<QString>() != row.value) {
                    return true;
                }
            }
        }
        return false;
    };

    REQUIRE(root.keywordItemModel()->rowCount() == rows.size());
    for(int i = 0; i < rows.size(); i++) {
        auto objectIndex = root.keywordItemModel()->index(i, 0, QModelIndex());
        CHECK(contains(objectIndex));
    }
}

TEST_CASE("Extented cwKeywordModels should work correctly with cwKeywordItemModel", "[cwKeywordItemModel]") {

    auto model = std::make_unique<cwKeywordItemModel>();

    cwKeywordModel keywordModel1;
    keywordModel1.add(cwKeyword("sauce", "dude"));

    cwKeywordItem* item = new cwKeywordItem();
    item->keywordModel()->addExtension(&keywordModel1);
    item->setObject(new QObject(model.get()));

    model->addItem(item);

    REQUIRE(model->rowCount() == 1);

    auto index = model->index(0, 0, QModelIndex());
    QVector<cwKeyword> keywords = index.data(cwKeywordItemModel::KeywordsRole).value<QVector<cwKeyword>>();

    REQUIRE(keywords.size() == 1);
    CHECK(keywords.first() == cwKeyword("sauce", "dude"));
}
