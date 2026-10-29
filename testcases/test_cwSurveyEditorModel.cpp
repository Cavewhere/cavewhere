//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "LoadProjectHelper.h"

//Our includes
#include "cwTrip.h"
#include "cwSurveyEditorModel.h"
#include "cwSurveyChunk.h"
#include "cwSurveyEditorBoxData.h"
#include "cwErrorListModel.h"
#include "cwFixStationModel.h"
#include "cwEntranceModel.h"
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwFutureManagerModel.h"
#include "Account.h"
#include <asyncfuture.h>

//Qt includes
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>

//Test includes
#include "SpyChecker.h"
#include "SurveyEditorTestHelper.h"
#include "TestHelper.h"

namespace {

/**
 * True when \a spy saw a dataChanged whose row range covers \a row and whose
 * roles include \a role. An empty roles list is Qt's spelling of "every role",
 * so it counts as covering anything.
 */
bool dataChangedCovers(const QSignalSpy& spy, int row, cwSurveyEditorModel::Role role)
{
    for(const auto& call : spy) {
        const auto topLeft = call.at(0).toModelIndex();
        const auto bottomRight = call.at(1).toModelIndex();
        const auto roles = call.at(2).value<QList<int>>();
        if(row >= topLeft.row() && row <= bottomRight.row()
            && (roles.contains(role) || roles.isEmpty()))
        {
            return true;
        }
    }
    return false;
}

}

