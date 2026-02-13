// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwTeamMember.h"
#include "cwTrip.h"
#include "cwTripMergeApplier.h"
#include "cwTripMergePlanBuilder.h"
#include "cwSurveyChunkData.h"

namespace {

cwTripData makeTripDataWithIdentity(const cwTrip& trip)
{
    return trip.data();
}

} // namespace

TEST_CASE("cwTrip merge plan builder maps loaded trips by stable id", "[cwTripMerge][sync]")
{
    cwTrip firstTrip;
    cwTrip secondTrip;

    const cwTripData firstLoaded = makeTripDataWithIdentity(firstTrip);
    const cwTripData secondLoaded = makeTripDataWithIdentity(secondTrip);

    const auto preparation = cwTripMergePlanBuilder::build({&firstTrip, &secondTrip},
                                                            {&secondLoaded, &firstLoaded},
                                                            {});
    REQUIRE_FALSE(preparation.hasError());
    REQUIRE(preparation.value().plans.size() == 2);
    CHECK(preparation.value().plans[0].currentTrip == &secondTrip);
    CHECK(preparation.value().plans[0].loadedTripData == &secondLoaded);
    CHECK(preparation.value().plans[1].currentTrip == &firstTrip);
    CHECK(preparation.value().plans[1].loadedTripData == &firstLoaded);
}

TEST_CASE("cwTrip merge plan builder rejects ambiguous loaded trip ids", "[cwTripMerge][sync]")
{
    cwTrip firstTrip;
    cwTrip secondTrip;

    cwTripData firstLoaded = makeTripDataWithIdentity(firstTrip);
    cwTripData secondLoaded = makeTripDataWithIdentity(secondTrip);
    secondLoaded.id = firstLoaded.id;

    const auto preparation = cwTripMergePlanBuilder::build({&firstTrip, &secondTrip},
                                                            {&firstLoaded, &secondLoaded},
                                                            {});
    CHECK(preparation.hasError());
    CHECK(preparation.errorMessage() == QStringLiteral("Ambiguous loaded trip ids."));
}

TEST_CASE("cwTrip merge applier merges name and date bundles independently", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;
    cwTripData baseTripData = currentTrip.data();
    baseTripData.name = QStringLiteral("base-name");
    baseTripData.date = QDateTime(QDate(2020, 1, 1), QTime());
    currentTrip.setName(baseTripData.name);
    currentTrip.setDate(baseTripData.date);

    cwTripData loadedTripData = currentTrip.data();
    loadedTripData.name = baseTripData.name;
    loadedTripData.date = QDateTime(QDate(2024, 5, 10), QTime());

    currentTrip.setName(QStringLiteral("ours-name"));

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = &loadedTripData;
    plan.baseTripData = baseTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK(currentTrip.name() == QStringLiteral("ours-name"));
    CHECK(currentTrip.date() == QDateTime(QDate(2024, 5, 10), QTime()));
}

TEST_CASE("cwTrip merge applier merges calibration fields independently", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;

    cwTripData baseTripData = currentTrip.data();
    baseTripData.calibrations.setTapeCalibration(1.0);
    baseTripData.calibrations.setDeclination(2.0);
    baseTripData.calibrations.setFrontSights(true);
    currentTrip.calibrations()->setData(baseTripData.calibrations);

    cwTripData loadedTripData = currentTrip.data();
    loadedTripData.calibrations.setTapeCalibration(3.0); // Remote-only change.
    loadedTripData.calibrations.setDeclination(2.0); // Unchanged from base.
    loadedTripData.calibrations.setFrontSights(false); // Remote-only change.

    cwTripCalibrationData oursCalibration = currentTrip.calibrations()->data();
    oursCalibration.setDeclination(4.0); // Local-only change.
    currentTrip.calibrations()->setData(oursCalibration);

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = &loadedTripData;
    plan.baseTripData = baseTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());

    const cwTripCalibrationData merged = currentTrip.calibrations()->data();
    CHECK(merged.tapeCalibration() == 3.0);
    CHECK(merged.declination() == 4.0);
    CHECK(merged.hasFrontSights() == false);
}

TEST_CASE("cwTrip merge applier returns false when non-mergeable trip subobjects differ", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;
    cwTripData loadedTripData = currentTrip.data();

    cwSurveyChunkData remoteOnlyChunk;
    remoteOnlyChunk.id = QUuid::createUuid();
    loadedTripData.chunks.append(remoteOnlyChunk);

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = &loadedTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    CHECK(applyResult.hasError());
    CHECK(applyResult.errorMessage()
          == QStringLiteral("Trip subobject merge is not implemented for incremental trip merge."));
}
