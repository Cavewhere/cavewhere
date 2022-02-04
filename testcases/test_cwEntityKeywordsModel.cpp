//Our includes
#include "cwEntityAndKeywords.h"
#include "cwEntityKeywordsModel.h"
#include "SpyChecker.h"

//Catch includes
#include "catch.hpp"

//Qt includes
#include <QSignalSpy>

TEST_CASE("cwEntityKeywordsModel should initilize correctly", "[cwEntityKeywordsModel]") {
    cwEntityKeywordsModel model;
    CHECK(model.rowCount() == 0);
}

TEST_CASE("cwEntityKeywordsModel should add and remove cwEntityAndKeywords correctly", "[cwEntityKeywordsModel]") {

    cwEntityKeywordsModel model;

    auto spyChecker = SpyChecker::makeChecker(&model);

    struct Row {
        QSharedPointer<QObject> obj;
        QVector<cwKeyword> keywords;

        bool operator<(const Row& other) const {
            return obj < other.obj;
        }
    };

    QVector<Row> rows;
    for(int i = 0; i < 5; i++) {
        rows.append({QSharedPointer<QObject>::create(), {cwKeyword(QString("key%1").arg(i),
                                     QString("value%1").arg(i))}});
    }
    std::sort(rows.begin(), rows.end());

    auto checkRows = [&model](const QVector<Row>& rows) {
        REQUIRE(model.rowCount() == rows.size());
        for(int i = 0; i < model.rowCount(); i++) {
            auto index = model.index(i);
            const auto& row = rows.at(i);

            auto entryAndKeyword = index.data(cwEntityKeywordsModel::EntityKeywordsRole).value<cwEntityAndKeywords>();

            CHECK(entryAndKeyword.entity() == row.obj);
            CHECK(entryAndKeyword.keywords() == row.keywords);

            CHECK(index.data(cwEntityKeywordsModel::EntityRole).value<QObject*>() == row.obj);
            CHECK(index.data(cwEntityKeywordsModel::KeywordsRole).value<QVector<cwKeyword>>() == row.keywords);
        }
    };

    //Out of order insertion
    auto insert = [&model, &rows](int i) {
        model.insert({rows[i].obj.data(), rows[i].keywords});
    };

    insert(1);
    insert(3);
    insert(4);
    insert(2);
    insert(0);

    auto rowsInsertedSpy = spyChecker.findSpy(&cwEntityKeywordsModel::rowsInserted);
    auto rowsAboutToBeInsertedSpy = spyChecker.findSpy(&cwEntityKeywordsModel::rowsAboutToBeInserted);
    spyChecker[rowsInsertedSpy] = 5;
    spyChecker[rowsAboutToBeInsertedSpy] = 5;
    spyChecker.checkSpies();

    checkRows(rows);

    SECTION("Remove rows") {
        model.remove(rows.at(2).obj.data());
        rows.removeAt(2);

        auto rowsRemovedSpy = spyChecker.findSpy(&cwEntityKeywordsModel::rowsRemoved);
        auto rowsAboutToBeRemovedSpy = spyChecker.findSpy(&cwEntityKeywordsModel::rowsAboutToBeRemoved);
        spyChecker[rowsRemovedSpy] = 1;
        spyChecker[rowsAboutToBeRemovedSpy] = 1;
        spyChecker.checkSpies();
        checkRows(rows);
    }

    SECTION("Update data") {
        rows[3].keywords.append({QString("bestKey"), QString("bestValue")});
        insert(3);

        auto dataChangedSpy = spyChecker.findSpy(&cwEntityKeywordsModel::dataChanged);
        spyChecker[dataChangedSpy] = 1;

        checkRows(rows);
    }
}
