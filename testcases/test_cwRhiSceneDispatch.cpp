// CPU-only tests for the render-pass dispatch foundation in cwRhiScene.
// None of these tests touch a real QRhi device.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "cwRHIObject.h"
#include "cwRhiScene.h"
#include "cwSceneUpdate.h"

namespace {

// Fabricate sentinel render-pass descriptor pointers. They are never
// dereferenced — only their pointer identity matters for cache keying and
// eviction.
QRhiRenderPassDescriptor* sentinelDescriptor(std::uintptr_t value)
{
    return reinterpret_cast<QRhiRenderPassDescriptor*>(value);
}

cwRhiPipelineKey makeKey(QRhiRenderPassDescriptor* rpDesc, const QString& vsTag)
{
    cwRhiPipelineKey key;
    key.renderPass = rpDesc;
    key.sampleCount = 1;
    key.vertexShader = vsTag;
    key.fragmentShader = vsTag;  // unique-per-key, fragment tag mirrors vertex
    return key;
}

// PipelineRecord with null pipeline/layout — the eviction's `delete` is then
// a safe no-op so tests don't have to fabricate real QRhi pipelines.
cwRhiScene::PipelineRecord* makeStubRecord(QRhi*)
{
    auto* r = new cwRhiScene::PipelineRecord;
    r->pipeline = nullptr;
    r->layout = nullptr;
    return r;
}

} // namespace

TEST_CASE("RenderPass enum exposes Background and PointCloud in expected order",
          "[cwRhiSceneDispatch]")
{
    using RP = cwRHIObject::RenderPass;
    // Background must come first (so the radial gradient is drawn before
    // depth-tested geometry); PointCloud must sit before Opaque so an EDL
    // compositor can read PointCloud-pass output before line/grid output.
    REQUIRE(static_cast<int>(RP::Background) < static_cast<int>(RP::PointCloud));
    REQUIRE(static_cast<int>(RP::PointCloud) < static_cast<int>(RP::Opaque));
    REQUIRE(static_cast<int>(RP::Opaque) < static_cast<int>(RP::Transparent));
    REQUIRE(static_cast<int>(RP::Transparent) < static_cast<int>(RP::Overlay));
    REQUIRE(static_cast<int>(RP::Overlay) < static_cast<int>(RP::ShadowMap));
    REQUIRE(static_cast<int>(RP::ShadowMap) < static_cast<int>(RP::Count));
}

TEST_CASE("cwRhiScene defaults to empty PassConfig table (fallthrough to swap-chain)",
          "[cwRhiSceneDispatch]")
{
    cwRhiScene scene;
    REQUIRE(scene.passConfigs().empty());
    REQUIRE(scene.pipelineCache().isEmpty());
}

TEST_CASE("evictPipelinesFor removes cache entries keyed on the freed descriptor",
          "[cwRhiSceneDispatch]")
{
    cwRhiScene scene;

    auto* keptDesc = sentinelDescriptor(0xCA5E1001);
    auto* doomedDesc = sentinelDescriptor(0xCA5E2002);

    const cwRhiPipelineKey keptKey = makeKey(keptDesc, QStringLiteral("kept.vert"));
    const cwRhiPipelineKey doomedKeyA = makeKey(doomedDesc, QStringLiteral("doomedA.vert"));
    const cwRhiPipelineKey doomedKeyB = makeKey(doomedDesc, QStringLiteral("doomedB.vert"));

    REQUIRE(scene.acquirePipeline(keptKey, nullptr, makeStubRecord) != nullptr);
    REQUIRE(scene.acquirePipeline(doomedKeyA, nullptr, makeStubRecord) != nullptr);
    REQUIRE(scene.acquirePipeline(doomedKeyB, nullptr, makeStubRecord) != nullptr);
    REQUIRE(scene.pipelineCache().size() == 3);

    scene.evictPipelinesFor(doomedDesc);

    REQUIRE(scene.pipelineCache().size() == 1);
    REQUIRE(scene.pipelineCache().contains(keptKey));
    REQUIRE_FALSE(scene.pipelineCache().contains(doomedKeyA));
    REQUIRE_FALSE(scene.pipelineCache().contains(doomedKeyB));
}

TEST_CASE("evictPipelinesFor with null descriptor is a no-op",
          "[cwRhiSceneDispatch]")
{
    cwRhiScene scene;

    auto* keepDesc = sentinelDescriptor(0xCA5E3003);
    const cwRhiPipelineKey key = makeKey(keepDesc, QStringLiteral("safe.vert"));
    REQUIRE(scene.acquirePipeline(key, nullptr, makeStubRecord) != nullptr);

    scene.evictPipelinesFor(nullptr);

    REQUIRE(scene.pipelineCache().contains(key));
}

TEST_CASE("cwSceneUpdate::Flag::ViewportSize is a distinct bit",
          "[cwRhiSceneDispatch]")
{
    using F = cwSceneUpdate::Flag;
    // Pin the exact bit so any future hand-edit that renumbers flags trips here
    // before silently colliding with an existing flag.
    REQUIRE(static_cast<int>(F::ViewportSize) == 0x8);
    REQUIRE(F::ViewportSize != F::None);
    REQUIRE(F::ViewportSize != F::ViewMatrix);
    REQUIRE(F::ViewportSize != F::ProjectionMatrix);
    REQUIRE(F::ViewportSize != F::DevicePixelRatio);

    F combined = F::DevicePixelRatio | F::ViewportSize;
    REQUIRE(cwSceneUpdate::isFlagSet(combined, F::DevicePixelRatio));
    REQUIRE(cwSceneUpdate::isFlagSet(combined, F::ViewportSize));
    REQUIRE_FALSE(cwSceneUpdate::isFlagSet(combined, F::ViewMatrix));
}
