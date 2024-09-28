//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include "cwSignalSpy.h"
#include <QHash>

//Our includes
#include "cwCSVLineModel.h"
#include "cwColumnNameModel.h"

//Test includes
#include "SpyChecker.h"

TEST_CASE("cwCSVLineModel should initilize it's self correctly", "[cwCSVLineModel]") {
    cwCSVLineModel model;
    CHECK(model.lines().isEmpty() == true);
    CHECK(model.columnModel() == nullptr);
    CHECK(model.rowCount() == 0);
    CHECK(model.columnCount() == 0);
}

TEST_CASE("cwCSVLineModel should return the correct cells", "[cwCSVLineModel]") {

    QList<QStringList> lines = {
        {"a", "b", "c"},
        {"a1", "b2", "c3"},
        {"a12", "b23", "c34"},
        {"a123", "b234", "c345"}
    };

    cwColumnNameModel columnModel;
    columnModel.append(cwColumnName("First", 0));
    columnModel.append(cwColumnName("Second", 1));
    columnModel.append(cwColumnName("Third", 2));

    cwCSVLineModel model;

    auto checkRows = [&]() {
        for(int i = 0; i < model.rowCount(); i++) {
            if(i == 0) {
                for(int c = 0; c < model.columnCount(); c++) {
                    QModelIndex index = model.index(i, c);
                    CHECK(index.data().toString().toStdString() == columnModel.at(c).name().toStdString());
                }
            } else {
                int ii = i - 1;
                for(int c = 0; c < model.columnCount(); c++) {
                    QModelIndex index = model.index(i, c);
                    QString str = c < lines.at(ii).size() ? lines.at(ii).at(c) : QString();
                    CHECK(index.data().toString().toStdString() == str.toStdString());
                }
            }
        }
    };

    cwSignalSpy resetSpy(&model, SIGNAL(modelReset()));
    cwSignalSpy rowsRemoveSpy(&model, SIGNAL(rowsRemoved(QModelIndex, int, int)));
    cwSignalSpy rowsInsertedSpy(&model, SIGNAL(rowsInserted(QModelIndex, int, int)));
    cwSignalSpy columnsRemoveSpy(&model, SIGNAL(columnsRemoved(QModelIndex, int, int)));
    cwSignalSpy columnsInsertedSpy(&model, SIGNAL(columnsInserted(QModelIndex, int, int)));

    SpyChecker spyCounts {
        {&resetSpy, 0},
        {&rowsRemoveSpy, 0},
        {&rowsInsertedSpy, 0},
        {&columnsRemoveSpy, 0},
        {&columnsInsertedSpy, 0}
    };

    model.setLines(lines);
    spyCounts[&resetSpy] = 1;
    spyCounts.checkSpies();
    spyCounts.clearSpyCounts();

    CHECK(model.rowCount() == 0);
    CHECK(model.columnCount() == 0);

    model.setColumnModel(&columnModel);
    spyCounts[&resetSpy] = 1;
    spyCounts.checkSpies();
    spyCounts.clearSpyCounts();

    CHECK(model.rowCount() == lines.size() + 1);
    CHECK(model.columnCount() == columnModel.size());

    checkRows();

    //Insert a new column
    columnModel.insert(1, cwColumnName("Inserted", 4));

    spyCounts[&columnsInsertedSpy] = 1;
    spyCounts.requireSpies();

    CHECK(columnsInsertedSpy.first().at(0).toModelIndex() == QModelIndex());
    CHECK(columnsInsertedSpy.first().at(1).toInt() == 1);
    CHECK(columnsInsertedSpy.first().at(2).toInt() == 1);

    spyCounts.clearSpyCounts();
    checkRows();

    //Remove columns
    columnModel.remove(0);
    columnModel.remove(2);

    spyCounts[&columnsRemoveSpy] = 2;
    spyCounts.requireSpies();

    CHECK(columnsRemoveSpy.at(0).at(0).toModelIndex() == QModelIndex());
    CHECK(columnsRemoveSpy.at(0).at(1).toInt() == 0);
    CHECK(columnsRemoveSpy.at(0).at(2).toInt() == 0);

    CHECK(columnsRemoveSpy.at(1).at(0).toModelIndex() == QModelIndex());
    CHECK(columnsRemoveSpy.at(1).at(1).toInt() == 2);
    CHECK(columnsRemoveSpy.at(1).at(2).toInt() == 2);

    checkRows();
}
