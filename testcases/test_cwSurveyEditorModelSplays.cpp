/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyEditorModel.h"
#include "cwTrip.h"

//Test includes
#include "SplayFixtureHelper.h"

namespace {

/**
 * A trip of one chunk with stations a1, a2, a3, where a2 carries the three
 * splays from the TopoDroid fixture.
 *
 * Row counts are read through checkRowCount, which also holds the model to what
 * it told the view: the rows announced through beginInsertRows and
 * beginRemoveRows have to add up to the rows the model really gained or lost.
 * Splay clusters make that easy to get wrong, and a view that's told the wrong
 * range shows stale rows rather than failing.
 */
struct SplayFixture {
    SplayFixture()
        : chunk(new cwSurveyChunk())
    {
        chunk->appendShot(cwStation("a1"), cwStation("a2"), cwShot("10.52", "52.2", "232.2", "-31.5", "31.5"));
        chunk->appendShot(cwStation("a2"), cwStation("a3"), cwShot("11.34", "62.2", "242.2", "-21.5", "21.5"));
        chunk->setStationSplays(1, a4Splays());
        trip.addChunk(chunk);
        model.setTrip(&trip);

        QObject::connect(&model, &QAbstractItemModel::rowsInserted, &model,
                         [this](const QModelIndex&, int first, int last) {
                             announcedRows += last - first + 1;
                         });
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &model,
                         [this](const QModelIndex&, int first, int last) {
                             announcedRows -= last - first + 1;
                         });

        //A reset tells the view to throw every row away, so the tally restarts
        QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [this]() {
            lastCheckedRowCount = model.rowCount();
            announcedRows = 0;
        });

        lastCheckedRowCount = model.rowCount();
    }

    void checkRowCount(int expected)
    {
        CHECK(model.rowCount() == expected);
        CHECK(lastCheckedRowCount + announcedRows == expected);

        lastCheckedRowCount = model.rowCount();
        announcedRows = 0;
    }

    cwSurveyEditorRowIndex stationRow(int stationIndex) const
    {
        return cwSurveyEditorRowIndex(chunk, stationIndex, cwSurveyEditorRowIndex::StationRow);
    }

    QVariant rowData(int row, cwSurveyEditorModel::Role role) const
    {
        return model.index(row).data(role);
    }

    cwSurveyEditorRowIndex rowIndexOf(int row) const
    {
        return rowData(row, cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();
    }

    cwTrip trip;
    cwSurveyChunk* chunk;
    cwSurveyEditorModel model;
    int lastCheckedRowCount = 0;
    int announcedRows = 0;
};

}

TEST_CASE("A station reports how many splays it carries", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    //title, a1, shot, a2, shot, a3
    fixture.checkRowCount(6);

    CHECK(fixture.rowData(1, cwSurveyEditorModel::StationSplayCountRole).toInt() == 0);
    CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 3);

    //Clusters start closed, so a trip full of splays opens as it always has
    CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
}

