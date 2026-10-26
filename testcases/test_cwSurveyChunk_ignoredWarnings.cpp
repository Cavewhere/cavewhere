/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//Test includes
#include "SignalSpyChecker.h"
#include "EmptyShotChunkHelper.h"

//Std includes
#include <algorithm>
#include <array>
#include <memory>

namespace {

int toInt(cwErrorTypeId id)
{
    return static_cast<int>(id);
}

constexpr std::array<std::pair<cwSurveyChunk::DataRole, cwErrorTypeId>, 4> kLrudIds = {{
    {cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft},
    {cwSurveyChunk::StationRightRole, cwErrorTypeId::SurveyMissingRight},
    {cwSurveyChunk::StationUpRole, cwErrorTypeId::SurveyMissingUp},
    {cwSurveyChunk::StationDownRole, cwErrorTypeId::SurveyMissingDown},
}};

/**
 * A trip holding one chunk, A1 to A2, with every LRUD box empty. The shot's
 * compass reads 10 and its backsight 195, so the pair disagrees by 5 degrees
 * and the compass cell carries a disagreement warning (backsights are on by
 * default).
 */
struct IgnoreFixture {
    IgnoreFixture()
        : trip(std::make_unique<cwTrip>()),
          chunk(new cwSurveyChunk())
    {
        chunk->appendShot(cwStation(QStringLiteral("A1")),
                          cwStation(QStringLiteral("A2")),
                          cwShot(QStringLiteral("10"),
                                 QStringLiteral("10"),
                                 QStringLiteral("195"),
                                 QStringLiteral("0"),
                                 QStringLiteral("0")));
        trip->addChunk(chunk);
    }

    QList<cwError> errorsAt(int index, cwSurveyChunk::DataRole role) const
    {
        const cwErrorModel* model = chunk->errorsAt(index, role);
        return model == nullptr ? QList<cwError>() : model->errors()->toList();
    }

    //! The cell's warning with errorTypeId id, or a default cwError when none matches
    cwError warningWithId(int index, cwSurveyChunk::DataRole role, cwErrorTypeId id) const
    {
        const auto errors = errorsAt(index, role);
        for(const auto& error : errors) {
            if(error.type() == cwError::Warning && error.errorTypeId() == toInt(id)) {
                return error;
            }
        }
        return cwError();
    }

    bool hasWarningWithId(int index, cwSurveyChunk::DataRole role, cwErrorTypeId id) const
    {
        return warningWithId(index, role, id).errorTypeId() == toInt(id);
    }

    //! Ticks the "suppress" checkbox of the cell's first error, the way ErrorListQuoteBox does
    void tickCheckbox(int index, cwSurveyChunk::DataRole role, bool checked)
    {
        cwErrorModel* model = chunk->errorsAt(index, role);
        REQUIRE(model != nullptr);
        cwErrorListModel* list = model->errors();
        REQUIRE(list->size() >= 1);
        list->setData(list->index(0),
                      checked,
                      static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole));
    }

    std::unique_ptr<cwTrip> trip;
    cwSurveyChunk* chunk;
};

}

TEST_CASE("Survey warnings carry a stable id per kind and cell", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;

    for(const auto& [role, id] : kLrudIds) {
        INFO("LRUD role " << role);
        const auto errors = fixture.errorsAt(0, role);
        REQUIRE(errors.size() == 1);
        CHECK(errors.first().type() == cwError::Warning);
        CHECK(errors.first().errorTypeId() == toInt(id));
    }

    CHECK(fixture.hasWarningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement));
}

TEST_CASE("Ticking a warning's checkbox ignores it on the station", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;
    const int warningsBefore = fixture.trip->errorModel()->warningCount();

    SignalSpyChecker::SignalSpy ignoredSpy(fixture.chunk, &cwSurveyChunk::ignoredWarningsChanged);
    SignalSpyChecker::SignalSpy dataChangedSpy(fixture.chunk, &cwSurveyChunk::dataChanged);
    SignalSpyChecker::Constant checker {
        {&ignoredSpy, 0},
        {&dataChangedSpy, 0}
    };

    fixture.tickCheckbox(0, cwSurveyChunk::StationLeftRole, true);

    CHECK(fixture.chunk->station(0).isWarningIgnored(cwErrorTypeId::SurveyMissingLeft));
    CHECK(fixture.chunk->station(0).ignoredWarnings() == QSet<int>({toInt(cwErrorTypeId::SurveyMissingLeft)}));

    checker[&ignoredSpy] = 1;
    checker.checkSpies();

    CHECK(fixture.trip->errorModel()->warningCount() == warningsBefore - 1);
}

