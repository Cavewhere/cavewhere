// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwCavingRegion.h"
#include "cwCavingRegionMergeApplier.h"
#include "cwCavingRegionMergePlanBuilder.h"

TEST_CASE("cwCavingRegion merge plan builder rejects null current region", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegionData loadedData;
    loadedData.name = QStringLiteral("loaded-name");

    const auto result = cwCavingRegionMergePlanBuilder::build(nullptr, &loadedData);
    CHECK(result.hasError());
}

TEST_CASE("cwCavingRegion merge plan builder rejects null loaded data", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegion region;
    region.setName(QStringLiteral("current-name"));

    const auto result = cwCavingRegionMergePlanBuilder::build(&region, nullptr);
    CHECK(result.hasError());
}

TEST_CASE("cwCavingRegion merge plan builder succeeds with valid inputs", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegion region;
    region.setName(QStringLiteral("current-name"));

    cwCavingRegionData loadedData;
    loadedData.name = QStringLiteral("loaded-name");

    const auto result = cwCavingRegionMergePlanBuilder::build(&region, &loadedData);
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().currentRegion == &region);
    CHECK(result.value().loadedRegionData == &loadedData);
    CHECK_FALSE(result.value().baseName.has_value());
}

TEST_CASE("cwCavingRegion merge plan builder preserves optional base name", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegion region;
    cwCavingRegionData loadedData;

    const auto result = cwCavingRegionMergePlanBuilder::build(
        &region, &loadedData, std::optional<QString>(QStringLiteral("base-name")));
    REQUIRE_FALSE(result.hasError());
    REQUIRE(result.value().baseName.has_value());
    CHECK(result.value().baseName.value() == QStringLiteral("base-name"));
}

TEST_CASE("cwCavingRegion merge applier applies remote name when local matches base", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegion region;
    region.setName(QStringLiteral("base-name"));

    cwCavingRegionData loadedData;
    loadedData.name = QStringLiteral("remote-name");

    cwCavingRegionMergePlan plan;
    plan.currentRegion = &region;
    plan.loadedRegionData = &loadedData;
    plan.baseName = QStringLiteral("base-name");

    REQUIRE_FALSE(cwCavingRegionMergeApplier::applyRegionMergePlan(plan).hasError());
    CHECK(region.name() == QStringLiteral("remote-name"));
}

TEST_CASE("cwCavingRegion merge applier keeps local name on conflict", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegion region;
    region.setName(QStringLiteral("ours-name"));

    cwCavingRegionData loadedData;
    loadedData.name = QStringLiteral("remote-name");

    cwCavingRegionMergePlan plan;
    plan.currentRegion = &region;
    plan.loadedRegionData = &loadedData;
    plan.baseName = QStringLiteral("base-name");

    REQUIRE_FALSE(cwCavingRegionMergeApplier::applyRegionMergePlan(plan).hasError());
    CHECK(region.name() == QStringLiteral("ours-name"));
}

TEST_CASE("cwCavingRegion merge applier uses remote name when no base", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegion region;
    region.setName(QStringLiteral("current-name"));

    cwCavingRegionData loadedData;
    loadedData.name = QStringLiteral("remote-name");

    cwCavingRegionMergePlan plan;
    plan.currentRegion = &region;
    plan.loadedRegionData = &loadedData;
    // no baseName

    REQUIRE_FALSE(cwCavingRegionMergeApplier::applyRegionMergePlan(plan).hasError());
    CHECK(region.name() == QStringLiteral("remote-name"));
}

TEST_CASE("cwCavingRegion merge applier rejects null current region", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegionData loadedData;

    cwCavingRegionMergePlan plan;
    plan.currentRegion = nullptr;
    plan.loadedRegionData = &loadedData;

    CHECK(cwCavingRegionMergeApplier::applyRegionMergePlan(plan).hasError());
}

TEST_CASE("cwCavingRegion merge applier rejects null loaded data", "[cwCavingRegionMerge][sync]")
{
    cwCavingRegion region;

    cwCavingRegionMergePlan plan;
    plan.currentRegion = &region;
    plan.loadedRegionData = nullptr;

    CHECK(cwCavingRegionMergeApplier::applyRegionMergePlan(plan).hasError());
}
