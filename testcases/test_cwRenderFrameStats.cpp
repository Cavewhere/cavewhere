// test_cwRenderFrameStats.cpp
// Catch2 unit tests for the process-wide per-frame render counts.

#include <catch2/catch_test_macros.hpp>

#include "cwRenderFrameStats.h"

namespace {

using Culling = cwRenderFrameStats::Culling;
using Streaming = cwRenderFrameStats::Streaming;

constexpr qint64 kReadyCpuBytes = 3 * 1024 * 1024;

constexpr Culling kCulling {
    .objectsTotal = 12,
    .objectsCulled = 5,
    .itemsTotal = 40,
    .itemsCulled = 31
};

constexpr Streaming kStreaming {
    .streamedItems = 7,
    .loadsInFlight = 2,
    .itemsBelowDesired = 4,
    .readyCpuBytes = kReadyCpuBytes,
    .demotionsInFlight = 1
};

} // namespace

TEST_CASE("cwRenderFrameStats: publish and read round-trip", "[RenderCullingStats]") {
    auto* stats = cwRenderFrameStats::instance();

    stats->publishCulling(kCulling);
    stats->publishStreaming(kStreaming);

    CHECK(stats->culling() == kCulling);
    CHECK(stats->streaming() == kStreaming);
}

TEST_CASE("cwRenderFrameStats: every publish bumps the one revision",
          "[RenderCullingStats]") {
    auto* stats = cwRenderFrameStats::instance();

    const quint64 startRevision = stats->revision();

    stats->publishCulling(kCulling);
    CHECK(stats->revision() == startRevision + 1);

    //The same values republished still count as a new frame
    stats->publishCulling(kCulling);
    CHECK(stats->revision() == startRevision + 2);

    //Both halves share the revision, so a streamed frame moves it too
    stats->publishStreaming(kStreaming);
    CHECK(stats->revision() == startRevision + 3);
}

TEST_CASE("cwRenderFrameStats: publish replaces the whole struct",
          "[RenderCullingStats]") {
    auto* stats = cwRenderFrameStats::instance();

    stats->publishCulling(kCulling);
    stats->publishCulling(Culling {});

    CHECK(stats->culling() == Culling {});

    stats->publishStreaming(kStreaming);
    stats->publishStreaming(Streaming {});

    CHECK(stats->streaming() == Streaming {});
}

TEST_CASE("cwRenderFrameStats: publishing one half keeps the other",
          "[RenderCullingStats]") {
    auto* stats = cwRenderFrameStats::instance();

    stats->publishCulling(kCulling);
    stats->publishStreaming(kStreaming);
    stats->publishCulling(Culling {});

    CHECK(stats->streaming() == kStreaming);
}