TEST_CASE("Renaming a station keeps its ignored warnings", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;
    fixture.tickCheckbox(0, cwSurveyChunk::StationLeftRole, true);

    fixture.chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("A9"));

    const cwError left = fixture.warningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft);
    CHECK(left.errorTypeId() == toInt(cwErrorTypeId::SurveyMissingLeft));
    CHECK(left.suppressed());
    CHECK(left.message().contains(QStringLiteral("A9")));
    CHECK(fixture.chunk->station(0).isWarningIgnored(cwErrorTypeId::SurveyMissingLeft));
}

TEST_CASE("Editing a cell clears its ignored warnings", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;
    const int left = toInt(cwErrorTypeId::SurveyMissingLeft);

    fixture.chunk->setWarningIgnored(0, cwSurveyChunk::StationLeftRole, left, true);
    CHECK(fixture.chunk->station(0).isWarningIgnored(cwErrorTypeId::SurveyMissingLeft));

    fixture.chunk->setData(cwSurveyChunk::StationLeftRole, 0, QStringLiteral("2.0"));
    CHECK(fixture.chunk->errorsAt(0, cwSurveyChunk::StationLeftRole) == nullptr);

    fixture.chunk->setData(cwSurveyChunk::StationLeftRole, 0, QString());
    const cwError missingLeft = fixture.warningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft);
    CHECK(missingLeft.errorTypeId() == left);
    CHECK_FALSE(missingLeft.suppressed());
    CHECK_FALSE(fixture.chunk->station(0).isWarningIgnored(cwErrorTypeId::SurveyMissingLeft));

    SECTION("Committing the same empty text keeps it ignored") {
        fixture.chunk->setWarningIgnored(0, cwSurveyChunk::StationLeftRole, left, true);
        fixture.chunk->setData(cwSurveyChunk::StationLeftRole, 0, QString());

        CHECK(fixture.chunk->station(0).isWarningIgnored(cwErrorTypeId::SurveyMissingLeft));
        CHECK(fixture.warningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft).suppressed());
    }
}

TEST_CASE("Editing a backsight clears the pair's ignored disagreement", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;
    const int disagreement = toInt(cwErrorTypeId::SurveyCompassDisagreement);

    fixture.chunk->setWarningIgnored(0, cwSurveyChunk::ShotCompassRole, disagreement, true);
    CHECK(fixture.warningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement).suppressed());

    //197 still disagrees with a frontsight of 10 (by 7 degrees), so the
    //warning stays and shows again
    fixture.chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, QStringLiteral("197"));

    const cwError afterEdit = fixture.warningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement);
    CHECK(afterEdit.errorTypeId() == disagreement);
    CHECK_FALSE(afterEdit.suppressed());
    CHECK_FALSE(fixture.chunk->shot(0).isWarningIgnored(cwErrorTypeId::SurveyCompassDisagreement));
}

TEST_CASE("Committing a shot cell's same text keeps its warning ignored", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;
    const int disagreement = toInt(cwErrorTypeId::SurveyCompassDisagreement);

    fixture.chunk->setWarningIgnored(0, cwSurveyChunk::ShotCompassRole, disagreement, true);

    fixture.chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, QStringLiteral("195"));
    fixture.chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("10"));

    CHECK(fixture.chunk->shot(0).isWarningIgnored(cwErrorTypeId::SurveyCompassDisagreement));
    CHECK(fixture.warningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement).suppressed());

    SECTION("Clearing the backsight shows the warning again") {
        fixture.chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, QString());
        CHECK_FALSE(fixture.chunk->shot(0).isWarningIgnored(cwErrorTypeId::SurveyCompassDisagreement));
    }
}

