//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwKeywordGroupByKeyModel.h"
#include "SignalSpyChecker.h"
#include "cwKeywordItem.h"
#include "cwKeywordModel.h"
#include "cwKeywordItemModel.h"

//Qt includes
#include <QFuture>
#include <QSignalSpy>
#include <QVector>
#include <QStandardItemModel>


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

    auto spyCheck = SignalSpyChecker::Constant::makeChecker(model.get());

    auto addSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::rowsInserted);
    auto removeSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::rowsRemoved);
    auto moveSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::rowsMoved);
    auto dataChangedSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::dataChanged);
    auto resetSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::modelReset);
    auto aboutToResetSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::modelAboutToBeReset);
    auto keySpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::keyChanged);
    auto sourceSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::sourceChanged);
    auto acceptByDefaultChangedSpy = spyCheck.findSpy(&cwKeywordGroupByKeyModel::acceptByDefaultChanged);

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

        SECTION("Add new category") {
            auto component = new cwKeywordItem();
            component->setObjectName("newComponent");
            cwKeywordModel* keywordModel = component->keywordModel();
            keywordModel->setObjectName("newKeywordModel");
            auto entity = std::make_unique<QObject>();
            entity->setObjectName("newEntity");

            keywordModel->add({"type", "area"});
            keywordModel->add({"trip", "trip1"});
            keywordModel->add({"cave", "cave2"});

            component->setObject(entity.get());
            keywordEntityModel->addItem(component);

            lists = {
                {"area", {entity.get()}, true},
                {"line", {entity3.get()}, true},
                {"point", {entity4.get()}, true},
                {"scrap", {entity1.get(), entity2.get()}, true},
                {cwKeywordGroupByKeyModel::otherCategory(), {}, false}
            };

            check(QModelIndex(), lists);
        }
    }

    SECTION("acceptedByDefault is false") {
        CHECK(model->acceptByDefault() == true);
        model->setAcceptedByDefault(false);
        CHECK(model->acceptByDefault() == false);

        spyCheck[acceptByDefaultChangedSpy]++;

        SECTION("Set key") {
            model->setKey("type");

            lists = {
                {"line", {entity3.get()}, false},
                {"point", {entity4.get()}, false},
                {"scrap", {entity1.get(), entity2.get()}, false},
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
                    {"trip1", {entity1.get()}, false},
                    {"trip2", {entity2.get(), entity3.get()}, false},
                    {"trip3", {entity4.get(),}, false},
                    {cwKeywordGroupByKeyModel::otherCategory(), {}, false}
                };

                check(QModelIndex(), lists);
            }

            SECTION("Add new category") {
                auto component = new cwKeywordItem();
                component->setObjectName("newComponent");
                cwKeywordModel* keywordModel = component->keywordModel();
                keywordModel->setObjectName("newKeywordModel");
                auto entity = std::make_unique<QObject>();
                entity->setObjectName("newEntity");

                keywordModel->add({"type", "area"});
                keywordModel->add({"trip", "trip1"});
                keywordModel->add({"cave", "cave2"});

                component->setObject(entity.get());
                keywordEntityModel->addItem(component);

                lists = {
                    {"area", {entity.get()}, false},
                    {"line", {entity3.get()}, false},
                    {"point", {entity4.get()}, false},
                    {"scrap", {entity1.get(), entity2.get()}, false},
                    {cwKeywordGroupByKeyModel::otherCategory(), {}, false}
                };

                check(QModelIndex(), lists);
            }
        }
    }
}

TEST_CASE("cwKeywordGroupByKeyModel survives extension destruction during filtering", "[cwKeywordGroupByKeyModel][regression]") {
    cwKeywordItemModel keywordItemModel;
    cwKeywordGroupByKeyModel groupedModel;
    groupedModel.setKey("Trip");
    groupedModel.setSourceModel(&keywordItemModel);

    auto* keywordItem = new cwKeywordItem();
    QObject entity;
    entity.setObjectName("entity");
    keywordItem->setObject(&entity);
    keywordItemModel.addItem(keywordItem);

    auto* baseKeywordModel = keywordItem->keywordModel();
    REQUIRE(baseKeywordModel != nullptr);
    baseKeywordModel->add({"Type", "Note"});

    auto extensionModel = std::make_unique<cwKeywordModel>();
    extensionModel->add({"Trip", "Trip-1"});
    baseKeywordModel->addExtension(extensionModel.get());

    REQUIRE(groupedModel.rowCount() >= 2);

    // Destroying an extension while groupedModel is connected used to crash due to
    // re-entrant reads of a dying extension model during beginRemoveRows.
    extensionModel.reset();

    CHECK(groupedModel.rowCount() >= 1);
    CHECK(keywordItemModel.rowCount() == 1);
}

