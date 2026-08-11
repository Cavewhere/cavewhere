/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyEditorBoxData.h"
#include "cwSurveyEditorModel.h"
#include "cwTrip.h"

//Test includes
#include "SplayFixtureHelper.h"

//Std includes
#include <algorithm>

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

    //! A splay reading, as it reads in the box the table draws for it
    QString splayReading(int row, cwSurveyEditorModel::Role role) const
    {
        return rowData(row, role).value<cwSurveyEditorBoxData>().reading().value();
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
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.88"));
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayCompassRole) == QStringLiteral("124.1"));
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayClinoRole) == QStringLiteral("4.6"));
        CHECK(fixture.splayReading(6, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("8.96"));
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

TEST_CASE("Opening a cluster tells the shot below it", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    //The shot's boxes reach up over the boundary into the station row above
    //them, which is where an open cluster goes. The shot row only drops them
    //below the cluster if it hears that the cluster opened, so a rowsInserted
    //on its own leaves the shot drawn over the last splay
    QSignalSpy changes(&fixture.model, &QAbstractItemModel::dataChanged);

    const auto shotRowsTold = [&changes](int row) {
        int count = 0;
        for(const auto& change : changes) {
            const int first = change.at(0).value<QModelIndex>().row();
            const int last = change.at(1).value<QModelIndex>().row();
            const auto roles = change.at(2).value<QList<int>>();
            if(first <= row && row <= last
                && roles.contains(cwSurveyEditorModel::StationSplaysExpandedRole))
            {
                ++count;
            }
        }
        return count;
    };

    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    //a2's shot, now sitting under the three splay rows
    CHECK(fixture.rowIndexOf(7).rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(shotRowsTold(7) == 1);

    //a1's shot is above the cluster and keeps reaching up as it always has
    CHECK(shotRowsTold(2) == 0);

    changes.clear();
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(shotRowsTold(4) == 1);
    CHECK_FALSE(fixture.rowData(4, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
}

TEST_CASE("A station with no splays has nothing to expand", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    fixture.model.toggleSplaysExpanded(fixture.stationRow(0));

    fixture.checkRowCount(6);
    CHECK_FALSE(fixture.rowData(1, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
}

TEST_CASE("A splay row holds none of the station's or shot's cells", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    for(auto role : {cwSurveyEditorCellIndex::StationNameCell, cwSurveyEditorCellIndex::ShotDistanceCell}) {
        const auto cell = fixture.model.cellIndex(4, role);
        CHECK_FALSE(fixture.model.isCellValid(cell));
        CHECK_FALSE(fixture.model.setDataAt(cell, QStringLiteral("99")));
    }

    CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.88"));
}

TEST_CASE("An open cluster follows the station's splays", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    SECTION("a new splay becomes a new row") {
        QList<cwShotMeasurement> splays = a4Splays();
        splays.append(makeSplay("9.48", "163.6", "21.9"));
        fixture.chunk->setStationSplays(1, splays);

        fixture.checkRowCount(10);
        CHECK(fixture.splayReading(7, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("9.48"));
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

TEST_CASE("An open cluster follows a splay edit", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(9);

    SECTION("appending a splay adds a row at the bottom of the cluster") {
        fixture.chunk->appendStationSplay(1, makeSplay("9.48", "163.6", "21.9"));

        fixture.checkRowCount(10);
        CHECK(fixture.splayReading(7, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("9.48"));
        CHECK(fixture.rowIndexOf(8).rowType() == cwSurveyEditorRowIndex::ShotRow);
    }

    SECTION("removing a splay takes one row, and the rest renumber") {
        fixture.chunk->removeStationSplay(1, 0);

        fixture.checkRowCount(8);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 2);
        CHECK(fixture.rowIndexOf(4).splayIndex() == 0);
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.42"));
        CHECK(fixture.splayReading(5, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("8.96"));
        CHECK(fixture.rowIndexOf(6).rowType() == cwSurveyEditorRowIndex::ShotRow);
    }

    SECTION("clearing the splays collapses the cluster") {
        fixture.chunk->clearStationSplays(1);

        fixture.checkRowCount(6);
        CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 0);
        CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::ShotRow);
    }

    SECTION("a written reading shows up in its row") {
        fixture.chunk->setStationSplayData(cwSurveyChunk::ShotCompassRole, 1, 1, QStringLiteral("119.4"));

        fixture.checkRowCount(9);
        CHECK(fixture.splayReading(5, cwSurveyEditorModel::SplayCompassRole) == QStringLiteral("119.4"));
    }
}

TEST_CASE("The editor deletes splays through the rows it shows", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(9);

    const auto splayRow = [&fixture](int splayIndex) {
        return cwSurveyEditorRowIndex(fixture.chunk, 1, splayIndex, cwSurveyEditorRowIndex::SplayRow);
    };

    SECTION("removing one splay takes its row and renumbers the rest") {
        fixture.model.removeSplayAt(splayRow(0));

        fixture.checkRowCount(8);
        CHECK(fixture.chunk->stationSplayCount(1) == 2);
        CHECK(fixture.rowIndexOf(4).splayIndex() == 0);
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.42"));
    }

    SECTION("removing the last splay closes the cluster") {
        for(int splayIndex = 2; splayIndex >= 0; --splayIndex) {
            fixture.model.removeSplayAt(splayRow(splayIndex));
        }

        fixture.checkRowCount(6);
        CHECK(fixture.chunk->stationSplayCount(1) == 0);
        CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    }

    SECTION("clearing from the station row empties the cluster") {
        fixture.model.clearSplaysAt(fixture.stationRow(1));

        fixture.checkRowCount(6);
        CHECK(fixture.chunk->stationSplayCount(1) == 0);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 0);
        CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::ShotRow);
    }

    SECTION("a splay row names the same cluster to clear") {
        fixture.model.clearSplaysAt(splayRow(2));

        fixture.checkRowCount(6);
        CHECK(fixture.chunk->stationSplayCount(1) == 0);
    }

    SECTION("rows that name no cluster leave the splays alone") {
        fixture.model.removeSplayAt(fixture.stationRow(1));
        fixture.model.removeSplayAt(splayRow(3));
        fixture.model.removeSplayAt(cwSurveyEditorRowIndex(nullptr, 1, 0, cwSurveyEditorRowIndex::SplayRow));
        fixture.model.clearSplaysAt(cwSurveyEditorRowIndex(fixture.chunk, 1, cwSurveyEditorRowIndex::ShotRow));
        fixture.model.clearSplaysAt(cwSurveyEditorRowIndex(fixture.chunk, 7, cwSurveyEditorRowIndex::StationRow));

        cwSurveyChunk otherChunk;
        otherChunk.appendShot(cwStation("b1"), cwStation("b2"), cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));
        fixture.model.clearSplaysAt(cwSurveyEditorRowIndex(&otherChunk, 1, cwSurveyEditorRowIndex::StationRow));

        fixture.checkRowCount(9);
        CHECK(fixture.chunk->stationSplayCount(1) == 3);
    }
}

TEST_CASE("Moved splays leave one cluster and land in another", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.chunk->setStationSplays(2, {makeSplay("7.56", "307.7", "18.6")});

    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.model.toggleSplaysExpanded(fixture.stationRow(2));

    //title, a1, shot, a2, 3 splays, shot, a3, 1 splay
    fixture.checkRowCount(10);

    SECTION("one splay moves down to the station below") {
        cwSurveyChunk::moveStationSplays(fixture.chunk, 1, fixture.chunk, 2, {0});

        fixture.checkRowCount(10);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 2);
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.42"));

        const int a3Row = fixture.model.toModelRow(fixture.stationRow(2));
        REQUIRE(a3Row == 7);
        CHECK(fixture.rowData(a3Row, cwSurveyEditorModel::StationSplayCountRole).toInt() == 2);
        CHECK(fixture.splayReading(a3Row + 2, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.88"));
    }

    SECTION("a whole cluster moving away closes the cluster it left") {
        cwSurveyChunk::moveStationSplays(fixture.chunk, 1, fixture.chunk, 2, {0, 1, 2});

        //title, a1, shot, a2, shot, a3, 4 splays
        fixture.checkRowCount(10);
        CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(fixture.rowData(5, cwSurveyEditorModel::StationSplayCountRole).toInt() == 4);
        CHECK(fixture.splayReading(9, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("8.96"));
    }

    SECTION("splays move into a station in another chunk") {
        auto secondChunk = new cwSurveyChunk();
        secondChunk->appendShot(cwStation("b1"), cwStation("b2"), cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));
        secondChunk->setStationSplays(1, {makeSplay("3.21", "12.4", "-8.2")});
        fixture.trip.addChunk(secondChunk);

        //The second chunk brings a title, two stations, and a shot
        fixture.checkRowCount(14);

        const cwSurveyEditorRowIndex b2Row(secondChunk, 1, cwSurveyEditorRowIndex::StationRow);
        fixture.model.toggleSplaysExpanded(b2Row);
        fixture.checkRowCount(15);

        cwSurveyChunk::moveStationSplays(fixture.chunk, 1, secondChunk, 1, {1, 2});

        //a2 gives up two rows and b2 gains them, so the table stays the same height
        fixture.checkRowCount(15);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 1);
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.88"));

        const int b2ModelRow = fixture.model.toModelRow(b2Row);
        REQUIRE(b2ModelRow > 0);
        CHECK(fixture.rowData(b2ModelRow, cwSurveyEditorModel::StationSplayCountRole).toInt() == 3);
        CHECK(fixture.rowIndexOf(b2ModelRow + 1).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.splayReading(b2ModelRow + 2, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("5.42"));
        CHECK(fixture.splayReading(b2ModelRow + 3, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("8.96"));
    }
}

TEST_CASE("A move waits for the station the user picks", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(9);

    const auto splayRow = [&fixture](int splayIndex) {
        return cwSurveyEditorRowIndex(fixture.chunk, 1, splayIndex, cwSurveyEditorRowIndex::SplayRow);
    };

    QSignalSpy moveChanged(&fixture.model, &cwSurveyEditorModel::splayMoveChanged);
    REQUIRE(moveChanged.isValid());

    SECTION("the whole cluster moves onto the station the user clicks") {
        fixture.model.startSplayMove(fixture.stationRow(1), true);

        CHECK(fixture.model.splayMoveActive());
        CHECK(fixture.model.splayMoveCount() == 3);
        CHECK(fixture.model.splayMoveStationName() == QStringLiteral("a2"));
        CHECK(moveChanged.count() == 1);

        //a3 takes them, a1 and the station they came off don't
        CHECK(fixture.model.isSplayMoveTarget(fixture.stationRow(2)));
        CHECK(fixture.model.isSplayMoveTarget(fixture.stationRow(0)));
        CHECK_FALSE(fixture.model.isSplayMoveTarget(fixture.stationRow(1)));
        CHECK(fixture.model.isSplayMoveSource(fixture.stationRow(1)));
        CHECK(fixture.model.isSplayMoveSource(splayRow(0)));
        CHECK_FALSE(fixture.model.isSplayMoveTarget(
            cwSurveyEditorRowIndex(fixture.chunk, 1, cwSurveyEditorRowIndex::ShotRow)));

        fixture.model.commitSplayMove(fixture.stationRow(2));

        CHECK_FALSE(fixture.model.splayMoveActive());
        CHECK(fixture.model.splayMoveCount() == 0);
        CHECK(fixture.chunk->stationSplayCount(1) == 0);
        CHECK(fixture.chunk->stationSplayCount(2) == 3);

        //a2's cluster empties and closes, and a3 opens on what it just took
        //title, a1, shot, a2, shot, a3, 3 splays
        fixture.checkRowCount(9);
        const int a3Row = fixture.model.toModelRow(fixture.stationRow(2));
        REQUIRE(a3Row == 5);
        CHECK(fixture.rowData(a3Row, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.splayReading(a3Row + 1, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("5.88"));
    }

    SECTION("one splay moves on its own") {
        fixture.model.startSplayMove(splayRow(1), false);

        CHECK(fixture.model.splayMoveCount() == 1);

        fixture.model.commitSplayMove(fixture.stationRow(0));

        CHECK(fixture.chunk->stationSplayCount(0) == 1);
        CHECK(fixture.chunk->stationSplayCount(1) == 2);

        //a1 opens on the splay it took, a2 keeps the two it has left
        //title, a1, 1 splay, shot, a2, 2 splays, shot, a3
        fixture.checkRowCount(9);
        CHECK(fixture.splayReading(2, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("5.42"));
        CHECK(fixture.splayReading(5, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("5.88"));
    }

    SECTION("splays move into a station in another chunk") {
        auto secondChunk = new cwSurveyChunk();
        secondChunk->appendShot(cwStation("b1"), cwStation("b2"), cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));
        fixture.trip.addChunk(secondChunk);
        fixture.checkRowCount(13);

        const cwSurveyEditorRowIndex b2Row(secondChunk, 1, cwSurveyEditorRowIndex::StationRow);

        fixture.model.startSplayMove(fixture.stationRow(1), true);
        CHECK(fixture.model.isSplayMoveTarget(b2Row));

        fixture.model.commitSplayMove(b2Row);

        CHECK(fixture.chunk->stationSplayCount(1) == 0);
        CHECK(secondChunk->stationSplayCount(1) == 3);

        //a2 gives up its three rows and b2 opens on them
        fixture.checkRowCount(13);
        const int b2ModelRow = fixture.model.toModelRow(b2Row);
        REQUIRE(b2ModelRow > 0);
        CHECK(fixture.rowData(b2ModelRow, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowIndexOf(b2ModelRow + 1).rowType() == cwSurveyEditorRowIndex::SplayRow);
    }

    SECTION("canceling leaves nothing pending and nothing moved") {
        fixture.model.startSplayMove(fixture.stationRow(1), true);
        fixture.model.cancelSplayMove();

        CHECK_FALSE(fixture.model.splayMoveActive());
        CHECK(fixture.model.splayMoveStationName().isEmpty());
        CHECK(moveChanged.count() == 2);
        CHECK_FALSE(fixture.model.isSplayMoveTarget(fixture.stationRow(2)));

        //A second cancel has nothing left to say
        fixture.model.cancelSplayMove();
        CHECK(moveChanged.count() == 2);

        //And a commit with nothing armed leaves the splays where they are
        fixture.model.commitSplayMove(fixture.stationRow(2));
        CHECK(fixture.chunk->stationSplayCount(1) == 3);
        CHECK(fixture.chunk->stationSplayCount(2) == 0);
        fixture.checkRowCount(9);
    }

    SECTION("rows that name no splays arm nothing") {
        fixture.model.startSplayMove(fixture.stationRow(0), true);
        CHECK_FALSE(fixture.model.splayMoveActive());

        fixture.model.startSplayMove(splayRow(3), false);
        CHECK_FALSE(fixture.model.splayMoveActive());

        fixture.model.startSplayMove(fixture.stationRow(1), false);
        CHECK_FALSE(fixture.model.splayMoveActive());

        fixture.model.startSplayMove(
            cwSurveyEditorRowIndex(nullptr, 1, 0, cwSurveyEditorRowIndex::SplayRow), true);
        CHECK_FALSE(fixture.model.splayMoveActive());

        CHECK(moveChanged.count() == 0);
    }

    SECTION("arming a move calls off the one before it") {
        fixture.model.startSplayMove(fixture.stationRow(1), true);
        fixture.model.startSplayMove(splayRow(0), false);

        CHECK(fixture.model.splayMoveCount() == 1);
        //The second arming cancels the first before it announces itself
        CHECK(moveChanged.count() == 3);
    }

    SECTION("a station leaving takes the move it armed with it") {
        fixture.model.startSplayMove(fixture.stationRow(1), true);
        fixture.chunk->removeStation(1, cwSurveyChunk::Above);

        CHECK_FALSE(fixture.model.splayMoveActive());
    }

    SECTION("a station arriving takes the move with it") {
        fixture.model.startSplayMove(fixture.stationRow(1), true);
        fixture.chunk->insertStation(0, cwSurveyChunk::Above);

        //a2 is at index 2 now, so an armed index 1 would name somebody else
        CHECK_FALSE(fixture.model.splayMoveActive());
    }

    SECTION("editing the splays the move armed calls it off") {
        fixture.model.startSplayMove(splayRow(2), false);
        fixture.chunk->removeStationSplay(1, 0);

        //The armed index 2 named the last splay, which is index 1 now
        CHECK_FALSE(fixture.model.splayMoveActive());
        CHECK(fixture.chunk->stationSplayCount(1) == 2);
    }

    SECTION("splays changing on another station leave the move armed") {
        fixture.model.startSplayMove(fixture.stationRow(1), true);
        fixture.chunk->setStationSplays(0, a4Splays());

        CHECK(fixture.model.splayMoveActive());
        CHECK(fixture.model.splayMoveCount() == 3);
    }

    SECTION("leaving the trip calls the move off") {
        fixture.model.startSplayMove(fixture.stationRow(1), true);

        cwTrip otherTrip;
        fixture.model.setTrip(&otherTrip);

        CHECK_FALSE(fixture.model.splayMoveActive());
    }
}

TEST_CASE("Splays moving onto a closed station stay out of sight", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    //title, a1, shot, a2, 3 splays, shot, a3
    fixture.checkRowCount(9);

    QSignalSpy dataChanged(&fixture.model, &QAbstractItemModel::dataChanged);
    REQUIRE(dataChanged.isValid());

    cwSurveyChunk::moveStationSplays(fixture.chunk, 1, fixture.chunk, 2, {0});

    //a2 gives up a row and the closed a3 shows the splay only on its chip
    fixture.checkRowCount(8);

    const int a3Row = fixture.model.toModelRow(fixture.stationRow(2));
    REQUIRE(a3Row == 7);
    CHECK(fixture.rowData(a3Row, cwSurveyEditorModel::StationSplayCountRole).toInt() == 1);
    CHECK_FALSE(fixture.rowData(a3Row, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 2);

    const bool chipToldToRedraw = std::any_of(dataChanged.begin(), dataChanged.end(),
                                              [a3Row](const QList<QVariant>& arguments) {
                                                  const int first = arguments.at(0).value<QModelIndex>().row();
                                                  const int last = arguments.at(1).value<QModelIndex>().row();
                                                  return first <= a3Row && a3Row <= last;
                                              });
    CHECK(chipToldToRedraw);
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
        CHECK(fixture.splayReading(stationRow + 1, cwSurveyEditorModel::SplayDistanceRole)
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
    CHECK(fixture.splayReading(9, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("7.56"));
}

TEST_CASE("The Splays cell is the last cell in a station row", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    //title, a1, shot, a2, shot, a3
    const int a1Row = 1;
    const int a2Row = 3;
    const int a3Row = 5;

    auto cell = [&fixture](int row, cwSurveyEditorCellIndex::CellRole role) {
        return fixture.model.cellIndex(row, role);
    };

    auto next = [&fixture](const cwSurveyEditorCellIndex& from, cwSurveyEditorModel::NavigationKey key) {
        return fixture.model.nextCell(from, key, true, false);
    };

    SECTION("the cell is reachable on every station row, splays or not") {
        for(int row : {a1Row, a2Row, a3Row}) {
            CHECK(fixture.model.isCellValid(cell(row, cwSurveyEditorCellIndex::StationSplaysCell)));
        }
    }

    SECTION("D tabs into it, and it tabs on the way D used to") {
        const auto splays = next(cell(a1Row, cwSurveyEditorCellIndex::StationDownCell),
                                 cwSurveyEditorModel::Tab);
        CHECK(splays == cell(a1Row, cwSurveyEditorCellIndex::StationSplaysCell));

        //The first station's row leads into the second station's LRUD
        CHECK(next(splays, cwSurveyEditorModel::Tab) == cell(a2Row, cwSurveyEditorCellIndex::StationLeftCell));
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Tab)
              == cell(a3Row, cwSurveyEditorCellIndex::StationNameCell));
    }

    SECTION("shift-tab walks back through it into D") {
        CHECK(next(cell(a1Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::BackTab)
              == cell(a1Row, cwSurveyEditorCellIndex::StationDownCell));
        CHECK(next(cell(a3Row, cwSurveyEditorCellIndex::StationNameCell), cwSurveyEditorModel::BackTab)
              == cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell));
    }

    SECTION("right stops at it, and left goes back to D") {
        const auto splays = cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell);
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationDownCell), cwSurveyEditorModel::Right) == splays);
        CHECK_FALSE(fixture.model.isCellValid(next(splays, cwSurveyEditorModel::Right)));
        CHECK(next(splays, cwSurveyEditorModel::Left) == cell(a2Row, cwSurveyEditorCellIndex::StationDownCell));
    }

    SECTION("up and down stay in the column") {
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Down)
              == cell(a3Row, cwSurveyEditorCellIndex::StationSplaysCell));
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Up)
              == cell(a1Row, cwSurveyEditorCellIndex::StationSplaysCell));
    }

    SECTION("an open cluster moves the rows but not the chain") {
        fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
        fixture.checkRowCount(9);

        const int shiftedA3Row = a3Row + 3;
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Tab)
              == cell(shiftedA3Row, cwSurveyEditorCellIndex::StationNameCell));
        CHECK(next(cell(shiftedA3Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Up)
              == cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell));
    }

    SECTION("the cell holds no reading to write") {
        CHECK_FALSE(fixture.model.setDataAt(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell),
                                            QStringLiteral("99")));
    }
}

TEST_CASE("A splay's readings are cells the table writes", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(9);

    //title, a1, shot, a2, s1, s2, s3, shot, a3
    const int firstSplayRow = 4;

    auto cell = [&fixture](int row, cwSurveyEditorCellIndex::CellRole role) {
        return fixture.model.cellIndex(row, role);
    };

    SECTION("each reading cell is valid on the row that shows it") {
        for(auto role : {cwSurveyEditorCellIndex::SplayDistanceCell,
                          cwSurveyEditorCellIndex::SplayCompassCell,
                          cwSurveyEditorCellIndex::SplayClinoCell})
        {
            CHECK(fixture.model.isCellValid(cell(firstSplayRow, role)));

            //A splay cell names nothing on the station row above the cluster
            CHECK_FALSE(fixture.model.isCellValid(cell(3, role)));
        }
    }

    SECTION("a written reading lands in the chunk and reaches the row") {
        QSignalSpy changes(&fixture.model, &QAbstractItemModel::dataChanged);

        CHECK(fixture.model.setDataAt(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                                      QStringLiteral("6.02")));
        CHECK(fixture.model.setDataAt(cell(firstSplayRow + 1, cwSurveyEditorCellIndex::SplayCompassCell),
                                      QStringLiteral("119.4")));
        CHECK(fixture.model.setDataAt(cell(firstSplayRow + 2, cwSurveyEditorCellIndex::SplayClinoCell),
                                      QStringLiteral("-8.25")));

        fixture.checkRowCount(9);
        CHECK(fixture.chunk->stationSplayAt(1, 0).distance.value() == QStringLiteral("6.02"));
        CHECK(fixture.chunk->stationSplayAt(1, 1).compass.value() == QStringLiteral("119.4"));
        CHECK(fixture.chunk->stationSplayAt(1, 2).clino.value() == QStringLiteral("-8.25"));

        CHECK(fixture.splayReading(firstSplayRow, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("6.02"));
        CHECK(fixture.splayReading(firstSplayRow + 1, cwSurveyEditorModel::SplayCompassRole)
              == QStringLiteral("119.4"));
        CHECK(fixture.splayReading(firstSplayRow + 2, cwSurveyEditorModel::SplayClinoRole)
              == QStringLiteral("-8.25"));

        const auto rowsTold = [&changes](int row, int changedRole) {
            int count = 0;
            for(const auto& change : changes) {
                const int first = change.at(0).value<QModelIndex>().row();
                const int last = change.at(1).value<QModelIndex>().row();
                const auto roles = change.at(2).value<QList<int>>();
                if(first <= row && row <= last && roles.contains(changedRole)) {
                    ++count;
                }
            }
            return count;
        };

        CHECK(rowsTold(firstSplayRow, cwSurveyEditorModel::SplayDistanceRole) > 0);
        CHECK(rowsTold(firstSplayRow + 1, cwSurveyEditorModel::SplayCompassRole) > 0);
        CHECK(rowsTold(firstSplayRow + 2, cwSurveyEditorModel::SplayClinoRole) > 0);
    }

    SECTION("a cell on a row that shows no splay writes nothing") {
        //The station row above the cluster, a splay row of a station that has
        //none, and a row past the end of the model
        CHECK_FALSE(fixture.model.setDataAt(cell(3, cwSurveyEditorCellIndex::SplayDistanceCell),
                                            QStringLiteral("99")));
        CHECK_FALSE(fixture.model.setDataAt(cell(7, cwSurveyEditorCellIndex::SplayCompassCell),
                                            QStringLiteral("99")));
        CHECK_FALSE(fixture.model.setDataAt(cell(fixture.model.rowCount(),
                                                 cwSurveyEditorCellIndex::SplayClinoCell),
                                            QStringLiteral("99")));

        fixture.checkRowCount(9);
        CHECK(fixture.splayReading(firstSplayRow, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("5.88"));
        CHECK(fixture.chunk->shot(1).compass().value() != QStringLiteral("99"));
    }

    SECTION("writing the reading a splay already has tells the view nothing") {
        QSignalSpy changes(&fixture.model, &QAbstractItemModel::dataChanged);

        CHECK(fixture.model.setDataAt(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell),
                                      QStringLiteral("124.1")));

        CHECK(changes.isEmpty());
        CHECK(fixture.chunk->stationSplayAt(1, 0).compass.value() == QStringLiteral("124.1"));
    }
}

TEST_CASE("The keyboard walks a cluster's readings", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(9);

    //title, a1, shot, a2, s1, s2, s3, shot, a3
    const int a2Row = 3;
    const int firstSplayRow = 4;
    const int lastSplayRow = 6;
    const int a2ShotRow = 7;
    const int a3Row = 8;

    auto cell = [&fixture](int row, cwSurveyEditorCellIndex::CellRole role) {
        return fixture.model.cellIndex(row, role);
    };

    auto next = [&fixture](const cwSurveyEditorCellIndex& from,
                           cwSurveyEditorModel::NavigationKey key,
                           bool frontSights = true,
                           bool backSights = false) {
        return fixture.model.nextCell(from, key, frontSights, backSights);
    };

    SECTION("tab runs the three readings and drops onto the next splay") {
        const auto distance = cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell);
        const auto compass = next(distance, cwSurveyEditorModel::Tab);
        CHECK(compass == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell));

        const auto clino = next(compass, cwSurveyEditorModel::Tab);
        CHECK(clino == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayClinoCell));

        CHECK(next(clino, cwSurveyEditorModel::Tab)
              == cell(firstSplayRow + 1, cwSurveyEditorCellIndex::SplayDistanceCell));
    }

    SECTION("tab out of the last splay leaves the row the way the cluster's cell does") {
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Tab)
              == cell(a3Row, cwSurveyEditorCellIndex::StationNameCell));
    }

    SECTION("shift-tab walks back up the cluster and out to its cell") {
        CHECK(next(cell(firstSplayRow + 1, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::BackTab)
              == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayClinoCell));
        CHECK(next(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell),
                   cwSurveyEditorModel::BackTab)
              == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(next(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::BackTab)
              == cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell));
    }

    SECTION("left and right stay inside the splay") {
        const auto compass = cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell);
        CHECK(next(compass, cwSurveyEditorModel::Left)
              == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(next(compass, cwSurveyEditorModel::Right)
              == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayClinoCell));

        CHECK_FALSE(fixture.model.isCellValid(
            next(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                 cwSurveyEditorModel::Left)));
        CHECK_FALSE(fixture.model.isCellValid(
            next(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayClinoCell),
                 cwSurveyEditorModel::Right)));
    }

    SECTION("up and down run the column and leave the cluster at its ends") {
        CHECK(next(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell),
                   cwSurveyEditorModel::Down)
              == cell(firstSplayRow + 1, cwSurveyEditorCellIndex::SplayCompassCell));
        CHECK(next(cell(firstSplayRow + 1, cwSurveyEditorCellIndex::SplayCompassCell),
                   cwSurveyEditorModel::Up)
              == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell));

        //Above the cluster is the cell it hangs from; below it is the shot
        CHECK(next(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::Up)
              == cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell));
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::Down)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Down)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotClinoCell));
    }

    SECTION("a trip on back sights leaves the cluster on the back sight boxes") {
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayCompassCell),
                   cwSurveyEditorModel::Down, false, true)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotBackCompassCell));
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Down, false, true)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotBackClinoCell));
    }

    SECTION("a trip that records neither sight drops the whole cluster onto the distance box") {
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayCompassCell),
                   cwSurveyEditorModel::Down, false, false)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Down, false, false)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
    }

    SECTION("the last station's cluster has no shot below it to step onto") {
        fixture.chunk->setStationSplays(2, {makeSplay("7.56", "201.3", "-12.4")});
        fixture.model.toggleSplaysExpanded(fixture.stationRow(2));
        fixture.checkRowCount(10);

        const int a3SplayRow = a3Row + 1;
        CHECK(fixture.rowIndexOf(a3SplayRow).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK_FALSE(fixture.model.isCellValid(
            next(cell(a3SplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                 cwSurveyEditorModel::Down)));
    }
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
