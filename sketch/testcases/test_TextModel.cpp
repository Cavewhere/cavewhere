//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our inculdes
#include "TextModel.h"

using namespace cwSketch;

TEST_CASE("TextModel initial state", "[TextModel]") {
    TextModel model;
    REQUIRE(model.rowCount() == 0);
    auto roles = model.roleNames();
    REQUIRE(roles.contains(TextModel::TextRole));
    REQUIRE(roles.contains(TextModel::PositionRole));
    REQUIRE(roles.contains(TextModel::FontRole));
    REQUIRE(roles.contains(TextModel::FillColorRole));
    REQUIRE(roles.contains(TextModel::StrokeColorRole));
}

TEST_CASE("Adding TextRow entries emits rowsInserted and data is retrievable", "[TextModel]") {
    TextModel model;
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);

    TextModel::TextRow row;
    row.text = "Hello";
    row.position = QPointF(10.0, 20.0);
    row.font = QFont("Arial", 12);
    row.fillColor = QColor(Qt::red);
    row.strokeColor = QColor(Qt::blue);

    model.addText(0, row);
    REQUIRE(insertedSpy.count() == 1);
    REQUIRE(model.rowCount() == 1);

    QModelIndex idx = model.index(0, 0);
    REQUIRE(idx.isValid());
    CHECK(model.data(idx, TextModel::TextRole).toString() == "Hello");
}

TEST_CASE("setData updates values and emits dataChanged", "[TextModel]") {
    TextModel model;
    TextModel::TextRow row{"Orig", QPointF(0,0), QFont(), QColor(), QColor()};
    model.addText(0, row);

    QModelIndex idx = model.index(0, 0);
    QSignalSpy dataSpy(&model, &TextModel::dataChanged);

    bool result = model.setData(idx, QVariant("Updated"), TextModel::TextRole);
    REQUIRE(result);
    REQUIRE(dataSpy.count() == 1);
    CHECK(model.data(idx, TextModel::TextRole).toString() == "Updated");
}

TEST_CASE("Removing, setting rows, and clearing model emit proper signals", "[TextModel]") {
    TextModel model;
    QVector<TextModel::TextRow> rows;
    for (int i = 0; i < 3; ++i) {
        TextModel::TextRow r;
        r.text = QString("Item%1").arg(i);
        rows.append(r);
    }

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setRows(rows);
    REQUIRE(resetSpy.count() == 1);
    REQUIRE(model.rowCount() == 3);

    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeText(model.index(1, 0));
    REQUIRE(removedSpy.count() == 1);
    REQUIRE(model.rowCount() == 2);
    CHECK(model.data(model.index(1, 0), TextModel::TextRole).toString() == "Item2");

    QSignalSpy clearSpy(&model, &QAbstractItemModel::modelReset);
    model.clear();
    REQUIRE(clearSpy.count() == 1);
    REQUIRE(model.rowCount() == 0);
}

TEST_CASE("removeText(index, count) removes multiple rows and emits rowsRemoved", "[TextModel]") {
    TextModel model;

    // Prepare 5 rows
    QVector<TextModel::TextRow> rows;
    for (int i = 0; i < 5; ++i) {
        TextModel::TextRow r;
        r.text = QString("Item%1").arg(i);
        rows.append(r);
    }
    model.setRows(rows);
    REQUIRE(model.rowCount() == 5);

    // Spy for removal signals
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);

    // Remove 2 rows starting at row 1 ("Item1" and "Item2")
    QModelIndex startIdx = model.index(1, 0);
    model.removeText(startIdx, 2);

    // Expect one rowsRemoved() emission covering rows 1..2
    REQUIRE(removedSpy.count() == 1);
    QList<QVariant> args = removedSpy.takeFirst();
    // args: parent, first, last
    QCOMPARE(args.at(1).toInt(), 1);
    QCOMPARE(args.at(2).toInt(), 2);

    // Remaining count should be 3
    REQUIRE(model.rowCount() == 3);

    // Check that new row 1 is the original "Item3"
    QModelIndex newIdx = model.index(1, 0);
    REQUIRE(newIdx.isValid());
    CHECK(model.data(newIdx, TextModel::TextRole).toString() == "Item3");

    // Removing zero rows should do nothing
    removedSpy.clear();
    model.removeText(model.index(0,0), 0);
    REQUIRE(removedSpy.isEmpty());
    REQUIRE(model.rowCount() == 3);
}

