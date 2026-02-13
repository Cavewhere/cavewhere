// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwTeamMember.h"
#include "cwTrip.h"
#include "cwTripMergeApplier.h"
#include "cwTripMergePlanBuilder.h"

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
                                                            {},
                                                            nullptr);
    REQUIRE(preparation.has_value());
    REQUIRE(preparation->plans.size() == 2);
    CHECK(preparation->plans[0].currentTrip == &secondTrip);
    CHECK(preparation->plans[0].loadedTripData == &secondLoaded);
    CHECK(preparation->plans[1].currentTrip == &firstTrip);
    CHECK(preparation->plans[1].loadedTripData == &firstLoaded);
}

TEST_CASE("cwTrip merge plan builder rejects ambiguous loaded trip ids", "[cwTripMerge][sync]")
{
    cwTrip firstTrip;
    cwTrip secondTrip;

    cwTripData firstLoaded = makeTripDataWithIdentity(firstTrip);
    cwTripData secondLoaded = makeTripDataWithIdentity(secondTrip);
    secondLoaded.id = firstLoaded.id;

    QString failureReason;
    const auto preparation = cwTripMergePlanBuilder::build({&firstTrip, &secondTrip},
                                                            {&firstLoaded, &secondLoaded},
                                                            {},
                                                            &failureReason);
    CHECK_FALSE(preparation.has_value());
    CHECK(failureReason == QStringLiteral("Ambiguous loaded trip ids."));
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

    QString failureReason;
    REQUIRE(cwTripMergeApplier::applyTripMergePlan(plan, &failureReason));
    CHECK(failureReason.isEmpty());
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

    QString failureReason;
    REQUIRE(cwTripMergeApplier::applyTripMergePlan(plan, &failureReason));
    CHECK(failureReason.isEmpty());

    const cwTripCalibrationData merged = currentTrip.calibrations()->data();
    CHECK(merged.tapeCalibration() == 3.0);
    CHECK(merged.declination() == 4.0);
    CHECK(merged.hasFrontSights() == false);
}

TEST_CASE("cwTrip merge applier returns false when trip subobjects differ", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;
    cwTripData loadedTripData = currentTrip.data();

    cwTeamMember remoteOnlyMember;
    remoteOnlyMember.setId(QUuid::createUuid());
    remoteOnlyMember.setName(QStringLiteral("remote-member"));
    remoteOnlyMember.setJobs({QStringLiteral("Sketch")});
    loadedTripData.team.members.append(remoteOnlyMember);

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = &loadedTripData;

    QString failureReason;
    CHECK_FALSE(cwTripMergeApplier::applyTripMergePlan(plan, &failureReason));
    CHECK(failureReason == QStringLiteral("Trip subobject merge is not implemented for incremental trip merge."));
}
