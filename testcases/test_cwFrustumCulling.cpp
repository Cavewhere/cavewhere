// CPU-only tests for whole-object frustum culling in cwRhiFrameRenderer::gatherScene.
// None of these tests touch a real QRhi device.

#include <catch2/catch_test_macros.hpp>

#include "cwFrustum.h"
#include "cwRenderFrameStats.h"
#include "cwRHIObject.h"
#include "cwRhiFrameRenderer.h"
#include "cwSceneVisibility.h"

#include <QBox3D>
#include <QMatrix4x4>
#include <QVector3D>

#include <optional>

namespace {

constexpr float kFieldOfView = 60.0f;
constexpr float kAspectRatio = 1.0f;
constexpr float kNearPlane = 1.0f;
constexpr float kFarPlane = 100.0f;

constexpr float kBoxHalfSize = 1.0f;

// A box here sits comfortably inside the frustum; the same box mirrored to
// +Z sits behind the camera.
constexpr float kInFrustumZ = -10.0f;
constexpr float kBehindCameraZ = 10.0f;

// Records what gatherScene handed it, without touching a device.
class CountingObject : public cwRHIObject
{
public:
    void initialize(const ResourceUpdateData&) override {}
    void synchronize(const SynchronizeData&) override {}
    void updateResources(const ResourceUpdateData&) override {}

    bool gather(const GatherContext& context, QVector<PipelineBatch>&) override
    {
        ++gatherCount;
        lastObjectOrder = context.objectOrder;
        lastFrustum = context.frustum;
        lastCullingStats = context.cullingStats;
        return false;
    }

    // The context is copied apart rather than kept: gatherScene's frustum and
    // culling tally live on its own stack, so only what they said survives the
    // call.
    void gatherCulled(const GatherContext& context) override
    {
        ++gatherCulledCount;
        culledPass = context.renderPass;
        culledObjectOrder = context.objectOrder;
        culledLiveFrame = context.liveFrame;
        culledAppearanceSlot = context.appearanceSlot;
        culledViewProjection =
            context.renderData ? context.renderData->viewProjectionMatrix : QMatrix4x4();
        culledHasRenderData = context.renderData != nullptr;
        culledHasVisibility = context.visibility != nullptr;
        culledHasFrustum = context.frustum != nullptr;
        culledHasCullingStats = context.cullingStats != nullptr;
    }

    std::optional<QBox3D> worldBounds() const override { return bounds; }

    std::optional<QBox3D> bounds;
    int gatherCount = 0;
    int gatherCulledCount = 0;
    RenderPass culledPass = RenderPass::Overlay;
    quint32 culledObjectOrder = 0;
    bool culledLiveFrame = false;
    int culledAppearanceSlot = -1;
    QMatrix4x4 culledViewProjection;
    bool culledHasRenderData = false;
    bool culledHasVisibility = false;
    bool culledHasFrustum = false;
    bool culledHasCullingStats = false;
    quint32 lastObjectOrder = 0;
    const cwFrustum* lastFrustum = nullptr;
    const cwRenderFrameStats::Culling* lastCullingStats = nullptr;
};

QMatrix4x4 viewProjectionLookingDownNegativeZ()
{
    QMatrix4x4 projection;
    projection.perspective(kFieldOfView, kAspectRatio, kNearPlane, kFarPlane);

    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 0.0f, -1.0f),
                QVector3D(0.0f, 1.0f, 0.0f));

    return projection * view;
}

cwRHIObject::PerPassRenderData perPassDataWithCamera()
{
    cwRHIObject::RenderData base;
    base.cb = nullptr;
    base.renderer = nullptr;
    base.updateFlag = cwSceneUpdate::Flag::None;
    base.viewProjectionMatrix = viewProjectionLookingDownNegativeZ();

    cwRHIObject::PerPassRenderData perPass;
    perPass.fill(base);
    return perPass;
}

QBox3D boxAt(const QVector3D& center)
{
    const QVector3D half(kBoxHalfSize, kBoxHalfSize, kBoxHalfSize);
    return QBox3D(center - half, center + half);
}

