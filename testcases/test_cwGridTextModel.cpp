//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QFont>
#include <QSignalSpy>

//Our includes
#include "cwGridTextModel.h"

namespace {
cwGridTextModel::TextRow makeRow(const QString &text, const QPointF &pos = QPointF()) {
    cwGridTextModel::TextRow row;
    row.text = text;
    row.position = pos;
    row.font = QFont();
    row.fillColor = Qt::black;
    row.strokeColor = Qt::transparent;
    return row;
}
} // namespace

TEST_CASE("cwGridTextModel defaults", "[cwGridTextModel]") {
    cwGridTextModel model;
    CHECK(model.rowCount() == 0);
    CHECK(model.rows().isEmpty());

    const auto names = model.roleNames();
    CHECK(names.value(cwGridTextModel::TextRole)        == "textRole");
    CHECK(names.value(cwGridTextModel::PositionRole)    == "positionRole");
    CHECK(names.value(cwGridTextModel::FontRole)        == "fontRole");
    CHECK(names.value(cwGridTextModel::FillColorRole)   == "fillColorRole");
    CHECK(names.value(cwGridTextModel::StrokeColorRole) == "strokeColorRole");
}

TEST_CASE("cwGridTextModel addText inserts a single row", "[cwGridTextModel]") {
    cwGridTextModel model;
    QSignalSpy spy(&model, &QAbstractItemModel::rowsInserted);

    model.addText(0, makeRow("A"));

    REQUIRE(model.rowCount() == 1);
    REQUIRE(spy.count() == 1);
    auto args = spy.takeFirst();
    CHECK(args.at(1).toInt() == 0);
    CHECK(args.at(2).toInt() == 0);

    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "A");
}

TEST_CASE("cwGridTextModel addText inserts a batch of rows", "[cwGridTextModel]") {
    cwGridTextModel model;
    QSignalSpy spy(&model, &QAbstractItemModel::rowsInserted);

    QVector<cwGridTextModel::TextRow> batch{ makeRow("A"), makeRow("B"), makeRow("C") };
    model.addText(0, batch);

    REQUIRE(model.rowCount() == 3);
    REQUIRE(spy.count() == 1);
    auto args = spy.takeFirst();
    CHECK(args.at(1).toInt() == 0);
    CHECK(args.at(2).toInt() == 2);

    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "A");
    CHECK(model.data(model.index(2), cwGridTextModel::TextRole).toString() == "C");
}

TEST_CASE("cwGridTextModel removeText removes a single row", "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A"), makeRow("B"), makeRow("C") });
    REQUIRE(model.rowCount() == 3);

    QSignalSpy spy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeText(model.index(1));

    REQUIRE(model.rowCount() == 2);
    REQUIRE(spy.count() == 1);
    auto args = spy.takeFirst();
    CHECK(args.at(1).toInt() == 1);
    CHECK(args.at(2).toInt() == 1);

    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "A");
    CHECK(model.data(model.index(1), cwGridTextModel::TextRole).toString() == "C");
}

TEST_CASE("cwGridTextModel removeText removes a contiguous range", "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A"), makeRow("B"), makeRow("C"), makeRow("D") });
    REQUIRE(model.rowCount() == 4);

    QSignalSpy spy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeText(model.index(1), 2);

    REQUIRE(model.rowCount() == 2);
    REQUIRE(spy.count() == 1);
    auto args = spy.takeFirst();
    CHECK(args.at(1).toInt() == 1);
    CHECK(args.at(2).toInt() == 2);

    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "A");
    CHECK(model.data(model.index(1), cwGridTextModel::TextRole).toString() == "D");
}

TEST_CASE("cwGridTextModel setRows fully resets the model", "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A"), makeRow("B") });

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setRows({ makeRow("X") });

    CHECK(resetSpy.count() == 1);
    CHECK(model.rowCount() == 1);
    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "X");
}

TEST_CASE("cwGridTextModel clear empties via modelReset", "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A"), makeRow("B") });

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.clear();

    CHECK(resetSpy.count() == 1);
    CHECK(model.rowCount() == 0);
}