TEST_CASE("cwKeywordGroupByKeyModel takes a source range insert in one pass",
          "[cwKeywordGroupByKeyModel][MainThreadBudget]")
{
    constexpr int kRangeCount = 50;

    QObject owner;
    QStandardItemModel source;
    cwKeywordGroupByKeyModel groupBy;
    groupBy.setSourceModel(&source);

    QList<QStandardItem*> rows;
    for (int i = 0; i < kRangeCount; ++i) {
        auto* row = new QStandardItem();
        row->setData(QVariant::fromValue<QObject*>(new QObject(&owner)), cwKeywordItemModel::ObjectRole);
        row->setData(QVariant::fromValue(QVector<cwKeyword>({cwKeyword(QStringLiteral("type"), QStringLiteral("line"))})),
                     cwKeywordItemModel::KeywordsRole);
        rows.append(row);
    }

    QSignalSpy groupChanged(&groupBy, &QAbstractItemModel::dataChanged);
    QSignalSpy acceptedInserted(groupBy.acceptedModel(), &QAbstractItemModel::rowsInserted);

    source.invisibleRootItem()->appendRows(rows);

    const int otherRow = groupBy.otherIndex().row();
    int otherChanges = 0;
    for (const QList<QVariant>& arguments : std::as_const(groupChanged)) {
        const QModelIndex topLeft = arguments.at(0).value<QModelIndex>();
        const QModelIndex bottomRight = arguments.at(1).value<QModelIndex>();
        if (topLeft.row() <= otherRow && otherRow <= bottomRight.row()) {
            ++otherChanges;
        }
    }

    CHECK(otherChanges == 1);
    CHECK(acceptedInserted.count() == 1);
    CHECK(groupBy.otherIndex().data(cwKeywordGroupByKeyModel::ObjectCountRole).toInt() == kRangeCount);
    CHECK(groupBy.acceptedModel()->rowCount() == kRangeCount);
}

namespace {

bool isAccepted(const cwKeywordGroupByKeyModel& model, QObject* object)
{
    const QAbstractItemModel* accepted = model.acceptedModel();
    for (int i = 0; i < accepted->rowCount(); ++i) {
        if (accepted->index(i, 0).data(cwKeywordItemModel::ObjectRole).value<QObject*>() == object) {
            return true;
        }
    }
    return false;
}

QModelIndex valueIndex(const cwKeywordGroupByKeyModel& model, const QString& value)
{
    for (int i = 0; i < model.rowCount() - 1; ++i) {
        const QModelIndex index = model.index(i);
        if (index.data(cwKeywordGroupByKeyModel::ValueRole).toString() == value) {
            return index;
        }
    }
    return QModelIndex();
}

int objectCount(const QModelIndex& index)
{
    return index.data(cwKeywordGroupByKeyModel::ObjectCountRole).toInt();
}

cwKeywordItem* addItem(cwKeywordItemModel& items, QObject* object, const QVector<cwKeyword>& keywords)
{
    auto item = new cwKeywordItem();
    for (const auto& keyword : keywords) {
        item->keywordModel()->add(keyword);
    }
    item->setObject(object);
    items.addItem(item);
    return item;
}

} // namespace

TEST_CASE("cwKeywordGroupByKeyModel keeps an object in its value row when another key's keyword changes",
          "[cwKeywordGroupByKeyModel]")
{
    cwKeywordItemModel items;
    QObject object;
    auto item = addItem(items, &object, {{QStringLiteral("Type"), QStringLiteral("Line Plot")},
                                         {QStringLiteral("Caver"), QStringLiteral("Alice")}});

    cwKeywordGroupByKeyModel model;
    model.setSourceModel(&items);
    model.setKey(QStringLiteral("Type"));
    REQUIRE(objectCount(valueIndex(model, QStringLiteral("Line Plot"))) == 1);
    REQUIRE(objectCount(model.otherIndex()) == 0);
    REQUIRE(isAccepted(model, &object));

    SECTION("Replacing the other key's value") {
        item->keywordModel()->replace({QStringLiteral("Caver"), QStringLiteral("Bob")});
    }

    SECTION("Removing and re-adding the other key's keyword") {
        item->keywordModel()->removeAll(QStringLiteral("Caver"));
        item->keywordModel()->add({QStringLiteral("Caver"), QStringLiteral("Bob")});
    }

    CHECK(objectCount(model.otherIndex()) == 0);
    CHECK(objectCount(valueIndex(model, QStringLiteral("Line Plot"))) == 1);
    CHECK(isAccepted(model, &object));
}

