// Regression test for the render-thread lifetime rule around streamed textures:
// when a streamed mip chain finishes and the item swaps its QRhiTexture, the
// item's shader resource bindings have to be rebuilt to match — even when the
// item's pipeline record was purged earlier in the same frame by a render
// target rebuild. Otherwise the old SRB keeps binding the just-deleted texture
// and the next draw reads freed memory.

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMatrix4x4>
#include <QSize>
#include <QTemporaryDir>
#include <QThread>
#include <QVector3D>

//Qt RHI
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

//Std includes
#include <array>
#include <memory>

//Our includes
#include "cwDiskCacher.h"
#include "cwGeometry.h"
#include "cwKtx2Codec.h"
#include "cwRHIObject.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiFrameRenderer.h"
#include "cwRhiItemRenderer.h"
#include "cwRhiTexturedItems.h"
#include "cwScene.h"
#include "cwSceneUpdate.h"
#include "cwStreamedTexture.h"
#include "cwTextureResidency.h"

#include "CwRhiTexturedItemsTestAccess.h"
#include "TestGeometryBuilders.h"

namespace {

    // 1024 is the smallest level-0 that still has a level above its pinned base
    // (512), so the item can be refined from base to level 0 — the swap this
    // test rides in on — while the UASTC encode stays fast.
    constexpr int kTextureDimension = 1024;
    constexpr int kFinestLevel = 0;
    constexpr float kQuadHalfExtent = 1.0f;
    constexpr int kLiveTargetDimension = 256;
    constexpr int kScratchTargetDimension = 512;
    constexpr float kCameraDistance = 5.0f;
    constexpr float kFieldOfView = 45.0f;
    constexpr float kNearPlane = 0.1f;
    constexpr float kFarPlane = 1000.0f;
    constexpr int kSelectionViewportWidth = 3840;
    constexpr int kSelectionViewportHeight = 2160;
    constexpr float kSelectionEyeDistance = 1.5f;
    constexpr int kWaitTimeoutMs = 30000;
    constexpr int kFramePauseMs = 2;
    constexpr int kFramesAfterTheSwap = 10;

    QImage gradientImage(int dimension)
    {
        QImage image(dimension, dimension, QImage::Format_RGBA8888);
        for (int y = 0; y < dimension; y++) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < dimension; x++) {
                line[x] = qRgba(x % 256, y % 256, (x + y) % 256, 255);
            }
        }
        return image;
    }

    // A color + depth offscreen target, standing in for the live swap chain and
    // for the export scratch target the offscreen renderer rebuilds per size.
    struct RenderTarget {
        std::unique_ptr<QRhiTexture> color;
        std::unique_ptr<QRhiRenderBuffer> depth;
        std::unique_ptr<QRhiTextureRenderTarget> target;
        std::unique_ptr<QRhiRenderPassDescriptor> renderPassDescriptor;
    };

    RenderTarget makeRenderTarget(QRhi* rhi, QSize size)
    {
        RenderTarget made;
        made.color.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                         QRhiTexture::RenderTarget
                                             | QRhiTexture::UsedAsTransferSource));
        made.color->create();

        made.depth.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
        made.depth->create();

        QRhiTextureRenderTargetDescription description({ QRhiColorAttachment(made.color.get()) });
        description.setDepthStencilBuffer(made.depth.get());

        made.target.reset(rhi->newTextureRenderTarget(description));
        made.renderPassDescriptor.reset(made.target->newCompatibleRenderPassDescriptor());
        made.target->setRenderPassDescriptor(made.renderPassDescriptor.get());
        made.target->create();
        return made;
    }

    QMatrix4x4 liveProjection(QSize size)
    {
        QMatrix4x4 projection;
        projection.perspective(kFieldOfView, float(size.width()) / float(size.height()),
                               kNearPlane, kFarPlane);
        return projection;
    }

    QMatrix4x4 liveView()
    {
        QMatrix4x4 view;
        view.lookAt(QVector3D(0.0f, 0.0f, kCameraDistance), QVector3D(),
                    QVector3D(0.0f, 1.0f, 0.0f));
        return view;
    }

    // A camera close enough, at a high enough output resolution, that selection
    // wants the finest level. Only the matrices and the viewport are read, so no
    // render target of this size is ever built.
    cwRHIObject::RenderData closeCameraRenderData()
    {
        cwRHIObject::RenderData renderData{nullptr, nullptr, cwSceneUpdate::Flag::None, nullptr, 1};
        renderData.viewportSize = QSize(kSelectionViewportWidth, kSelectionViewportHeight);

        QMatrix4x4 projection;
        projection.perspective(kFieldOfView,
                               float(kSelectionViewportWidth) / float(kSelectionViewportHeight),
                               kNearPlane, kFarPlane);
        QMatrix4x4 view;
        view.lookAt(QVector3D(0.0f, 0.0f, kSelectionEyeDistance), QVector3D(),
                    QVector3D(0.0f, 1.0f, 0.0f));

        renderData.projectionMatrix = projection;
        renderData.viewMatrix = view;
        renderData.viewProjectionMatrix = projection * view;
        return renderData;
    }

} // namespace