// Frames gathered when checking that a call happens once per frame.
constexpr int kSettleFrames = 3;

// Gathers one frame with the camera at the origin, into throwaway batches.
void gatherOnce(cwRhiFrameRenderer& frame)
{
    std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiFrameRenderer::kPassCount> passBatches;
    frame.gatherScene(passBatches, perPassDataWithCamera(), {});
}

} // namespace

TEST_CASE("gatherScene gathers an object whose world box is inside the frustum",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    gatherOnce(frame);

    REQUIRE(object->gatherCount > 0);
    REQUIRE(object->gatherCulledCount == 0);
}

TEST_CASE("gatherScene skips an object whose world box is outside the frustum",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kBehindCameraZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    gatherOnce(frame);

    REQUIRE(object->gatherCount == 0);
}

TEST_CASE("gatherScene always gathers an object that reports no world bounds",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = std::nullopt;
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    gatherOnce(frame);

    REQUIRE(object->gatherCount > 0);
}

TEST_CASE("gatherScene honors the visibility gate for an in-frustum object",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    cwSceneVisibility visibility;
    visibility.setObjectVisible(cwRenderObjectId{1}, false);
    frame.setVisibilitySnapshot(visibility.snapshot());

    gatherOnce(frame);

    REQUIRE(object->gatherCount == 0);
}

TEST_CASE("A culled object still advances objectOrder for the objects behind it",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;

    auto* culled = new CountingObject;
    culled->bounds = boxAt(QVector3D(0.0f, 0.0f, kBehindCameraZ));
    frame.registerRenderObject(cwRenderObjectId{1}, culled);

    auto* drawn = new CountingObject;
    drawn->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{2}, drawn);

    gatherOnce(frame);

    REQUIRE(culled->gatherCount == 0);
    REQUIRE(drawn->gatherCount > 0);
    // The second object keeps the order it would have had with the first drawn.
    REQUIRE(drawn->lastObjectOrder == 1);
}

TEST_CASE("gatherScene stamps the frame's frustum into every GatherContext",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = std::nullopt;
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    gatherOnce(frame);

    REQUIRE(object->lastFrustum != nullptr);
    REQUIRE(object->lastFrustum->isValid());
    REQUIRE(object->lastFrustum->intersects(boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ))));
    REQUIRE_FALSE(object->lastFrustum->intersects(boxAt(QVector3D(0.0f, 0.0f, kBehindCameraZ))));
}

TEST_CASE("gatherScene publishes the frame's culled and total object counts",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;

    auto* inFrustum = new CountingObject;
    inFrustum->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{1}, inFrustum);

    auto* outOfFrustum = new CountingObject;
    outOfFrustum->bounds = boxAt(QVector3D(0.0f, 0.0f, kBehindCameraZ));
    frame.registerRenderObject(cwRenderObjectId{2}, outOfFrustum);

    auto* unbounded = new CountingObject;
    unbounded->bounds = std::nullopt;
    frame.registerRenderObject(cwRenderObjectId{3}, unbounded);

    const quint64 startRevision = cwRenderFrameStats::instance()->revision();

    gatherOnce(frame);

    REQUIRE(cwRenderFrameStats::instance()->revision() == startRevision + 1);

    const cwRenderFrameStats::Culling counts = cwRenderFrameStats::instance()->culling();
    REQUIRE(counts.objectsTotal == 3);
    REQUIRE(counts.objectsCulled == 1);

    //The stub objects gather no items, so the item tally stays empty
    REQUIRE(counts.itemsTotal == 0);
    REQUIRE(counts.itemsCulled == 0);

    REQUIRE(inFrustum->lastCullingStats != nullptr);
    REQUIRE(unbounded->lastCullingStats == inFrustum->lastCullingStats);
}

TEST_CASE("gatherScene leaves a hidden object out of the culled and total counts",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;

    auto* hidden = new CountingObject;
    hidden->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{1}, hidden);

    auto* drawn = new CountingObject;
    drawn->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{2}, drawn);

    cwSceneVisibility visibility;
    visibility.setObjectVisible(cwRenderObjectId{1}, false);
    frame.setVisibilitySnapshot(visibility.snapshot());

    gatherOnce(frame);

    const cwRenderFrameStats::Culling counts = cwRenderFrameStats::instance()->culling();
    REQUIRE(counts.objectsTotal == 1);
    REQUIRE(counts.objectsCulled == 0);
}

