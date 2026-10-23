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
#include "cwTripCalibration.h"

//Test includes
#include "SplayFixtureHelper.h"

namespace {

/**
 * A trip of one chunk with stations a1, a2, a3, where a2 carries two splays.
 */
struct ColumnsFixture {
    ColumnsFixture()
        : chunk(new cwSurveyChunk())
    {
        chunk->appendShot(cwStation(QStringLiteral("a1")), cwStation(QStringLiteral("a2")),
                          cwShot(QStringLiteral("10.52"), QStringLiteral("52.2"), QStringLiteral("232.2"),
                                 QStringLiteral("-31.5"), QStringLiteral("31.5")));
        chunk->appendShot(cwStation(QStringLiteral("a2")), cwStation(QStringLiteral("a3")),
                          cwShot(QStringLiteral("11.34"), QStringLiteral("62.2"), QStringLiteral("242.2"),
                                 QStringLiteral("-21.5"), QStringLiteral("21.5")));
        chunk->setStationSplays(kSplayStation, a4Splays().mid(0, kSplayCount));
        trip.addChunk(chunk);
        model.setTrip(&trip);
    }

    cwSurveyEditorRowIndex stationRow(int stationIndex) const
    {
        return cwSurveyEditorRowIndex(chunk, stationIndex, cwSurveyEditorRowIndex::StationRow);
    }

    static constexpr int kSplayStation = 1;
    static constexpr int kSplayCount = 2;

    cwTrip trip;
    cwSurveyChunk* chunk;
    cwSurveyEditorModel model;
};

}

TEST_CASE("Hiding the Splays column removes the splay rows", "[cwSurveyEditorModel]")
{
    ColumnsFixture fixture;
    const int collapsedRows = fixture.model.rowCount();

    fixture.model.toggleSplaysExpanded(fixture.stationRow(ColumnsFixture::kSplayStation));
    //The splays, plus the blank row the next one is typed into
    REQUIRE(fixture.model.rowCount() == collapsedRows + ColumnsFixture::kSplayCount + 1);

    fixture.trip.calibrations()->setSplays(false);
    CHECK(fixture.model.rowCount() == collapsedRows);
    CHECK(fixture.model.splayEntryCell(fixture.stationRow(ColumnsFixture::kSplayStation)).cellRole()
          == cwSurveyEditorCellIndex::InvalidCell);

    fixture.model.toggleSplaysExpanded(fixture.stationRow(ColumnsFixture::kSplayStation));
    CHECK(fixture.model.rowCount() == collapsedRows);

    fixture.trip.calibrations()->setSplays(true);
    CHECK(fixture.model.rowCount() == collapsedRows);
}

TEST_CASE("With the LRUD column hidden, Tab from the clino lands on the next station", "[cwSurveyEditorModel]")
{
    ColumnsFixture fixture;
    fixture.trip.calibrations()->setLruds(false);

    auto tabFromClino = [&fixture](int shotIndex) {
        const int shotRow = fixture.model.modelRowForCellRole(fixture.chunk, shotIndex,
                                                              cwSurveyEditorCellIndex::ShotClinoCell);
        REQUIRE(shotRow >= 0);
        return fixture.model.nextCell(fixture.model.cellIndex(shotRow, cwSurveyEditorCellIndex::ShotClinoCell),
                                      cwSurveyEditorModel::Tab,
                                      true,
                                      false);
    };

    //Shot i's clino leads into station i + 1's row, where the Splays cell is
    //the first box still shown
    constexpr int kLastShotIndex = 1;
    const int lastStationRow = fixture.model.modelRowForCellRole(fixture.chunk, kLastShotIndex + 1,
                                                                 cwSurveyEditorCellIndex::StationNameCell);
    REQUIRE(lastStationRow >= 0);

    SECTION("Splays shown")
    {
        const auto next = tabFromClino(kLastShotIndex);
        CHECK(next.modelRow() == lastStationRow);
        CHECK(next.cellRole() == cwSurveyEditorCellIndex::StationSplaysCell);
    }

    SECTION("Splays hidden too")
    {
        //With both columns gone the first shot's clino walks past the empty
        //rows of stations 0 and 1 onto station 2's name
        fixture.trip.calibrations()->setSplays(false);
        const auto next = tabFromClino(0);
        CHECK(next.modelRow() == lastStationRow);
        CHECK(next.cellRole() == cwSurveyEditorCellIndex::StationNameCell);
    }
}