TEST_CASE("cwKeywordGroupByKeyModel accepts an object while any of its values is checked",
          "[cwKeywordGroupByKeyModel]")
{
    cwKeywordItemModel items;
    QObject object;
    addItem(items, &object, {{QStringLiteral("Caver"), QStringLiteral("Alice")},
                             {QStringLiteral("Caver"), QStringLiteral("Bob")}});

    cwKeywordGroupByKeyModel model;
    model.setSourceModel(&items);
    model.setKey(QStringLiteral("Caver"));
    REQUIRE(isAccepted(model, &object));

    REQUIRE(model.setData(valueIndex(model, QStringLiteral("Alice")), false, cwKeywordGroupByKeyModel::AcceptedRole));
    CHECK(isAccepted(model, &object));

    REQUIRE(model.setData(valueIndex(model, QStringLiteral("Bob")), false, cwKeywordGroupByKeyModel::AcceptedRole));
    CHECK_FALSE(isAccepted(model, &object));

    REQUIRE(model.setData(valueIndex(model, QStringLiteral("Alice")), true, cwKeywordGroupByKeyModel::AcceptedRole));
    CHECK(isAccepted(model, &object));
}

TEST_CASE("cwKeywordGroupByKeyModel invert keeps an object accepted while any of its values is checked",
          "[cwKeywordGroupByKeyModel]")
{
    cwKeywordItemModel items;
    QObject object;
    addItem(items, &object, {{QStringLiteral("Caver"), QStringLiteral("Alice")},
                             {QStringLiteral("Caver"), QStringLiteral("Bob")}});

    cwKeywordGroupByKeyModel model;
    model.setSourceModel(&items);
    model.setKey(QStringLiteral("Caver"));
    REQUIRE(model.setData(valueIndex(model, QStringLiteral("Bob")), false, cwKeywordGroupByKeyModel::AcceptedRole));
    REQUIRE(isAccepted(model, &object));

    model.invert();
    CHECK_FALSE(valueIndex(model, QStringLiteral("Alice")).data(cwKeywordGroupByKeyModel::AcceptedRole).toBool());
    CHECK(valueIndex(model, QStringLiteral("Bob")).data(cwKeywordGroupByKeyModel::AcceptedRole).toBool());
    CHECK(isAccepted(model, &object));

    model.invert();
    CHECK(valueIndex(model, QStringLiteral("Alice")).data(cwKeywordGroupByKeyModel::AcceptedRole).toBool());
    CHECK(isAccepted(model, &object));
}

TEST_CASE("cwKeywordGroupByKeyModel keeps an object accepted by a checked value through keyword edits",
          "[cwKeywordGroupByKeyModel]")
{
    cwKeywordItemModel items;
    QObject aliceOnly;
    addItem(items, &aliceOnly, {{QStringLiteral("Caver"), QStringLiteral("Alice")}});

    QObject object;
    auto item = addItem(items, &object, {{QStringLiteral("Caver"), QStringLiteral("Bob")},
                                         {QStringLiteral("Type"), QStringLiteral("Line Plot")}});

    cwKeywordGroupByKeyModel model;
    model.setSourceModel(&items);
    model.setKey(QStringLiteral("Caver"));
    REQUIRE(model.setData(valueIndex(model, QStringLiteral("Alice")), false, cwKeywordGroupByKeyModel::AcceptedRole));
    REQUIRE_FALSE(isAccepted(model, &aliceOnly));
    REQUIRE(isAccepted(model, &object));

    SECTION("Gaining a value whose row is unchecked") {
        item->keywordModel()->add({QStringLiteral("Caver"), QStringLiteral("Alice")});
        CHECK(objectCount(valueIndex(model, QStringLiteral("Alice"))) == 2);
        CHECK(isAccepted(model, &object));
        CHECK_FALSE(isAccepted(model, &aliceOnly));
    }

    SECTION("Editing another key while it also holds an unchecked value") {
        item->keywordModel()->add({QStringLiteral("Caver"), QStringLiteral("Alice")});
        item->keywordModel()->replace({QStringLiteral("Type"), QStringLiteral("Scrap")});
        CHECK(objectCount(model.otherIndex()) == 0);
        CHECK(isAccepted(model, &object));
    }

    SECTION("Losing its only checked value") {
        item->keywordModel()->add({QStringLiteral("Caver"), QStringLiteral("Alice")});
        item->keywordModel()->remove(cwKeyword(QStringLiteral("Caver"), QStringLiteral("Bob")));
        CHECK_FALSE(valueIndex(model, QStringLiteral("Bob")).isValid());
        CHECK_FALSE(isAccepted(model, &object));
    }
}