TEST_CASE("Expanding a station shows its splays under it", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    fixture.checkRowCount(9);
    CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());

    SECTION("the splay rows sit between the station and its shot") {
        CHECK(fixture.rowIndexOf(3).rowType() == cwSurveyEditorRowIndex::StationRow);
        for(int splayIndex = 0; splayIndex < 3; ++splayIndex) {
            const auto rowIndex = fixture.rowIndexOf(4 + splayIndex);
            CHECK(rowIndex.rowType() == cwSurveyEditorRowIndex::SplayRow);
            CHECK(rowIndex.chunk() == fixture.chunk);
            CHECK(rowIndex.indexInChunk() == 1);
            CHECK(rowIndex.splayIndex() == splayIndex);
        }
        CHECK(fixture.rowIndexOf(7).rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(fixture.rowIndexOf(7).indexInChunk() == 1);
        CHECK(fixture.rowIndexOf(8).rowType() == cwSurveyEditorRowIndex::StationRow);
        CHECK(fixture.rowIndexOf(8).indexInChunk() == 2);
    }

    SECTION("each row shows its reading as it was written") {
        CHECK(fixture.rowData(4, cwSurveyEditorModel::SplayDistanceRole).toString() == QStringLiteral("5.88"));
        CHECK(fixture.rowData(4, cwSurveyEditorModel::SplayCompassRole).toString() == QStringLiteral("124.1"));
        CHECK(fixture.rowData(4, cwSurveyEditorModel::SplayClinoRole).toString() == QStringLiteral("4.6"));
        CHECK(fixture.rowData(6, cwSurveyEditorModel::SplayDistanceRole).toString() == QStringLiteral("8.96"));
    }

    SECTION("the shot below the cluster knows to step out of its way") {
        CHECK(fixture.rowData(7, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK_FALSE(fixture.rowData(2, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    }

    SECTION("toModelRow finds the rows it just made") {
        const cwSurveyEditorRowIndex splayRow(fixture.chunk, 1, 2, cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.model.toModelRow(splayRow) == 6);

        //A splay past the end of the cluster has no row
        const cwSurveyEditorRowIndex pastEnd(fixture.chunk, 1, 3, cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.model.toModelRow(pastEnd) == -1);
    }

    SECTION("collapsing takes the rows back out") {
        fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

        fixture.checkRowCount(6);
        CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::ShotRow);
    }
}

TEST_CASE("A station with no splays has nothing to expand", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    fixture.model.toggleSplaysExpanded(fixture.stationRow(0));

    fixture.checkRowCount(6);
    CHECK_FALSE(fixture.rowData(1, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
}

TEST_CASE("Splay rows are read-only and can't be navigated into", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    for(auto role : {cwSurveyChunk::StationNameRole, cwSurveyChunk::ShotDistanceRole}) {
        const auto cell = fixture.model.cellIndex(4, role);
        CHECK_FALSE(fixture.model.isCellValid(cell));
        CHECK_FALSE(fixture.model.setDataAt(cell, QStringLiteral("99")));
    }

    CHECK(fixture.rowData(4, cwSurveyEditorModel::SplayDistanceRole).toString() == QStringLiteral("5.88"));
}

TEST_CASE("An open cluster follows the station's splays", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    SECTION("a new splay becomes a new row") {
        QList<cwShotMeasurement> splays = a4Splays();
        splays.append(makeSplay("9.48", "163.6", "21.9"));
        fixture.chunk->setStationSplays(1, splays);

        fixture.checkRowCount(10);
        CHECK(fixture.rowData(7, cwSurveyEditorModel::SplayDistanceRole).toString() == QStringLiteral("9.48"));
        CHECK(fixture.rowIndexOf(8).rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 4);
    }

    SECTION("a splay going away takes its row with it") {
        fixture.chunk->setStationSplays(1, {makeSplay("5.88", "124.1", "4.6")});

        fixture.checkRowCount(7);
        CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.rowIndexOf(5).rowType() == cwSurveyEditorRowIndex::ShotRow);
    }

    SECTION("the last splay leaving closes the cluster") {
        fixture.chunk->setStationSplays(1, {});

        fixture.checkRowCount(6);
        CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 0);
    }
}

TEST_CASE("An open cluster stays on its station when the chunk changes", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    SECTION("a station inserted above pushes the cluster down with a2") {
        fixture.chunk->insertStation(0, cwSurveyChunk::Below);

        fixture.checkRowCount(11);

        //a2 moved from index 1 to index 2, and its splays came along
        const int stationRow = fixture.model.toModelRow(fixture.stationRow(2));
        REQUIRE(stationRow > 0);
        CHECK(fixture.rowData(stationRow, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowIndexOf(stationRow + 1).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.rowIndexOf(stationRow + 1).indexInChunk() == 2);
        CHECK(fixture.rowData(stationRow + 1, cwSurveyEditorModel::SplayDistanceRole).toString()
              == QStringLiteral("5.88"));
    }

    SECTION("removing the station closes the cluster with it") {
        fixture.chunk->removeStation(1, cwSurveyChunk::Above);

        fixture.checkRowCount(4);
        CHECK(fixture.rowIndexOf(1).rowType() == cwSurveyEditorRowIndex::StationRow);
        CHECK(fixture.rowIndexOf(2).rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(fixture.rowIndexOf(3).rowType() == cwSurveyEditorRowIndex::StationRow);
    }

    SECTION("removing a station above keeps the cluster on a2") {
        fixture.chunk->removeStation(0, cwSurveyChunk::Below);

        //title, a2, 3 splays, shot, a3
        fixture.checkRowCount(7);
        CHECK(fixture.rowData(1, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowIndexOf(2).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.rowIndexOf(2).indexInChunk() == 0);
    }
}

TEST_CASE("Two open clusters stack up in station order", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.chunk->setStationSplays(2, {makeSplay("7.56", "307.7", "18.6")});

    fixture.model.toggleSplaysExpanded(fixture.stationRow(2));
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    //title, a1, shot, a2, 3 splays, shot, a3, 1 splay
    fixture.checkRowCount(10);
    CHECK(fixture.model.toModelRow(fixture.stationRow(1)) == 3);
    CHECK(fixture.model.toModelRow(fixture.stationRow(2)) == 8);
    CHECK(fixture.rowIndexOf(9).rowType() == cwSurveyEditorRowIndex::SplayRow);
    CHECK(fixture.rowData(9, cwSurveyEditorModel::SplayDistanceRole).toString() == QStringLiteral("7.56"));
}

TEST_CASE("Retiring a trip forgets which clusters were open", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(9);

    cwTrip otherTrip;
    fixture.model.setTrip(&otherTrip);
    fixture.checkRowCount(0);

    fixture.model.setTrip(&fixture.trip);
    fixture.checkRowCount(6);
    CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
}