TEST_CASE("Ignored warnings move with the station row", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;
    fixture.chunk->setWarningIgnored(1, cwSurveyChunk::StationUpRole, toInt(cwErrorTypeId::SurveyMissingUp), true);
    CHECK(fixture.chunk->station(1).isWarningIgnored(cwErrorTypeId::SurveyMissingUp));

    fixture.chunk->insertStation(1, cwSurveyChunk::Above);

    REQUIRE(fixture.chunk->stationCount() == 3);
    CHECK(fixture.chunk->station(2).name() == QStringLiteral("A2"));
    CHECK(fixture.chunk->station(2).isWarningIgnored(cwErrorTypeId::SurveyMissingUp));
    CHECK(fixture.chunk->station(1).ignoredWarnings().isEmpty());
}

TEST_CASE("ignoreWarnings and clearIgnoredWarnings sweep the chunk", "[cwSurveyChunk][ignoredWarnings]")
{
    //A1's four LRUD boxes are empty (4 warnings), the compass pair disagrees
    //(1 warning), and the distance is missing (1 fatal). A2's LRUDs are filled.
    auto makeChunk = [](cwTrip* trip) {
        auto chunk = new cwSurveyChunk();
        cwStation a2(QStringLiteral("A2"));
        a2.setLeft(cwDistanceReading(QStringLiteral("1")));
        a2.setRight(cwDistanceReading(QStringLiteral("1")));
        a2.setUp(cwDistanceReading(QStringLiteral("1")));
        a2.setDown(cwDistanceReading(QStringLiteral("1")));
        chunk->appendShot(cwStation(QStringLiteral("A1")),
                          a2,
                          cwShot(QString(),
                                 QStringLiteral("10"),
                                 QStringLiteral("195"),
                                 QStringLiteral("0"),
                                 QStringLiteral("0")));
        trip->addChunk(chunk);
        return chunk;
    };

    cwTrip trip;
    cwSurveyChunk* chunk = makeChunk(&trip);
    cwErrorModel* errors = chunk->errorModel();
    REQUIRE(errors->warningCount() == 5);
    REQUIRE(errors->fatalCount() == 1);

    CHECK(chunk->ignoreWarnings() == 5);
    CHECK(errors->warningCount() == 0);
    CHECK(errors->ignoredWarningCount() == 5);
    CHECK(errors->fatalCount() == 1);

    CHECK(chunk->clearIgnoredWarnings() == 5);
    CHECK(errors->warningCount() == 5);
    CHECK(errors->ignoredWarningCount() == 0);
    CHECK(errors->fatalCount() == 1);

    cwTrip freshTrip;
    cwSurveyChunk* fresh = makeChunk(&freshTrip);
    CHECK(fresh->ignoreWarnings({toInt(cwErrorTypeId::SurveyMissingLeft)}) == 1);
    CHECK(fresh->errorModel()->warningCount() == 4);
    CHECK(fresh->station(0).isWarningIgnored(cwErrorTypeId::SurveyMissingLeft));
    CHECK_FALSE(fresh->station(0).isWarningIgnored(cwErrorTypeId::SurveyMissingRight));
}

TEST_CASE("Turning the LRUD column off silences only missing LRUDs", "[cwSurveyChunk][ignoredWarnings]")
{
    IgnoreFixture fixture;
    fixture.chunk->setWarningIgnored(1, cwSurveyChunk::StationLeftRole, toInt(cwErrorTypeId::SurveyMissingLeft), true);
    fixture.chunk->setData(cwSurveyChunk::StationRightRole, 0, QStringLiteral("abc"));

    fixture.trip->calibrations()->setLruds(false);

    for(int station = 0; station < 2; ++station) {
        for(const auto& [role, id] : kLrudIds) {
            INFO("station " << station << " role " << role);
            const auto errors = fixture.errorsAt(station, role);
            const bool hasWarning = std::any_of(errors.begin(), errors.end(), [](const cwError& error) {
                return error.type() == cwError::Warning;
            });
            CHECK_FALSE(hasWarning);
        }
    }

    const auto invalidRight = fixture.errorsAt(0, cwSurveyChunk::StationRightRole);
    REQUIRE(invalidRight.size() == 1);
    CHECK(invalidRight.first().type() == cwError::Fatal);

    fixture.trip->calibrations()->setLruds(true);

    CHECK(fixture.hasWarningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft));
    CHECK_FALSE(fixture.warningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft).suppressed());

    const cwError ignoredLeft = fixture.warningWithId(1, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft);
    CHECK(ignoredLeft.errorTypeId() == toInt(cwErrorTypeId::SurveyMissingLeft));
    CHECK(ignoredLeft.suppressed());
}

