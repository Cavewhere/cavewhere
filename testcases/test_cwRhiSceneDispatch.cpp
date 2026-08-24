// CPU-only tests for the render-pass dispatch foundation in cwRhiFrameRenderer.
// None of these tests touch a real QRhi device.

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "cwRHIObject.h"
#include "cwRhiFrameRenderer.h"
#include "cwRhiScene.h"
#include "cwRhiTexturedItems.h"
#include "cwRenderTexturedItems.h"
#include "cwScene.h"
#include "cwSceneUpdate.h"
#include "cwRenderMemoryLedger.h"
#include "cwStreamedTexture.h"
#include "cwTextureResidency.h"

#include "CwRhiSceneTestAccess.h"
#include "CwRhiTexturedItemsTestAccess.h"

#include <QElapsedTimer>
#include <QSize>
#include <QThread>
#include <QVector2D>
#include <QVector3D>

#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

// cwRhiPipelineRecord with null pipeline/layout — the eviction's `delete` is then
// a safe no-op so tests don't have to fabricate real QRhi pipelines.
cwRhiPipelineRecord* makeStubRecord(QRhi*)
{
    auto* r = new cwRhiPipelineRecord;
    r->pipeline = nullptr;
    r->layout = nullptr;
    return r;
}

// A quad two meters on a side carrying the full 0..1 UV range: its uv area is 1
// over a world area of 4, so the density is sqrt(1/4).
constexpr float kQuadHalfExtent = 1.0f;
constexpr double kQuadUvPerMeter = 0.5;
constexpr int kStreamedTextureDimension = 2048;

cwGeometry unitQuad()
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    });

    geometry.resizeVertices(4);
    const auto* position = geometry.attribute(cwGeometry::Semantic::Position);
    const auto* texCoord = geometry.attribute(cwGeometry::Semantic::TexCoord0);

    const QVector<QVector3D> positions = {
        QVector3D(-kQuadHalfExtent, -kQuadHalfExtent, 0.0f),
        QVector3D( kQuadHalfExtent, -kQuadHalfExtent, 0.0f),
        QVector3D( kQuadHalfExtent,  kQuadHalfExtent, 0.0f),
        QVector3D(-kQuadHalfExtent,  kQuadHalfExtent, 0.0f),
    };
    const QVector<QVector2D> uvs = {
        QVector2D(0.0f, 0.0f), QVector2D(1.0f, 0.0f),
        QVector2D(1.0f, 1.0f), QVector2D(0.0f, 1.0f),
    };
    for (int i = 0; i < positions.size(); ++i) {
        geometry.set(position, i, positions.at(i));
        geometry.set(texCoord, i, uvs.at(i));
    }

    geometry.setIndices({0u, 1u, 2u, 0u, 2u, 3u});
    geometry.setType(cwGeometry::Type::Triangles);
    return geometry;
}

cwStreamedTexture streamedSource(const QString& id)
{
    cwStreamedTexture source;
    source.dataRootPath = QStringLiteral("/not/read/without/a/drain");
    source.key.id = id;
    source.key.path = QStringLiteral("textures");
    source.key.checksum = QStringLiteral("checksum-") + id;
    source.size = QSize(kStreamedTextureDimension, kStreamedTextureDimension);
    return source;
}

// Loads run on cwConcurrent, so the render-thread state they settle has to be
// waited for rather than assumed.
constexpr int kWaitTimeoutMs = 30000;
constexpr int kPollIntervalMs = 1;

template <typename Predicate>
bool waitFor(Predicate predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kWaitTimeoutMs) {
        if (predicate()) {
            return true;
        }
        QThread::msleep(kPollIntervalMs);
    }
    return predicate();
}

qint64 ledgerStreamedCpuBytes()
{
    return cwRenderMemoryLedger::instance()->bytes(
        cwRenderMemoryLedger::Category::TexturedItemTexture,
        cwRenderMemoryLedger::Residency::Cpu);
}

