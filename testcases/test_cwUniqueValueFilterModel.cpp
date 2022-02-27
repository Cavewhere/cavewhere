//Catch includes
#include "catch.hpp"

//Our includes
#include "cwUniqueValueFilterModel.h"
#include "SpyChecker.h"

//Qt includes
#include <QStandardItemModel>
#include <QVector>
#include <QStandardItem>

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

    for(auto item : qAsConst(items)) {
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

        for(auto obj : qAsConst(filterObjs)) {
            CHECK(modelContains(obj));
        }
    };

    auto spyChecker = SpyChecker::makeChecker(&filter);

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
        return left.value<QObject*>() < right.value<QObject*>();
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

    for(auto obj : qAsConst(objs)) {
        delete obj;
    }
}

