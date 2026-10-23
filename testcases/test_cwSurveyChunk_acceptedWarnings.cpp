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
struct AcceptFixture {
    AcceptFixture()
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

TEST_CASE("Survey warnings carry a stable id per kind and cell", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;

    for(const auto& [role, id] : kLrudIds) {
        INFO("LRUD role " << role);
        const auto errors = fixture.errorsAt(0, role);
        REQUIRE(errors.size() == 1);
        CHECK(errors.first().type() == cwError::Warning);
        CHECK(errors.first().errorTypeId() == toInt(id));
    }

    CHECK(fixture.hasWarningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement));
}

TEST_CASE("Ticking a warning's checkbox accepts it on the station", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;
    const int warningsBefore = fixture.trip->errorModel()->warningCount();

    SignalSpyChecker::SignalSpy acceptedSpy(fixture.chunk, &cwSurveyChunk::acceptedWarningsChanged);
    SignalSpyChecker::SignalSpy dataChangedSpy(fixture.chunk, &cwSurveyChunk::dataChanged);
    SignalSpyChecker::Constant checker {
        {&acceptedSpy, 0},
        {&dataChangedSpy, 0}
    };

    fixture.tickCheckbox(0, cwSurveyChunk::StationLeftRole, true);

    CHECK(fixture.chunk->station(0).isWarningAccepted(cwErrorTypeId::SurveyMissingLeft));
    CHECK(fixture.chunk->station(0).acceptedWarnings() == QSet<int>({toInt(cwErrorTypeId::SurveyMissingLeft)}));

    checker[&acceptedSpy] = 1;
    checker.checkSpies();

    CHECK(fixture.trip->errorModel()->warningCount() == warningsBefore - 1);
}

TEST_CASE("Renaming a station keeps its accepted warnings", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;
    fixture.tickCheckbox(0, cwSurveyChunk::StationLeftRole, true);

    fixture.chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("A9"));

    const cwError left = fixture.warningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft);
    CHECK(left.errorTypeId() == toInt(cwErrorTypeId::SurveyMissingLeft));
    CHECK(left.suppressed());
    CHECK(left.message().contains(QStringLiteral("A9")));
    CHECK(fixture.chunk->station(0).isWarningAccepted(cwErrorTypeId::SurveyMissingLeft));
}

TEST_CASE("Editing a cell clears its accepted warnings", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;
    const int left = toInt(cwErrorTypeId::SurveyMissingLeft);

    fixture.chunk->setWarningAccepted(0, cwSurveyChunk::StationLeftRole, left, true);
    CHECK(fixture.chunk->station(0).isWarningAccepted(cwErrorTypeId::SurveyMissingLeft));

    fixture.chunk->setData(cwSurveyChunk::StationLeftRole, 0, QStringLiteral("2.0"));
    CHECK(fixture.chunk->errorsAt(0, cwSurveyChunk::StationLeftRole) == nullptr);

    fixture.chunk->setData(cwSurveyChunk::StationLeftRole, 0, QString());
    const cwError missingLeft = fixture.warningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft);
    CHECK(missingLeft.errorTypeId() == left);
    CHECK_FALSE(missingLeft.suppressed());
    CHECK_FALSE(fixture.chunk->station(0).isWarningAccepted(cwErrorTypeId::SurveyMissingLeft));

    SECTION("Committing the same empty text keeps the acceptance") {
        fixture.chunk->setWarningAccepted(0, cwSurveyChunk::StationLeftRole, left, true);
        fixture.chunk->setData(cwSurveyChunk::StationLeftRole, 0, QString());

        CHECK(fixture.chunk->station(0).isWarningAccepted(cwErrorTypeId::SurveyMissingLeft));
        CHECK(fixture.warningWithId(0, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft).suppressed());
    }
}