cwRhiTexturedItems* syncedBackend(cwRhiScene& rhiScene, cwScene& scene, cwRenderTexturedItems& render)
{
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    auto* rhiObject = CwRhiSceneTestAccess::renderObjectForId(rhiScene, render.renderObjectId());
    return static_cast<cwRhiTexturedItems*>(rhiObject);
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

TEST_CASE("cwRhiFrameRenderer defaults to unrouted passes (fallthrough to swap-chain)",
          "[cwRhiSceneDispatch]")
{
    cwRhiFrameRenderer frame;
    // Before the first render() the per-pass routing is unset, so objects fall
    // back to the swap-chain target and nothing is allocated yet.
    using RP = cwRHIObject::RenderPass;
    REQUIRE(frame.passRenderPassDescriptor(RP::Background) == nullptr);
    REQUIRE(frame.passRenderPassDescriptor(RP::Opaque) == nullptr);
    REQUIRE(frame.passRenderPassDescriptor(RP::PointCloud) == nullptr);
    REQUIRE(frame.passSampleCount(RP::Opaque) == 0);
    REQUIRE(frame.pipelineCache().isEmpty());
}

TEST_CASE("evictPipelinesFor removes cache entries keyed on the freed descriptor",
          "[cwRhiSceneDispatch]")
{
    cwRhiFrameRenderer frame;

    auto* keptDesc = sentinelDescriptor(0xCA5E1001);
    auto* doomedDesc = sentinelDescriptor(0xCA5E2002);

    const cwRhiPipelineKey keptKey = makeKey(keptDesc, QStringLiteral("kept.vert"));
    const cwRhiPipelineKey doomedKeyA = makeKey(doomedDesc, QStringLiteral("doomedA.vert"));
    const cwRhiPipelineKey doomedKeyB = makeKey(doomedDesc, QStringLiteral("doomedB.vert"));

    REQUIRE(frame.acquirePipeline(keptKey, nullptr, makeStubRecord) != nullptr);
    REQUIRE(frame.acquirePipeline(doomedKeyA, nullptr, makeStubRecord) != nullptr);
    REQUIRE(frame.acquirePipeline(doomedKeyB, nullptr, makeStubRecord) != nullptr);
    REQUIRE(frame.pipelineCache().size() == 3);

    frame.evictPipelinesFor(doomedDesc);

    REQUIRE(frame.pipelineCache().size() == 1);
    REQUIRE(frame.pipelineCache().contains(keptKey));
    REQUIRE_FALSE(frame.pipelineCache().contains(doomedKeyA));
    REQUIRE_FALSE(frame.pipelineCache().contains(doomedKeyB));
}

TEST_CASE("evictPipelinesFor with null descriptor is a no-op",
          "[cwRhiSceneDispatch]")
{
    cwRhiFrameRenderer frame;

    auto* keepDesc = sentinelDescriptor(0xCA5E3003);
    const cwRhiPipelineKey key = makeKey(keepDesc, QStringLiteral("safe.vert"));
    REQUIRE(frame.acquirePipeline(key, nullptr, makeStubRecord) != nullptr);

    frame.evictPipelinesFor(nullptr);

    REQUIRE(frame.pipelineCache().contains(key));
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

TEST_CASE("a streamed item measures its texel density when it reaches the render thread",
          "[TexturedItemsStreaming]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    cwRenderTexturedItems render;
    render.setScene(&scene);
    render.setParent(nullptr);

    cwRenderTexturedItems::Item item;
    item.geometry = unitQuad();
    item.streamedTexture = streamedSource(QStringLiteral("scrap-1"));
    const uint32_t id = render.addItem(item);

    cwRhiTexturedItems* backend = syncedBackend(rhiScene, scene, render);
    REQUIRE(backend != nullptr);
    REQUIRE(CwRhiTexturedItemsTestAccess::hasItem(*backend, id));

    // Density is measured in updateBoundsFromGeometry, the one moment the
    // geometry is on the render thread.
    CHECK_THAT(CwRhiTexturedItemsTestAccess::uvPerMeter(*backend, id),
               Catch::Matchers::WithinAbs(kQuadUvPerMeter, 1e-6));
    CHECK(CwRhiTexturedItemsTestAccess::boundsValid(*backend, id));
    CHECK(CwRhiTexturedItemsTestAccess::streamSource(*backend, id) == item.streamedTexture);
    // Nothing is resident yet, and the streamed path leaves the legacy
    // whole-texture upload alone.
    CHECK(CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, id)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());
    CHECK_FALSE(CwRhiTexturedItemsTestAccess::textureNeedsUpdate(*backend, id));

    render.removeItem(id);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
}

TEST_CASE("re-sending a streamed descriptor keeps residency, changing it resets residency",
          "[TexturedItemsStreaming]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    cwRenderTexturedItems render;
    render.setScene(&scene);
    render.setParent(nullptr);

    const cwStreamedTexture first = streamedSource(QStringLiteral("scrap-1"));
    const cwStreamedTexture second = streamedSource(QStringLiteral("scrap-2"));

    cwRenderTexturedItems::Item item;
    item.geometry = unitQuad();
    item.streamedTexture = first;
    const uint32_t id = render.addItem(item);

    cwRhiTexturedItems* backend = syncedBackend(rhiScene, scene, render);
    REQUIRE(backend != nullptr);

    constexpr int kResidentLevel = 3;
    CwRhiTexturedItemsTestAccess::setResidentTopLevel(*backend, id, kResidentLevel);

    // A producer that re-runs publishes the same descriptor. Restarting the
    // load from the pinned base each time would undo everything streamed so far.
    render.updateStreamedTexture(id, first);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    CHECK(CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, id) == kResidentLevel);

    // A different descriptor means different pixels, so what is resident no
    // longer describes the item.
    render.updateStreamedTexture(id, second);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
    CHECK(CwRhiTexturedItemsTestAccess::streamSource(*backend, id) == second);
    CHECK(CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, id)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());
    CHECK(CwRhiTexturedItemsTestAccess::requestedTopLevel(*backend, id)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());

    render.removeItem(id);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
}

