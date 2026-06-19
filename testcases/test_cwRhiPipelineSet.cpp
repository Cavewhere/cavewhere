// CPU-only tests for cwRhiPipelineSet, the per-render-object pipeline cache that
// keeps one pipeline resident per render target so live↔offscreen toggling does
// not rebuild pipelines every frame. None of these touch a real QRhi device:
// stub PipelineRecords (null pipeline/layout) flow through the frame's shared,
// refcounted cache, and only pointer identity and cache membership are checked.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "cwRhiPipelineSet.h"
#include "cwRhiFrameRenderer.h"

namespace {

QRhiRenderPassDescriptor* sentinelDescriptor(std::uintptr_t value)
{
    return reinterpret_cast<QRhiRenderPassDescriptor*>(value);
}

cwRhiPipelineKey makeKey(QRhiRenderPassDescriptor* rpDesc, const QString& tag)
{
    cwRhiPipelineKey key;
    key.renderPass = rpDesc;
    key.sampleCount = 1;
    key.vertexShader = tag;
    key.fragmentShader = tag;
    return key;
}

// A miss callback that mints a stub record through the frame's shared cache,
// taking exactly one reference — mirrors what a real object's createFn does.
auto missFor(cwRhiFrameRenderer& frame, const cwRhiPipelineKey& key)
{
    return [&frame, key]() -> cwRhiPipelineRecord* {
        return frame.acquirePipeline(key, nullptr, [](QRhi*) {
            auto* r = new cwRhiPipelineRecord;
            r->pipeline = nullptr;
            r->layout = nullptr;
            return r;
        });
    };
}

} // namespace

TEST_CASE("cwRhiPipelineSet caches one record per key and reuses it",
          "[cwRhiPipelineSet]")
{
    cwRhiFrameRenderer frame;
    cwRhiPipelineSet set;

    auto* liveDesc = sentinelDescriptor(0xA11AE001);
    auto* offscreenDesc = sentinelDescriptor(0x0FF5C002);
    const cwRhiPipelineKey liveKey = makeKey(liveDesc, QStringLiteral("live"));
    const cwRhiPipelineKey offscreenKey = makeKey(offscreenDesc, QStringLiteral("offscreen"));

    auto* liveRecord = set.acquire(&frame, liveKey, missFor(frame, liveKey));
    REQUIRE(liveRecord != nullptr);
    REQUIRE(frame.pipelineCache().size() == 1);

    // Re-acquiring the same key returns the identical record without minting a
    // new one (no PSO churn) and without inflating the shared cache.
    REQUIRE(set.acquire(&frame, liveKey, missFor(frame, liveKey)) == liveRecord);
    REQUIRE(frame.pipelineCache().size() == 1);

    // A second target's key adds a second resident record.
    auto* offscreenRecord = set.acquire(&frame, offscreenKey, missFor(frame, offscreenKey));
    REQUIRE(offscreenRecord != nullptr);
    REQUIRE(offscreenRecord != liveRecord);
    REQUIRE(frame.pipelineCache().size() == 2);

    // Toggling back and forth — the live↔offscreen pattern within a frame —
    // never rebuilds: both records stay resident and identical.
    for (int i = 0; i < 5; ++i) {
        REQUIRE(set.acquire(&frame, liveKey, missFor(frame, liveKey)) == liveRecord);
        REQUIRE(set.acquire(&frame, offscreenKey, missFor(frame, offscreenKey)) == offscreenRecord);
    }
    REQUIRE(frame.pipelineCache().size() == 2);
    REQUIRE(set.size() == 2);
}

TEST_CASE("cwRhiPipelineSet::purgeFor drops only the targeted descriptor",
          "[cwRhiPipelineSet]")
{
    cwRhiFrameRenderer frame;
    cwRhiPipelineSet set;

    auto* keptDesc = sentinelDescriptor(0xCEED0001);
    auto* doomedDesc = sentinelDescriptor(0xD00D0002);
    const cwRhiPipelineKey keptKey = makeKey(keptDesc, QStringLiteral("kept"));
    const cwRhiPipelineKey doomedKey = makeKey(doomedDesc, QStringLiteral("doomed"));

    set.acquire(&frame, keptKey, missFor(frame, keptKey));
    set.acquire(&frame, doomedKey, missFor(frame, doomedKey));
    REQUIRE(frame.pipelineCache().size() == 2);

    set.purgeFor(doomedDesc);

    // The doomed record is released (the set held the only reference, so the
    // shared cache frees it); the kept record survives.
    REQUIRE(set.size() == 1);
    REQUIRE(frame.pipelineCache().size() == 1);
    REQUIRE(frame.pipelineCache().contains(keptKey));
    REQUIRE_FALSE(frame.pipelineCache().contains(doomedKey));
}

TEST_CASE("cwRhiPipelineSet::releaseAll frees every held record",
          "[cwRhiPipelineSet]")
{
    cwRhiFrameRenderer frame;

    {
        cwRhiPipelineSet set;
        auto* descA = sentinelDescriptor(0xAAAA0001);
        auto* descB = sentinelDescriptor(0xBBBB0002);
        const cwRhiPipelineKey keyA = makeKey(descA, QStringLiteral("a"));
        const cwRhiPipelineKey keyB = makeKey(descB, QStringLiteral("b"));

        set.acquire(&frame, keyA, missFor(frame, keyA));
        set.acquire(&frame, keyB, missFor(frame, keyB));
        REQUIRE(frame.pipelineCache().size() == 2);

        set.releaseAll();
        REQUIRE(set.isEmpty());
        REQUIRE(frame.pipelineCache().isEmpty());
    }

    // The set's destructor running releaseAll a second time is harmless.
    REQUIRE(frame.pipelineCache().isEmpty());
}