TEST_CASE("cwSurveyEditorModel new chunk should work correctly", "[cwSurveyEditorModel]") {
    cwTrip trip;
    trip.addNewChunk();

    REQUIRE(trip.chunkCount() > 0);
    auto firstChunk = trip.chunk(0);

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    REQUIRE(model.rowCount() == 4);

    auto i0 = model.index(0);
    CHECK(i0.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i0.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::TitleRow);
    CHECK(i0.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == -1);

    auto i1 = model.index(1); //Station 1
    CHECK(!i1.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(i1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == 0);

    auto i2 = model.index(2); //Shot
    CHECK(i2.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotCalibrationRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(i2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == 0);

    auto i3 = model.index(3); //Station
    CHECK(!i3.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(i3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == 1);

    CHECK(i3.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotCalibrationRole).isNull());
}

TEST_CASE("cwSurveyEditorModel row+role APIs should be consistent",
          "[cwSurveyEditorModel][row-role-api]")
{
    cwTrip trip;
    trip.addNewChunk();
    auto* chunk = trip.chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(&trip);
    model.setFocusedChunk(chunk);

    const int stationRow = model.toModelRow(model.rowIndex(chunk, 1, cwSurveyEditorRowIndex::StationRow));
    const int shotRow = model.toModelRow(model.rowIndex(chunk, 0, cwSurveyEditorRowIndex::ShotRow));
    REQUIRE(stationRow >= 0);
    REQUIRE(shotRow >= 0);

    CHECK(model.setDataAt(model.cellIndex(stationRow, cwSurveyEditorCellIndex::StationNameCell), "S1"));
    CHECK(chunk->station(1).name() == "S1");

    CHECK_FALSE(model.setDataAt(model.cellIndex(stationRow, cwSurveyEditorCellIndex::ShotDistanceCell), "bad-role"));

    CHECK(model.shotDistanceIncludedAt(model.cellIndex(shotRow, cwSurveyEditorCellIndex::ShotDistanceCell)));
    CHECK_FALSE(model.shotDistanceIncludedAt(model.cellIndex(stationRow, cwSurveyEditorCellIndex::StationNameCell)));

    CHECK(model.canInsertStationAt(model.cellIndex(stationRow, cwSurveyEditorCellIndex::StationNameCell), cwSurveyChunk::Above));
    CHECK_FALSE(model.canRemoveStationAt(model.cellIndex(stationRow, cwSurveyEditorCellIndex::StationNameCell), cwSurveyChunk::Above));
    CHECK(model.canInsertShotAt(model.cellIndex(shotRow, cwSurveyEditorCellIndex::ShotDistanceCell), cwSurveyChunk::Above));
    CHECK_FALSE(model.canRemoveShotAt(model.cellIndex(shotRow, cwSurveyEditorCellIndex::ShotDistanceCell), cwSurveyChunk::Above));

    CHECK_FALSE(model.canInsertStationAt(model.cellIndex(stationRow, cwSurveyEditorCellIndex::ShotDistanceCell), cwSurveyChunk::Above));
    CHECK_FALSE(model.canInsertShotAt(model.cellIndex(shotRow, cwSurveyEditorCellIndex::StationNameCell), cwSurveyChunk::Above));
}

TEST_CASE("cwSurveyEditorModel setDataAt appends from virtual trailing rows",
          "[cwSurveyEditorModel][row-role-api][virtual]")
{
    cwTrip trip;
    trip.addNewChunk();
    auto* chunk = trip.chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(&trip);
    model.setFocusedChunk(chunk);

    // Make trailing real station non-empty so virtual station/shot rows are visible.
    REQUIRE(model.setDataAt(model.cellIndex(model.toModelRow(model.rowIndex(chunk, 1, cwSurveyEditorRowIndex::StationRow)),
                                            cwSurveyEditorCellIndex::StationNameCell),
                            "S1"));
    REQUIRE(model.toModelRow(model.rowIndex(chunk, chunk->stationCount(), cwSurveyEditorRowIndex::StationRow)) >= 0);

    const int stationsBefore = chunk->stationCount();
    const int shotsBefore = chunk->shotCount();

    const int virtualStationRow = model.toModelRow(model.rowIndex(chunk,
                                                                  chunk->stationCount(),
                                                                  cwSurveyEditorRowIndex::StationRow));
    REQUIRE(virtualStationRow >= 0);
    REQUIRE(model.setDataAt(model.cellIndex(virtualStationRow, cwSurveyEditorCellIndex::StationNameCell), "S2"));

    CHECK(chunk->stationCount() == stationsBefore + 1);
    CHECK(chunk->shotCount() == shotsBefore + 1);
    CHECK(chunk->station(chunk->stationCount() - 1).name() == "S2");
}

TEST_CASE("cwSurveyEditorModel setDataAt appends below a nameless trailing station",
          "[cwSurveyEditorModel][row-role-api][virtual]")
{
    cwTrip trip;
    trip.addNewChunk();
    auto* chunk = trip.chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(&trip);
    model.setFocusedChunk(chunk);

    const auto cellAt = [&](int indexInChunk, cwSurveyEditorRowIndex::RowType rowType, cwSurveyEditorCellIndex::CellRole role) {
        return model.cellIndex(model.toModelRow(model.rowIndex(chunk, indexInChunk, rowType)), role);
    };

    REQUIRE(model.setDataAt(cellAt(0, cwSurveyEditorRowIndex::StationRow, cwSurveyEditorCellIndex::StationNameCell), "1"));
    REQUIRE(model.setDataAt(cellAt(1, cwSurveyEditorRowIndex::StationRow, cwSurveyEditorCellIndex::StationNameCell), "2"));
    REQUIRE(model.setDataAt(cellAt(0, cwSurveyEditorRowIndex::ShotRow, cwSurveyEditorCellIndex::ShotDistanceCell), "10"));
    REQUIRE(model.setDataAt(cellAt(1, cwSurveyEditorRowIndex::StationRow, cwSurveyEditorCellIndex::StationNameCell), ""));
    REQUIRE(chunk->stationCount() == 2);
    REQUIRE(chunk->shotCount() == 1);

    const int virtualStationRow = model.toModelRow(model.rowIndex(chunk, chunk->stationCount(), cwSurveyEditorRowIndex::StationRow));
    REQUIRE(virtualStationRow >= 0);

    cwSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);
    const int rowsBefore = model.rowCount();

    REQUIRE(model.setDataAt(model.cellIndex(virtualStationRow, cwSurveyEditorCellIndex::StationNameCell), "3"));

    CHECK(chunk->stationCount() == 3);
    CHECK(chunk->shotCount() == 2);
    CHECK(chunk->station(2).name() == "3");
    CHECK(chunk->shot(0).distance().value() == "10");

    //The chunk grew one shot and one station, and a fresh blank pair sits below
    const int titleRows = 1;
    const int blankPairRows = 2;
    CHECK(rowsInsertedSpy.count() == 1);
    CHECK(model.rowCount() == rowsBefore + blankPairRows);
    CHECK(model.rowCount() == titleRows + chunk->stationCount() + chunk->shotCount() + blankPairRows);
}

TEST_CASE("cwSurveyEditorModel exposes row metadata roles for real and virtual rows",
          "[cwSurveyEditorModel][row-role-api][row-metadata]")
{
    cwTrip trip;
    trip.addNewChunk();
    auto* chunk = trip.chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(&trip);
    model.setFocusedChunk(chunk);

    const int titleRow = model.toModelRow(model.rowIndex(chunk, -1, cwSurveyEditorRowIndex::TitleRow));
    const int station0Row = model.toModelRow(model.rowIndex(chunk, 0, cwSurveyEditorRowIndex::StationRow));
    const int shot0Row = model.toModelRow(model.rowIndex(chunk, 0, cwSurveyEditorRowIndex::ShotRow));
    REQUIRE(titleRow >= 0);
    REQUIRE(station0Row >= 0);
    REQUIRE(shot0Row >= 0);

    auto titleIndex = model.index(titleRow, 0);
    auto station0Index = model.index(station0Row, 0);
    auto shot0Index = model.index(shot0Row, 0);

    CHECK(titleIndex.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorRowIndex::TitleRow);
    CHECK(titleIndex.data(cwSurveyEditorModel::IndexInChunkRole).toInt() == -1);
    CHECK(titleIndex.data(cwSurveyEditorModel::ChunkRole).value<cwSurveyChunk*>() == chunk);
    CHECK_FALSE(titleIndex.data(cwSurveyEditorModel::IsVirtualRole).toBool());

    CHECK(station0Index.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorRowIndex::StationRow);
    CHECK(station0Index.data(cwSurveyEditorModel::IndexInChunkRole).toInt() == 0);
    CHECK(station0Index.data(cwSurveyEditorModel::ChunkRole).value<cwSurveyChunk*>() == chunk);
    CHECK_FALSE(station0Index.data(cwSurveyEditorModel::IsVirtualRole).toBool());

    CHECK(shot0Index.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(shot0Index.data(cwSurveyEditorModel::IndexInChunkRole).toInt() == 0);
    CHECK(shot0Index.data(cwSurveyEditorModel::ChunkRole).value<cwSurveyChunk*>() == chunk);
    CHECK_FALSE(shot0Index.data(cwSurveyEditorModel::IsVirtualRole).toBool());

    // Make trailing real station non-empty so virtual trailing rows appear.
    REQUIRE(model.setDataAt(model.cellIndex(model.toModelRow(model.rowIndex(chunk, 1, cwSurveyEditorRowIndex::StationRow)),
                                            cwSurveyEditorCellIndex::StationNameCell),
                            "S1"));

    const int virtualStationRow = model.toModelRow(model.rowIndex(chunk,
                                                                  chunk->stationCount(),
                                                                  cwSurveyEditorRowIndex::StationRow));
    const int virtualShotRow = model.toModelRow(model.rowIndex(chunk,
                                                               chunk->shotCount(),
                                                               cwSurveyEditorRowIndex::ShotRow));
    REQUIRE(virtualStationRow >= 0);
    REQUIRE(virtualShotRow >= 0);

    auto virtualStationIndex = model.index(virtualStationRow, 0);
    auto virtualShotIndex = model.index(virtualShotRow, 0);

    CHECK(virtualStationIndex.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorRowIndex::StationRow);
    CHECK(virtualStationIndex.data(cwSurveyEditorModel::IndexInChunkRole).toInt() == chunk->stationCount());
    CHECK(virtualStationIndex.data(cwSurveyEditorModel::ChunkRole).value<cwSurveyChunk*>() == chunk);
    CHECK(virtualStationIndex.data(cwSurveyEditorModel::IsVirtualRole).toBool());

    CHECK(virtualShotIndex.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(virtualShotIndex.data(cwSurveyEditorModel::IndexInChunkRole).toInt() == chunk->shotCount());
    CHECK(virtualShotIndex.data(cwSurveyEditorModel::ChunkRole).value<cwSurveyChunk*>() == chunk);
    CHECK(virtualShotIndex.data(cwSurveyEditorModel::IsVirtualRole).toBool());
}

TEST_CASE("cwSurveyEditorModel row+role helpers and nextCell navigation",
          "[cwSurveyEditorModel][row-role-api][navigation]")
{
    cwTrip trip;
    trip.addNewChunk();
    auto* chunk = trip.chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(&trip);
    model.setFocusedChunk(chunk);

    const int station1Row = model.modelRowForCellRole(chunk, 1, cwSurveyEditorCellIndex::StationNameCell);
    const int shot0Row = model.modelRowForCellRole(chunk, 0, cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(station1Row >= 0);
    REQUIRE(shot0Row >= 0);

    CHECK(model.data(model.index(station1Row, 0), cwSurveyEditorModel::RowTypeRole).toInt()
          == cwSurveyEditorRowIndex::StationRow);
    CHECK(model.chunkForRow(station1Row) == chunk);
    CHECK(model.isCellValid(model.cellIndex(station1Row, cwSurveyEditorCellIndex::StationNameCell)));
    CHECK_FALSE(model.isCellValid(model.cellIndex(station1Row, cwSurveyEditorCellIndex::ShotDistanceCell)));

    CHECK(model.isCellSelected(model.cellIndex(station1Row, cwSurveyEditorCellIndex::StationNameCell),
                               model.cellIndex(station1Row, cwSurveyEditorCellIndex::StationNameCell)));
    CHECK_FALSE(model.isCellSelected(model.cellIndex(station1Row, cwSurveyEditorCellIndex::StationNameCell),
                                     model.cellIndex(shot0Row, cwSurveyEditorCellIndex::ShotDistanceCell)));

    const auto tabFromStation = model.nextCell(model.cellIndex(station1Row, cwSurveyEditorCellIndex::StationNameCell),
                                               cwSurveyEditorModel::Tab,
                                               false,
                                               false);
    CHECK(tabFromStation.modelRow() == shot0Row);
    CHECK(tabFromStation.cellRole() == cwSurveyEditorCellIndex::ShotDistanceCell);

    const auto backTabFromShot = model.nextCell(model.cellIndex(shot0Row, cwSurveyEditorCellIndex::ShotDistanceCell),
                                                cwSurveyEditorModel::BackTab,
                                                false,
                                                false);
    CHECK(backTabFromShot.modelRow() == station1Row);
    CHECK(backTabFromShot.cellRole() == cwSurveyEditorCellIndex::StationNameCell);

    const auto rightFromShotNoFront = model.nextCell(model.cellIndex(shot0Row, cwSurveyEditorCellIndex::ShotDistanceCell),
                                                     cwSurveyEditorModel::Right,
                                                     false,
                                                     false);
    CHECK(rightFromShotNoFront.cellRole() == cwSurveyEditorCellIndex::StationLeftCell);

    const auto rightFromShotFront = model.nextCell(model.cellIndex(shot0Row, cwSurveyEditorCellIndex::ShotDistanceCell),
                                                   cwSurveyEditorModel::Right,
                                                   true,
                                                   false);
    CHECK(rightFromShotFront.cellRole() == cwSurveyEditorCellIndex::ShotCompassCell);

    const auto tabFromShotBoth = model.nextCell(model.cellIndex(shot0Row, cwSurveyEditorCellIndex::ShotDistanceCell),
                                                cwSurveyEditorModel::Tab,
                                                true,
                                                true);
    CHECK(tabFromShotBoth.cellRole() == cwSurveyEditorCellIndex::ShotCompassCell);

    const auto rightFromShotBoth = model.nextCell(model.cellIndex(shot0Row, cwSurveyEditorCellIndex::ShotDistanceCell),
                                                  cwSurveyEditorModel::Right,
                                                  true,
                                                  true);
    CHECK(rightFromShotBoth.cellRole() == cwSurveyEditorCellIndex::ShotBackCompassCell);

    const int station0Row = model.modelRowForCellRole(chunk, 0, cwSurveyEditorCellIndex::StationNameCell);
    REQUIRE(station0Row >= 0);
    const auto backTabFromStationLeftBoth = model.nextCell(model.cellIndex(station0Row, cwSurveyEditorCellIndex::StationLeftCell),
                                                           cwSurveyEditorModel::BackTab,
                                                           true,
                                                           true);
    CHECK(backTabFromStationLeftBoth.cellRole() == cwSurveyEditorCellIndex::ShotBackClinoCell);
}

TEST_CASE("cwSurveyEditorModel ShotDistance Tab prefers front/back readings then from-station LRUD",
          "[cwSurveyEditorModel][row-role-api][navigation][regression]")
{
    cwTrip trip;
    trip.addNewChunk();
    auto* chunk = trip.chunk(0);
    REQUIRE(chunk != nullptr);

    // Materialize a second real shot/station pair so we can validate non-zero shot-index behavior.
    chunk->setData(cwSurveyChunk::StationNameRole, 1, "S1");
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10");
    chunk->appendNewShot();

    cwSurveyEditorModel model;
    model.setTrip(&trip);
    model.setFocusedChunk(chunk);

    const int shot0Row = model.modelRowForCellRole(chunk, 0, cwSurveyEditorCellIndex::ShotDistanceCell);
    const int shot1Row = model.modelRowForCellRole(chunk, 1, cwSurveyEditorCellIndex::ShotDistanceCell);
    const int compass0Row = model.modelRowForCellRole(chunk, 0, cwSurveyEditorCellIndex::ShotCompassCell);
    const int compass1Row = model.modelRowForCellRole(chunk, 1, cwSurveyEditorCellIndex::ShotCompassCell);
    const int station0Row = model.modelRowForCellRole(chunk, 0, cwSurveyEditorCellIndex::StationLeftCell);
    const int station1Row = model.modelRowForCellRole(chunk, 1, cwSurveyEditorCellIndex::StationLeftCell);
    REQUIRE(station0Row >= 0);
    REQUIRE(station1Row >= 0);
    REQUIRE(shot0Row >= 0);
    REQUIRE(shot1Row >= 0);
    REQUIRE(compass0Row >= 0);
    REQUIRE(compass1Row >= 0);

    CHECK(model.index(shot0Row, 0).data(cwSurveyEditorModel::IndexInChunkRole).toInt() == 0);
    CHECK(model.index(shot1Row, 0).data(cwSurveyEditorModel::IndexInChunkRole).toInt() == 1);

    struct TabExpectation {
        int shotRow;
        int expectedStationRow;
        bool frontSights;
        bool backSights;
        int expectedRole;
    };

    const QVector<TabExpectation> cases {
        // Shot 0
        { shot0Row, compass0Row, true,  true,  cwSurveyEditorCellIndex::ShotCompassCell },
        { shot0Row, compass0Row, true,  false, cwSurveyEditorCellIndex::ShotCompassCell },
        { shot0Row, compass0Row, false, true,  cwSurveyEditorCellIndex::ShotBackCompassCell },
        { shot0Row, station0Row, false, false, cwSurveyEditorCellIndex::StationLeftCell },
        // Shot 1
        { shot1Row, compass1Row, true,  true,  cwSurveyEditorCellIndex::ShotCompassCell },
        { shot1Row, compass1Row, true,  false, cwSurveyEditorCellIndex::ShotCompassCell },
        { shot1Row, compass1Row, false, true,  cwSurveyEditorCellIndex::ShotBackCompassCell },
        { shot1Row, station1Row, false, false, cwSurveyEditorCellIndex::StationLeftCell },
    };

    for(const auto& c : cases) {
        const auto next = model.nextCell(model.cellIndex(c.shotRow, cwSurveyEditorCellIndex::ShotDistanceCell),
                                         cwSurveyEditorModel::Tab,
                                         c.frontSights,
                                         c.backSights);

        if(c.expectedRole == cwSurveyEditorCellIndex::StationLeftCell) {
            // Regression contract: with no sights, tab from distance must go to LEFT
            // on the from-station (shot 0 -> station 0, shot 1 -> station 1), not +1.
            CHECK(next.modelRow() == c.expectedStationRow);
        } else {
            // Compass/back-compass stay on the same shot row.
            CHECK(next.modelRow() == c.shotRow);
        }
        CHECK(next.cellRole() == c.expectedRole);
    }
}

TEST_CASE("cwSurveyEditorModel virtual row commit emits coherent row/data signals",
          "[cwSurveyEditorModel][row-role-api][virtual-signals]")
{
    cwTrip trip;
    trip.addNewChunk();
    auto* chunk = trip.chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(&trip);
    model.setFocusedChunk(chunk);

    // Make trailing real station/shot non-empty so virtual rows are visible.
    REQUIRE(model.setDataAt(model.cellIndex(model.modelRowForCellRole(chunk, 1, cwSurveyEditorCellIndex::StationNameCell),
                                            cwSurveyEditorCellIndex::StationNameCell),
                            "S1"));
    const int virtualShotRowBefore = model.modelRowForCellRole(chunk, chunk->shotCount(), cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(virtualShotRowBefore >= 0);

    QSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    rowsInsertedSpy.clear();
    rowsRemovedSpy.clear();
    dataChangedSpy.clear();

    const int rowsBefore = model.rowCount();
    REQUIRE(model.setDataAt(model.cellIndex(virtualShotRowBefore, cwSurveyEditorCellIndex::ShotDistanceCell), "30"));

    CHECK(model.rowCount() == rowsBefore + 2);
    CHECK(rowsRemovedSpy.count() == 0);  // virtual rows are preserved and remapped to real rows
    CHECK(rowsInsertedSpy.count() >= 1); // trailing virtual station/shot rows inserted
    CHECK(dataChangedSpy.count() >= 1);

    const int committedShotIndex = chunk->shotCount() - 1;
    const int committedShotRow = model.modelRowForCellRole(chunk, committedShotIndex, cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(committedShotRow >= 0);
    const auto committedShotData = model.data(model.index(committedShotRow, 0), cwSurveyEditorModel::ShotDistanceRole)
                                       .value<cwSurveyEditorBoxData>();
    CHECK(committedShotData.reading().value() == "30");

    const int virtualShotRowAfter = model.modelRowForCellRole(chunk, chunk->shotCount(), cwSurveyEditorCellIndex::ShotDistanceCell);
    CHECK(virtualShotRowAfter >= 0);
}

TEST_CASE("cwSurveyEditorModel focused row tracks persistent row shifts",
          "[cwSurveyEditorModel][focus][persistent]")
{
    cwTrip trip;
    trip.addNewChunk();
    trip.addNewChunk();

    auto* firstChunk = trip.chunk(0);
    auto* secondChunk = trip.chunk(1);
    REQUIRE(firstChunk != nullptr);
    REQUIRE(secondChunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    const int secondStation0Row = model.modelRowForCellRole(secondChunk, 0, cwSurveyEditorCellIndex::StationNameCell);
    REQUIRE(secondStation0Row >= 0);
    model.setFocusedCell(model.cellIndex(secondStation0Row, cwSurveyEditorCellIndex::StationNameCell));
    REQUIRE(model.focusedRow() == secondStation0Row);
    REQUIRE(model.focusedRole() == static_cast<int>(cwSurveyEditorCellIndex::StationNameCell));

    QSignalSpy focusedRowSpy(&model, &cwSurveyEditorModel::focusedRowChanged);
    QSignalSpy focusedRoleSpy(&model, &cwSurveyEditorModel::focusedRoleChanged);
    focusedRowSpy.clear();
    focusedRoleSpy.clear();

    firstChunk->insertShot(0, cwSurveyChunk::Below); // Inserts two rows before the focused row.

    CHECK(model.focusedRow() == secondStation0Row + 2);
    CHECK(model.focusedRole() == static_cast<int>(cwSurveyEditorCellIndex::StationNameCell));
    CHECK(focusedRowSpy.count() >= 1);
    CHECK(focusedRoleSpy.count() == 0);
}

TEST_CASE("cwSurveyEditorModel trims trailing empty rows when focus changes after insert above",
          "[cwSurveyEditorModel][focus][regression]")
{
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());

    const QString filename = copyToTempFolder(testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
    project->loadOrConvert(filename);
    project->waitLoadToFinish();

    REQUIRE(project->cavingRegion() != nullptr);
    REQUIRE(project->cavingRegion()->caveCount() > 0);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(cave != nullptr);
    REQUIRE(cave->tripCount() > 0);
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    REQUIRE(trip->chunkCount() > 1);

    auto* firstChunk = trip->chunk(0);
    auto* secondChunk = trip->chunk(1);
    REQUIRE(firstChunk != nullptr);
    REQUIRE(secondChunk != nullptr);
    REQUIRE(firstChunk->stationCount() > 2);
    REQUIRE(firstChunk->shotCount() > 1);

    cwSurveyEditorModel model;
    model.setTrip(trip);
    model.setFocusedChunk(firstChunk);

    auto isStationShotEmpty = [](cwSurveyChunk* chunk, int stationIndex) {
        if(chunk == nullptr || stationIndex <= 0 || stationIndex >= chunk->stationCount()) {
            return false;
        }
        const cwStation station = chunk->station(stationIndex);
        const cwShot shot = chunk->shot(stationIndex - 1);
        return station.name().trimmed().isEmpty()
                && station.left().state() == cwDistanceReading::State::Empty
                && station.right().state() == cwDistanceReading::State::Empty
                && station.up().state() == cwDistanceReading::State::Empty
                && station.down().state() == cwDistanceReading::State::Empty
                && shot.distance().state() == cwDistanceReading::State::Empty
                && shot.compass().state() == cwCompassReading::State::Empty
                && shot.backCompass().state() == cwCompassReading::State::Empty
                && shot.clino().state() == cwClinoReading::State::Empty
                && shot.backClino().state() == cwClinoReading::State::Empty;
    };

    while(firstChunk->stationCount() > 2 && isStationShotEmpty(firstChunk, firstChunk->stationCount() - 1)) {
        firstChunk->removeStation(firstChunk->stationCount() - 1, cwSurveyChunk::Above);
    }

    const int lastNonEmptyStationIndex = firstChunk->stationCount() - 1;
    REQUIRE(lastNonEmptyStationIndex >= 1);
    if(firstChunk->station(lastNonEmptyStationIndex).name().trimmed().isEmpty()) {
        const int stationRow = model.modelRowForCellRole(firstChunk, lastNonEmptyStationIndex, cwSurveyEditorCellIndex::StationNameCell);
        REQUIRE(stationRow >= 0);
        REQUIRE(model.setDataAt(model.cellIndex(stationRow, cwSurveyEditorCellIndex::StationNameCell), "temp_last_station"));
    }

    const int insertShotRow = model.modelRowForCellRole(firstChunk, lastNonEmptyStationIndex - 1, cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(insertShotRow >= 0);
    REQUIRE(model.canInsertShotAt(model.cellIndex(insertShotRow, cwSurveyEditorCellIndex::ShotDistanceCell), cwSurveyChunk::Above));
    model.insertShotAt(model.cellIndex(insertShotRow, cwSurveyEditorCellIndex::ShotDistanceCell), cwSurveyChunk::Above);

    const int movedStationIndex = firstChunk->stationCount() - 1;
    REQUIRE(movedStationIndex >= 1);
    const int movedStationRow = model.modelRowForCellRole(firstChunk, movedStationIndex, cwSurveyEditorCellIndex::StationNameCell);
    REQUIRE(movedStationRow >= 0);

    REQUIRE(model.setDataAt(model.cellIndex(movedStationRow, cwSurveyEditorCellIndex::StationNameCell), ""));
    REQUIRE(model.setDataAt(model.cellIndex(movedStationRow, cwSurveyEditorCellIndex::StationLeftCell), ""));
    REQUIRE(model.setDataAt(model.cellIndex(movedStationRow, cwSurveyEditorCellIndex::StationRightCell), ""));
    REQUIRE(model.setDataAt(model.cellIndex(movedStationRow, cwSurveyEditorCellIndex::StationUpCell), ""));
    REQUIRE(model.setDataAt(model.cellIndex(movedStationRow, cwSurveyEditorCellIndex::StationDownCell), ""));

    const int chunk0TitleBefore = model.toModelRow(model.rowIndex(firstChunk, -1, cwSurveyEditorRowIndex::TitleRow));
    const int chunk1TitleBefore = model.toModelRow(model.rowIndex(secondChunk, -1, cwSurveyEditorRowIndex::TitleRow));
    REQUIRE(chunk0TitleBefore >= 0);
    REQUIRE(chunk1TitleBefore > chunk0TitleBefore);

    CHECK(model.toModelRow(model.rowIndex(firstChunk, firstChunk->stationCount(), cwSurveyEditorRowIndex::StationRow)) >= 0);
    CHECK(model.toModelRow(model.rowIndex(firstChunk, firstChunk->shotCount(), cwSurveyEditorRowIndex::ShotRow)) >= 0);
    CHECK(model.toModelRow(model.rowIndex(firstChunk, firstChunk->stationCount() + 1, cwSurveyEditorRowIndex::StationRow)) == -1);
    CHECK(model.toModelRow(model.rowIndex(firstChunk, firstChunk->shotCount() + 1, cwSurveyEditorRowIndex::ShotRow)) == -1);

    const int expectedFocusedRows = firstChunk->stationCount() + firstChunk->shotCount() + 1 + 2;
    CHECK((chunk1TitleBefore - chunk0TitleBefore) == expectedFocusedRows);

    model.setFocusedChunk(secondChunk);

    const int chunk0TitleAfter = model.toModelRow(model.rowIndex(firstChunk, -1, cwSurveyEditorRowIndex::TitleRow));
    const int chunk1TitleAfter = model.toModelRow(model.rowIndex(secondChunk, -1, cwSurveyEditorRowIndex::TitleRow));
    REQUIRE(chunk0TitleAfter >= 0);
    REQUIRE(chunk1TitleAfter > chunk0TitleAfter);

    CHECK(model.toModelRow(model.rowIndex(firstChunk, firstChunk->stationCount(), cwSurveyEditorRowIndex::StationRow)) == -1);
    CHECK(model.toModelRow(model.rowIndex(firstChunk, firstChunk->shotCount(), cwSurveyEditorRowIndex::ShotRow)) == -1);
    CHECK((chunk1TitleAfter - chunk0TitleAfter) == (firstChunk->stationCount() + firstChunk->shotCount() + 1));
}

TEST_CASE("cwSurveyEditorModel box data stays correct after appendNewShot", "[cwSurveyEditorModel]") {
    cwTrip trip;
    trip.addNewChunk();
    cwSurveyChunk* chunk = trip.chunk(0);

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    auto setStationData = [&](int index, const QString& name, const QString& left, const QString& right, const QString& up, const QString& down) {
        chunk->setData(cwSurveyChunk::StationNameRole, index, name);
        chunk->setData(cwSurveyChunk::StationLeftRole, index, left);
        chunk->setData(cwSurveyChunk::StationRightRole, index, right);
        chunk->setData(cwSurveyChunk::StationUpRole, index, up);
        chunk->setData(cwSurveyChunk::StationDownRole, index, down);
    };

    auto setShotData = [&](int index, const QString& distance, const QString& compass, const QString& backCompass, const QString& clino, const QString& backClino) {
        chunk->setData(cwSurveyChunk::ShotDistanceRole, index, distance);
        chunk->setData(cwSurveyChunk::ShotCompassRole, index, compass);
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, index, backCompass);
        chunk->setData(cwSurveyChunk::ShotClinoRole, index, clino);
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, index, backClino);
    };

    setStationData(0, "A0", "1.0", "2.0", "3.0", "4.0");
    setStationData(1, "A1", "1.1", "2.1", "3.1", "4.1");
    setShotData(0, "10.0", "100.0", "200.0", "30.0", "40.0");

    chunk->appendNewShot();
    setStationData(2, "A2", "1.2", "2.2", "3.2", "4.2");
    setShotData(1, "20.0", "110.0", "210.0", "31.0", "41.0");

    int previousRowCount = model.rowCount();
    chunk->appendNewShot();

    REQUIRE(model.rowCount() == previousRowCount + 2);

    auto findStationRow = [&](const QString& name) {
        for(int row = 0; row < model.rowCount(); ++row) {
            auto index = model.index(row);
            auto stationData = index.data(cwSurveyEditorModel::StationNameRole);
            if(stationData.isNull()) {
                continue;
            }
            const auto boxData = stationData.value<cwSurveyEditorBoxData>();
            if(boxData.reading().value() == name) {
                return index;
            }
        }
        return QModelIndex();
    };

    auto a0Row = findStationRow("A0");
    auto a1Row = findStationRow("A1");
    auto a2Row = findStationRow("A2");

    REQUIRE(a0Row.isValid());
    REQUIRE(a1Row.isValid());
    REQUIRE(a2Row.isValid());

    CHECK(a0Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 0);
    CHECK(a1Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 1);
    CHECK(a2Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 2);

    auto newShotRow = model.index(model.rowCount() - 2);
    auto newStationRow = model.index(model.rowCount() - 1);

    auto newShotRowIndex = newShotRow.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();
    auto newStationRowIndex = newStationRow.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();

    CHECK(newShotRowIndex.rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(newShotRowIndex.indexInChunk() == 2);
    CHECK(newShotRow.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());

    CHECK(newStationRowIndex.rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(newStationRowIndex.indexInChunk() == 3);
    CHECK(newStationRow.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
}

TEST_CASE("cwSurveyEditorModel updates box indices after insertStation", "[cwSurveyEditorModel]") {
    cwTrip trip;
    trip.addNewChunk();
    cwSurveyChunk* chunk = trip.chunk(0);

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    auto setStationData = [&](int index, const QString& name, const QString& left, const QString& right, const QString& up, const QString& down) {
        chunk->setData(cwSurveyChunk::StationNameRole, index, name);
        chunk->setData(cwSurveyChunk::StationLeftRole, index, left);
        chunk->setData(cwSurveyChunk::StationRightRole, index, right);
        chunk->setData(cwSurveyChunk::StationUpRole, index, up);
        chunk->setData(cwSurveyChunk::StationDownRole, index, down);
    };

    auto setShotData = [&](int index, const QString& distance, const QString& compass, const QString& backCompass, const QString& clino, const QString& backClino) {
        chunk->setData(cwSurveyChunk::ShotDistanceRole, index, distance);
        chunk->setData(cwSurveyChunk::ShotCompassRole, index, compass);
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, index, backCompass);
        chunk->setData(cwSurveyChunk::ShotClinoRole, index, clino);
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, index, backClino);
    };

    setStationData(0, "A0", "1.0", "2.0", "3.0", "4.0");
    setStationData(1, "A1", "1.1", "2.1", "3.1", "4.1");
    setShotData(0, "10.0", "100.0", "200.0", "30.0", "40.0");

    chunk->appendNewShot();
    setStationData(2, "A2", "1.2", "2.2", "3.2", "4.2");
    setShotData(1, "20.0", "110.0", "210.0", "31.0", "41.0");

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    dataChangedSpy.clear();

    int previousRowCount = model.rowCount();
    chunk->insertStation(1, cwSurveyChunk::Above);

    REQUIRE(model.rowCount() == previousRowCount + 2);

    auto findStationRow = [&](const QString& name) {
        for(int row = 0; row < model.rowCount(); ++row) {
            auto index = model.index(row);
            auto stationData = index.data(cwSurveyEditorModel::StationNameRole);
            if(stationData.isNull()) {
                continue;
            }
            const auto boxData = stationData.value<cwSurveyEditorBoxData>();
            if(boxData.reading().value() == name) {
                return index;
            }
        }
        return QModelIndex();
    };

    auto findShotRow = [&](const QString& distance) {
        for(int row = 0; row < model.rowCount(); ++row) {
            auto index = model.index(row);
            auto shotData = index.data(cwSurveyEditorModel::ShotDistanceRole);
            if(shotData.isNull()) {
                continue;
            }
            const auto boxData = shotData.value<cwSurveyEditorBoxData>();
            if(boxData.reading().value() == distance) {
                return index;
            }
        }
        return QModelIndex();
    };

    auto a1Row = findStationRow("A1");
    auto a2Row = findStationRow("A2");
    auto shot10Row = findShotRow("10.0");
    auto shot20Row = findShotRow("20.0");

    REQUIRE(a1Row.isValid());
    REQUIRE(a2Row.isValid());
    REQUIRE(shot10Row.isValid());
    REQUIRE(shot20Row.isValid());

    CHECK(a1Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 2);
    CHECK(a2Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 3);
    CHECK(shot10Row.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 0);
    CHECK(shot20Row.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 2);

    CHECK(dataChangedCovers(dataChangedSpy, a1Row.row(), cwSurveyEditorModel::StationNameRole));
    CHECK(dataChangedCovers(dataChangedSpy, a2Row.row(), cwSurveyEditorModel::StationNameRole));
    CHECK(dataChangedCovers(dataChangedSpy, shot20Row.row(), cwSurveyEditorModel::ShotDistanceRole));
}

TEST_CASE("cwSurveyEditorModel reports which stations the cave's fixes anchor",
          "[cwSurveyEditorModel][FixStation]")
{
    cwCave cave;
    cave.addTrip();
    auto trip = cave.trip(0);
    REQUIRE(trip != nullptr);

    trip->addNewChunk();
    auto chunk = trip->chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(trip);
    model.setFocusedChunk(chunk);

    auto stationRow = [&model, chunk](int indexInChunk) {
        const int row = model.toModelRow(model.rowIndex(chunk, indexInChunk, cwSurveyEditorRowIndex::StationRow));
        REQUIRE(row >= 0);
        return model.index(row, 0);
    };

    auto isFixed = [](const QModelIndex& index) {
        return index.data(cwSurveyEditorModel::StationFixedRole).toBool();
    };

    REQUIRE(model.setDataAt(model.cellIndex(stationRow(0).row(), cwSurveyEditorCellIndex::StationNameCell), "A1"));
    REQUIRE(model.setDataAt(model.cellIndex(stationRow(1).row(), cwSurveyEditorCellIndex::StationNameCell), "A2"));

    auto fixStations = cave.fixStations();
    REQUIRE(fixStations != nullptr);

    SECTION("no fixes means no fixed stations") {
        CHECK_FALSE(isFixed(stationRow(0)));
        CHECK_FALSE(isFixed(stationRow(1)));
    }

    SECTION("a fix marks its station, and only its station") {
        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        fixStations->addFixStation("A1");

        CHECK(isFixed(stationRow(0)));
        CHECK_FALSE(isFixed(stationRow(1)));

        //The rows are invalidated so a view can rebind, not just re-read. Every
        //row is covered, not only the one that changed: which stations moved
        //isn't knowable without re-reading them all.
        REQUIRE(dataChangedSpy.count() >= 1);
        CHECK(dataChangedSpy.last().at(2).value<QList<int>>()
              == QList<int>({cwSurveyEditorModel::StationFixedRole}));
        CHECK(dataChangedSpy.last().at(0).toModelIndex().row() == 0);
        CHECK(dataChangedSpy.last().at(1).toModelIndex().row() == model.rowCount() - 1);
    }

    SECTION("matching is case-insensitive") {
        fixStations->addFixStation("a1");
        CHECK(isFixed(stationRow(0)));
    }

    SECTION("matching ignores space around the stored fix name") {
        //addFixStation trims, so the untrimmed name has to be set directly for
        //the match itself to be the thing under test
        fixStations->addFixStation("A1");
        fixStations->setData(fixStations->index(0), "  a1  ", cwFixStationModel::StationNameRole);

        CHECK(isFixed(stationRow(0)));
    }

    SECTION("removing the fix clears its station") {
        fixStations->addFixStation("A1");
        REQUIRE(isFixed(stationRow(0)));

        fixStations->removeFixStation("A1");
        CHECK_FALSE(isFixed(stationRow(0)));
    }

    SECTION("renaming the fix moves it to the other station") {
        fixStations->addFixStation("A1");
        REQUIRE(isFixed(stationRow(0)));

        fixStations->setData(fixStations->index(0), "A2", cwFixStationModel::StationNameRole);

        CHECK_FALSE(isFixed(stationRow(0)));
        CHECK(isFixed(stationRow(1)));
    }

    SECTION("renaming the station moves the badge with it") {
        fixStations->addFixStation("A1");
        REQUIRE(isFixed(stationRow(0)));

        //Spied only from here: adding the fix above already invalidated every
        //row, so a spy opened before it would report the rename's own emission
        //as seen no matter what the rename emits
        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        const int renamedRow = stationRow(0).row();
        REQUIRE(model.setDataAt(model.cellIndex(renamedRow, cwSurveyEditorCellIndex::StationNameCell), "B1"));
        CHECK_FALSE(isFixed(stationRow(0)));

        //A rename is a chunk edit, so nothing else would tell a view the answer moved
        CHECK(dataChangedCovers(dataChangedSpy, renamedRow, cwSurveyEditorModel::StationFixedRole));
    }

    SECTION("renaming a station onto an existing fix turns the badge on") {
        fixStations->addFixStation("B1");
        REQUIRE_FALSE(isFixed(stationRow(0)));

        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        const int renamedRow = stationRow(0).row();
        REQUIRE(model.setDataAt(model.cellIndex(renamedRow, cwSurveyEditorCellIndex::StationNameCell), "B1"));

        CHECK(isFixed(stationRow(0)));
        CHECK(dataChangedCovers(dataChangedSpy, renamedRow, cwSurveyEditorModel::StationFixedRole));
    }

    SECTION("naming the trailing virtual row onto a fix turns its badge on") {
        fixStations->addFixStation("A3");

        //The virtual row commits through setDataAt's own dataChanged rather
        //than the chunk's, so it needs the roles list to be right there too
        const int virtualRow = model.toModelRow(model.rowIndex(chunk,
                                                               chunk->stationCount(),
                                                               cwSurveyEditorRowIndex::StationRow));
        REQUIRE(virtualRow >= 0);

        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        REQUIRE(model.setDataAt(model.cellIndex(virtualRow, cwSurveyEditorCellIndex::StationNameCell), "A3"));

        CHECK(isFixed(model.index(virtualRow, 0)));
        CHECK(dataChangedCovers(dataChangedSpy, virtualRow, cwSurveyEditorModel::StationFixedRole));
    }

    SECTION("the trailing virtual station row is never fixed") {
        fixStations->addFixStation("A1");

        const int virtualRow = model.toModelRow(model.rowIndex(chunk,
                                                               chunk->stationCount(),
                                                               cwSurveyEditorRowIndex::StationRow));
        REQUIRE(virtualRow >= 0);
        CHECK_FALSE(isFixed(model.index(virtualRow, 0)));
    }

    SECTION("a trip with no cave has no fixes to read") {
        cwTrip orphan;
        orphan.addNewChunk();

        cwSurveyEditorModel orphanModel;
        orphanModel.setTrip(&orphan);

        const int row = orphanModel.toModelRow(orphanModel.rowIndex(orphan.chunk(0), 0,
                                                                    cwSurveyEditorRowIndex::StationRow));
        REQUIRE(row >= 0);
        CHECK_FALSE(isFixed(orphanModel.index(row, 0)));
    }

    SECTION("moving the trip to another cave reads that cave's fixes") {
        fixStations->addFixStation("A1");
        REQUIRE(isFixed(stationRow(0)));

        cwCave otherCave;
        otherCave.fixStations()->addFixStation("A2");
        otherCave.addTrip(trip);

        //The fixes follow the trip's new parent, not the cave it was loaded in
        CHECK_FALSE(isFixed(stationRow(0)));
        CHECK(isFixed(stationRow(1)));
    }

    SECTION("switching to a trip in another cave reads that cave's fixes") {
        fixStations->addFixStation("A1");
        REQUIRE(isFixed(stationRow(0)));

        cwCave otherCave;
        otherCave.addTrip();
        auto otherTrip = otherCave.trip(0);
        REQUIRE(otherTrip != nullptr);
        otherTrip->addNewChunk();
        auto otherChunk = otherTrip->chunk(0);

        model.setTrip(otherTrip);
        model.setFocusedChunk(otherChunk);

        auto otherStationRow = [&model, otherChunk](int indexInChunk) {
            const int row = model.toModelRow(model.rowIndex(otherChunk, indexInChunk,
                                                            cwSurveyEditorRowIndex::StationRow));
            REQUIRE(row >= 0);
            return model.index(row, 0);
        };

        REQUIRE(model.setDataAt(model.cellIndex(otherStationRow(0).row(),
                                                cwSurveyEditorCellIndex::StationNameCell), "A1"));

        //Same station name, different cave: the first cave's fix must not follow
        CHECK_FALSE(isFixed(otherStationRow(0)));

        otherCave.fixStations()->addFixStation("A1");
        CHECK(isFixed(otherStationRow(0)));
    }
}

TEST_CASE("cwSurveyEditorModel reports which stations the cave marks as entrances",
          "[cwSurveyEditorModel][Entrance]")
{
    cwCave cave;
    cave.addTrip();
    auto trip = cave.trip(0);
    REQUIRE(trip != nullptr);

    trip->addNewChunk();
    auto chunk = trip->chunk(0);
    REQUIRE(chunk != nullptr);

    cwSurveyEditorModel model;
    model.setTrip(trip);
    model.setFocusedChunk(chunk);

    auto stationRow = [&model, chunk](int indexInChunk) {
        const int row = model.toModelRow(model.rowIndex(chunk, indexInChunk, cwSurveyEditorRowIndex::StationRow));
        REQUIRE(row >= 0);
        return model.index(row, 0);
    };

    auto isEntrance = [](const QModelIndex& index) {
        return index.data(cwSurveyEditorModel::StationEntranceRole).toBool();
    };

    auto isFixed = [](const QModelIndex& index) {
        return index.data(cwSurveyEditorModel::StationFixedRole).toBool();
    };

    REQUIRE(model.setDataAt(model.cellIndex(stationRow(0).row(), cwSurveyEditorCellIndex::StationNameCell), "A1"));
    REQUIRE(model.setDataAt(model.cellIndex(stationRow(1).row(), cwSurveyEditorCellIndex::StationNameCell), "A2"));

    auto entrances = cave.entrances();
    REQUIRE(entrances != nullptr);

    CHECK(model.roleNames().value(cwSurveyEditorModel::StationEntranceRole) == QByteArray("stationEntrance"));

    SECTION("an entrance marks its station in one ranged dataChanged") {
        CHECK_FALSE(isEntrance(stationRow(0)));
        CHECK_FALSE(isEntrance(stationRow(1)));

        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        entrances->addEntrance("A1");

        CHECK(isEntrance(stationRow(0)));
        CHECK_FALSE(isEntrance(stationRow(1)));

        //One emission over every row, never one per row
        REQUIRE(dataChangedSpy.count() == 1);
        CHECK(dataChangedSpy.at(0).at(2).value<QList<int>>()
              == QList<int>({cwSurveyEditorModel::StationEntranceRole}));
        CHECK(dataChangedSpy.at(0).at(0).toModelIndex().row() == 0);
        CHECK(dataChangedSpy.at(0).at(1).toModelIndex().row() == model.rowCount() - 1);
    }

    SECTION("matching is case-insensitive") {
        entrances->addEntrance("a1");
        CHECK(isEntrance(stationRow(0)));
    }

    SECTION("a label edit leaves the rows alone") {
        entrances->addEntrance("A1");
        REQUIRE(isEntrance(stationRow(0)));

        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        REQUIRE(entrances->setData(entrances->index(0), QStringLiteral("Main Entrance"),
                                   cwEntranceModel::LabelRole));

        CHECK(dataChangedSpy.count() == 0);
        CHECK(isEntrance(stationRow(0)));
    }

    SECTION("removing the entrance clears its station") {
        entrances->addEntrance("A1");
        REQUIRE(isEntrance(stationRow(0)));

        entrances->removeEntrance("A1");
        CHECK_FALSE(isEntrance(stationRow(0)));
    }

    SECTION("renaming the station moves the badge with it") {
        entrances->addEntrance("A1");
        REQUIRE(isEntrance(stationRow(0)));

        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        const int renamedRow = stationRow(0).row();
        REQUIRE(model.setDataAt(model.cellIndex(renamedRow, cwSurveyEditorCellIndex::StationNameCell), "B1"));
        CHECK_FALSE(isEntrance(stationRow(0)));

        CHECK(dataChangedCovers(dataChangedSpy, renamedRow, cwSurveyEditorModel::StationEntranceRole));
    }

    SECTION("renaming a station onto an existing entrance turns the badge on") {
        entrances->addEntrance("B1");
        REQUIRE_FALSE(isEntrance(stationRow(0)));

        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        const int renamedRow = stationRow(0).row();
        REQUIRE(model.setDataAt(model.cellIndex(renamedRow, cwSurveyEditorCellIndex::StationNameCell), "B1"));

        CHECK(isEntrance(stationRow(0)));
        CHECK(dataChangedCovers(dataChangedSpy, renamedRow, cwSurveyEditorModel::StationEntranceRole));
    }

    SECTION("naming the trailing virtual row onto an entrance turns its badge on") {
        entrances->addEntrance("A3");

        const int virtualRow = model.toModelRow(model.rowIndex(chunk,
                                                               chunk->stationCount(),
                                                               cwSurveyEditorRowIndex::StationRow));
        REQUIRE(virtualRow >= 0);
        REQUIRE_FALSE(isEntrance(model.index(virtualRow, 0)));

        cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

        REQUIRE(model.setDataAt(model.cellIndex(virtualRow, cwSurveyEditorCellIndex::StationNameCell), "A3"));

        CHECK(isEntrance(model.index(virtualRow, 0)));
        CHECK(dataChangedCovers(dataChangedSpy, virtualRow, cwSurveyEditorModel::StationEntranceRole));
    }

    SECTION("the trailing virtual station row is never an entrance") {
        entrances->addEntrance("A1");
        REQUIRE(isEntrance(stationRow(0)));

        const int virtualRow = model.toModelRow(model.rowIndex(chunk,
                                                               chunk->stationCount(),
                                                               cwSurveyEditorRowIndex::StationRow));
        REQUIRE(virtualRow >= 0);
        CHECK_FALSE(isEntrance(model.index(virtualRow, 0)));
    }

    SECTION("a trip with no cave has no entrances to read") {
        entrances->addEntrance("A1");
        REQUIRE(isEntrance(stationRow(0)));

        cwTrip orphan;
        orphan.addNewChunk();
        orphan.chunk(0)->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("A1"));

        model.setTrip(&orphan);

        const int row = model.toModelRow(model.rowIndex(orphan.chunk(0), 0,
                                                        cwSurveyEditorRowIndex::StationRow));
        REQUIRE(row >= 0);
        CHECK_FALSE(isEntrance(model.index(row, 0)));

        model.setTrip(nullptr);
    }

    SECTION("moving the trip to another cave reads that cave's entrances") {
        entrances->addEntrance("A1");
        REQUIRE(isEntrance(stationRow(0)));

        cwCave otherCave;
        otherCave.entrances()->addEntrance("A2");
        otherCave.addTrip(trip);

        CHECK_FALSE(isEntrance(stationRow(0)));
        CHECK(isEntrance(stationRow(1)));
    }

    SECTION("switching to a trip in another cave reads that cave's entrances") {
        entrances->addEntrance("A1");
        REQUIRE(isEntrance(stationRow(0)));

        cwCave otherCave;
        otherCave.addTrip();
        auto otherTrip = otherCave.trip(0);
        REQUIRE(otherTrip != nullptr);
        otherTrip->addNewChunk();
        auto otherChunk = otherTrip->chunk(0);

        model.setTrip(otherTrip);
        model.setFocusedChunk(otherChunk);

        auto otherStationRow = [&model, otherChunk](int indexInChunk) {
            const int row = model.toModelRow(model.rowIndex(otherChunk, indexInChunk,
                                                            cwSurveyEditorRowIndex::StationRow));
            REQUIRE(row >= 0);
            return model.index(row, 0);
        };

        REQUIRE(model.setDataAt(model.cellIndex(otherStationRow(0).row(),
                                                cwSurveyEditorCellIndex::StationNameCell), "A1"));

        //Same station name, different cave: the first cave's entrance must not follow
        CHECK_FALSE(isEntrance(otherStationRow(0)));

        otherCave.entrances()->addEntrance("A1");
        CHECK(isEntrance(otherStationRow(0)));
    }

    SECTION("the fix verbs move the fixed and entrance answers together") {
        cave.addFixedEntrance("A1");
        CHECK(isFixed(stationRow(0)));
        CHECK(isEntrance(stationRow(0)));

        cave.removeFixAndEntrance("A1");
        CHECK_FALSE(isFixed(stationRow(0)));
        CHECK_FALSE(isEntrance(stationRow(0)));
    }
}

TEST_CASE("cwSurveyEditorModel should update when survey data changes", "[cwSurveyEditorModel]") {
    cwTrip trip;
    cwSurveyEditorModel model;

    cwSignalSpy modelResetSpy(&model, &QAbstractItemModel::modelReset);
    cwSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);
    cwSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);
    cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    modelResetSpy.setObjectName("modelResetSpy");
    rowsInsertedSpy.setObjectName("rowsInsertSpy");
    rowsRemovedSpy.setObjectName("rowsRemovedSpy");
    dataChangedSpy.setObjectName("dataChangedSpy");

    SpyChecker spyChecker({
        { &modelResetSpy, 0 },
        { &rowsInsertedSpy, 0 },
        { &rowsRemovedSpy, 0 },
        { &dataChangedSpy, 0 }
    });

    // Set the trip into the model (this will trigger a complete reset).
    model.setTrip(&trip);

    spyChecker[&modelResetSpy]++;
    spyChecker.checkSpies();
    spyChecker.clearSpyCounts();

    // Add a new survey chunk to the trip.
    trip.addNewChunk();
    spyChecker[&rowsInsertedSpy]++;
    spyChecker.checkSpies();
    spyChecker.clearSpyCounts();

    // The new chunk creates 2 stations (plus a tile row at index 0), so rowCount should be 4.
    REQUIRE(model.rowCount() == 4);

    // Get the new chunk (assumed to be the last one).
    int chunkCount = trip.chunkCount();
    cwSurveyChunk* newChunk = trip.chunk(chunkCount - 1);
    REQUIRE(newChunk != nullptr);

    // Verify that the station data is initially empty.
    auto checkStationDataEmpty = [&](const QModelIndex &index) {
        CHECK(model.data(index, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    };

    auto checkShotDataEmpty = [&](const QModelIndex &index) {
        CHECK(model.data(index, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotCalibrationRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    };

    //Checks the surveyEditorModel agaist what it's watching
    auto checkModelData = [&]() {
        const auto tripChunks = trip.chunks();
        int row = 0;
        for(int c = 0; c < tripChunks.size(); c++) {
            const auto& chunk = tripChunks.at(c);
            INFO("Row:" << row);
            INFO("Chunk:" << c << " " << chunk);
            //Check skip rows
            const auto stations = chunk->stations();
            const auto shots = chunk->shots();

            //Check the title row is empty
            auto titleIndex = model.index(row);
            checkStationDataEmpty(titleIndex);
            checkShotDataEmpty(titleIndex);
            CHECK(titleIndex.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::TitleRow);
            // CHECK(titleIndex.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::TitleRow);

            row++;

            REQUIRE(stations.size() == shots.size() + 1);

            for(int i = 0; i < stations.size(); i++) {
                INFO("station i:" << i);
                auto station = chunk->station(i);

                auto index = model.index(row);
                INFO("modelIndex:" << index);
                // CHECK(index.data(cwSurveyEditorModel::TitleVisibleRole).toBool() == false);                
                auto rowIndex = index.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();
                CHECK(rowIndex.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(rowIndex.chunk() == chunk);
                CHECK(rowIndex.indexInChunk() == i);

                const auto stationNameData = model.data(index, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>();
                CHECK(stationNameData.reading().value().toStdString() == station.name().toStdString());
                CHECK(stationNameData.cellRole() == cwSurveyEditorCellIndex::StationNameCell);
                CHECK(stationNameData.rowIndex() == rowIndex);

                const auto stationLeftData = model.data(index, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>();
                CHECK(stationLeftData.reading().value().toStdString() == station.left().value().toStdString());
                CHECK(stationLeftData.cellRole() == cwSurveyEditorCellIndex::StationLeftCell);
                CHECK(stationLeftData.rowIndex() == rowIndex);

                const auto stationRightData = model.data(index, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>();
                CHECK(stationRightData.reading().value().toStdString() == station.right().value().toStdString());
                CHECK(stationRightData.cellRole() == cwSurveyEditorCellIndex::StationRightCell);
                CHECK(stationRightData.rowIndex() == rowIndex);

                const auto stationUpData = model.data(index, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>();
                CHECK(stationUpData.reading().value().toStdString() == station.up().value().toStdString());
                CHECK(stationUpData.cellRole() == cwSurveyEditorCellIndex::StationUpCell);
                CHECK(stationUpData.rowIndex() == rowIndex);

                const auto stationDownData = model.data(index, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>();
                CHECK(stationDownData.reading().value().toStdString() == station.down().value().toStdString());
                CHECK(stationDownData.cellRole() == cwSurveyEditorCellIndex::StationDownCell);
                CHECK(stationDownData.rowIndex() == rowIndex);

                row++;

                index = model.index(row);
                INFO("modelIndex:" << index);

                if(i < chunk->shotCount()) {
                    auto shot = chunk->shot(i);

                    auto rowIndex = index.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();
                    CHECK(rowIndex.rowType() == cwSurveyEditorRowIndex::ShotRow);
                    CHECK(rowIndex.chunk() == chunk);
                    CHECK(rowIndex.indexInChunk() == i);

                    const auto distance = model.data(index, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>();
                    const auto compass = model.data(index, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>();
                    const auto backCompass = model.data(index, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>();
                    const auto clino = model.data(index, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>();
                    const auto backClino = model.data(index, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>();

                    CHECK(distance.reading().value().toStdString() == shot.distance().value().toStdString());
                    CHECK(compass.reading().value().toStdString() == shot.compass().value().toStdString());
                    CHECK(backCompass.reading().value().toStdString() == shot.backCompass().value().toStdString());
                    CHECK(clino.reading().value().toStdString() == shot.clino().value().toStdString());
                    CHECK(backClino.reading().value().toStdString() == shot.backClino().value().toStdString());

                    CHECK(distance.cellRole() == cwSurveyEditorCellIndex::ShotDistanceCell);
                    CHECK(compass.cellRole() == cwSurveyEditorCellIndex::ShotCompassCell);
                    CHECK(backCompass.cellRole() == cwSurveyEditorCellIndex::ShotBackCompassCell);
                    CHECK(clino.cellRole() == cwSurveyEditorCellIndex::ShotClinoCell);
                    CHECK(backClino.cellRole() == cwSurveyEditorCellIndex::ShotBackClinoCell);

                    CHECK(distance.rowIndex() == rowIndex);
                    CHECK(compass.rowIndex() == rowIndex);
                    CHECK(backCompass.rowIndex() == rowIndex);
                    CHECK(clino.rowIndex() == rowIndex);
                    CHECK(backClino.rowIndex() == rowIndex);

                    // CHECK(index.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::ShotRow);
                    row++;
                }
            }
        }

        CHECK(row == model.rowCount());
    };

    QModelIndex idx1 = model.index(1);
    QModelIndex idx2 = model.index(2);
    QModelIndex idx3 = model.index(3);

    CHECK(idx1.isValid());
    CHECK(idx2.isValid());
    CHECK(idx3.isValid());

    checkStationDataEmpty(idx1);
    checkShotDataEmpty(idx2);
    checkStationDataEmpty(idx1);

    auto checkData = [&](const QModelIndex& index, cwSurveyEditorModel::Role role, int errorCountChanged = 0) {
        spyChecker[&dataChangedSpy] += errorCountChanged;
        spyChecker[&dataChangedSpy]++;

        spyChecker.requireSpies();
        REQUIRE(dataChangedSpy.last().size() == 3);
        // qDebug() << "dataChange0:" << dataChangedSpy.last().at(0).toModelIndex() << index;
        CHECK(dataChangedSpy.first().at(0).toModelIndex() == index);
        // qDebug() << "dataChange1:" << dataChangedSpy.last().at(1).toModelIndex() << index;
        CHECK(dataChangedSpy.first().at(1).toModelIndex() == index);
        //Renaming a station also moves whether the cave's fixes anchor it and
        //whether the cave marks it an entrance, so that edit carries
        //StationFixedRole and StationEntranceRole alongside the role that
        //changed. Checked as a set: Qt's roles argument carries no ordering
        //promise, and every consumer reads it with contains().
        const bool renamesStation = (role == cwSurveyEditorModel::StationNameRole);
        const auto actualRoles = dataChangedSpy.first().at(2).value<QList<int>>();
        CHECK(actualRoles.size() == (renamesStation ? 3 : 1));
        CHECK(actualRoles.contains(role));
        CHECK(actualRoles.contains(cwSurveyEditorModel::StationFixedRole) == renamesStation);
        CHECK(actualRoles.contains(cwSurveyEditorModel::StationEntranceRole) == renamesStation);

        spyChecker.clearSpyCounts();
    };

    // Now change the data in the new survey chunk.
    // Update station at index 0.
    // Now change the data in the new survey chunk.
    // Update station at index 0.
    newChunk->setData(cwSurveyChunk::StationNameRole, 0, "A1");
    checkData(idx1, cwSurveyEditorModel::StationNameRole, 4); //Added 4 LRUD warning

    newChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.0");
    checkData(idx1, cwSurveyEditorModel::StationLeftRole, 1); //Clears LRUD warning (error model deleted)

    newChunk->setData(cwSurveyChunk::StationRightRole, 0, "2.0");
    checkData(idx1, cwSurveyEditorModel::StationRightRole, 1); //Clears LRUD warning (error model deleted)

    newChunk->setData(cwSurveyChunk::StationUpRole, 0, "3.0");
    checkData(idx1, cwSurveyEditorModel::StationUpRole, 1); //Clears LRUD warning (error model deleted)

    newChunk->setData(cwSurveyChunk::StationDownRole, 0, "4.0");
    checkData(idx1, cwSurveyEditorModel::StationDownRole, 1); //Clears LRUD warning (error model deleted)

    // Update shot at index 0.
    newChunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.0");
    checkData(idx2, cwSurveyEditorModel::ShotDistanceRole, 1); //Added a fatel error to station

    newChunk->setData(cwSurveyChunk::ShotCompassRole, 0, "100.0");
    checkData(idx2, cwSurveyEditorModel::ShotCompassRole);

    newChunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "200.0");
    checkData(idx2, cwSurveyEditorModel::ShotBackCompassRole, 1); //Adds a warning because more than 2deg off compass

    newChunk->setData(cwSurveyChunk::ShotClinoRole, 0, "30.0");
    checkData(idx2, cwSurveyEditorModel::ShotClinoRole);

    newChunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "40.0"); //Adds a warning because more than 2deg off clino
    checkData(idx2, cwSurveyEditorModel::ShotBackClinoRole, 1);

    // Update station at index 1.
    newChunk->setData(cwSurveyChunk::StationNameRole, 1, "A2");
    checkData(idx3, cwSurveyEditorModel::StationNameRole, 5); //Adds 4 LRUD warnings, deletes 1 station name error on dependent

    newChunk->setData(cwSurveyChunk::StationLeftRole, 1, "1.1");
    checkData(idx3, cwSurveyEditorModel::StationLeftRole, 1); //Clears LRUD warning (error model deleted)

    newChunk->setData(cwSurveyChunk::StationRightRole, 1, "2.1");
    checkData(idx3, cwSurveyEditorModel::StationRightRole, 1); //Clears LRUD warning (error model deleted)

    newChunk->setData(cwSurveyChunk::StationUpRole, 1, "3.1");
    checkData(idx3, cwSurveyEditorModel::StationUpRole, 1); //Clears LRUD warning (error model deleted)

    newChunk->setData(cwSurveyChunk::StationDownRole, 1, "4.1");
    checkData(idx3, cwSurveyEditorModel::StationDownRole, 1); //Clears LRUD warning (error model deleted)

    // Since signals are delivered directly, we can now check that the model reflects the updated data.
    CHECK(model.data(idx1, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "A1");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.0");

    CHECK(model.data(idx2, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "10.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "100.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "200.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "30.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "40.0");

    CHECK(model.data(idx3, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "A2");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.1");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.1");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.1");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.1");

    // Verify that the expected signals were emitted.
    spyChecker.checkSpies();

    checkModelData();

    SECTION("Add new shot to the first chunk") {
        newChunk->appendNewShot();

        spyChecker[&rowsInsertedSpy]++;
        // appendNewShot() also triggers a model dataChanged range refresh.
        spyChecker[&dataChangedSpy]++;
        spyChecker.requireSpies();
        REQUIRE(rowsInsertedSpy.last().size() == 3);
        REQUIRE(rowsInsertedSpy.last().at(0) == QModelIndex());
        REQUIRE(rowsInsertedSpy.last().at(1).toInt() == 4);
        REQUIRE(rowsInsertedSpy.last().at(2).toInt() == 5);
        spyChecker.clearSpyCounts();

        CHECK(model.rowCount() == 6); //3 stations + 2 shots + 1 title
        auto idx3 = model.index(4); //New shot
        checkShotDataEmpty(idx3);
        auto idx4 = model.index(5); //New station
        checkStationDataEmpty(idx4);

        CHECK(idx3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(idx4.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
        // CHECK(idx3.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::ShotRow);
        // CHECK(idx4.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::StationRow);

        //--- Update the stations new shot and station data
        newChunk->setData(cwSurveyChunk::StationNameRole, 2, "A3"); //Adds warning 4 to the a3's LRUD and adds fatals 5 for all the survey data
        checkData(idx4, cwSurveyEditorModel::StationNameRole, 4 + 5);

        newChunk->setData(cwSurveyChunk::StationLeftRole, 2, "1.02");
        checkData(idx4, cwSurveyEditorModel::StationLeftRole, 1); //Clears LRUD warning (error model deleted)

        newChunk->setData(cwSurveyChunk::StationRightRole, 2, "2.02");
        checkData(idx4, cwSurveyEditorModel::StationRightRole, 1); //Clears LRUD warning (error model deleted)

        newChunk->setData(cwSurveyChunk::StationUpRole, 2, "3.02");
        checkData(idx4, cwSurveyEditorModel::StationUpRole, 1); //Clears LRUD warning (error model deleted)

        newChunk->setData(cwSurveyChunk::StationDownRole, 2, "4.02");
        checkData(idx4, cwSurveyEditorModel::StationDownRole, 1); //Clears LRUD warning (error model deleted)

        // Update shot at index 1.
        newChunk->setData(cwSurveyChunk::ShotDistanceRole, 1, "10.02");
        checkData(idx3, cwSurveyEditorModel::ShotDistanceRole, 1); //Clears distance fatal (error model deleted)

        newChunk->setData(cwSurveyChunk::ShotCompassRole, 1, "100.02"); //Fatal deleted, dependent error changes
        checkData(idx3, cwSurveyEditorModel::ShotCompassRole, 2);

        newChunk->setData(cwSurveyChunk::ShotBackCompassRole, 1, "200.02"); //Warning added 2 deg off, fatal deleted
        checkData(idx3, cwSurveyEditorModel::ShotBackCompassRole, 2);

        newChunk->setData(cwSurveyChunk::ShotClinoRole, 1, "30.02"); //Fatal deleted, dependent error changes
        checkData(idx3, cwSurveyEditorModel::ShotClinoRole, 2);

        newChunk->setData(cwSurveyChunk::ShotBackClinoRole, 1, "40.02"); //Warning added, 2 deg off, fatal deleted
        checkData(idx3, cwSurveyEditorModel::ShotBackClinoRole, 2);


        CHECK(model.data(idx3, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "10.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "100.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "200.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "30.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "40.02");

        CHECK(model.data(idx4, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "A3");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.02");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.02");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.02");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.02");

        checkModelData();

        SECTION("Add another chunk") {
            // Remember the current number of rows before adding a new chunk.
            int prevRowCount = model.rowCount();

            // Add another new survey chunk to the trip.
            trip.addNewChunk();
            spyChecker[&rowsInsertedSpy]++;
            spyChecker.requireSpies();

            // The new chunk creates 2 stations plus a tile row, so it adds 4 rows.
            REQUIRE(model.rowCount() == prevRowCount + 4);

            // Get the newly added chunk (should be the last one in the trip).
            int newChunkCount = trip.chunkCount();
            cwSurveyChunk* secondChunk = trip.chunk(newChunkCount - 1);
            REQUIRE(secondChunk != nullptr);

            // Compute the starting index for the new chunk's rows in the model.
            int startIndex = prevRowCount;
            // The new chunk layout:
            //   Row at startIndex: tile row (unused for data updates).
            //   Row at startIndex+1: station (and shot) data for index 0.
            //   Row at startIndex+2: station data for index 1.
            QModelIndex titleIdx = model.index(startIndex);
            QModelIndex stationIdx1 = model.index(startIndex + 1);
            QModelIndex shotIdx1 = model.index(startIndex + 2);
            QModelIndex stationIdx2 = model.index(startIndex + 3);

            // Verify that the station data for the new chunk is initially empty.
            checkStationDataEmpty(titleIdx);
            checkShotDataEmpty(titleIdx);
            checkStationDataEmpty(stationIdx1);
            checkShotDataEmpty(shotIdx1);
            checkStationDataEmpty(stationIdx2);
            // (No shot row exists for station index 1 by default.)

            CHECK(titleIdx.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::TitleRow);
            CHECK(stationIdx1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
            CHECK(shotIdx1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::ShotRow);
            CHECK(stationIdx2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);

            // CHECK(titleIdx.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::TitleRow);
            // CHECK(stationIdx1.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::StationRow);
            // CHECK(shotIdx1.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::ShotRow);
            // CHECK(stationIdx2.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::StationRow);

            //--- Update station 0 data in the new chunk.
            secondChunk->setData(cwSurveyChunk::StationNameRole, 0, "B1"); //4 warnings for LRUD's
            checkData(stationIdx1, cwSurveyEditorModel::StationNameRole, 4);

            secondChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationLeftRole, 1); //Clears LRUD warning (error model deleted)

            secondChunk->setData(cwSurveyChunk::StationRightRole, 0, "2.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationRightRole, 1); //Clears LRUD warning (error model deleted)

            secondChunk->setData(cwSurveyChunk::StationUpRole, 0, "3.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationUpRole, 1); //Clears LRUD warning (error model deleted)

            secondChunk->setData(cwSurveyChunk::StationDownRole, 0, "4.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationDownRole, 1); //Clears LRUD warning (error model deleted)

            // Update shot data for station 0 in the new chunk.
            secondChunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "15.0"); //Fatal for station name
            checkData(shotIdx1, cwSurveyEditorModel::ShotDistanceRole, 1);

            secondChunk->setData(cwSurveyChunk::ShotCompassRole, 0, "150.0");
            checkData(shotIdx1, cwSurveyEditorModel::ShotCompassRole);

            secondChunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "250.0"); //Two degrees off warning
            checkData(shotIdx1, cwSurveyEditorModel::ShotBackCompassRole, 1);

            secondChunk->setData(cwSurveyChunk::ShotClinoRole, 0, "35.0");
            checkData(shotIdx1, cwSurveyEditorModel::ShotClinoRole);

            secondChunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "45.0"); //Two degrees off warning
            checkData(shotIdx1, cwSurveyEditorModel::ShotBackClinoRole, 1);

            //--- Update station 1 data in the new chunk.
            secondChunk->setData(cwSurveyChunk::StationNameRole, 1, "B2"); //4 LRUD warnings, deletes 1 station name error on dependent
            checkData(stationIdx2, cwSurveyEditorModel::StationNameRole, 5);

            secondChunk->setData(cwSurveyChunk::StationLeftRole, 1, "1.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationLeftRole, 1); //Clears LRUD warning (error model deleted)

            secondChunk->setData(cwSurveyChunk::StationRightRole, 1, "2.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationRightRole, 1); //Clears LRUD warning (error model deleted)

            secondChunk->setData(cwSurveyChunk::StationUpRole, 1, "3.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationUpRole, 1); //Clears LRUD warning (error model deleted)

            secondChunk->setData(cwSurveyChunk::StationDownRole, 1, "4.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationDownRole, 1); //Clears LRUD warning (error model deleted)

            //--- Verify that the model reflects the updated data for the second chunk.
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "B1");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.5");

            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "15.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "150.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "250.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "35.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "45.0");

            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "B2");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.6");

            // Verify that the expected signals were emitted.
            spyChecker.checkSpies();

            checkModelData();

            SECTION("Remove chunk") {
                int prevRowCount = model.rowCount();

                // Add a new chunk; this new chunk creates (stationCount() + 1) rows.
                trip.addNewChunk();
                spyChecker[&rowsInsertedSpy]++;
                spyChecker.requireSpies();

                // For the newly added chunk, compute its row count.
                cwSurveyChunk* addedChunk = trip.chunk(trip.chunkCount() - 1);
                int newChunkRowCount = addedChunk->stationCount() + addedChunk->shotCount() + 1;
                REQUIRE(model.rowCount() == prevRowCount + newChunkRowCount);

                SECTION("Remove the first chunk") {
                    int currentRowCount = model.rowCount();
                    int currentChunkCount = trip.chunkCount();
                    cwSurveyChunk* firstChunk = trip.chunk(0);
                    REQUIRE(firstChunk != nullptr);

                    int chunkRowCount = firstChunk->stationCount() + firstChunk->shotCount() + 1;
                    int firstRow = 0; // first chunk always starts at row 0
                    int lastRow = firstRow + chunkRowCount - 1;

                    trip.removeChunk(firstChunk);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == firstRow);
                    CHECK(params.at(2).toInt() == lastRow);

                    REQUIRE(model.rowCount() == currentRowCount - chunkRowCount);
                    REQUIRE(trip.chunkCount() == currentChunkCount - 1);

                    checkModelData();
                }

                SECTION("Remove the middle chunk") {
                    int currentRowCount = model.rowCount();
                    int currentChunkCount = trip.chunkCount();

                    // Determine the middle chunk's index.
                    int midIndex = 1;
                    cwSurveyChunk* middleChunk = trip.chunk(midIndex);
                    REQUIRE(middleChunk != nullptr);

                    int chunkRowCount = middleChunk->stationCount() + middleChunk->shotCount() + 1;

                    // Calculate the starting row for the middle chunk.
                    int firstRow = 0;
                    for (int i = 0; i < midIndex; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        firstRow += chunk->stationCount() + chunk->shotCount() + 1;
                    }
                    int lastRow = firstRow + chunkRowCount - 1;

                    trip.removeChunk(middleChunk);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == firstRow);
                    CHECK(params.at(2).toInt() == lastRow);

                    REQUIRE(model.rowCount() == currentRowCount - chunkRowCount);
                    REQUIRE(trip.chunkCount() == currentChunkCount - 1);

                    checkModelData();
                }

                SECTION("Remove the last chunk") {
                    int currentRowCount = model.rowCount();
                    int currentChunkCount = trip.chunkCount();
                    int lastIndex = currentChunkCount - 1;
                    cwSurveyChunk* lastChunk = trip.chunk(lastIndex);
                    REQUIRE(lastChunk != nullptr);

                    int chunkRowCount = lastChunk->stationCount() + lastChunk->shotCount() + 1;

                    // Calculate the starting row for the last chunk.
                    int firstRow = 0;
                    for (int i = 0; i < lastIndex; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        firstRow += chunk->stationCount() + chunk->shotCount() + 1;
                    }
                    int lastRow = firstRow + chunkRowCount - 1;

                    trip.removeChunk(lastChunk);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == firstRow);
                    CHECK(params.at(2).toInt() == lastRow);

                    REQUIRE(model.rowCount() == currentRowCount - chunkRowCount);
                    REQUIRE(trip.chunkCount() == currentChunkCount - 1);

                    checkModelData();
                }

                SECTION("Remove all the chunks") {
                    int currentChunkCount = trip.chunkCount();
                    int totalRows = 0;
                    for (int i = 0; i < currentChunkCount; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        totalRows += chunk->stationCount() + chunk->shotCount() + 1;
                    }

                    trip.removeChunks(0, currentChunkCount - 1);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == 0);
                    CHECK(params.at(2).toInt() == totalRows - 1);

                    REQUIRE(trip.chunkCount() == 0);
                    REQUIRE(model.rowCount() == 0);

                    checkModelData();
                }
            }
        }

        SECTION("Remove a station for newChunk") {
            // newChunk should have 3 stations and 2 shots
            // newChunk is assumed to be the first chunk in the trip.
            REQUIRE(trip.chunkCount() >= 1);
            REQUIRE(newChunk == trip.chunk(0));
            REQUIRE(newChunk->stationCount() == 3);
            REQUIRE(newChunk->shotCount() == 2);

            auto findNewChunkBase = [&](const cwSurveyChunk* chunk) {
                for(int i = 0; i < model.rowCount(); i++) {
                    auto index = model.index(i);

                    if(chunk == index.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk()) {
                        return index.row();
                    }
                }
                return -1;
            };

            int baseRowIndex = findNewChunkBase(newChunk);

            SECTION("Remove the first station - down") {
                int prevModelRow = model.rowCount();
                int prevStationCount = newChunk->stationCount();
                int prevShotCount = newChunk->shotCount();

                newChunk->removeStation(0, cwSurveyChunk::Below);
                spyChecker[&dataChangedSpy] += 2; //Error model change + shifted indices
                spyChecker[&rowsRemovedSpy]++;
                spyChecker.requireSpies();

                int firstStationRow = baseRowIndex + 1; //+1 for title

                auto params = rowsRemovedSpy.last();
                REQUIRE(params.size() == 3);
                // Expect the parent index to be invalid (i.e. QModelIndex())
                CHECK(params.at(0).toModelIndex() == QModelIndex());
                // The removed row should be at the expected row.
                CHECK(params.at(1).toInt() == firstStationRow);
                CHECK(params.at(2).toInt() == firstStationRow + 1);

                // Verify the new counts.
                CHECK(newChunk->stationCount() == prevStationCount - 1);
                CHECK(newChunk->shotCount() == prevShotCount - 1);
                CHECK(model.rowCount() == prevModelRow - 2);

                checkModelData();
            }

            SECTION("Remove the second Station") {
                // Two scenarios: removal with 'Above' and with 'Below'.
                SECTION("Direction Above") {
                    int prevModelRow = model.rowCount();
                    int prevStationCount = newChunk->stationCount();
                    int prevShotCount = newChunk->shotCount();

                    newChunk->removeStation(1, cwSurveyChunk::Above);
                    spyChecker[&dataChangedSpy] += 2; //Error model change + shifted indices
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto removeSpyResults = rowsRemovedSpy.last();

                    int stationRow = baseRowIndex + 3; //+1 for title +2 for the second station

                    REQUIRE(removeSpyResults.size() == 3);
                    CHECK(removeSpyResults.at(0).toModelIndex() == QModelIndex());
                    CHECK(removeSpyResults.at(1).toInt() == stationRow - 1); //Deleting the shot above the station
                    CHECK(removeSpyResults.at(2).toInt() == stationRow);

                    CHECK(newChunk->stationCount() == prevStationCount - 1);
                    CHECK(newChunk->shotCount() == prevShotCount - 1);
                    CHECK(model.rowCount() == prevModelRow - 2);

                    checkModelData();
                }

                SECTION("Direction down") {
                    int prevModelRow = model.rowCount();
                    int prevStationCount = newChunk->stationCount();
                    int prevShotCount = newChunk->shotCount();

                    newChunk->removeStation(1, cwSurveyChunk::Below);
                    spyChecker[&dataChangedSpy]++; //Shifted indices
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto removeSpyResults = rowsRemovedSpy.last();

                    int stationRow = baseRowIndex + 3; //+1 for title +2 for the second station

                    REQUIRE(removeSpyResults.size() == 3);
                    CHECK(removeSpyResults.at(0).toModelIndex() == QModelIndex());
                    CHECK(removeSpyResults.at(1).toInt() == stationRow);
                    CHECK(removeSpyResults.at(2).toInt() == stationRow + 1); //Deleting the shot below the station

                    CHECK(newChunk->stationCount() == prevStationCount - 1);
                    CHECK(newChunk->shotCount() == prevShotCount - 1);
                    CHECK(model.rowCount() == prevModelRow - 2);

                    checkModelData();
                }
            }

            SECTION("Remove the last Station - up") {
                int prevModelRow = model.rowCount();
                int prevStationCount = newChunk->stationCount();
                int prevShotCount = newChunk->shotCount();

                int removedRow = baseRowIndex + 5; //+1 for title +4 for the third (and last) station

                newChunk->removeStation(newChunk->stationCount() - 1, cwSurveyChunk::Above);
                // spyChecker[&dataChangedSpy]++; //Error Model change
                spyChecker[&rowsRemovedSpy]++;
                spyChecker.requireSpies();

                auto params = rowsRemovedSpy.last();
                REQUIRE(params.size() == 3);
                CHECK(params.at(0).toModelIndex() == QModelIndex());
                CHECK(params.at(1).toInt() == removedRow - 1); //Deleting the shot above the station
                CHECK(params.at(2).toInt() == removedRow);

                CHECK(newChunk->stationCount() == prevStationCount - 1);
                CHECK(newChunk->shotCount() == prevShotCount - 1);
                CHECK(model.rowCount() == prevModelRow - 2);

                checkModelData();
            }
        }
    }
}

namespace {

/**
 * A trip shown in the editor, with row helpers shared by the split and merge
 * fixtures.
 */
struct EditorTripFixture {
    void show(cwSurveyChunk* focusChunk)
    {
        model.setTrip(&trip);
        model.setFocusedCell(cell(focusChunk, 0, cwSurveyEditorCellIndex::StationNameCell));
        rowTally.restart();
    }

    cwSurveyEditorCellIndex cell(cwSurveyChunk* inChunk, int indexInChunk, cwSurveyEditorCellIndex::CellRole role) const
    {
        return model.cellIndex(model.modelRowForCellRole(inChunk, indexInChunk, role), role);
    }

    int stationRow(cwSurveyChunk* inChunk, int stationIndex) const
    {
        return model.toModelRow(cwSurveyEditorRowIndex(inChunk, stationIndex, cwSurveyEditorRowIndex::StationRow));
    }

    QVariant rowData(int row, cwSurveyEditorModel::Role role) const
    {
        return model.index(row).data(role);
    }

    QString stationNameAt(cwSurveyChunk* inChunk, int stationIndex) const
    {
        return rowData(stationRow(inChunk, stationIndex), cwSurveyEditorModel::StationNameRole)
            .value<cwSurveyEditorBoxData>().reading().value();
    }

    bool hasVirtualRows(cwSurveyChunk* inChunk) const
    {
        return model.toModelRow(cwSurveyEditorRowIndex(inChunk, inChunk->stationCount(), cwSurveyEditorRowIndex::StationRow)) >= 0;
    }

    void checkRowCount(int expected)
    {
        rowTally.check(expected);
    }

    cwTrip trip;
    cwSurveyEditorModel model;
    SurveyEditorRowTally rowTally {&model};
};

/**
 * One chunk in a trip, shown in the editor with the keyboard on its first
 * station, so the chunk carries the virtual trailing rows when its last station
 * is named.
 */
struct EditorSplitFixture : EditorTripFixture {
    EditorSplitFixture(const QStringList& names, const QList<cwShot>& shots)
        : chunk(new cwSurveyChunk())
    {
        chunk->setData(cwSurveyChunkData{QUuid::createUuid(), namedStations(names), shots});
        trip.addChunk(chunk);
        show(chunk);
    }

    //Five named stations, A1 to A5, with readings on every shot
    static EditorSplitFixture fiveStations()
    {
        return EditorSplitFixture({"A1", "A2", "A3", "A4", "A5"},
                                  {readingShot(), readingShot(), readingShot(), readingShot()});
    }

    cwSurveyChunk* chunk;
};

}

TEST_CASE("cwSurveyEditorModel splits a chunk at a shot with readings",
          "[cwSurveyEditorModel][SplitChunk]")
{
    auto fixture = EditorSplitFixture::fiveStations();
    //title, five stations, four shots, and the two virtual rows
    REQUIRE(fixture.model.rowCount() == 12);

    const auto shotCell = fixture.cell(fixture.chunk, 2, cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(fixture.model.canSplitChunkAt(shotCell));

    fixture.model.splitChunkAt(shotCell);

    REQUIRE(fixture.trip.chunkCount() == 2);
    auto* newChunk = fixture.trip.chunk(1);
    CHECK(fixture.trip.chunk(0) == fixture.chunk);

    //One more title row and the A3 copy; the virtual rows move with the focus
    fixture.checkRowCount(14);

    CHECK(stationNames(fixture.chunk) == QStringList({"A1", "A2", "A3"}));
    CHECK(stationNames(newChunk) == QStringList({"A3", "A4", "A5"}));
    CHECK(fixture.stationNameAt(newChunk, 0) == QStringLiteral("A3"));
    CHECK(fixture.stationNameAt(newChunk, 1) == QStringLiteral("A4"));

    const auto focused = fixture.model.focusedRowIndex();
    CHECK(focused.chunk() == newChunk);
    CHECK(focused.indexInChunk() == 0);
    CHECK(focused.rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(fixture.model.focusedRole() == static_cast<int>(cwSurveyEditorCellIndex::StationNameCell));
    CHECK(fixture.model.focusedRow() == fixture.stationRow(newChunk, 0));

    CHECK(fixture.hasVirtualRows(newChunk));
    CHECK_FALSE(fixture.hasVirtualRows(fixture.chunk));
}

TEST_CASE("cwSurveyEditorModel splitting at an empty shot drops the shot's row",
          "[cwSurveyEditorModel][SplitChunk]")
{
    EditorSplitFixture fixture({"A1", "A2", "A3", "A4", "A5"},
                               {readingShot(), readingShot(), cwShot(), readingShot()});
    REQUIRE(fixture.model.rowCount() == 12);

    const auto shotCell = fixture.cell(fixture.chunk, 2, cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(fixture.model.canSplitChunkAt(shotCell));

    fixture.model.splitChunkAt(shotCell);

    REQUIRE(fixture.trip.chunkCount() == 2);
    auto* newChunk = fixture.trip.chunk(1);

    //A title row comes in and the empty shot's row goes out
    fixture.checkRowCount(12);

    CHECK(stationNames(fixture.chunk) == QStringList({"A1", "A2", "A3"}));
    CHECK(stationNames(newChunk) == QStringList({"A4", "A5"}));
    CHECK(fixture.model.focusedRowIndex().chunk() == newChunk);
    CHECK(fixture.model.focusedRowIndex().indexInChunk() == 0);
}

TEST_CASE("cwSurveyEditorModel splitting from a station cell splits at the shot below it",
          "[cwSurveyEditorModel][SplitChunk]")
{
    auto byShot = EditorSplitFixture::fiveStations();
    auto byStation = EditorSplitFixture::fiveStations();

    const auto stationRole = GENERATE(cwSurveyEditorCellIndex::StationNameCell,
                                      cwSurveyEditorCellIndex::StationLeftCell,
                                      cwSurveyEditorCellIndex::StationSplaysCell);
    INFO("station cell role " << stationRole);

    const auto stationCell = byStation.cell(byStation.chunk, 2, stationRole);
    REQUIRE(byStation.model.canSplitChunkAt(stationCell));
    byStation.model.splitChunkAt(stationCell);

    byShot.model.splitChunkAt(byShot.cell(byShot.chunk, 2, cwSurveyEditorCellIndex::ShotCompassCell));

    REQUIRE(byStation.trip.chunkCount() == 2);
    REQUIRE(byShot.trip.chunkCount() == 2);
    CHECK(stationNames(byStation.trip.chunk(0)) == stationNames(byShot.trip.chunk(0)));
    CHECK(stationNames(byStation.trip.chunk(1)) == stationNames(byShot.trip.chunk(1)));
    CHECK(stationNames(byStation.trip.chunk(1)) == QStringList({"A3", "A4", "A5"}));
    CHECK(byStation.model.rowCount() == byShot.model.rowCount());
    CHECK(byStation.model.focusedRow() == byShot.model.focusedRow());
}

TEST_CASE("cwSurveyEditorModel splitting closes the splay clusters that move to the new chunk",
          "[cwSurveyEditorModel][SplitChunk]")
{
    auto fixture = EditorSplitFixture::fiveStations();
    fixture.chunk->setStationSplays(1, {splayWithDistance(QStringLiteral("1")), splayWithDistance(QStringLiteral("2"))});
    fixture.chunk->setStationSplays(3, {splayWithDistance(QStringLiteral("3")), splayWithDistance(QStringLiteral("4"))});

    fixture.model.toggleSplaysExpanded(cwSurveyEditorRowIndex(fixture.chunk, 1, cwSurveyEditorRowIndex::StationRow));
    fixture.model.toggleSplaysExpanded(cwSurveyEditorRowIndex(fixture.chunk, 3, cwSurveyEditorRowIndex::StationRow));
    //Each cluster shows its two splays and the blank row under them
    fixture.checkRowCount(12 + 3 + 3);

    const cwSurveyEditorRowIndex a4Splay(fixture.chunk, 3, 0, cwSurveyEditorRowIndex::SplayRow);
    fixture.model.startSplayMove(a4Splay, true);
    REQUIRE(fixture.model.splayMoveActive());

    const auto shotCell = fixture.cell(fixture.chunk, 2, cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(fixture.model.canSplitChunkAt(shotCell));
    fixture.model.splitChunkAt(shotCell);

    REQUIRE(fixture.trip.chunkCount() == 2);
    auto* newChunk = fixture.trip.chunk(1);

    //The A4 cluster closes, a title row and the A3 copy come in
    fixture.checkRowCount(12 + 3 + 3 - 3 + 2);

    CHECK_FALSE(fixture.model.splayMoveActive());

    REQUIRE(newChunk->stationSplayCount(1) == 2);
    CHECK_FALSE(fixture.rowData(fixture.stationRow(newChunk, 1), cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    CHECK(fixture.model.toModelRow(cwSurveyEditorRowIndex(newChunk, 1, 0, cwSurveyEditorRowIndex::SplayRow)) == -1);

    //The A2 cluster stays where it was
    CHECK(fixture.rowData(fixture.stationRow(fixture.chunk, 1), cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    CHECK(fixture.model.toModelRow(cwSurveyEditorRowIndex(fixture.chunk, 1, 0, cwSurveyEditorRowIndex::SplayRow)) >= 0);
}

TEST_CASE("cwSurveyEditorModel splitting leaves a trailing empty station out of the new chunk",
          "[cwSurveyEditorModel][SplitChunk]")
{
    EditorSplitFixture fixture({"A1", "A2", "A3", "A4", ""},
                               {readingShot(), readingShot(), readingShot(), cwShot()});
    //The empty station at the end is a real row, so there are no virtual rows
    REQUIRE(fixture.model.rowCount() == 10);

    fixture.model.splitChunkAt(fixture.cell(fixture.chunk, 1, cwSurveyEditorCellIndex::ShotDistanceCell));

    REQUIRE(fixture.trip.chunkCount() == 2);
    auto* newChunk = fixture.trip.chunk(1);
    CHECK(stationNames(fixture.chunk) == QStringList({"A1", "A2"}));
    CHECK(stationNames(newChunk) == QStringList({"A2", "A3", "A4"}));
    CHECK(newChunk->shotCount() == 2);

    //Upper: title, two stations, a shot. New: title, three stations, two shots,
    //and the virtual rows the focus brings
    fixture.checkRowCount(4 + 6 + 2);
}

TEST_CASE("cwSurveyEditorModel splitting after the trim takes out an empty station above",
          "[cwSurveyEditorModel][SplitChunk]")
{
    //Station 3 has no name and an empty shot above it, so the trim before the
    //split removes it and the shot A3 to A5 moves up to index 2
    EditorSplitFixture fixture({"A1", "A2", "A3", "", "A5"},
                               {readingShot(), readingShot(), cwShot(), readingShot()});
    const QUuid lastShotId = fixture.chunk->shot(3).id();

    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.cell(fixture.chunk, 2, cwSurveyEditorCellIndex::ShotDistanceCell)));

    const auto shotCell = fixture.cell(fixture.chunk, 3, cwSurveyEditorCellIndex::ShotDistanceCell);
    REQUIRE(fixture.model.canSplitChunkAt(shotCell));
    fixture.model.splitChunkAt(shotCell);

    REQUIRE(fixture.trip.chunkCount() == 2);
    auto* newChunk = fixture.trip.chunk(1);
    CHECK(stationNames(fixture.chunk) == QStringList({"A1", "A2", "A3"}));
    CHECK(stationNames(newChunk) == QStringList({"A3", "A5"}));
    REQUIRE(newChunk->shotCount() == 1);
    CHECK(newChunk->shot(0).id() == lastShotId);
    //Upper: title, 3 stations, 2 shots. New: title, 2 stations, 1 shot, then the 2 virtual rows
    fixture.checkRowCount(6 + 4 + 2);
}

TEST_CASE("cwSurveyEditorModel canSplitChunkAt answers for real split points only",
          "[cwSurveyEditorModel][SplitChunk]")
{
    auto fixture = EditorSplitFixture::fiveStations();
    auto* chunk = fixture.chunk;
    REQUIRE(fixture.hasVirtualRows(chunk));

    using Cell = cwSurveyEditorCellIndex;

    CHECK(fixture.model.canSplitChunkAt(fixture.cell(chunk, 1, Cell::ShotDistanceCell)));
    CHECK(fixture.model.canSplitChunkAt(fixture.cell(chunk, 3, Cell::ShotClinoCell)));
    CHECK(fixture.model.canSplitChunkAt(fixture.cell(chunk, 1, Cell::StationNameCell)));
    CHECK(fixture.model.canSplitChunkAt(fixture.cell(chunk, 3, Cell::StationDownCell)));

    //The first shot and the first station would leave the upper chunk no shot
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.cell(chunk, 0, Cell::ShotDistanceCell)));
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.cell(chunk, 0, Cell::StationNameCell)));

    //The last real station has no shot below it
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.cell(chunk, 4, Cell::StationNameCell)));

    //The virtual trailing station and shot
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.cell(chunk, chunk->stationCount(), Cell::StationNameCell)));
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.cell(chunk, chunk->shotCount(), Cell::ShotDistanceCell)));

    //The title row, a role that doesn't match its row, and rows off the model
    const int titleRow = fixture.model.toModelRow(cwSurveyEditorRowIndex(chunk, -1, cwSurveyEditorRowIndex::TitleRow));
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.model.cellIndex(titleRow, Cell::StationNameCell)));
    const int shotRow = fixture.model.modelRowForCellRole(chunk, 2, Cell::ShotDistanceCell);
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.model.cellIndex(shotRow, Cell::StationNameCell)));
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.model.cellIndex(-1, Cell::ShotDistanceCell)));
    CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.model.cellIndex(fixture.model.rowCount(), Cell::ShotDistanceCell)));

    SECTION("A splay row is no split point") {
        chunk->setStationSplays(2, {splayWithDistance(QStringLiteral("1"))});
        fixture.model.toggleSplaysExpanded(cwSurveyEditorRowIndex(chunk, 2, cwSurveyEditorRowIndex::StationRow));
        const int splayRow = fixture.model.toModelRow(cwSurveyEditorRowIndex(chunk, 2, 0, cwSurveyEditorRowIndex::SplayRow));
        REQUIRE(splayRow >= 0);
        CHECK_FALSE(fixture.model.canSplitChunkAt(fixture.model.cellIndex(splayRow, Cell::SplayDistanceCell)));
        CHECK(fixture.model.canSplitChunkAt(fixture.cell(chunk, 2, Cell::StationSplaysCell)));
    }

    SECTION("An empty last shot would leave the new chunk no shot") {
        EditorSplitFixture emptyLast({"A1", "A2", "A3", "A4", "A5"},
                                     {readingShot(), readingShot(), readingShot(), cwShot()});
        CHECK_FALSE(emptyLast.model.canSplitChunkAt(emptyLast.cell(emptyLast.chunk, 3, Cell::ShotDistanceCell)));
        CHECK_FALSE(emptyLast.model.canSplitChunkAt(emptyLast.cell(emptyLast.chunk, 3, Cell::StationNameCell)));
        CHECK(emptyLast.model.canSplitChunkAt(emptyLast.cell(emptyLast.chunk, 2, Cell::ShotDistanceCell)));
    }

    SECTION("Without a trip nothing splits") {
        const auto shotCell = fixture.cell(chunk, 1, Cell::ShotDistanceCell);
        fixture.model.setTrip(nullptr);
        CHECK_FALSE(fixture.model.canSplitChunkAt(shotCell));
        fixture.model.splitChunkAt(shotCell);
        CHECK(fixture.trip.chunkCount() == 1);
    }
}

namespace {

/**
 * Two chunks in a trip, shown in the editor with the keyboard on the upper
 * chunk's first station.
 */
struct EditorMergeFixture : EditorTripFixture {
    EditorMergeFixture(const QStringList& upperNames, const QList<cwShot>& upperShots,
                       const QStringList& lowerNames, const QList<cwShot>& lowerShots)
        : upper(new cwSurveyChunk()),
          lower(new cwSurveyChunk())
    {
        upper->setData(cwSurveyChunkData{QUuid::createUuid(), namedStations(upperNames), upperShots});
        lower->setData(cwSurveyChunkData{QUuid::createUuid(), namedStations(lowerNames), lowerShots});
        trip.addChunk(upper);
        trip.addChunk(lower);
        show(upper);
    }

    //A1 to A3, then A3 to A5, with readings on every shot
    static EditorMergeFixture threeAndThree()
    {
        return EditorMergeFixture({"A1", "A2", "A3"}, {readingShot(), readingShot()},
                                  {"A3", "A4", "A5"}, {readingShot(), readingShot()});
    }

    int splayRow(cwSurveyChunk* inChunk, int stationIndex, int splayIndex) const
    {
        return model.toModelRow(cwSurveyEditorRowIndex(inChunk, stationIndex, splayIndex, cwSurveyEditorRowIndex::SplayRow));
    }

    bool splaysExpanded(cwSurveyChunk* inChunk, int stationIndex) const
    {
        return model.index(stationRow(inChunk, stationIndex)).data(cwSurveyEditorModel::StationSplaysExpandedRole).toBool();
    }

    cwSurveyChunk* upper;
    cwSurveyChunk* lower;
};

}

TEST_CASE("cwSurveyEditorModel merging a split pair restores the chunk",
          "[cwSurveyEditorModel][MergeChunk]")
{
    const bool fromBelow = GENERATE(true, false);
    INFO("merge with chunk above from the lower chunk: " << fromBelow);

    auto fixture = EditorSplitFixture::fiveStations();
    REQUIRE(fixture.model.rowCount() == 12);

    fixture.model.splitChunkAt(fixture.cell(fixture.chunk, 2, cwSurveyEditorCellIndex::ShotDistanceCell));
    REQUIRE(fixture.trip.chunkCount() == 2);
    fixture.checkRowCount(14);
    auto* newChunk = fixture.trip.chunk(1);
    //The split leaves the keyboard on the new chunk's A3
    REQUIRE(fixture.model.focusedRowIndex().chunk() == newChunk);

    if(fromBelow) {
        CHECK(fixture.model.canMergeWithAbove(newChunk));
        fixture.model.mergeWithAbove(newChunk);
    } else {
        CHECK(fixture.model.canMergeWithBelow(fixture.chunk));
        fixture.model.mergeWithBelow(fixture.chunk);
    }

    REQUIRE(fixture.trip.chunkCount() == 1);
    CHECK(fixture.trip.chunk(0) == fixture.chunk);
    CHECK(stationNames(fixture.chunk) == QStringList({"A1", "A2", "A3", "A4", "A5"}));
    CHECK(fixture.stationNameAt(fixture.chunk, 4) == QStringLiteral("A5"));

    //One title row and the A3 copy go; the virtual rows follow the focus up
    fixture.checkRowCount(12);
    CHECK(fixture.hasVirtualRows(fixture.chunk));

    //The keyboard stays on A3, now addressed in the surviving chunk
    const auto focused = fixture.model.focusedRowIndex();
    CHECK(focused.chunk() == fixture.chunk);
    CHECK(focused.indexInChunk() == 2);
    CHECK(focused.rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(fixture.model.focusedRole() == static_cast<int>(cwSurveyEditorCellIndex::StationNameCell));
    CHECK(fixture.model.focusedRow() == fixture.stationRow(fixture.chunk, 2));
}

TEST_CASE("cwSurveyEditorModel merging trims the upper chunk's trailing empty row first",
          "[cwSurveyEditorModel][MergeChunk]")
{
    EditorMergeFixture fixture({"A1", "A2", "A3", ""}, {readingShot(), readingShot(), cwShot()},
                               {"A3", "A4"}, {readingShot()});
    //Upper: title, four stations, three shots. Lower: title, two stations, a shot
    REQUIRE(fixture.model.rowCount() == 8 + 4);
    REQUIRE_FALSE(fixture.hasVirtualRows(fixture.upper));

    CHECK(fixture.model.canMergeWithBelow(fixture.upper));
    CHECK(fixture.model.canMergeWithAbove(fixture.lower));

    fixture.model.mergeWithBelow(fixture.upper);

    REQUIRE(fixture.trip.chunkCount() == 1);
    CHECK(stationNames(fixture.upper) == QStringList({"A1", "A2", "A3", "A4"}));
    CHECK(fixture.upper->shotCount() == 3);
    CHECK(fixture.upper->isValid());

    //Title, four stations, three shots, and the virtual rows of the focused chunk
    fixture.checkRowCount(8 + 2);
    CHECK(fixture.hasVirtualRows(fixture.upper));

    //Focus in the upper chunk stays where it was
    CHECK(fixture.model.focusedRowIndex().chunk() == fixture.upper);
    CHECK(fixture.model.focusedRow() == fixture.stationRow(fixture.upper, 0));
}

TEST_CASE("cwSurveyEditorModel merging keeps splay clusters open on the moved stations",
          "[cwSurveyEditorModel][MergeChunk]")
{
    auto fixture = EditorMergeFixture::threeAndThree();
    fixture.upper->setStationSplays(2, {splayWithDistance(QStringLiteral("1"))});
    fixture.lower->setStationSplays(0, {splayWithDistance(QStringLiteral("2")), splayWithDistance(QStringLiteral("3"))});
    fixture.lower->setStationSplays(1, {splayWithDistance(QStringLiteral("4")), splayWithDistance(QStringLiteral("5"))});

    //Upper: title, three stations, two shots and the virtual rows. Lower: title,
    //three stations, two shots
    REQUIRE(fixture.model.rowCount() == 8 + 6);
    fixture.rowTally.restart();

    //The joined chunk: title, five stations, four shots and the virtual rows
    constexpr int kMergedRows = 12;

    SECTION("A cluster on a lower station moves to its new index") {
        fixture.model.toggleSplaysExpanded(cwSurveyEditorRowIndex(fixture.lower, 1, cwSurveyEditorRowIndex::StationRow));
        //Two splays and the blank row under them
        fixture.checkRowCount(14 + 3);

        fixture.model.mergeWithBelow(fixture.upper);

        REQUIRE(fixture.trip.chunkCount() == 1);
        const int a4 = 3;
        REQUIRE(stationNames(fixture.upper).value(a4) == QStringLiteral("A4"));
        CHECK(fixture.splaysExpanded(fixture.upper, a4));
        CHECK(fixture.splayRow(fixture.upper, a4, 0) >= 0);
        CHECK(fixture.splayRow(fixture.upper, a4, 1) >= 0);
        CHECK_FALSE(fixture.splaysExpanded(fixture.upper, 2));
        fixture.checkRowCount(kMergedRows + 3);
    }

    SECTION("A cluster on the lower tie opens on the upper tie with every splay") {
        fixture.model.toggleSplaysExpanded(cwSurveyEditorRowIndex(fixture.lower, 0, cwSurveyEditorRowIndex::StationRow));
        fixture.checkRowCount(14 + 3);

        fixture.model.mergeWithAbove(fixture.lower);

        REQUIRE(fixture.trip.chunkCount() == 1);
        REQUIRE(fixture.upper->stationSplayCount(2) == 3);
        CHECK(fixture.splaysExpanded(fixture.upper, 2));
        CHECK(fixture.splayRow(fixture.upper, 2, 0) >= 0);
        CHECK(fixture.splayRow(fixture.upper, 2, 2) >= 0);
        CHECK_FALSE(fixture.splaysExpanded(fixture.upper, 3));
        //Three splays and the blank row under them
        fixture.checkRowCount(kMergedRows + 4);
    }
}

TEST_CASE("cwSurveyEditorModel merging moves focus from the lower chunk onto the upper one",
          "[cwSurveyEditorModel][MergeChunk]")
{
    auto fixture = EditorMergeFixture::threeAndThree();

    //Shot 1 of the lower chunk runs A4 to A5, which is shot 3 once joined
    fixture.model.setFocusedCell(fixture.cell(fixture.lower, 1, cwSurveyEditorCellIndex::ShotCompassCell));
    REQUIRE(fixture.model.focusedRowIndex().chunk() == fixture.lower);
    fixture.rowTally.restart();

    fixture.model.mergeWithBelow(fixture.upper);

    REQUIRE(fixture.trip.chunkCount() == 1);
    const auto focused = fixture.model.focusedRowIndex();
    CHECK(focused.chunk() == fixture.upper);
    CHECK(focused.indexInChunk() == 3);
    CHECK(focused.rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(fixture.model.focusedRole() == static_cast<int>(cwSurveyEditorCellIndex::ShotCompassCell));

    //Title, five stations, four shots, and the virtual rows of the focused chunk
    fixture.checkRowCount(12);
    CHECK(fixture.hasVirtualRows(fixture.upper));
}

TEST_CASE("cwSurveyEditorModel canMergeWith answers only for a neighbor with a matching tie",
          "[cwSurveyEditorModel][MergeChunk]")
{
    auto fixture = EditorMergeFixture::threeAndThree();

    //The pair does merge, so the refusals below are the rule and not the stub
    REQUIRE(fixture.model.canMergeWithBelow(fixture.upper));
    REQUIRE(fixture.model.canMergeWithAbove(fixture.lower));

    CHECK_FALSE(fixture.model.canMergeWithAbove(fixture.upper));
    CHECK_FALSE(fixture.model.canMergeWithBelow(fixture.lower));
    CHECK_FALSE(fixture.model.canMergeWithAbove(nullptr));
    CHECK_FALSE(fixture.model.canMergeWithBelow(nullptr));

    SECTION("A chunk from another trip has no neighbor here") {
        auto other = EditorMergeFixture::threeAndThree();
        CHECK_FALSE(fixture.model.canMergeWithBelow(other.upper));
        CHECK_FALSE(fixture.model.canMergeWithAbove(other.lower));
    }

    SECTION("A tie that differs refuses the merge") {
        auto data = fixture.lower->data();
        data.stations.replace(0, cwStation(QStringLiteral("B3")));
        fixture.lower->setData(data);
        fixture.rowTally.restart();

        CHECK_FALSE(fixture.model.canMergeWithBelow(fixture.upper));
        CHECK_FALSE(fixture.model.canMergeWithAbove(fixture.lower));

        fixture.model.mergeWithBelow(fixture.upper);
        fixture.model.mergeWithAbove(fixture.lower);
        CHECK(fixture.trip.chunkCount() == 2);
        CHECK(stationNames(fixture.upper) == QStringList({"A1", "A2", "A3"}));
        fixture.checkRowCount(fixture.model.rowCount());
    }

    SECTION("Without a trip nothing merges") {
        fixture.model.setTrip(nullptr);
        CHECK_FALSE(fixture.model.canMergeWithBelow(fixture.upper));
        fixture.model.mergeWithBelow(fixture.upper);
        CHECK(fixture.trip.chunkCount() == 2);
    }
}

TEST_CASE("cwSurveyEditorModel merging cancels a splay move from the lower chunk",
          "[cwSurveyEditorModel][MergeChunk]")
{
    auto fixture = EditorMergeFixture::threeAndThree();
    fixture.lower->setStationSplays(1, {splayWithDistance(QStringLiteral("1"))});
    fixture.model.toggleSplaysExpanded(cwSurveyEditorRowIndex(fixture.lower, 1, cwSurveyEditorRowIndex::StationRow));

    fixture.model.startSplayMove(cwSurveyEditorRowIndex(fixture.lower, 1, 0, cwSurveyEditorRowIndex::SplayRow), true);
    REQUIRE(fixture.model.splayMoveActive());

    fixture.model.mergeWithBelow(fixture.upper);

    CHECK(fixture.trip.chunkCount() == 1);
    CHECK_FALSE(fixture.model.splayMoveActive());
}

namespace {

/**
 * Three chunks in a trip, A1 to A3, then B1 to B2, then C1 to C3, shown in the
 * editor with the keyboard on A1. A carries the virtual rows, so its rows are
 * the title, three stations, two shots and two virtual rows.
 */
struct EditorMoveFixture : EditorTripFixture {
    EditorMoveFixture()
        : a(chunkWithNames({"A1", "A2", "A3"})),
          b(chunkWithNames({"B1", "B2"})),
          c(chunkWithNames({"C1", "C2", "C3"}))
    {
        trip.addChunk(a);
        trip.addChunk(b);
        trip.addChunk(c);
        show(a);
    }

    int titleRow(cwSurveyChunk* inChunk) const
    {
        return model.toModelRow(cwSurveyEditorRowIndex(inChunk, -1, cwSurveyEditorRowIndex::TitleRow));
    }

    int splayRow(cwSurveyChunk* inChunk, int stationIndex, int splayIndex) const
    {
        return model.toModelRow(cwSurveyEditorRowIndex(inChunk, stationIndex, splayIndex, cwSurveyEditorRowIndex::SplayRow));
    }

    cwSurveyChunk* chunkAt(int row) const
    {
        return rowData(row, cwSurveyEditorModel::ChunkRole).value<cwSurveyChunk*>();
    }

    QString stationNameAtRow(int row) const
    {
        return rowData(row, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value();
    }

    cwSurveyChunk* a;
    cwSurveyChunk* b;
    cwSurveyChunk* c;
};

constexpr int kFocusedARows = 8;
constexpr int kBRows = 4;
constexpr int kCRows = 6;
constexpr int kMoveFixtureRows = kFocusedARows + kBRows + kCRows;

//The one rowsMoved a move announces, read as start, end and destination rows
struct AnnouncedMove {
    int start = -1;
    int end = -1;
    int destination = -1;
    bool rootParents = false;

    bool operator==(const AnnouncedMove& other) const = default;
};

AnnouncedMove announcedMove(const QSignalSpy& spy)
{
    REQUIRE(spy.size() == 1);
    const auto call = spy.at(0);
    return AnnouncedMove {
        call.at(1).toInt(),
        call.at(2).toInt(),
        call.at(4).toInt(),
        !call.at(0).toModelIndex().isValid() && !call.at(3).toModelIndex().isValid()
    };
}

}

TEST_CASE("cwSurveyEditorModel moving a chunk announces its rows as one move",
          "[cwSurveyEditorModel][MoveChunk]")
{
    EditorMoveFixture fixture;
    REQUIRE(fixture.model.rowCount() == kMoveFixtureRows);
    REQUIRE(fixture.hasVirtualRows(fixture.a));

    QSignalSpy movedSpy(&fixture.model, &QAbstractItemModel::rowsMoved);
    QSignalSpy insertedSpy(&fixture.model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&fixture.model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(&fixture.model, &QAbstractItemModel::modelReset);

    SECTION("Down: A's title row through its virtual rows land below B") {
        fixture.model.moveChunkDown(fixture.a);

        REQUIRE(fixture.trip.chunks() == QList<cwSurveyChunk*>({fixture.b, fixture.a, fixture.c}));
        CHECK(announcedMove(movedSpy) == AnnouncedMove {0, kFocusedARows - 1, kFocusedARows + kBRows, true});

        //B's rows come first now, and A's read A
        CHECK(fixture.chunkAt(0) == fixture.b);
        CHECK(fixture.stationNameAtRow(1) == QStringLiteral("B1"));
        CHECK(fixture.chunkAt(kBRows) == fixture.a);
        CHECK(fixture.stationNameAtRow(kBRows + 1) == QStringLiteral("A1"));
        CHECK(fixture.titleRow(fixture.a) == kBRows);
        CHECK(fixture.hasVirtualRows(fixture.a));
    }

    SECTION("Up: B's rows land above A") {
        fixture.model.moveChunkUp(fixture.b);

        REQUIRE(fixture.trip.chunks() == QList<cwSurveyChunk*>({fixture.b, fixture.a, fixture.c}));
        CHECK(announcedMove(movedSpy) == AnnouncedMove {kFocusedARows, kFocusedARows + kBRows - 1, 0, true});

        CHECK(fixture.chunkAt(0) == fixture.b);
        CHECK(fixture.stationRow(fixture.b, 1) == 3);
        CHECK(fixture.stationNameAtRow(3) == QStringLiteral("B2"));
        CHECK(fixture.chunkAt(kBRows) == fixture.a);
        CHECK(fixture.titleRow(fixture.c) == kFocusedARows + kBRows);
    }

    CHECK(insertedSpy.isEmpty());
    CHECK(removedSpy.isEmpty());
    CHECK(resetSpy.isEmpty());
    fixture.checkRowCount(kMoveFixtureRows);
}

TEST_CASE("cwSurveyEditorModel an open splay cluster moves with its chunk",
          "[cwSurveyEditorModel][MoveChunk]")
{
    EditorMoveFixture fixture;
    fixture.b->setStationSplays(1, {splayWithDistance(QStringLiteral("1")), splayWithDistance(QStringLiteral("2"))});
    fixture.model.toggleSplaysExpanded(cwSurveyEditorRowIndex(fixture.b, 1, cwSurveyEditorRowIndex::StationRow));

    //Two splays and the blank row under them
    constexpr int kClusterRows = 3;
    fixture.checkRowCount(kMoveFixtureRows + kClusterRows);

    QSignalSpy movedSpy(&fixture.model, &QAbstractItemModel::rowsMoved);

    fixture.model.moveChunkUp(fixture.b);

    REQUIRE(fixture.trip.chunk(0) == fixture.b);
    CHECK(announcedMove(movedSpy) == AnnouncedMove {kFocusedARows, kFocusedARows + kBRows + kClusterRows - 1, 0, true});

    //Title, B1, the shot, B2, and then B2's splays
    const int b2Row = 3;
    CHECK(fixture.stationRow(fixture.b, 1) == b2Row);
    CHECK(fixture.splayRow(fixture.b, 1, 0) == b2Row + 1);
    CHECK(fixture.splayRow(fixture.b, 1, 1) == b2Row + 2);
    CHECK(fixture.chunkAt(b2Row + 1) == fixture.b);
    CHECK(fixture.model.index(b2Row).data(cwSurveyEditorModel::StationSplaysExpandedRole).toBool());
    CHECK(fixture.titleRow(fixture.a) == kBRows + kClusterRows);
    fixture.checkRowCount(kMoveFixtureRows + kClusterRows);
}

TEST_CASE("cwSurveyEditorModel focus follows a moved chunk",
          "[cwSurveyEditorModel][MoveChunk][focus]")
{
    EditorMoveFixture fixture;

    SECTION("Focus inside the moved chunk") {
        //B takes the virtual rows from A: A is six rows, B is six
        fixture.model.setFocusedCell(fixture.cell(fixture.b, 1, cwSurveyEditorCellIndex::StationNameCell));
        REQUIRE(fixture.hasVirtualRows(fixture.b));
        REQUIRE(fixture.model.focusedRow() == 6 + 3);
        fixture.rowTally.restart();

        QSignalSpy focusedRowSpy(&fixture.model, &cwSurveyEditorModel::focusedRowChanged);
        QSignalSpy focusedRoleSpy(&fixture.model, &cwSurveyEditorModel::focusedRoleChanged);

        fixture.model.moveChunkUp(fixture.b);

        REQUIRE(fixture.trip.chunk(0) == fixture.b);
        const auto focused = fixture.model.focusedRowIndex();
        CHECK(focused.chunk() == fixture.b);
        CHECK(focused.indexInChunk() == 1);
        CHECK(focused.rowType() == cwSurveyEditorRowIndex::StationRow);
        CHECK(fixture.model.focusedRole() == static_cast<int>(cwSurveyEditorCellIndex::StationNameCell));
        CHECK(fixture.model.focusedRow() == 3);
        CHECK(fixture.model.focusedRow() == fixture.stationRow(fixture.b, 1));
        CHECK(focusedRowSpy.count() == 1);
        CHECK(focusedRoleSpy.count() == 0);
        CHECK(fixture.hasVirtualRows(fixture.b));
        fixture.checkRowCount(kMoveFixtureRows);
    }

    SECTION("Focus in a chunk the move pushes down") {
        //A3 sits on row 5, under A's title, A1, a shot, A2 and a shot
        fixture.model.setFocusedCell(fixture.cell(fixture.a, 2, cwSurveyEditorCellIndex::StationNameCell));
        REQUIRE(fixture.model.focusedRow() == 5);

        QSignalSpy focusedRowSpy(&fixture.model, &cwSurveyEditorModel::focusedRowChanged);

        fixture.model.moveChunkUp(fixture.b);

        REQUIRE(fixture.trip.chunk(0) == fixture.b);
        const auto focused = fixture.model.focusedRowIndex();
        CHECK(focused.chunk() == fixture.a);
        CHECK(focused.indexInChunk() == 2);
        CHECK(fixture.model.focusedRow() == kBRows + 5);
        CHECK(focusedRowSpy.count() == 1);
        fixture.checkRowCount(kMoveFixtureRows);
    }
}

TEST_CASE("cwSurveyEditorModel canMoveChunk answers only for a chunk with a neighbor that way",
          "[cwSurveyEditorModel][MoveChunk]")
{
    EditorMoveFixture fixture;

    //The middle chunk moves both ways, so the refusals below are the rule and not the stub
    REQUIRE(fixture.model.canMoveChunkUp(fixture.b));
    REQUIRE(fixture.model.canMoveChunkDown(fixture.b));
    CHECK(fixture.model.canMoveChunkDown(fixture.a));
    CHECK(fixture.model.canMoveChunkUp(fixture.c));

    CHECK_FALSE(fixture.model.canMoveChunkUp(fixture.a));
    CHECK_FALSE(fixture.model.canMoveChunkDown(fixture.c));
    CHECK_FALSE(fixture.model.canMoveChunkUp(nullptr));
    CHECK_FALSE(fixture.model.canMoveChunkDown(nullptr));

    QSignalSpy movedSpy(&fixture.model, &QAbstractItemModel::rowsMoved);

    SECTION("The ends stay put") {
        fixture.model.moveChunkUp(fixture.a);
        fixture.model.moveChunkDown(fixture.c);
        fixture.model.moveChunkUp(nullptr);
        CHECK(fixture.trip.chunks() == QList<cwSurveyChunk*>({fixture.a, fixture.b, fixture.c}));
        CHECK(movedSpy.isEmpty());
    }

    SECTION("A chunk from another trip has no place here") {
        EditorMoveFixture other;
        CHECK_FALSE(fixture.model.canMoveChunkUp(other.b));
        CHECK_FALSE(fixture.model.canMoveChunkDown(other.b));
        fixture.model.moveChunkUp(other.b);
        CHECK(fixture.trip.chunks() == QList<cwSurveyChunk*>({fixture.a, fixture.b, fixture.c}));
        CHECK(other.trip.chunks() == QList<cwSurveyChunk*>({other.a, other.b, other.c}));
        CHECK(movedSpy.isEmpty());
    }

    SECTION("Without a trip nothing moves") {
        fixture.model.setTrip(nullptr);
        CHECK_FALSE(fixture.model.canMoveChunkUp(fixture.b));
        CHECK_FALSE(fixture.model.canMoveChunkDown(fixture.b));
        fixture.model.moveChunkDown(fixture.b);
        CHECK(fixture.trip.chunks() == QList<cwSurveyChunk*>({fixture.a, fixture.b, fixture.c}));
    }
}

TEST_CASE("cwSurveyEditorModel a moved chunk keeps its place through a save and reload",
          "[cwSurveyEditorModel][MoveChunk][cwSaveLoad]")
{
    constexpr int kLoadWaitMs = 10000;

    auto root = std::make_unique<cwRootData>();
    root->account()->setName(QStringLiteral("Test User"));
    root->account()->setEmail(QStringLiteral("test@example.com"));
    auto project = root->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);
    auto* a = chunkWithNames({"A1", "A2"});
    auto* b = chunkWithNames({"B1", "B2"});
    auto* c = chunkWithNames({"C1", "C2"});
    trip->addChunk(a);
    trip->addChunk(b);
    trip->addChunk(c);

    QTemporaryDir saveDir;
    REQUIRE(saveDir.isValid());
    REQUIRE(project->saveAs(saveDir.filePath(QStringLiteral("move-chunk-%1.cwproj")
                                                 .arg(QCoreApplication::applicationPid()))));
    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    cwSurveyEditorModel model;
    model.setTrip(trip);
    REQUIRE(model.canMoveChunkDown(a));
    model.moveChunkDown(a);
    REQUIRE(trip->chunks() == QList<cwSurveyChunk*>({b, a, c}));
    project->waitSaveToFinish();

    auto loadFuture = cwSaveLoad::loadAll(project->filename());
    REQUIRE(AsyncFuture::waitForFinished(loadFuture, kLoadWaitMs));
    REQUIRE_FALSE(loadFuture.result().hasError());

    const auto& caves = loadFuture.result().value().region.caves;
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first().trips.size() == 1);

    QList<QUuid> loadedOrder;
    for(const auto& chunkData : caves.first().trips.first().chunks) {
        loadedOrder.append(chunkData.id);
    }
    CHECK(loadedOrder == QList<QUuid>({b->id(), a->id(), c->id()}));
}