TEST_CASE("selection asks for the pinned base before anything is resident",
          "[TexturedItemsStreaming]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    cwRenderTexturedItems render;
    render.setScene(&scene);
    render.setParent(nullptr);

    cwRenderTexturedItems::Item item;
    item.geometry = unitQuad();
    item.streamedTexture = streamedSource(QStringLiteral("scrap-1"));
    const uint32_t id = render.addItem(item);

    cwRhiTexturedItems* backend = syncedBackend(rhiScene, scene, render);
    REQUIRE(backend != nullptr);

    // Selection reads the camera, the viewport, and the budgets — never the
    // command buffer — so hand-built render data is enough to drive it.
    cwRHIObject::RenderData renderData{nullptr, nullptr, cwSceneUpdate::Flag::None, nullptr, 1};
    renderData.viewportSize = QSize(1920, 1080);

    QMatrix4x4 projection;
    projection.perspective(45.0f, 16.0f / 9.0f, 1.0f, 1000.0f);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(), QVector3D(0.0f, 1.0f, 0.0f));
    renderData.projectionMatrix = projection;
    renderData.viewProjectionMatrix = projection * view;

    cwRHIObject::GatherContext context;
    context.renderData = &renderData;
    context.renderPass = cwRHIObject::RenderPass::Opaque;

    CwRhiTexturedItemsTestAccess::selectStreamLevel(*backend, id, context);

    const int base = cw::residency::pinnedBaseLevel(item.streamedTexture.size);
    CHECK(CwRhiTexturedItemsTestAccess::requestedTopLevel(*backend, id) == base);
    CHECK(CwRhiTexturedItemsTestAccess::hasStreamingWork(*backend));

    // A second pass over an item whose base load is still running asks for
    // nothing more.
    CwRhiTexturedItemsTestAccess::selectStreamLevel(*backend, id, context);
    CHECK(CwRhiTexturedItemsTestAccess::requestedTopLevel(*backend, id) == base);

    render.removeItem(id);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
}