TEST_CASE("gatherScene leaves the published counts alone for an offscreen job",
          "[FrustumCulling][RenderCullingStats]")
{
    cwRhiFrameRenderer frame;

    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    //A live frame first, so the published counts belong to a known frame
    gatherOnce(frame);

    const quint64 liveRevision = cwRenderFrameStats::instance()->revision();
    const cwRenderFrameStats::Culling liveCounts = cwRenderFrameStats::instance()->culling();

    std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiFrameRenderer::kPassCount> passBatches;
    frame.gatherScene(passBatches, perPassDataWithCamera(), {.liveFrame = false});

    REQUIRE(object->gatherCount > 1);
    REQUIRE(cwRenderFrameStats::instance()->revision() == liveRevision);
    REQUIRE(cwRenderFrameStats::instance()->culling() == liveCounts);
}

TEST_CASE("gatherScene tells a frustum-culled object once a frame that it drew nothing",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kBehindCameraZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    for (int i = 0; i < kSettleFrames; i++) {
        gatherOnce(frame);
    }

    REQUIRE(object->gatherCount == 0);
    //Once per frame, not once per pass: an object that draws nothing settles once
    REQUIRE(object->gatherCulledCount == kSettleFrames);
}

TEST_CASE("gatherScene tells a hidden object once a frame that it drew nothing",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    cwSceneVisibility visibility;
    visibility.setObjectVisible(cwRenderObjectId{1}, false);
    frame.setVisibilitySnapshot(visibility.snapshot());

    for (int i = 0; i < kSettleFrames; i++) {
        gatherOnce(frame);
    }

    REQUIRE(object->gatherCount == 0);
    REQUIRE(object->gatherCulledCount == kSettleFrames);
}

TEST_CASE("An object hidden by a job's overlay settles for that job",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kInFrustumZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    cwSceneGatherOptions hideObject;
    hideObject.hiddenObjectIds.insert(cwRenderObjectId{1});

    std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiFrameRenderer::kPassCount> passBatches;
    frame.gatherScene(passBatches, perPassDataWithCamera(), hideObject);

    REQUIRE(object->gatherCount == 0);
    REQUIRE(object->gatherCulledCount == 1);
}

TEST_CASE("gatherScene hands a culled object the frame it drew nothing in",
          "[FrustumCulling]")
{
    cwRhiFrameRenderer frame;
    auto* object = new CountingObject;
    object->bounds = boxAt(QVector3D(0.0f, 0.0f, kBehindCameraZ));
    frame.registerRenderObject(cwRenderObjectId{1}, object);

    cwSceneVisibility visibility;
    visibility.setObjectVisible(cwRenderObjectId{1}, true);
    frame.setVisibilitySnapshot(visibility.snapshot());

    cwSceneGatherOptions options;

    SECTION("the live frame") {
        //options as they come
    }

    SECTION("an offscreen job") {
        options.liveFrame = false;
    }

    std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiFrameRenderer::kPassCount> passBatches;
    frame.gatherScene(passBatches, perPassDataWithCamera(), options);

    REQUIRE(object->gatherCulledCount == 1);
    CHECK(object->culledLiveFrame == options.liveFrame);

    //The first pass's render data stands in for the frame: every pass shares
    //this job's camera, which is all an object that draws nothing can read
    CHECK(object->culledHasRenderData);
    CHECK(object->culledPass == cwRHIObject::RenderPass::Background);
    CHECK(object->culledViewProjection == viewProjectionLookingDownNegativeZ());
    CHECK(object->culledObjectOrder == 0);
    CHECK(object->culledHasVisibility);
    CHECK(object->culledHasFrustum);
    CHECK(object->culledHasCullingStats);
    CHECK(object->culledAppearanceSlot == 0);
}