TEST_CASE("A streamed texture swap rebuilds the shader resource bindings when the pipeline "
          "was purged",
          "[TexturedItemsStreaming][StreamingSrbSwap]")
{
#ifdef Q_OS_MACOS
    QRhiMetalInitParams initParams;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Metal, &initParams));
#else
    std::unique_ptr<QRhi> rhi;
#endif
    if (!rhi) {
        SKIP("A real QRhi backend is required to watch GPU resource lifetimes; "
             "this test runs on Metal");
    }

    // The KTX2 bytes the streamer's default loader reads back off disk.
    QTemporaryDir cacheDirectory;
    REQUIRE(cacheDirectory.isValid());

    const auto encoded = cw::ktx2::encodeRgba(gradientImage(kTextureDimension));
    REQUIRE_FALSE(encoded.hasError());

    cwDiskCacher::Key cacheKey;
    cacheKey.id = QStringLiteral("streamed-srb-swap-%1").arg(QCoreApplication::applicationPid());
    cacheKey.path = QDir(QStringLiteral("streamed-textures"));
    cacheKey.checksum = QStringLiteral("checksum");

    cwDiskCacher cacher{QDir(cacheDirectory.path())};
    cacher.insert(cacheKey, encoded.value());

    cwStreamedTexture source;
    source.dataRootPath = cacheDirectory.path();
    source.key = cacheKey;
    source.size = QSize(kTextureDimension, kTextureDimension);

    // The front end: one streamed textured item.
    cwScene scene;
    cwRenderTexturedItems render;
    render.setScene(&scene);

    cwRenderTexturedItems::Item item;
    item.geometry = cwTestGeometry::texturedQuad(kQuadHalfExtent);
    item.streamedTexture = source;
    const uint32_t itemId = render.addItem(item);

    // The render thread. cwRhiItemRenderer owns the cwRhiScene whose frame
    // renderer holds the global camera UBO the item's SRB binds, so the renderer
    // and the engine driving the draws have to be the same one.
    cwRhiItemRenderer renderer;
    cwRhiFrameRenderer* frameRenderer = renderer.frameRenderer();

    auto* backend = new cwRhiTexturedItems;
    frameRenderer->registerRenderObject(render.renderObjectId(), backend);
    backend->synchronize({&render, &renderer});
    REQUIRE(CwRhiTexturedItemsTestAccess::hasItem(*backend, itemId));

    const QSize liveSize(kLiveTargetDimension, kLiveTargetDimension);
    RenderTarget live = makeRenderTarget(rhi.get(), liveSize);

    // The export scratch target: a second render-pass descriptor the item builds
    // a pipeline against, exactly as a high-resolution capture does.
    RenderTarget scratch = makeRenderTarget(rhi.get(),
                                            QSize(kScratchTargetDimension, kScratchTargetDimension));

    bool backendInitialized = false;

    // One frame against @a into: optionally purge first (what
    // cwRhiOffscreenRenderer::destroyTarget and destroyEdlOffscreen do when a
    // target is rebuilt at a new size), then the live frame's order —
    // updateResources, streamResources, gatherScene, drawScene.
    const auto renderFrame = [&](const RenderTarget& into,
                                 QRhiRenderPassDescriptor* purgeDescriptor = nullptr) {
        QRhiCommandBuffer* cb = nullptr;
        REQUIRE(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);

        frameRenderer->initialize(cb);

        if (purgeDescriptor) {
            frameRenderer->evictPipelinesFor(purgeDescriptor);
        }

        QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
        const QSize size = into.target->pixelSize();

        cwRHIObject::RenderData renderData{cb, &renderer, cwSceneUpdate::Flag::None,
                                           into.renderPassDescriptor.get(),
                                           into.target->sampleCount()};

        frameRenderer->setupPassRouting(into.target.get(), nullptr);

        constexpr int kLiveCameraSlot = 0;
        const cwRhiFrameRenderer::ClipSpaceCamera camera =
            frameRenderer->stampCamera(batch, rhi.get(), renderData, kLiveCameraSlot,
                                       into.target.get(), liveProjection(size), liveView(),
                                       1.0f, size);

        const cwRHIObject::PerPassRenderData perPassRenderData =
            frameRenderer->buildPerPassRenderData(renderData);

        cwRHIObject::ResourceUpdateData resourceUpdateData{batch, renderData, &perPassRenderData};

        // Only the first frame runs updateResources, so from then on streaming is
        // the one path that can rebuild the item's SRB.
        if (!backendInitialized) {
            backend->initialize(resourceUpdateData);
            backend->updateResources(resourceUpdateData);
            backendInitialized = true;
        }

        qint64 remainingUploadBytes = renderData.budgets.uploadBudgetBytesPerFrame;
        backend->streamResources(resourceUpdateData, remainingUploadBytes);

        std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiFrameRenderer::kPassCount> passBatches;
        frameRenderer->gatherScene(passBatches, perPassRenderData);

        const cwRhiPostProcessEffect::FrameUniformContext frameContext{
            camera.projectionCorrected, size, 1.0f
        };

        frameRenderer->drawScene(cb, into.target.get(), nullptr, passBatches, perPassRenderData,
                                 batch, frameContext, 0, QColor::fromRgbF(0.0, 0.0, 0.0, 0.0));

        rhi->endOffscreenFrame();
    };

    // Bring the item all the way up: the pinned base loads, uploads, and the
    // item draws with a texture, an SRB, and a pipeline record.
    renderFrame(live);

    QElapsedTimer timer;
    timer.start();
    while (CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, itemId)
               == CwRhiTexturedItemsTestAccess::noResidentLevel()
           && timer.elapsed() < kWaitTimeoutMs) {
        QThread::msleep(kFramePauseMs);
        renderFrame(live);
    }

    const int baseLevel = CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, itemId);
    REQUIRE(baseLevel != CwRhiTexturedItemsTestAccess::noResidentLevel());
    REQUIRE(baseLevel == cw::residency::pinnedBaseLevel(source.size));

    // The item is drawn into the export scratch target too, so it holds a real
    // pipeline keyed on that descriptor — the state a capture leaves behind.
    renderFrame(scratch);

    // Ask for a finer level from a camera that can see the detail. The request
    // is issued outside gather() so no frame runs between it and the purge.
    const cwRHIObject::RenderData selectionRenderData = closeCameraRenderData();
    cwRHIObject::GatherContext selectionContext;
    selectionContext.renderData = &selectionRenderData;
    selectionContext.renderPass = cwRHIObject::RenderPass::Opaque;
    CwRhiTexturedItemsTestAccess::selectStreamLevel(*backend, itemId, selectionContext);

    const int requested = CwRhiTexturedItemsTestAccess::requestedTopLevel(*backend, itemId);
    REQUIRE(requested < baseLevel);
    REQUIRE(requested == kFinestLevel);

    // Frames that purge the pipeline records first and stream second, the order
    // renderLiveFrame runs them in. Whichever frame the finer chain lands in is
    // the frame that swaps `texture` with a null pipeline record — and then draws.
    int frames = 0;
    const void* bindingsBeforeTheSwap = nullptr;
    const void* bindingsAfterTheSwap = nullptr;
    timer.restart();
    while (CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, itemId) != kFinestLevel
           && timer.elapsed() < kWaitTimeoutMs) {
        const void* textureBefore = CwRhiTexturedItemsTestAccess::texturePointer(*backend, itemId);
        const void* bindingsBefore = CwRhiTexturedItemsTestAccess::srbPointer(*backend, itemId);

        renderFrame(live, scratch.renderPassDescriptor.get());
        frames++;

        const void* textureAfter = CwRhiTexturedItemsTestAccess::texturePointer(*backend, itemId);
        if (textureAfter != textureBefore) {
            bindingsBeforeTheSwap = bindingsBefore;
            bindingsAfterTheSwap = CwRhiTexturedItemsTestAccess::srbPointer(*backend, itemId);
            qInfo() << "frame" << frames << "swapped the texture" << textureBefore << "->"
                    << textureAfter << "with bindings" << bindingsBefore << "->"
                    << bindingsAfterTheSwap;
        }

        QThread::msleep(kFramePauseMs);
    }

    REQUIRE(CwRhiTexturedItemsTestAccess::residentTopLevel(*backend, itemId) == kFinestLevel);

    // The invariant: the frame that deleted the old texture also replaced the
    // bindings that held it, so nothing downstream draws from freed memory.
    CHECK(bindingsAfterTheSwap != nullptr);
    CHECK(bindingsAfterTheSwap != bindingsBeforeTheSwap);

    // The bindings the swap left behind are the ones the item keeps drawing
    // with, and more frames — still without an updateResources — keep using
    // them, so a stale binding would be handed to the GPU repeatedly here.
    for (int i = 0; i < kFramesAfterTheSwap; i++) {
        renderFrame(live, scratch.renderPassDescriptor.get());
    }
    CHECK(CwRhiTexturedItemsTestAccess::srbPointer(*backend, itemId) != nullptr);

    frameRenderer->evictPipelinesFor(scratch.renderPassDescriptor.get());
    frameRenderer->evictPipelinesFor(live.renderPassDescriptor.get());
    frameRenderer->destroyRenderObject(render.renderObjectId());
}