TEST_CASE("the upload drain spends one frame's budget level by level",
          "[TexturedItemsStreaming]")
{
    using cw::residency::takeFromBudget;

    // What streamResources walks: level bytes charged in order until the frame's
    // budget runs out. The count of levels that fit is the frame's progress.
    const auto levelsUploadedInOneFrame = [](const QVector<qint64>& levelBytes, qint64 budget) {
        qint64 remaining = budget;
        bool anythingUploaded = false;
        int uploaded = 0;
        for (qint64 bytes : levelBytes) {
            if (!takeFromBudget(remaining, bytes, anythingUploaded)) {
                break;
            }
            anythingUploaded = true;
            uploaded++;
        }
        return uploaded;
    };

    constexpr qint64 kMegabyte = 1024 * 1024;

    SECTION("a chain that fits lands in one frame")
    {
        const QVector<qint64> levels {4 * kMegabyte, kMegabyte, kMegabyte / 4};
        CHECK(levelsUploadedInOneFrame(levels, 8 * kMegabyte) == levels.size());
    }

    SECTION("a chain past the budget is split across frames")
    {
        const QVector<qint64> levels {4 * kMegabyte, 4 * kMegabyte, 4 * kMegabyte};
        CHECK(levelsUploadedInOneFrame(levels, 8 * kMegabyte) == 2);
    }

    SECTION("a level larger than the whole budget still makes progress")
    {
        const QVector<qint64> levels {16 * kMegabyte, 4 * kMegabyte};
        // The first upload of a frame is always granted, and it overdraws the
        // budget, so the next level waits for the next frame.
        CHECK(levelsUploadedInOneFrame(levels, kMegabyte) == 1);
    }
}

TEST_CASE("a load for a replaced descriptor never lands on the item",
          "[TexturedItemsStreaming]")
{
    cwScene scene;
    cwRhiScene rhiScene;

    cwRenderTexturedItems render;
    render.setScene(&scene);
    render.setParent(nullptr);

    cwRenderTexturedItems::Item item;
    item.geometry = unitQuad();
    item.streamedTexture = streamedSource(QStringLiteral("scrap-1"));
    const uint32_t id = render.addItem(item);

    cwRhiTexturedItems* backend = syncedBackend(rhiScene, scene, render);
    REQUIRE(backend != nullptr);

    cwRHIObject::RenderData renderData{nullptr, nullptr, cwSceneUpdate::Flag::None, nullptr, 1};
    renderData.viewportSize = QSize(1920, 1080);

    cwRHIObject::GatherContext context;
    context.renderData = &renderData;
    context.renderPass = cwRHIObject::RenderPass::Opaque;

    REQUIRE(ledgerStreamedCpuBytes() == 0);
    CwRhiTexturedItemsTestAccess::selectStreamLevel(*backend, id, context);
    // The payload the base load will hold is charged against the CPU ledger the
    // moment it is asked for.
    CHECK(ledgerStreamedCpuBytes() > 0);

    // Replacing the descriptor makes everything in flight for it stale.
    render.updateStreamedTexture(id, streamedSource(QStringLiteral("scrap-2")));
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    REQUIRE(waitFor([&]() {
        return !CwRhiTexturedItemsTestAccess::hasStreamingWork(*backend);
    }));
    CHECK(ledgerStreamedCpuBytes() == 0);

    // Draining takes nothing onto the item: the stale load is gone, and the new
    // descriptor has not been asked for yet. No RHI is touched because no level
    // is waiting to upload.
    cwRHIObject::ResourceUpdateData resourceData{nullptr, renderData, nullptr};
    qint64 remainingUploadBytes = renderData.budgets.uploadBudgetBytesPerFrame;
    CHECK_FALSE(backend->streamResources(resourceData, remainingUploadBytes));
    CHECK(remainingUploadBytes == renderData.budgets.uploadBudgetBytesPerFrame);
    CHECK(CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, id)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());

    render.removeItem(id);
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);
}