TEST_CASE("The empty-shot warning is ignorable", "[cwSurveyChunk][ignoredWarnings]")
{
    auto fixture = EmptyShotChunk::Fixture::issue721();
    cwSurveyChunk* chunk = fixture.chunk;
    const int id = EmptyShotChunk::emptyShotId();
    REQUIRE(chunk->warningCountForTypeId(id) == 1);
    CHECK(chunk->ignorableWarningCount() == 1);

    CHECK(chunk->ignoreWarnings({id}) == 1);
    CHECK(chunk->shot(1).isWarningIgnored(cwErrorTypeId::SurveyEmptyShot));
    CHECK(chunk->warningCountForTypeId(id) == 0);
    CHECK(chunk->errorModel()->ignoredWarningCount() == 1);

    const cwErrorModel* distance = chunk->errorsAt(1, cwSurveyChunk::ShotDistanceRole);
    REQUIRE(distance != nullptr);
    REQUIRE(distance->errors()->size() == 1);
    CHECK(distance->errors()->first().suppressed());

    CHECK(chunk->clearIgnoredWarnings({id}) == 1);
    CHECK_FALSE(chunk->shot(1).isWarningIgnored(cwErrorTypeId::SurveyEmptyShot));
    CHECK(chunk->warningCountForTypeId(id) == 1);
}

TEST_CASE("Typing a distance clears the ignored empty-shot warning", "[cwSurveyChunk][ignoredWarnings]")
{
    auto fixture = EmptyShotChunk::Fixture::issue721();
    cwSurveyChunk* chunk = fixture.chunk;
    const int id = EmptyShotChunk::emptyShotId();

    chunk->setWarningIgnored(1, cwSurveyChunk::ShotDistanceRole, id, true);
    CHECK(chunk->shot(1).isWarningIgnored(cwErrorTypeId::SurveyEmptyShot));

    SECTION("Committing the same empty text keeps it ignored") {
        chunk->setData(cwSurveyChunk::ShotDistanceRole, 1, QString());

        CHECK(chunk->shot(1).isWarningIgnored(cwErrorTypeId::SurveyEmptyShot));
        const cwErrorModel* distance = chunk->errorsAt(1, cwSurveyChunk::ShotDistanceRole);
        REQUIRE(distance != nullptr);
        REQUIRE(distance->errors()->size() == 1);
        CHECK(distance->errors()->first().errorTypeId() == id);
        CHECK(distance->errors()->first().suppressed());
    }

    SECTION("Typing then clearing a distance shows it again") {
        chunk->setData(cwSurveyChunk::ShotDistanceRole, 1, QStringLiteral("5"));
        CHECK_FALSE(chunk->shot(1).isWarningIgnored(cwErrorTypeId::SurveyEmptyShot));

        chunk->setData(cwSurveyChunk::ShotDistanceRole, 1, QString());
        CHECK(chunk->warningCountForTypeId(id) == 1);
        const cwErrorModel* distance = chunk->errorsAt(1, cwSurveyChunk::ShotDistanceRole);
        REQUIRE(distance != nullptr);
        REQUIRE(distance->errors()->size() == 1);
        CHECK_FALSE(distance->errors()->first().suppressed());
    }
}

TEST_CASE("Splitting at the empty shot leaves neither chunk with the warning", "[cwSurveyChunk][ignoredWarnings]")
{
    auto fixture = EmptyShotChunk::Fixture::issue721();
    const int id = EmptyShotChunk::emptyShotId();
    REQUIRE(fixture.trip->warningCountForTypeId(id) == 1);
    REQUIRE(fixture.trip->canSplitChunk(fixture.chunk, 1));

    cwSurveyChunk* lower = fixture.trip->splitChunk(fixture.chunk, 1);
    REQUIRE(lower != nullptr);
    REQUIRE(fixture.trip->chunkCount() == 2);

    CHECK(fixture.chunk->warningCountForTypeId(id) == 0);
    CHECK(lower->warningCountForTypeId(id) == 0);
    CHECK(fixture.trip->warningCountForTypeId(id) == 0);
    CHECK(fixture.trip->errorModel()->fatalCount() == 0);
    CHECK(fixture.trip->errorModel()->warningCount() == 0);
}
