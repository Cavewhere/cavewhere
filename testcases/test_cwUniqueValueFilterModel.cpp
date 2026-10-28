//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwUniqueValueFilterModel.h"
#include "SignalSpyChecker.h"

//Qt includes
#include <QStandardItemModel>
#include <QVector>
#include <QStandardItem>
#include <QSignalSpy>

//Std includes
#include <functional>
#include <utility>

template<typename T, typename INIT>
QVector<T*> createObjs(int num, INIT func) {
    QVector<T*> objs;
    objs.reserve(num);
    for(int i = 0; i < num; i++) {
        auto obj = new T();

        if constexpr(std::is_invocable<INIT>::value) {
            func(obj, i);
        }

        objs.append(obj);
    }
    return objs;
}

TEST_CASE("cwUniqueValueFilterModel should filter values correctly", "[cwUniqueValueFilterModel]") {

    QVector<QObject*> objs = createObjs<QObject>(4,
                                        [](QObject* obj, int i)
    {
            obj->setObjectName(QString("obj%1").arg(i));
});

    QVector<QStandardItem*> items = createObjs<QStandardItem>(8, nullptr);

    items[0]->setData(QVariant::fromValue(objs.at(2)));
    items[1]->setData(QVariant::fromValue(objs.at(1)));
    items[2]->setData(QVariant::fromValue(objs.at(1)));
    items[3]->setData(QVariant::fromValue(objs.at(0)));
    items[4]->setData(QVariant::fromValue(objs.at(3)));
    items[5]->setData(QVariant::fromValue(objs.at(2)));
    items[6]->setData(QVariant::fromValue(objs.at(3)));
    items[7]->setData(QVariant::fromValue(objs.at(1)));

    QStandardItemModel model;

    for(auto item : std::as_const(items)) {
        model.appendRow(item);
    }

    REQUIRE(model.rowCount() == items.size());

    cwUniqueValueFilterModel filter;

    //Order doesn't matter
    QVector<QObject*> filterObjs;

    auto checkRows = [&filterObjs, &filter]() {
        CHECK(filterObjs.size() == filter.rowCount());

        auto modelContains = [&](const QObject* obj) {
            for(int i = 0; i < filter.rowCount(); i++) {
                auto index = filter.index(i, 0);
                if(index.data(Qt::UserRole + 1).value<QObject*>() == obj) {
                    return true;
                }
            }
            return false;
        };

        for(auto obj : std::as_const(filterObjs)) {
            CHECK(modelContains(obj));
        }
    };

    auto spyChecker = SignalSpyChecker::Constant::makeChecker(&filter);

    auto uniqueRoleSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::uniqueRoleChanged);
    auto rowsInsertedSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::rowsInserted);
    auto rowsAboutToBeInsertedSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::rowsAboutToBeInserted);
    auto rowsRemovedSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::rowsRemoved);
    auto rowsAboutToBeRemovedSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::rowsAboutToBeRemoved);
    auto sourceModelSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::sourceModelChanged);
    auto resetSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::modelReset);
    auto resetAboutToBeSpy = spyChecker.findSpy(&cwUniqueValueFilterModel::modelAboutToBeReset);

    filter.setUniqueRole(Qt::UserRole + 1);
    filter.setLessThan([](const QVariant& left, const QVariant& right) {
        return std::less<QObject*>()(left.value<QObject*>(), right.value<QObject*>());
    });
    filter.setSourceModel(&model);

    spyChecker[sourceModelSpy]++;
    spyChecker[uniqueRoleSpy]++;
    spyChecker[resetSpy]++;
    spyChecker[resetAboutToBeSpy]++;

    filterObjs = {
        objs.at(2),
        objs.at(1),
        objs.at(0),
        objs.at(3)
    };

    CHECK(filter.uniqueRole() == Qt::UserRole + 1);

    checkRows();
    spyChecker.checkSpies();

    SECTION("Add unique element") {
        auto newObj = new QObject();
        newObj->setObjectName("newObj");
        objs.append(newObj);
        QStandardItem* item = new QStandardItem();
        item->setData(QVariant::fromValue(newObj));
        model.appendRow(item);

        filterObjs = {
            objs.at(2),
            objs.at(1),
            objs.at(0),
            objs.at(3),
            newObj
        };

        spyChecker[rowsInsertedSpy]++;
        spyChecker[rowsAboutToBeInsertedSpy]++;

        checkRows();
        spyChecker.checkSpies();
    }

    SECTION("Add duplicate element") {
        QStandardItem* item = new QStandardItem();
        item->setData(QVariant::fromValue(objs.at(0)));
        model.appendRow(item);

        filterObjs = {
            objs.at(2),
            objs.at(1),
            objs.at(0),
            objs.at(3)
        };

        checkRows();
        spyChecker.checkSpies();
    }

    SECTION("Remove item full") {
        model.removeRow(3);

        filterObjs = {
            objs.at(2),
            objs.at(1),
            objs.at(3)
        };

        spyChecker[rowsAboutToBeRemovedSpy]++;
        spyChecker[rowsRemovedSpy]++;

        checkRows();
        spyChecker.checkSpies();
    }

    SECTION("Remove multi") {
        //Remove all the obj1
        model.removeRow(1);
        model.removeRow(1);
        model.removeRow(5);

        filterObjs = {
            objs.at(0),
            objs.at(2),
            objs.at(3)
        };

        spyChecker[rowsAboutToBeRemovedSpy] += 1;
        spyChecker[rowsRemovedSpy] += 1;

        checkRows();
        spyChecker.checkSpies();
    }

    SECTION("Data changed to multi") {
        items[0]->setData(QVariant::fromValue(objs.at(0))); //obj2 to obj0

        filterObjs = {
            objs.at(0),
            objs.at(1),
            objs.at(2),
            objs.at(3)
        };

        checkRows();
        spyChecker.checkSpies();
    }

    SECTION("Data change remove") {
        items[3]->setData(QVariant::fromValue(objs.at(2))); //obj0 to obj2

        filterObjs = {
            objs.at(1),
            objs.at(2),
            objs.at(3)
        };

        spyChecker[rowsAboutToBeRemovedSpy] += 1;
        spyChecker[rowsRemovedSpy] += 1;

        checkRows();
        spyChecker.checkSpies();
    }

    SECTION("Data change add") {
        auto newObj = new QObject();
        newObj->setObjectName("newObj");
        objs.append(newObj);
        items[3]->setData(QVariant::fromValue(newObj));

        filterObjs = {
            objs.at(1),
            objs.at(2),
            objs.at(3),
            newObj
        };

        spyChecker[rowsAboutToBeRemovedSpy] += 1;
        spyChecker[rowsRemovedSpy] += 1;
        spyChecker[rowsInsertedSpy]++;
        spyChecker[rowsAboutToBeInsertedSpy]++;

        checkRows();
        spyChecker.checkSpies();
    }

    for(auto obj : std::as_const(objs)) {
        delete obj;
    }
}

