// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwCave.h"
#include "cwCaveMergeApplier.h"
#include "cwCaveMergePlanBuilder.h"
#include "cwTrip.h"

TEST_CASE("cwCave merge plan builder maps loaded caves by stable id", "[cwCaveMerge][sync]")
{
    cwCave firstCave;
    cwCave secondCave;

    const cwCaveData firstLoaded = firstCave.data();
    const cwCaveData secondLoaded = secondCave.data();

    const auto preparation = cwCaveMergePlanBuilder::build({&firstCave, &secondCave},
                                                            {&secondLoaded, &firstLoaded},
                                                            {});
    REQUIRE_FALSE(preparation.hasError());
    REQUIRE(preparation.value().plans.size() == 2);
    CHECK(preparation.value().plans[0].currentCave == &secondCave);
    CHECK(preparation.value().plans[0].loadedCaveData == &secondLoaded);
    CHECK(preparation.value().plans[1].currentCave == &firstCave);
    CHECK(preparation.value().plans[1].loadedCaveData == &firstLoaded);
}

TEST_CASE("cwCave merge plan builder rejects ambiguous loaded cave ids", "[cwCaveMerge][sync]")
{
    cwCave firstCave;
    cwCave secondCave;

    cwCaveData firstLoaded = firstCave.data();
    cwCaveData secondLoaded = secondCave.data();
    secondLoaded.id = firstLoaded.id;

    const auto preparation = cwCaveMergePlanBuilder::build({&firstCave, &secondCave},
                                                            {&firstLoaded, &secondLoaded},
                                                            {});
    CHECK(preparation.hasError());
    CHECK(preparation.errorMessage() == QStringLiteral("Ambiguous loaded cave ids."));
}

TEST_CASE("cwCave merge applier merges name without replacing trips", "[cwCaveMerge][sync]")
{
    cwCave currentCave;
    currentCave.setName(QStringLiteral("base-name"));

    auto* trip = new cwTrip();
    currentCave.addTrip(trip);
    const QUuid tripIdBeforeMerge = trip->id();

    cwCaveData loadedCaveData = currentCave.data();
    loadedCaveData.name = QStringLiteral("remote-name");

    cwCaveData baseCaveData = currentCave.data();
    baseCaveData.name = QStringLiteral("base-name");

    cwCaveMergePlan plan;
    plan.currentCave = &currentCave;
    plan.loadedCaveData = &loadedCaveData;
    plan.baseCaveData = baseCaveData;

    REQUIRE_FALSE(cwCaveMergeApplier::applyCaveMergePlan(plan).hasError());
    CHECK(currentCave.name() == QStringLiteral("remote-name"));
    REQUIRE(currentCave.tripCount() == 1);
    CHECK(currentCave.trip(0)->id() == tripIdBeforeMerge);
}

TEST_CASE("cwCave merge applier keeps local name on conflict", "[cwCaveMerge][sync]")
{
    cwCave currentCave;
    currentCave.setName(QStringLiteral("ours-name"));

    cwCaveData loadedCaveData = currentCave.data();
    loadedCaveData.name = QStringLiteral("remote-name");

    cwCaveData baseCaveData = currentCave.data();
    baseCaveData.name = QStringLiteral("base-name");

    cwCaveMergePlan plan;
    plan.currentCave = &currentCave;
    plan.loadedCaveData = &loadedCaveData;
    plan.baseCaveData = baseCaveData;

    REQUIRE_FALSE(cwCaveMergeApplier::applyCaveMergePlan(plan).hasError());
    CHECK(currentCave.name() == QStringLiteral("ours-name"));
}