TEST_CASE("Editing a backsight clears the pair's disagreement acceptance", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;
    const int disagreement = toInt(cwErrorTypeId::SurveyCompassDisagreement);

    fixture.chunk->setWarningAccepted(0, cwSurveyChunk::ShotCompassRole, disagreement, true);
    CHECK(fixture.warningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement).suppressed());

    //197 still disagrees with a frontsight of 10 (by 7 degrees), so the
    //warning stays and only its acceptance goes
    fixture.chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, QStringLiteral("197"));

    const cwError afterEdit = fixture.warningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement);
    CHECK(afterEdit.errorTypeId() == disagreement);
    CHECK_FALSE(afterEdit.suppressed());
    CHECK_FALSE(fixture.chunk->shot(0).isWarningAccepted(cwErrorTypeId::SurveyCompassDisagreement));
}

TEST_CASE("Committing a shot cell's same text keeps its acceptance", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;
    const int disagreement = toInt(cwErrorTypeId::SurveyCompassDisagreement);

    fixture.chunk->setWarningAccepted(0, cwSurveyChunk::ShotCompassRole, disagreement, true);

    fixture.chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, QStringLiteral("195"));
    fixture.chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("10"));

    CHECK(fixture.chunk->shot(0).isWarningAccepted(cwErrorTypeId::SurveyCompassDisagreement));
    CHECK(fixture.warningWithId(0, cwSurveyChunk::ShotCompassRole, cwErrorTypeId::SurveyCompassDisagreement).suppressed());

    SECTION("Clearing the backsight clears the acceptance") {
        fixture.chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, QString());
        CHECK_FALSE(fixture.chunk->shot(0).isWarningAccepted(cwErrorTypeId::SurveyCompassDisagreement));
    }
}

TEST_CASE("Accepted warnings move with the station row", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;
    fixture.chunk->setWarningAccepted(1, cwSurveyChunk::StationUpRole, toInt(cwErrorTypeId::SurveyMissingUp), true);
    CHECK(fixture.chunk->station(1).isWarningAccepted(cwErrorTypeId::SurveyMissingUp));

    fixture.chunk->insertStation(1, cwSurveyChunk::Above);

    REQUIRE(fixture.chunk->stationCount() == 3);
    CHECK(fixture.chunk->station(2).name() == QStringLiteral("A2"));
    CHECK(fixture.chunk->station(2).isWarningAccepted(cwErrorTypeId::SurveyMissingUp));
    CHECK(fixture.chunk->station(1).acceptedWarnings().isEmpty());
}

TEST_CASE("acceptWarnings and clearAcceptedWarnings sweep the chunk", "[cwSurveyChunk][acceptedWarnings]")
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

    CHECK(chunk->acceptWarnings() == 5);
    CHECK(errors->warningCount() == 0);
    CHECK(errors->suppressedWarningCount() == 5);
    CHECK(errors->fatalCount() == 1);

    CHECK(chunk->clearAcceptedWarnings() == 5);
    CHECK(errors->warningCount() == 5);
    CHECK(errors->suppressedWarningCount() == 0);
    CHECK(errors->fatalCount() == 1);

    cwTrip freshTrip;
    cwSurveyChunk* fresh = makeChunk(&freshTrip);
    CHECK(fresh->acceptWarnings({toInt(cwErrorTypeId::SurveyMissingLeft)}) == 1);
    CHECK(fresh->errorModel()->warningCount() == 4);
    CHECK(fresh->station(0).isWarningAccepted(cwErrorTypeId::SurveyMissingLeft));
    CHECK_FALSE(fresh->station(0).isWarningAccepted(cwErrorTypeId::SurveyMissingRight));
}

TEST_CASE("Turning the LRUD column off silences only missing LRUDs", "[cwSurveyChunk][acceptedWarnings]")
{
    AcceptFixture fixture;
    fixture.chunk->setWarningAccepted(1, cwSurveyChunk::StationLeftRole, toInt(cwErrorTypeId::SurveyMissingLeft), true);
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

    const cwError acceptedLeft = fixture.warningWithId(1, cwSurveyChunk::StationLeftRole, cwErrorTypeId::SurveyMissingLeft);
    CHECK(acceptedLeft.errorTypeId() == toInt(cwErrorTypeId::SurveyMissingLeft));
    CHECK(acceptedLeft.suppressed());
}