TEST_CASE("cwUniqueValueFilterModel takes a source range insert as one insert",
          "[cwUniqueValueFilterModel][MainThreadBudget]")
{
    constexpr int kSourceRows = 40;
    constexpr int kDistinctValues = 8;
    constexpr int kUniqueRole = Qt::UserRole + 1;

    QObject owner;
    QVector<QObject*> values;
    for (int i = 0; i < kDistinctValues; ++i) {
        values.append(new QObject(&owner));
    }

    QStandardItemModel model;
    cwUniqueValueFilterModel filter;
    filter.setUniqueRole(kUniqueRole);
    filter.setLessThan([](const QVariant& left, const QVariant& right) {
        return std::less<QObject*>()(left.value<QObject*>(), right.value<QObject*>());
    });
    filter.setSourceModel(&model);
    REQUIRE(filter.rowCount() == 0);

    // The model contract: storage changes only after rowsAboutToBeInserted.
    QList<int> rowCountBeforeInsert;
    QList<int> firstInsertedRow;
    QObject::connect(&filter, &QAbstractItemModel::rowsAboutToBeInserted, &filter,
                     [&](const QModelIndex&, int first, int) {
        rowCountBeforeInsert.append(filter.rowCount());
        firstInsertedRow.append(first);
    });
    QSignalSpy inserted(&filter, &QAbstractItemModel::rowsInserted);

    QList<QStandardItem*> rows;
    for (int i = 0; i < kSourceRows; ++i) {
        auto* row = new QStandardItem();
        row->setData(QVariant::fromValue(values.at(i % kDistinctValues)), kUniqueRole);
        rows.append(row);
    }
    model.invisibleRootItem()->appendRows(rows);

    CHECK(filter.rowCount() == kDistinctValues);
    CHECK(inserted.count() == 1);
    REQUIRE_FALSE(rowCountBeforeInsert.isEmpty());
    for (int i = 0; i < rowCountBeforeInsert.size(); ++i) {
        INFO("insert " << i);
        CHECK(rowCountBeforeInsert.at(i) == firstInsertedRow.at(i));
    }
}