TEST_CASE("cwGridTextModel replaceRows grows via rowsInserted + dataChanged",
          "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A"), makeRow("B") });

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

    model.replaceRows({ makeRow("X"), makeRow("Y"), makeRow("Z"), makeRow("W") });

    CHECK(resetSpy.count() == 0);
    CHECK(removedSpy.count() == 0);
    REQUIRE(insertedSpy.count() == 1);
    auto insArgs = insertedSpy.takeFirst();
    CHECK(insArgs.at(1).toInt() == 2);
    CHECK(insArgs.at(2).toInt() == 3);

    // The pre-existing rows got overwritten in place → one dataChanged span.
    REQUIRE(dataSpy.count() == 1);
    auto dataArgs = dataSpy.takeFirst();
    CHECK(dataArgs.at(0).value<QModelIndex>() == model.index(0));
    CHECK(dataArgs.at(1).value<QModelIndex>() == model.index(1));

    REQUIRE(model.rowCount() == 4);
    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "X");
    CHECK(model.data(model.index(3), cwGridTextModel::TextRole).toString() == "W");
}

TEST_CASE("cwGridTextModel replaceRows shrinks via rowsRemoved + dataChanged",
          "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A"), makeRow("B"), makeRow("C"), makeRow("D") });

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

    model.replaceRows({ makeRow("X"), makeRow("Y") });

    CHECK(resetSpy.count() == 0);
    CHECK(insertedSpy.count() == 0);
    REQUIRE(removedSpy.count() == 1);
    auto remArgs = removedSpy.takeFirst();
    CHECK(remArgs.at(1).toInt() == 2);
    CHECK(remArgs.at(2).toInt() == 3);

    // Surviving rows got overwritten → one dataChanged span.
    REQUIRE(dataSpy.count() == 1);
    auto dataArgs = dataSpy.takeFirst();
    CHECK(dataArgs.at(0).value<QModelIndex>() == model.index(0));
    CHECK(dataArgs.at(1).value<QModelIndex>() == model.index(1));

    REQUIRE(model.rowCount() == 2);
    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "X");
    CHECK(model.data(model.index(1), cwGridTextModel::TextRole).toString() == "Y");
}

TEST_CASE("cwGridTextModel replaceRows same-size emits only dataChanged",
          "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A"), makeRow("B") });

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

    model.replaceRows({ makeRow("X"), makeRow("Y") });

    CHECK(resetSpy.count() == 0);
    CHECK(insertedSpy.count() == 0);
    CHECK(removedSpy.count() == 0);
    REQUIRE(dataSpy.count() == 1);
    auto dataArgs = dataSpy.takeFirst();
    CHECK(dataArgs.at(0).value<QModelIndex>() == model.index(0));
    CHECK(dataArgs.at(1).value<QModelIndex>() == model.index(1));

    REQUIRE(model.rowCount() == 2);
    CHECK(model.data(model.index(0), cwGridTextModel::TextRole).toString() == "X");
    CHECK(model.data(model.index(1), cwGridTextModel::TextRole).toString() == "Y");
}

TEST_CASE("cwGridTextModel data returns correct value per role", "[cwGridTextModel]") {
    cwGridTextModel model;
    cwGridTextModel::TextRow row;
    row.text = "Hello";
    row.position = QPointF(1.5, 2.5);
    row.font = QFont("Arial", 14);
    row.fillColor = Qt::red;
    row.strokeColor = Qt::blue;
    model.setRows({ row });

    const QModelIndex idx = model.index(0);
    CHECK(model.data(idx, cwGridTextModel::TextRole).toString() == "Hello");
    CHECK(model.data(idx, cwGridTextModel::PositionRole).toPointF() == QPointF(1.5, 2.5));
    CHECK(model.data(idx, cwGridTextModel::FontRole).value<QFont>().family() == QFont("Arial").family());
    CHECK(model.data(idx, cwGridTextModel::FillColorRole).value<QColor>() == QColor(Qt::red));
    CHECK(model.data(idx, cwGridTextModel::StrokeColorRole).value<QColor>() == QColor(Qt::blue));

    // Invalid index
    CHECK_FALSE(model.data(model.index(1), cwGridTextModel::TextRole).isValid());
}

TEST_CASE("cwGridTextModel setData mutates and emits dataChanged with role",
          "[cwGridTextModel]") {
    cwGridTextModel model;
    model.setRows({ makeRow("A") });

    QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

    const QModelIndex idx = model.index(0);
    REQUIRE(model.setData(idx, QString("B"), cwGridTextModel::TextRole));

    REQUIRE(spy.count() == 1);
    auto args = spy.takeFirst();
    CHECK(args.at(0).value<QModelIndex>() == idx);
    const auto roles = args.at(2).value<QVector<int>>();
    CHECK(roles.contains(cwGridTextModel::TextRole));

    CHECK(model.data(idx, cwGridTextModel::TextRole).toString() == "B");
}
