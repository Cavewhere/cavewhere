// test_cwRenderCullingStats.cpp
// Catch2 unit tests for the process-wide culled/total draw counts.

#include <catch2/catch_test_macros.hpp>

#include "cwRenderCullingStats.h"

namespace {

using Counts = cwRenderCullingStats::Counts;

constexpr Counts kCounts {
    .objectsTotal = 12,
    .objectsCulled = 5,
    .itemsTotal = 40,
    .itemsCulled = 31
};

} // namespace

TEST_CASE("cwRenderCullingStats: publish and counts round-trip", "[RenderCullingStats]") {
    auto* stats = cwRenderCullingStats::instance();

    stats->publish(kCounts);

    const Counts read = stats->counts();
    CHECK(read.objectsTotal == kCounts.objectsTotal);
    CHECK(read.objectsCulled == kCounts.objectsCulled);
    CHECK(read.itemsTotal == kCounts.itemsTotal);
    CHECK(read.itemsCulled == kCounts.itemsCulled);
}

TEST_CASE("cwRenderCullingStats: every publish bumps the revision", "[RenderCullingStats]") {
    auto* stats = cwRenderCullingStats::instance();

    const quint64 startRevision = stats->revision();

    stats->publish(kCounts);
    CHECK(stats->revision() == startRevision + 1);

    //The same values republished still count as a new frame
    stats->publish(kCounts);
    CHECK(stats->revision() == startRevision + 2);
}

TEST_CASE("cwRenderCullingStats: publish replaces the whole struct", "[RenderCullingStats]") {
    auto* stats = cwRenderCullingStats::instance();

    stats->publish(kCounts);
    stats->publish(Counts {});

    const Counts read = stats->counts();
    CHECK(read.objectsTotal == 0);
    CHECK(read.objectsCulled == 0);
    CHECK(read.itemsTotal == 0);
    CHECK(read.itemsCulled == 0);
}