TEST_CASE("addText(index, rows) inserts multiple rows and emits rowsInserted", "[TextModel]") {
    TextModel model;

    // Prepare initial rows: Item0, Item1, Item2
    QVector<TextModel::TextRow> initial;
    for (int i = 0; i < 3; ++i) {
        TextModel::TextRow r;
        r.text = QString("Item%1").arg(i);
        initial.append(r);
    }
    model.setRows(initial);
    REQUIRE(model.rowCount() == 3);

    // Spy on rowsInserted
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);

    // New rows to insert: "X", "Y"
    QVector<TextModel::TextRow> newRows;
    {
        TextModel::TextRow r1; r1.text = "X";
        TextModel::TextRow r2; r2.text = "Y";
        newRows << r1 << r2;
    }

    // Insert at position 1
    // QModelIndex idx = model.index(1, 0);
    model.addText(1, newRows);

    // Should have emitted one rowsInserted(first=1, last=2)
    REQUIRE(insertSpy.count() == 1);
    {
        auto args = insertSpy.takeFirst();
        QCOMPARE(args.at(1).toInt(), 1);
        QCOMPARE(args.at(2).toInt(), 2);
    }

    // Model should now have 5 rows: Item0, X, Y, Item1, Item2
    REQUIRE(model.rowCount() == 5);
    CHECK(model.data(model.index(1,0), TextModel::TextRole).toString() == "X");
    CHECK(model.data(model.index(2,0), TextModel::TextRole).toString() == "Y");
    CHECK(model.data(model.index(3,0), TextModel::TextRole).toString() == "Item1");

    // // Inserting with an invalid index should do nothing
    // model.addText(QModelIndex(), newRows);
    // REQUIRE(insertSpy.isEmpty());
    // REQUIRE(model.rowCount() == 5);

    // // Inserting an empty vector should do nothing
    // model.addText(idx, QVector<TextModel::TextRow>{});
    // REQUIRE(insertSpy.isEmpty());
    // REQUIRE(model.rowCount() == 5);
}

static void verifyModelMatchesRows(const TextModel &model, const QVector<TextModel::TextRow> &rows) {
    REQUIRE(model.rowCount() == rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        INFO("i:" << i);
        QModelIndex idx = model.index(i, 0);
        REQUIRE(idx.isValid());
        CHECK(model.data(idx, TextModel::TextRole).toString().toStdString() == rows.at(i).text.toStdString());
    }
}

