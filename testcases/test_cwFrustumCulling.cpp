// CPU-only tests for whole-object frustum culling in cwRhiFrameRenderer::gatherScene.
// None of these tests touch a real QRhi device.

#include <catch2/catch_test_macros.hpp>

#include "cwFrustum.h"
#include "cwRenderCullingStats.h"
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

    std::optional<QBox3D> worldBounds() const override { return bounds; }

    std::optional<QBox3D> bounds;
    int gatherCount = 0;
    quint32 lastObjectOrder = 0;
    const cwFrustum* lastFrustum = nullptr;
    const cwRenderCullingStats::Counts* lastCullingStats = nullptr;
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

    const quint64 startRevision = cwRenderCullingStats::instance()->revision();

    gatherOnce(frame);

    REQUIRE(cwRenderCullingStats::instance()->revision() == startRevision + 1);

    const cwRenderCullingStats::Counts counts = cwRenderCullingStats::instance()->counts();
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

    const cwRenderCullingStats::Counts counts = cwRenderCullingStats::instance()->counts();
    REQUIRE(counts.objectsTotal == 1);
    REQUIRE(counts.objectsCulled == 0);
}
