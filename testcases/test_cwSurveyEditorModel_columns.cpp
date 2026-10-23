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

    constexpr int kShotIndex = 1;
    const int shotRow = fixture.model.modelRowForCellRole(fixture.chunk, kShotIndex,
                                                          cwSurveyEditorCellIndex::ShotClinoCell);
    const int nextStationRow = fixture.model.modelRowForCellRole(fixture.chunk, kShotIndex + 1,
                                                                 cwSurveyEditorCellIndex::StationNameCell);
    REQUIRE(shotRow >= 0);
    REQUIRE(nextStationRow >= 0);

    const auto next = fixture.model.nextCell(fixture.model.cellIndex(shotRow, cwSurveyEditorCellIndex::ShotClinoCell),
                                             cwSurveyEditorModel::Tab,
                                             true,
                                             false);
    CHECK(next.modelRow() == nextStationRow);
    CHECK(next.cellRole() == cwSurveyEditorCellIndex::StationNameCell);
}