TEST_CASE("cwUniqueValueFilterModel forwards another role's change on a value's representative row",
          "[cwUniqueValueFilterModel]")
{
    constexpr int kUniqueRole = Qt::UserRole + 1;
    constexpr int kOtherRole = Qt::UserRole + 2;

    QObject a;
    QObject b;

    QStandardItemModel model;
    const auto appendValue = [&model](QObject* value) {
        auto* item = new QStandardItem();
        item->setData(QVariant::fromValue(value), kUniqueRole);
        model.appendRow(item);
        return item;
    };
    QStandardItem* aRepresentative = appendValue(&a);
    QStandardItem* aDuplicate = appendValue(&a);
    QStandardItem* bItem = appendValue(&b);

    cwUniqueValueFilterModel filter;
    filter.setUniqueRole(kUniqueRole);
    filter.setLessThan([](const QVariant& left, const QVariant& right) {
        return std::less<QObject*>()(left.value<QObject*>(), right.value<QObject*>());
    });
    filter.setSourceModel(&model);
    REQUIRE(filter.rowCount() == 2);

    const auto proxyRowOf = [&filter](const QObject* value) {
        for (int row = 0; row < filter.rowCount(); ++row) {
            if (filter.index(row).data(kUniqueRole).value<QObject*>() == value) {
                return row;
            }
        }
        return -1;
    };
    const int aRow = proxyRowOf(&a);
    const int bRow = proxyRowOf(&b);
    REQUIRE(aRow >= 0);
    REQUIRE(bRow >= 0);

    QSignalSpy dataChanged(&filter, &QAbstractItemModel::dataChanged);
    QSignalSpy rowsInserted(&filter, &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemoved(&filter, &QAbstractItemModel::rowsRemoved);

    const auto checkOneChangeOn = [&dataChanged](int row) {
        REQUIRE(dataChanged.count() == 1);
        const QList<QVariant> arguments = dataChanged.takeFirst();
        CHECK(arguments.at(0).toModelIndex().row() == row);
        CHECK(arguments.at(1).toModelIndex().row() == row);
        CHECK(arguments.at(2).value<QList<int>>() == QList<int>({kOtherRole}));
    };

    SECTION("An edit on the representative row reaches its proxy row") {
        aRepresentative->setData(QStringLiteral("edited"), kOtherRole);
        checkOneChangeOn(aRow);
        CHECK(filter.index(aRow).data(kOtherRole).toString() == QStringLiteral("edited"));
    }

    SECTION("An edit on a duplicate row emits nothing") {
        aDuplicate->setData(QStringLiteral("edited"), kOtherRole);
        CHECK(dataChanged.isEmpty());
    }

    SECTION("An edit on another value's row reaches that value's proxy row") {
        bItem->setData(QStringLiteral("edited"), kOtherRole);
        checkOneChangeOn(bRow);
    }

    SECTION("The former duplicate forwards once it becomes the representative") {
        model.removeRow(aRepresentative->row());
        REQUIRE(filter.rowCount() == 2);
        REQUIRE(proxyRowOf(&a) == aRow);
        rowsInserted.clear();
        rowsRemoved.clear();

        aDuplicate->setData(QStringLiteral("edited"), kOtherRole);
        checkOneChangeOn(aRow);
    }

    // Another role leaves the unique rows in place.
    CHECK(rowsInserted.isEmpty());
    CHECK(rowsRemoved.isEmpty());
    CHECK(filter.rowCount() == 2);
}
