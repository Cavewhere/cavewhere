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
#include <array>
#include <utility>

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

    //The three splays, plus the blank row the next one is typed into
    fixture.checkRowCount(10);
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
        CHECK(fixture.rowIndexOf(7).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.rowIndexOf(7).splayIndex() == 3);
        CHECK(fixture.rowIndexOf(8).rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(fixture.rowIndexOf(8).indexInChunk() == 1);
        CHECK(fixture.rowIndexOf(9).rowType() == cwSurveyEditorRowIndex::StationRow);
        CHECK(fixture.rowIndexOf(9).indexInChunk() == 2);
    }

    SECTION("each row shows its reading as it was written") {
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.88"));
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayCompassRole) == QStringLiteral("124.1"));
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayClinoRole) == QStringLiteral("4.6"));
        CHECK(fixture.splayReading(6, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("8.96"));
    }

    SECTION("the shot below the cluster knows to step out of its way") {
        CHECK(fixture.rowData(8, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK_FALSE(fixture.rowData(2, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    }

    SECTION("toModelRow finds the rows it just made") {
        const cwSurveyEditorRowIndex splayRow(fixture.chunk, 1, 2, cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.model.toModelRow(splayRow) == 6);

        //The blank row sits one past the last splay
        const cwSurveyEditorRowIndex blankRow(fixture.chunk, 1, 3, cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.model.toModelRow(blankRow) == 7);

        //And past the blank row the cluster is over
        const cwSurveyEditorRowIndex pastEnd(fixture.chunk, 1, 4, cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.model.toModelRow(pastEnd) == -1);
    }

    SECTION("the blank row reads empty and says it holds no splay yet") {
        const int blankRow = 7;
        CHECK(fixture.rowData(blankRow, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.splayReading(blankRow, cwSurveyEditorModel::SplayDistanceRole).isEmpty());
        CHECK(fixture.splayReading(blankRow, cwSurveyEditorModel::SplayCompassRole).isEmpty());
        CHECK(fixture.splayReading(blankRow, cwSurveyEditorModel::SplayClinoRole).isEmpty());

        //The rows above it hold splays, so none of them is the blank one
        for(int row = 4; row < blankRow; ++row) {
            CHECK_FALSE(fixture.rowData(row, cwSurveyEditorModel::IsVirtualRole).toBool());
        }

        //It hangs off the same station as the rest of the cluster
        CHECK(fixture.rowData(blankRow, cwSurveyEditorModel::SplayStationNameRole).toString()
              == QStringLiteral("a2"));
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

    //a2's shot, now sitting under the three splay rows and the blank one
    CHECK(fixture.rowIndexOf(8).rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(shotRowsTold(8) == 1);

    //a1's shot is above the cluster and keeps reaching up as it always has
    CHECK(shotRowsTold(2) == 0);

    changes.clear();
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(shotRowsTold(4) == 1);
    CHECK_FALSE(fixture.rowData(4, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
}

TEST_CASE("A station with no splays opens on the blank row", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    fixture.model.toggleSplaysExpanded(fixture.stationRow(0));

    //Manual entry when the download dies: the cluster opens on the one row
    //there is to type in
    fixture.checkRowCount(7);
    CHECK(fixture.rowData(1, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    CHECK(fixture.rowData(1, cwSurveyEditorModel::StationSplayCountRole).toInt() == 0);
    CHECK(fixture.rowIndexOf(2).rowType() == cwSurveyEditorRowIndex::SplayRow);
    CHECK(fixture.rowIndexOf(2).splayIndex() == 0);
    CHECK(fixture.rowData(2, cwSurveyEditorModel::IsVirtualRole).toBool());
    CHECK(fixture.rowIndexOf(3).rowType() == cwSurveyEditorRowIndex::ShotRow);

    SECTION("the entry cell is the blank row's distance") {
        const auto entry = fixture.model.splayEntryCell(fixture.stationRow(0));
        CHECK(entry == fixture.model.cellIndex(2, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(fixture.model.isCellValid(entry));
    }

    SECTION("typing a reading into it makes the station's first splay") {
        CHECK(fixture.model.setDataAt(fixture.model.cellIndex(2, cwSurveyEditorCellIndex::SplayDistanceCell),
                                      QStringLiteral("4.21")));

        //The blank row became a splay, and a fresh blank row took its place
        fixture.checkRowCount(8);
        CHECK(fixture.chunk->stationSplayCount(0) == 1);
        CHECK(fixture.chunk->stationSplayAt(0, 0).distance.value() == QStringLiteral("4.21"));
        CHECK_FALSE(fixture.rowData(2, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.rowData(3, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.rowData(1, cwSurveyEditorModel::StationSplayCountRole).toInt() == 1);
    }

    SECTION("closing it again takes the blank row back out") {
        fixture.model.toggleSplaysExpanded(fixture.stationRow(0));

        fixture.checkRowCount(6);
        CHECK_FALSE(fixture.rowData(1, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK_FALSE(fixture.model.isCellValid(fixture.model.splayEntryCell(fixture.stationRow(0))));
    }
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

        fixture.checkRowCount(11);
        CHECK(fixture.splayReading(7, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("9.48"));
        CHECK(fixture.rowData(8, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.rowIndexOf(9).rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 4);
    }

    SECTION("a splay going away takes its row with it") {
        fixture.chunk->setStationSplays(1, {makeSplay("5.88", "124.1", "4.6")});

        fixture.checkRowCount(8);
        CHECK(fixture.rowIndexOf(4).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK(fixture.rowData(5, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.rowIndexOf(6).rowType() == cwSurveyEditorRowIndex::ShotRow);
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
    fixture.checkRowCount(10);

    SECTION("appending a splay adds a row above the blank one") {
        fixture.chunk->appendStationSplay(1, makeSplay("9.48", "163.6", "21.9"));

        fixture.checkRowCount(11);
        CHECK(fixture.splayReading(7, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("9.48"));
        CHECK(fixture.rowData(8, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.rowIndexOf(9).rowType() == cwSurveyEditorRowIndex::ShotRow);
    }

    SECTION("removing a splay takes one row, and the rest renumber") {
        fixture.chunk->removeStationSplay(1, 0);

        fixture.checkRowCount(9);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 2);
        CHECK(fixture.rowIndexOf(4).splayIndex() == 0);
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.42"));
        CHECK(fixture.splayReading(5, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("8.96"));
        CHECK(fixture.rowData(6, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.rowIndexOf(7).rowType() == cwSurveyEditorRowIndex::ShotRow);
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

        fixture.checkRowCount(10);
        CHECK(fixture.splayReading(5, cwSurveyEditorModel::SplayCompassRole) == QStringLiteral("119.4"));
    }
}

TEST_CASE("The editor deletes splays through the rows it shows", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(10);

    const auto splayRow = [&fixture](int splayIndex) {
        return cwSurveyEditorRowIndex(fixture.chunk, 1, splayIndex, cwSurveyEditorRowIndex::SplayRow);
    };

    SECTION("removing one splay takes its row and renumbers the rest") {
        fixture.model.removeSplayAt(splayRow(0));

        fixture.checkRowCount(9);
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

        //The blank row holds no splay to remove
        fixture.model.removeSplayAt(splayRow(3));
        fixture.model.removeSplayAt(cwSurveyEditorRowIndex(nullptr, 1, 0, cwSurveyEditorRowIndex::SplayRow));
        fixture.model.clearSplaysAt(cwSurveyEditorRowIndex(fixture.chunk, 1, cwSurveyEditorRowIndex::ShotRow));
        fixture.model.clearSplaysAt(cwSurveyEditorRowIndex(fixture.chunk, 7, cwSurveyEditorRowIndex::StationRow));

        cwSurveyChunk otherChunk;
        otherChunk.appendShot(cwStation("b1"), cwStation("b2"), cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));
        fixture.model.clearSplaysAt(cwSurveyEditorRowIndex(&otherChunk, 1, cwSurveyEditorRowIndex::StationRow));

        fixture.checkRowCount(10);
        CHECK(fixture.chunk->stationSplayCount(1) == 3);
    }
}

TEST_CASE("Moved splays leave one cluster and land in another", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.chunk->setStationSplays(2, {makeSplay("7.56", "307.7", "18.6")});

    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.model.toggleSplaysExpanded(fixture.stationRow(2));

    //title, a1, shot, a2, 3 splays, blank, shot, a3, 1 splay, blank
    fixture.checkRowCount(12);

    SECTION("one splay moves down to the station below") {
        cwSurveyChunk::moveStationSplays(fixture.chunk, 1, fixture.chunk, 2, {0});

        fixture.checkRowCount(12);
        CHECK(fixture.rowData(3, cwSurveyEditorModel::StationSplayCountRole).toInt() == 2);
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.42"));

        const int a3Row = fixture.model.toModelRow(fixture.stationRow(2));
        REQUIRE(a3Row == 8);
        CHECK(fixture.rowData(a3Row, cwSurveyEditorModel::StationSplayCountRole).toInt() == 2);
        CHECK(fixture.splayReading(a3Row + 2, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("5.88"));
    }

    SECTION("a whole cluster moving away closes the cluster it left") {
        cwSurveyChunk::moveStationSplays(fixture.chunk, 1, fixture.chunk, 2, {0, 1, 2});

        //title, a1, shot, a2, shot, a3, 4 splays, blank
        fixture.checkRowCount(11);
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
        fixture.checkRowCount(16);

        const cwSurveyEditorRowIndex b2Row(secondChunk, 1, cwSurveyEditorRowIndex::StationRow);
        fixture.model.toggleSplaysExpanded(b2Row);
        fixture.checkRowCount(18);

        cwSurveyChunk::moveStationSplays(fixture.chunk, 1, secondChunk, 1, {1, 2});

        //a2 gives up two rows and b2 gains them, so the table stays the same height
        fixture.checkRowCount(18);
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
    fixture.checkRowCount(10);

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

        //a2's cluster empties and closes, and a3 stays closed on what it just
        //took — its count is what says the splays landed
        //title, a1, shot, a2, shot, a3
        fixture.checkRowCount(6);
        const int a3Row = fixture.model.toModelRow(fixture.stationRow(2));
        REQUIRE(a3Row == 5);
        CHECK_FALSE(fixture.rowData(a3Row, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowData(a3Row, cwSurveyEditorModel::StationSplayCountRole).toInt() == 3);
    }

    SECTION("one splay moves on its own") {
        fixture.model.startSplayMove(splayRow(1), false);

        CHECK(fixture.model.splayMoveCount() == 1);

        fixture.model.commitSplayMove(fixture.stationRow(0));

        CHECK(fixture.chunk->stationSplayCount(0) == 1);
        CHECK(fixture.chunk->stationSplayCount(1) == 2);

        //a1 stays closed on the splay it took, a2 keeps the two it has left
        //title, a1, shot, a2, 2 splays, blank, shot, a3
        fixture.checkRowCount(9);
        CHECK_FALSE(fixture.rowData(1, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowData(1, cwSurveyEditorModel::StationSplayCountRole).toInt() == 1);
        CHECK(fixture.splayReading(4, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("5.88"));
        CHECK(fixture.splayReading(5, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("8.96"));
    }

    SECTION("splays move into a station in another chunk") {
        auto secondChunk = new cwSurveyChunk();
        secondChunk->appendShot(cwStation("b1"), cwStation("b2"), cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));
        fixture.trip.addChunk(secondChunk);
        fixture.checkRowCount(14);

        const cwSurveyEditorRowIndex b2Row(secondChunk, 1, cwSurveyEditorRowIndex::StationRow);

        fixture.model.startSplayMove(fixture.stationRow(1), true);
        CHECK(fixture.model.isSplayMoveTarget(b2Row));

        fixture.model.commitSplayMove(b2Row);

        CHECK(fixture.chunk->stationSplayCount(1) == 0);
        CHECK(secondChunk->stationSplayCount(1) == 3);

        //a2 gives up its rows and b2 takes the splays without opening
        //title, a1, shot, a2, shot, a3, title, b1, shot, b2
        fixture.checkRowCount(10);
        const int b2ModelRow = fixture.model.toModelRow(b2Row);
        REQUIRE(b2ModelRow > 0);
        CHECK_FALSE(fixture.rowData(b2ModelRow, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowData(b2ModelRow, cwSurveyEditorModel::StationSplayCountRole).toInt() == 3);
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
        fixture.checkRowCount(10);
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

    //title, a1, shot, a2, 3 splays, blank, shot, a3
    fixture.checkRowCount(10);

    QSignalSpy dataChanged(&fixture.model, &QAbstractItemModel::dataChanged);
    REQUIRE(dataChanged.isValid());

    cwSurveyChunk::moveStationSplays(fixture.chunk, 1, fixture.chunk, 2, {0});

    //a2 gives up a row and the closed a3 shows the splay only on its chip
    fixture.checkRowCount(9);

    const int a3Row = fixture.model.toModelRow(fixture.stationRow(2));
    REQUIRE(a3Row == 8);
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

        fixture.checkRowCount(12);

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

        //title, a2, 3 splays, blank, shot, a3
        fixture.checkRowCount(8);
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

    //title, a1, shot, a2, 3 splays, blank, shot, a3, 1 splay, blank
    fixture.checkRowCount(12);
    CHECK(fixture.model.toModelRow(fixture.stationRow(1)) == 3);
    CHECK(fixture.model.toModelRow(fixture.stationRow(2)) == 9);
    CHECK(fixture.rowIndexOf(10).rowType() == cwSurveyEditorRowIndex::SplayRow);
    CHECK(fixture.splayReading(10, cwSurveyEditorModel::SplayDistanceRole) == QStringLiteral("7.56"));
}

TEST_CASE("The model says which cells the chunk stores a reading for",
          "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    SECTION("a station or shot cell belongs to the chunk") {
        for(auto role : {cwSurveyEditorCellIndex::StationNameCell,
                          cwSurveyEditorCellIndex::StationLeftCell,
                          cwSurveyEditorCellIndex::StationRightCell,
                          cwSurveyEditorCellIndex::StationUpCell,
                          cwSurveyEditorCellIndex::StationDownCell,
                          cwSurveyEditorCellIndex::ShotDistanceCell,
                          cwSurveyEditorCellIndex::ShotDistanceIncludedCell,
                          cwSurveyEditorCellIndex::ShotCompassCell,
                          cwSurveyEditorCellIndex::ShotBackCompassCell,
                          cwSurveyEditorCellIndex::ShotClinoCell,
                          cwSurveyEditorCellIndex::ShotBackClinoCell})
        {
            CHECK(fixture.model.isChunkCell(role));
        }
    }

    SECTION("the editor's own cells stand outside the chunk") {
        for(auto role : {cwSurveyEditorCellIndex::StationSplaysCell,
                          cwSurveyEditorCellIndex::SplayDistanceCell,
                          cwSurveyEditorCellIndex::SplayCompassCell,
                          cwSurveyEditorCellIndex::SplayClinoCell})
        {
            CHECK_FALSE(fixture.model.isChunkCell(role));
        }
    }
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

    SECTION("up and down stay in the column while the cluster is closed") {
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Down)
              == cell(a3Row, cwSurveyEditorCellIndex::StationSplaysCell));
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Up)
              == cell(a1Row, cwSurveyEditorCellIndex::StationSplaysCell));
    }

    SECTION("an open cluster moves the rows, and tab walks through it") {
        fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
        fixture.checkRowCount(10);

        //title, a1, shot, a2, s1, s2, s3, blank, shot, a3
        const int shiftedA3Row = a3Row + 4;

        //Tab leaves the cell into the cluster it has open, and comes back out
        //of the cluster's last row onto the station the closed cell tabs to
        CHECK(next(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Tab)
              == cell(a2Row + 1, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(next(cell(shiftedA3Row - 2, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Tab)
              == cell(shiftedA3Row, cwSurveyEditorCellIndex::StationNameCell));

        //The column the cell sits in runs past the cluster, since no splay
        //reading stands in it
        CHECK(next(cell(shiftedA3Row, cwSurveyEditorCellIndex::StationSplaysCell), cwSurveyEditorModel::Up)
              == cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell));
    }

    SECTION("down runs past an open cluster, and up comes back the same way") {
        fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
        fixture.checkRowCount(10);

        //A cluster's readings stand in the reading columns, not this one, so
        //the arrows walk station to station and tab is the way into the cluster
        const int shiftedA3Row = a3Row + 4;
        const auto splays = cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell);
        const auto below = next(splays, cwSurveyEditorModel::Down);
        CHECK(below == cell(shiftedA3Row, cwSurveyEditorCellIndex::StationSplaysCell));
        CHECK(next(below, cwSurveyEditorModel::Up) == splays);

        CHECK(next(splays, cwSurveyEditorModel::Tab)
              == cell(a2Row + 1, cwSurveyEditorCellIndex::SplayDistanceCell));
    }

    SECTION("the cell holds no reading to write") {
        CHECK_FALSE(fixture.model.setDataAt(cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell),
                                            QStringLiteral("99")));
    }
}

TEST_CASE("A splay's box data names a splay cell", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));

    //title, a1, shot, a2, s1, s2, s3, blank, shot, a3
    const int firstSplayRow = 4;
    const int blankRow = 7;
    const int a2ShotRow = 8;

    auto boxData = [&fixture](int row, cwSurveyEditorModel::Role role) {
        return fixture.rowData(row, role).value<cwSurveyEditorBoxData>();
    };

    const std::array<std::pair<cwSurveyEditorModel::Role, cwSurveyEditorCellIndex::CellRole>, 3>
        splayCells = {{
            {cwSurveyEditorModel::SplayDistanceRole, cwSurveyEditorCellIndex::SplayDistanceCell},
            {cwSurveyEditorModel::SplayCompassRole, cwSurveyEditorCellIndex::SplayCompassCell},
            {cwSurveyEditorModel::SplayClinoRole, cwSurveyEditorCellIndex::SplayClinoCell}
        }};

    SECTION("every reading of a splay carries its own cell") {
        for(const auto& [modelRole, cellRole] : splayCells) {
            for(int row : {firstSplayRow, firstSplayRow + 1, firstSplayRow + 2, blankRow}) {
                const auto data = boxData(row, modelRole);
                INFO("row: " << row << " cell: " << static_cast<int>(cellRole));
                CHECK(data.cellRole() == cellRole);

                //A splay hangs off a station at an index of its own, so no
                //chunk reading can name it
                CHECK_FALSE(cwSurveyEditorCellIndex::toChunkRole(data.cellRole()).has_value());
                CHECK(cwSurveyEditorCellIndex::toSplayReadingRole(data.cellRole()).has_value());
                CHECK(data.rowIndex() == fixture.rowIndexOf(row));
            }
        }
    }

    SECTION("the cell the box data names writes the reading it shows") {
        for(const auto& [modelRole, cellRole] : splayCells) {
            const auto data = boxData(firstSplayRow, modelRole);
            const auto cell = fixture.model.cellIndex(firstSplayRow, data.cellRole());
            INFO("cell: " << static_cast<int>(cellRole));
            CHECK(fixture.model.isCellValid(cell));
            CHECK(fixture.model.setDataAt(cell, QStringLiteral("7.75")));
            CHECK(fixture.splayReading(firstSplayRow, modelRole) == QStringLiteral("7.75"));
        }

        fixture.checkRowCount(10);
    }

    SECTION("a shot's box data still names its shot cell") {
        const auto distance = boxData(a2ShotRow, cwSurveyEditorModel::ShotDistanceRole);
        CHECK(distance.cellRole() == cwSurveyEditorCellIndex::ShotDistanceCell);
        CHECK(cwSurveyEditorCellIndex::toChunkRole(distance.cellRole())
              == cwSurveyChunk::ShotDistanceRole);

        const auto name = boxData(3, cwSurveyEditorModel::StationNameRole);
        CHECK(name.cellRole() == cwSurveyEditorCellIndex::StationNameCell);
        CHECK(cwSurveyEditorCellIndex::toChunkRole(name.cellRole())
              == cwSurveyChunk::StationNameRole);
    }
}

TEST_CASE("A splay's readings are cells the table writes", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(10);

    //title, a1, shot, a2, s1, s2, s3, blank, shot, a3
    const int firstSplayRow = 4;
    const int blankRow = 7;
    const int a2ShotRow = 8;

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

        fixture.checkRowCount(10);
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
        //The station row above the cluster, the shot row below it, and a row
        //past the end of the model
        CHECK_FALSE(fixture.model.setDataAt(cell(3, cwSurveyEditorCellIndex::SplayDistanceCell),
                                            QStringLiteral("99")));
        CHECK_FALSE(fixture.model.setDataAt(cell(a2ShotRow, cwSurveyEditorCellIndex::SplayCompassCell),
                                            QStringLiteral("99")));
        CHECK_FALSE(fixture.model.setDataAt(cell(fixture.model.rowCount(),
                                                 cwSurveyEditorCellIndex::SplayClinoCell),
                                            QStringLiteral("99")));

        fixture.checkRowCount(10);
        CHECK(fixture.splayReading(firstSplayRow, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("5.88"));
        CHECK(fixture.chunk->shot(1).compass().value() != QStringLiteral("99"));
    }

    SECTION("the blank row turns into a splay when a reading is written into it") {
        CHECK(fixture.rowData(blankRow, cwSurveyEditorModel::IsVirtualRole).toBool());

        CHECK(fixture.model.setDataAt(cell(blankRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                                      QStringLiteral("7.10")));

        //The row the reading landed on holds a splay now, and a fresh blank row
        //waits under it, so the next one is typed without reaching for anything
        fixture.checkRowCount(11);
        CHECK(fixture.chunk->stationSplayCount(1) == 4);
        CHECK(fixture.chunk->stationSplayAt(1, 3).distance.value() == QStringLiteral("7.10"));
        CHECK(fixture.splayReading(blankRow, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("7.10"));
        CHECK_FALSE(fixture.rowData(blankRow, cwSurveyEditorModel::IsVirtualRole).toBool());
        CHECK(fixture.rowData(blankRow + 1, cwSurveyEditorModel::IsVirtualRole).toBool());

        //The rest of the splay is written into the row it made, rather than
        //making another one
        CHECK(fixture.model.setDataAt(cell(blankRow, cwSurveyEditorCellIndex::SplayCompassCell),
                                      QStringLiteral("199.4")));
        fixture.checkRowCount(11);
        CHECK(fixture.chunk->stationSplayCount(1) == 4);
        CHECK(fixture.chunk->stationSplayAt(1, 3).compass.value() == QStringLiteral("199.4"));
    }

    SECTION("tabbing across the blank row without typing leaves it blank") {
        for(auto role : {cwSurveyEditorCellIndex::SplayDistanceCell,
                          cwSurveyEditorCellIndex::SplayCompassCell,
                          cwSurveyEditorCellIndex::SplayClinoCell})
        {
            CHECK(fixture.model.setDataAt(cell(blankRow, role), QString()));
        }

        fixture.checkRowCount(10);
        CHECK(fixture.chunk->stationSplayCount(1) == 3);
        CHECK(fixture.rowData(blankRow, cwSurveyEditorModel::IsVirtualRole).toBool());
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
    fixture.checkRowCount(10);

    //title, a1, shot, a2, s1, s2, s3, blank, shot, a3
    const int a1ShotRow = 2;
    const int a2Row = 3;
    const int firstSplayRow = 4;
    const int lastSplayRow = 6;
    const int blankRow = 7;
    const int a2ShotRow = 8;
    const int a3Row = 9;

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

    SECTION("tab out of the last splay carries on into the blank row") {
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Tab)
              == cell(blankRow, cwSurveyEditorCellIndex::SplayDistanceCell));
    }

    SECTION("tab through the blank row leaves the row the way the cluster's cell does") {
        //Nothing was typed, so the cluster is a pass-through and the chain
        //carries on to the next station's name, the way the table tabs today
        const auto compass = next(cell(blankRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                                  cwSurveyEditorModel::Tab);
        CHECK(compass == cell(blankRow, cwSurveyEditorCellIndex::SplayCompassCell));

        const auto clino = next(compass, cwSurveyEditorModel::Tab);
        CHECK(clino == cell(blankRow, cwSurveyEditorCellIndex::SplayClinoCell));

        CHECK(next(clino, cwSurveyEditorModel::Tab)
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

        //Above the cluster is the shot row the column runs onto; below the last
        //splay is the blank row, and below that the shot
        CHECK(next(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::Up)
              == cell(a1ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
        CHECK(next(cell(lastSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::Down)
              == cell(blankRow, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(next(cell(blankRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::Down)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
        CHECK(next(cell(blankRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Down)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotClinoCell));
    }

    SECTION("a trip on back sights leaves the cluster on the back sight boxes") {
        CHECK(next(cell(blankRow, cwSurveyEditorCellIndex::SplayCompassCell),
                   cwSurveyEditorModel::Down, false, true)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotBackCompassCell));
        CHECK(next(cell(blankRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Down, false, true)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotBackClinoCell));
    }

    SECTION("a trip that records neither sight drops the whole cluster onto the distance box") {
        CHECK(next(cell(blankRow, cwSurveyEditorCellIndex::SplayCompassCell),
                   cwSurveyEditorModel::Down, false, false)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
        CHECK(next(cell(blankRow, cwSurveyEditorCellIndex::SplayClinoCell),
                   cwSurveyEditorModel::Down, false, false)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
    }

    SECTION("the last station's cluster has no shot below it to step onto") {
        fixture.chunk->setStationSplays(2, {makeSplay("7.56", "201.3", "-12.4")});
        fixture.model.toggleSplaysExpanded(fixture.stationRow(2));
        fixture.checkRowCount(12);

        const int a3SplayRow = a3Row + 1;
        const int a3BlankRow = a3SplayRow + 1;
        CHECK(fixture.rowIndexOf(a3SplayRow).rowType() == cwSurveyEditorRowIndex::SplayRow);
        CHECK(next(cell(a3SplayRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                   cwSurveyEditorModel::Down)
              == cell(a3BlankRow, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK_FALSE(fixture.model.isCellValid(
            next(cell(a3BlankRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                 cwSurveyEditorModel::Down)));
    }
}

TEST_CASE("The keyboard leaves a cluster the way it came in", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(10);

    //title, a1, shot, a2, s1, s2, s3, blank, shot, a3
    const int a1ShotRow = 2;
    const int a2Row = 3;
    const int firstSplayRow = 4;
    const int blankRow = 7;
    const int a2ShotRow = 8;
    const int a3Row = 9;

    auto cell = [&fixture](int row, cwSurveyEditorCellIndex::CellRole role) {
        return fixture.model.cellIndex(row, role);
    };

    //Every step of a traversal, and the same traversal read back the other way.
    //A cluster is walked in by one key and out by its opposite, so the two lists
    //have to be each other reversed, cell for cell
    auto walk = [&fixture](const cwSurveyEditorCellIndex& start,
                           cwSurveyEditorModel::NavigationKey key,
                           int steps,
                           bool frontSights = true,
                           bool backSights = false) {
        QList<cwSurveyEditorCellIndex> cells{start};
        for(int step = 0; step < steps; ++step) {
            const auto nextCell = fixture.model.nextCell(cells.last(), key, frontSights, backSights);
            REQUIRE(fixture.model.isCellValid(nextCell));
            cells.append(nextCell);
        }
        return cells;
    };

    auto reversed = [](QList<cwSurveyEditorCellIndex> cells) {
        std::reverse(cells.begin(), cells.end());
        return cells;
    };

    SECTION("tab and shift-tab walk the same cells through the cluster") {
        //D, the Splays cell, the three readings of each splay and of the blank
        //row, then the next station's name
        const auto forward = walk(cell(a2Row, cwSurveyEditorCellIndex::StationDownCell),
                                  cwSurveyEditorModel::Tab, 14);

        CHECK(forward.at(1) == cell(a2Row, cwSurveyEditorCellIndex::StationSplaysCell));
        CHECK(forward.at(2) == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(forward.at(11) == cell(blankRow, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(forward.last() == cell(a3Row, cwSurveyEditorCellIndex::StationNameCell));

        CHECK(walk(forward.last(), cwSurveyEditorModel::BackTab, forward.size() - 1)
              == reversed(forward));
    }

    SECTION("up and down walk the same cells through every reading column") {
        struct Column {
            cwSurveyEditorCellIndex::CellRole splayRole;
            cwSurveyEditorCellIndex::CellRole shotRole;
        };

        //A trip on front sights alone draws each shot reading in one box, so
        //the splay column runs straight onto it
        for(const auto& column : {Column{cwSurveyEditorCellIndex::SplayDistanceCell,
                                          cwSurveyEditorCellIndex::ShotDistanceCell},
                                   Column{cwSurveyEditorCellIndex::SplayCompassCell,
                                          cwSurveyEditorCellIndex::ShotCompassCell},
                                   Column{cwSurveyEditorCellIndex::SplayClinoCell,
                                          cwSurveyEditorCellIndex::ShotClinoCell}})
        {
            //The shot above, the three splays, the blank row, and the shot below
            const auto down = walk(cell(a1ShotRow, column.shotRole),
                                   cwSurveyEditorModel::Down, 5);

            CHECK(down.at(1) == cell(firstSplayRow, column.splayRole));
            CHECK(down.at(4) == cell(blankRow, column.splayRole));
            CHECK(down.last() == cell(a2ShotRow, column.shotRole));

            CHECK(walk(down.last(), cwSurveyEditorModel::Up, down.size() - 1) == reversed(down));
        }
    }

    SECTION("a trip on both sights enters the shot row on the sight it shows first") {
        //Front and back stack in one column, so the cluster runs onto the front
        //box going down and off the back box going up
        const auto down = walk(cell(a1ShotRow, cwSurveyEditorCellIndex::ShotBackCompassCell),
                               cwSurveyEditorModel::Down, 5, true, true);

        CHECK(down.at(1) == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell));
        CHECK(down.last() == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotCompassCell));

        CHECK(walk(down.last(), cwSurveyEditorModel::Up, down.size() - 1, true, true)
              == reversed(down));
    }

    SECTION("a trip on back sights alone runs the column onto the back sight boxes") {
        const auto down = walk(cell(a1ShotRow, cwSurveyEditorCellIndex::ShotBackClinoCell),
                               cwSurveyEditorModel::Down, 5, false, true);

        CHECK(down.at(1) == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayClinoCell));
        CHECK(down.last() == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotBackClinoCell));

        CHECK(walk(down.last(), cwSurveyEditorModel::Up, down.size() - 1, false, true)
              == reversed(down));
    }

    SECTION("a trip that records neither sight walks the cluster on the distance boxes") {
        //The compass and clino boxes of a shot row are drawn for the sights the
        //trip records, so with neither recorded the distance column is the only
        //way in or out of the cluster
        const auto down = walk(cell(a1ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell),
                               cwSurveyEditorModel::Down, 5, false, false);

        CHECK(down.at(1) == cell(firstSplayRow, cwSurveyEditorCellIndex::SplayDistanceCell));
        CHECK(down.last() == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));

        CHECK(walk(down.last(), cwSurveyEditorModel::Up, down.size() - 1, false, false)
              == reversed(down));

        //The other two columns have no box of their own to land on, so they
        //leave the cluster on the distance box too
        CHECK(fixture.model.nextCell(cell(firstSplayRow, cwSurveyEditorCellIndex::SplayCompassCell),
                                     cwSurveyEditorModel::Up, false, false)
              == cell(a1ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
        CHECK(fixture.model.nextCell(cell(blankRow, cwSurveyEditorCellIndex::SplayClinoCell),
                                     cwSurveyEditorModel::Down, false, false)
              == cell(a2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
    }

    SECTION("a closed cluster leaves the column running shot to shot") {
        fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
        fixture.checkRowCount(6);

        const int closedA2ShotRow = 4;
        CHECK(fixture.model.nextCell(cell(a1ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell),
                                     cwSurveyEditorModel::Down, true, false)
              == cell(closedA2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
        CHECK(fixture.model.nextCell(cell(closedA2ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell),
                                     cwSurveyEditorModel::Up, true, false)
              == cell(a1ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell));
    }

    SECTION("a cluster at the head of the table has nowhere above it to go") {
        //Nothing stands above a1's cluster in the reading columns, so up out of
        //it stops where the caret is, the way down stops at the bottom row
        fixture.model.toggleSplaysExpanded(fixture.stationRow(0));
        fixture.checkRowCount(11);

        const int a1BlankRow = 2;
        for(auto role : {cwSurveyEditorCellIndex::SplayDistanceCell,
                          cwSurveyEditorCellIndex::SplayCompassCell,
                          cwSurveyEditorCellIndex::SplayClinoCell})
        {
            CHECK_FALSE(fixture.model.isCellValid(
                fixture.model.nextCell(cell(a1BlankRow, role),
                                       cwSurveyEditorModel::Up, true, false)));
        }

        //The cell that owns the cluster is still one shift-tab away
        CHECK(fixture.model.nextCell(cell(a1BlankRow, cwSurveyEditorCellIndex::SplayDistanceCell),
                                     cwSurveyEditorModel::BackTab, true, false)
              == cell(1, cwSurveyEditorCellIndex::StationSplaysCell));
    }

    SECTION("a cluster on a chunk's last station leaves into the chunk below it") {
        //A cluster hanging off the last station of a chunk has the next chunk's
        //rows below it, so both the tab chain and the reading columns carry on
        //into that chunk rather than stopping at the boundary
        auto secondChunk = new cwSurveyChunk();
        secondChunk->appendShot(cwStation("b1"), cwStation("b2"), cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));
        fixture.trip.addChunk(secondChunk);

        //The second chunk brings a title, two stations, and a shot
        fixture.checkRowCount(14);

        fixture.chunk->setStationSplays(2, {makeSplay("7.56", "201.3", "-12.4")});
        fixture.model.toggleSplaysExpanded(fixture.stationRow(2));
        fixture.checkRowCount(16);

        //title, a1, shot, a2, s1, s2, s3, blank, shot, a3, s1, blank,
        //title, b1, shot, b2
        const int a3BlankRow = 11;
        const int b1ShotRow = 14;
        const cwSurveyEditorRowIndex b1Row(secondChunk, 0, cwSurveyEditorRowIndex::StationRow);
        const int b1ModelRow = fixture.model.toModelRow(b1Row);
        REQUIRE(b1ModelRow == 13);
        REQUIRE(fixture.rowIndexOf(a3BlankRow).rowType() == cwSurveyEditorRowIndex::SplayRow);

        const auto down = walk(cell(a3BlankRow, cwSurveyEditorCellIndex::SplayCompassCell),
                               cwSurveyEditorModel::Down, 1);
        CHECK(down.last() == cell(b1ShotRow, cwSurveyEditorCellIndex::ShotCompassCell));
        CHECK(walk(down.last(), cwSurveyEditorModel::Up, 1) == reversed(down));

        const auto tabbed = walk(cell(a3BlankRow, cwSurveyEditorCellIndex::SplayClinoCell),
                                 cwSurveyEditorModel::Tab, 1);
        CHECK(tabbed.last() == cell(b1ModelRow, cwSurveyEditorCellIndex::StationNameCell));
        CHECK(walk(tabbed.last(), cwSurveyEditorModel::BackTab, 1) == reversed(tabbed));
    }
}

TEST_CASE("An entry row nobody typed into goes away with the focus",
          "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;

    //a1 carries no splays, so opening it is the blank entry row alone
    fixture.model.toggleSplaysExpanded(fixture.stationRow(0));
    fixture.checkRowCount(7);

    const int a1Row = 1;
    const int entryRow = 2;
    const auto entryCell = fixture.model.splayEntryCell(fixture.stationRow(0));
    REQUIRE(entryCell == fixture.model.cellIndex(entryRow,
                                                 cwSurveyEditorCellIndex::SplayDistanceCell));

    //Focusing a cell of the chunk brings its trailing blank station and shot
    //rows in, the same two rows the table shows the caver
    fixture.model.setFocusedCell(entryCell);
    fixture.checkRowCount(9);
    CHECK(fixture.model.focusedRow() == entryRow);

    auto stationCell = [&fixture](int stationIndex) {
        const int row = fixture.model.toModelRow(fixture.stationRow(stationIndex));
        return fixture.model.cellIndex(row, cwSurveyEditorCellIndex::StationNameCell);
    };

    SECTION("the focus landing elsewhere takes the row with it") {
        fixture.model.setFocusedCell(stationCell(2));

        fixture.checkRowCount(8);
        CHECK_FALSE(fixture.rowData(a1Row, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowIndexOf(entryRow).rowType() == cwSurveyEditorRowIndex::ShotRow);

        //The retracted row sat above the focus, so the focus rode the removal
        //up to the row a3 sits on now
        CHECK(fixture.model.focusedRow() == fixture.model.toModelRow(fixture.stationRow(2)));
    }

    SECTION("the station that owns the cluster keeps the row open") {
        //The Splays cell and the readings beside it are all part of typing into
        //the cluster the row hangs from
        fixture.model.setFocusedCell(stationCell(0));

        fixture.checkRowCount(9);
        CHECK(fixture.rowData(a1Row, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    }

    SECTION("a reading typed into the row keeps it") {
        REQUIRE(fixture.model.setDataAt(entryCell, QStringLiteral("4.2")));

        //The row that was blank holds a splay now, and a fresh blank waits under it
        fixture.checkRowCount(10);

        fixture.model.setFocusedCell(stationCell(2));

        fixture.checkRowCount(10);
        CHECK(fixture.rowData(a1Row, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.chunk->stationSplayCount(0) == 1);
        CHECK(fixture.splayReading(entryRow, cwSurveyEditorModel::SplayDistanceRole)
              == QStringLiteral("4.2"));
    }

    SECTION("a cluster with splays keeps the blank row it carries") {
        fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
        fixture.checkRowCount(13);

        fixture.model.setFocusedCell(stationCell(2));

        //a1's entry row goes, a2's cluster and its blank row stay
        fixture.checkRowCount(12);
        CHECK(fixture.rowData(fixture.model.toModelRow(fixture.stationRow(1)),
                              cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    }

    SECTION("the focus landing in another chunk takes the row with it") {
        auto secondChunk = new cwSurveyChunk();
        secondChunk->appendShot(cwStation("b1"), cwStation("b2"),
                                cwShot("5.0", "10.0", "190.0", "1.0", "-1.0"));
        fixture.trip.addChunk(secondChunk);

        //The second chunk brings a title, two stations, and a shot
        fixture.checkRowCount(13);

        const cwSurveyEditorRowIndex b1Row(secondChunk, 0, cwSurveyEditorRowIndex::StationRow);
        fixture.model.setFocusedCell(
            fixture.model.cellIndex(fixture.model.toModelRow(b1Row),
                                    cwSurveyEditorCellIndex::StationNameCell));

        //The trailing blank rows cross to the second chunk, so the table only
        //loses a1's abandoned entry row
        fixture.checkRowCount(12);
        CHECK_FALSE(fixture.rowData(a1Row, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
        CHECK(fixture.rowIndexOf(entryRow).rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(fixture.model.focusedRow() == fixture.model.toModelRow(b1Row));
    }
}

TEST_CASE("Retiring a trip forgets which clusters were open", "[cwSurveyEditorModel][SplayShot]") {
    SplayFixture fixture;
    fixture.model.toggleSplaysExpanded(fixture.stationRow(1));
    fixture.checkRowCount(10);

    cwTrip otherTrip;
    fixture.model.setTrip(&otherTrip);
    fixture.checkRowCount(0);

    fixture.model.setTrip(&fixture.trip);
    fixture.checkRowCount(6);
    CHECK_FALSE(fixture.rowData(3, cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
}