TEST_CASE("replaceRows edge cases", "[TextModel]") {
    TextModel model;

    SECTION("1. identical rows (same size)") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" } };
        QVector<TextModel::TextRow> rows    = initial;

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        // No change expected
        model.replaceRows(rows);

        // No rows inserted or removed
        REQUIRE(rowsInsertedSpy.count() == 0);
        REQUIRE(rowsRemovedSpy.count() == 0);
        // Data should be marked changed for all rows once
        REQUIRE(dataChangedSpy.count() == 1);

        // Verify the signal's range corresponds to all rows
        {
            auto args = dataChangedSpy.takeFirst();
            const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
            const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
            CHECK(topLeft.row() == 0);
            CHECK(bottomRight.row() == initial.size() - 1);
        }

        verifyModelMatchesRows(model, rows);
    }

    SECTION("2. partially equal data (same size)") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" } };
        QVector<TextModel::TextRow> rows    = { { "A" }, { "X" }, { "C" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // No insertions or removals on equal size
        REQUIRE(rowsInsertedSpy.count() == 0);
        REQUIRE(rowsRemovedSpy.count() == 0);
        // Data should be marked changed for all rows once
        REQUIRE(dataChangedSpy.count() == 1);

        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == initial.size() - 1);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("3. completely different data (same size)") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" } };
        QVector<TextModel::TextRow> rows    = { { "X" }, { "Y" }, { "Z" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        REQUIRE(rowsInsertedSpy.count() == 0);
        REQUIRE(rowsRemovedSpy.count() == 0);
        REQUIRE(dataChangedSpy.count() == 1);

        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == initial.size() - 1);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("4. equal at beginning, new at end (rows.size() > model.size())") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" } };
        QVector<TextModel::TextRow> rows    = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one insertion call for indices 3..4
        REQUIRE(rowsInsertedSpy.count() == 1);
        auto insertArgs = rowsInsertedSpy.takeFirst();
        const QModelIndex &parentIdxIns = insertArgs.at(0).value<QModelIndex>();
        int firstIns = insertArgs.at(1).toInt();
        int lastIns  = insertArgs.at(2).toInt();
        CHECK(parentIdxIns == QModelIndex());
        CHECK(firstIns == initial.size());
        CHECK(lastIns  == rows.size() - 1);

        // DataChanged should have been emitted once for old rows 0..2
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == initial.size() - 1);

        REQUIRE(rowsRemovedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("5. equal at end, new at beginning (rows.size() > model.size())") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" } };
        QVector<TextModel::TextRow> rows    = { { "X" }, { "Y" }, { "A" }, { "B" }, { "C" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one insertion call for indices 0..1
        REQUIRE(rowsInsertedSpy.count() == 1);
        auto insertArgs = rowsInsertedSpy.takeFirst();
        const QModelIndex &parentIdxIns = insertArgs.at(0).value<QModelIndex>();
        int firstIns = insertArgs.at(1).toInt();
        int lastIns  = insertArgs.at(2).toInt();
        CHECK(parentIdxIns == QModelIndex());
        CHECK(firstIns == 3);
        CHECK(lastIns  == 4);

        // DataChanged should have been emitted once for old rows now shifted 2..4
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == initial.size() - 1);

        REQUIRE(rowsRemovedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("6. equal in middle, new at beginning and end (rows.size() > model.size())") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" } };
        QVector<TextModel::TextRow> rows    = { { "X" }, { "A" }, { "B" }, { "C" }, { "Y" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one insertion call for indices 0..1
        REQUIRE(rowsInsertedSpy.count() == 1);
        auto insertArgs = rowsInsertedSpy.takeFirst();
        const QModelIndex &parentIdxIns = insertArgs.at(0).value<QModelIndex>();
        int firstIns = insertArgs.at(1).toInt();
        int lastIns  = insertArgs.at(2).toInt();
        CHECK(parentIdxIns == QModelIndex());
        CHECK(firstIns == 3);
        CHECK(lastIns  == 4);

        // DataChanged should have been emitted once for old rows now shifted 2..4
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == initial.size() - 1);

        REQUIRE(rowsRemovedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("7. equal at beginning, removed from end (rows.size() < model.size())") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };
        QVector<TextModel::TextRow> rows    = { { "A" }, { "B" }, { "C" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one removal call for indices 3..4
        REQUIRE(rowsRemovedSpy.count() == 1);
        auto removeArgs = rowsRemovedSpy.takeFirst();
        const QModelIndex &parentIdxRem = removeArgs.at(0).value<QModelIndex>();
        int firstRem = removeArgs.at(1).toInt();
        int lastRem  = removeArgs.at(2).toInt();
        CHECK(parentIdxRem == QModelIndex());
        CHECK(firstRem == rows.size());
        CHECK(lastRem  == initial.size() - 1);

        // DataChanged should have been emitted once for remaining rows 0..2
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == rows.size() - 1);

        REQUIRE(rowsInsertedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("8. equal at end, removed from beginning (rows.size() < model.size())") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };
        QVector<TextModel::TextRow> rows    = { { "C" }, { "D" }, { "E" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one removal call for indices 0..1
        REQUIRE(rowsRemovedSpy.count() == 1);
        auto removeArgs = rowsRemovedSpy.takeFirst();
        const QModelIndex &parentIdxRem = removeArgs.at(0).value<QModelIndex>();
        int firstRem = removeArgs.at(1).toInt();
        int lastRem  = removeArgs.at(2).toInt();
        CHECK(parentIdxRem == QModelIndex());
        CHECK(firstRem == 3);
        CHECK(lastRem  == 4);

        // DataChanged should have been emitted once for remaining rows now shifted to 0..2
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == rows.size() - 1);

        REQUIRE(rowsInsertedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("9. equal in middle, removed from both ends (rows.size() < model.size())") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };
        QVector<TextModel::TextRow> rows    = { { "B" }, { "C" }, { "D" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        REQUIRE(rowsRemovedSpy.count() == 1);
        auto removeArgs = rowsRemovedSpy.takeFirst();
        const QModelIndex &parentIdxRem = removeArgs.at(0).value<QModelIndex>();
        int firstRem = removeArgs.at(1).toInt();
        int lastRem  = removeArgs.at(2).toInt();
        CHECK(parentIdxRem == QModelIndex());
        CHECK(firstRem == 3);
        CHECK(lastRem  == 4);

        // DataChanged should have been emitted once for remaining rows now shifted to 0..2
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == rows.size() - 1);

        REQUIRE(rowsInsertedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("10. equal in middle, removed at beginning, added at end") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };
        QVector<TextModel::TextRow> rows    = { { "B" }, { "C" }, { "D" }, { "E" }, { "F" }, { "G" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one insertion call for indices 0..1
        REQUIRE(rowsInsertedSpy.count() == 1);
        auto insertArgs = rowsInsertedSpy.takeFirst();
        const QModelIndex &parentIdxIns = insertArgs.at(0).value<QModelIndex>();
        int firstIns = insertArgs.at(1).toInt();
        int lastIns  = insertArgs.at(2).toInt();
        CHECK(parentIdxIns == QModelIndex());
        CHECK(firstIns == 5);
        CHECK(lastIns  == 5);

        // DataChanged should have been emitted once for old rows now shifted 2..4
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == initial.size() - 1);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("11. equal in middle, added at beginning, removed at end") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };
        QVector<TextModel::TextRow> rows    = { { "G" }, { "X" }, { "Y" }, { "A" }, { "B" }, { "C" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one insertion call for indices 0..1
        REQUIRE(rowsInsertedSpy.count() == 1);
        auto insertArgs = rowsInsertedSpy.takeFirst();
        const QModelIndex &parentIdxIns = insertArgs.at(0).value<QModelIndex>();
        int firstIns = insertArgs.at(1).toInt();
        int lastIns  = insertArgs.at(2).toInt();
        CHECK(parentIdxIns == QModelIndex());
        CHECK(firstIns == 5);
        CHECK(lastIns  == 5);

        // DataChanged should have been emitted once for old rows now shifted 2..4
        REQUIRE(dataChangedSpy.count() == 1);
        auto args = dataChangedSpy.takeFirst();
        const QModelIndex &topLeft     = args.at(0).value<QModelIndex>();
        const QModelIndex &bottomRight = args.at(1).value<QModelIndex>();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == initial.size() - 1);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("12. Replace with empty") {
        QVector<TextModel::TextRow> initial = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };
        QVector<TextModel::TextRow> rows    = {};

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one removal call for indices 0..4
        REQUIRE(rowsRemovedSpy.count() == 1);
        auto removeArgs = rowsRemovedSpy.takeFirst();
        const QModelIndex &parentIdxRem = removeArgs.at(0).value<QModelIndex>();
        int firstRem = removeArgs.at(1).toInt();
        int lastRem  = removeArgs.at(2).toInt();
        CHECK(parentIdxRem == QModelIndex());
        CHECK(firstRem == 0);
        CHECK(lastRem  == initial.size() - 1);

        // No dataChanged for empty target
        REQUIRE(dataChangedSpy.count() == 0);
        REQUIRE(rowsInsertedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }

    SECTION("13. Replace with full") {
        QVector<TextModel::TextRow> initial = {};
        QVector<TextModel::TextRow> rows    = { { "A" }, { "B" }, { "C" }, { "D" }, { "E" } };

        model.setRows(initial);

        QSignalSpy dataChangedSpy(&model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(const QModelIndex&,int,int)));
        QSignalSpy rowsRemovedSpy(&model, SIGNAL(rowsRemoved(const QModelIndex&,int,int)));

        model.replaceRows(rows);

        // Expect one insertion call for indices 0..4
        REQUIRE(rowsInsertedSpy.count() == 1);
        auto insertArgs = rowsInsertedSpy.takeFirst();
        const QModelIndex &parentIdxIns = insertArgs.at(0).value<QModelIndex>();
        int firstIns = insertArgs.at(1).toInt();
        int lastIns  = insertArgs.at(2).toInt();
        CHECK(parentIdxIns == QModelIndex());
        CHECK(firstIns == 0);
        CHECK(lastIns  == rows.size() - 1);

        // No dataChanged because no preexisting rows
        REQUIRE(dataChangedSpy.count() == 0);
        REQUIRE(rowsRemovedSpy.count() == 0);

        verifyModelMatchesRows(model, rows);
    }
}

