// Regression tests for the hidden-view eviction path. Every 3D view owns its own
// cwRhiTexturedItems, and GPU-budget enforcement runs inside the frame — so a
// view whose QQuickRhiItem is hidden gets no frames, never demotes, and keeps its
// streamed textures resident against the process-wide budget forever. Hiding it
// now releases those textures outright (cwRhiViewer schedules a render job that
// reaches cwRhiTexturedItems::releaseStreamedTextures); showing it again
// re-streams them from the warm disk cache.

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
#include "cwMipMath.h"
#include "cwRHIObject.h"
#include "cwRenderMemoryLedger.h"
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

    // Large enough that a level above the pinned base exists, small enough that
    // the UASTC encode stays quick.
    constexpr int kTextureDimension = 1024;
    constexpr float kQuadHalfExtent = 1.0f;
    constexpr int kLiveTargetDimension = 256;
    constexpr float kCameraDistance = 5.0f;
    constexpr float kFieldOfView = 45.0f;
    constexpr float kNearPlane = 0.1f;
    constexpr float kFarPlane = 1000.0f;
    constexpr int kWaitTimeoutMs = 30000;
    constexpr int kFramePauseMs = 2;

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

    // A color + depth offscreen target standing in for the live swap chain.
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

    qint64 streamedTextureLedgerBytes()
    {
        return cwRenderMemoryLedger::instance()->bytes(
            cwRenderMemoryLedger::Category::TexturedItemTexture,
            cwRenderMemoryLedger::Residency::Gpu);
    }

    // One streamed textured item drawn into an offscreen target, driven through
    // the same call order cwRhiFrameRenderer::renderLiveFrame uses.
    class StreamedItemFixture {
    public:
        explicit StreamedItemFixture(QRhi* rhi, const QString& cacheId) :
            m_rhi(rhi),
            m_live(makeRenderTarget(rhi, QSize(kLiveTargetDimension, kLiveTargetDimension)))
        {
            REQUIRE(m_cacheDirectory.isValid());

            const auto encoded = cw::ktx2::encodeRgba(gradientImage(kTextureDimension));
            REQUIRE_FALSE(encoded.hasError());

            // Tests run as concurrent processes, so the pid keeps two of them
            // from sharing a cache entry.
            cwDiskCacher::Key cacheKey;
            cacheKey.id = QStringLiteral("%1-%2").arg(cacheId).arg(QCoreApplication::applicationPid());
            cacheKey.path = QDir(QStringLiteral("streamed-textures"));
            cacheKey.checksum = QStringLiteral("checksum");

            cwDiskCacher cacher{QDir(m_cacheDirectory.path())};
            cacher.insert(cacheKey, encoded.value());

            m_source.dataRootPath = m_cacheDirectory.path();
            m_source.key = cacheKey;
            m_source.size = QSize(kTextureDimension, kTextureDimension);

            m_render.setScene(&m_scene);

            cwRenderTexturedItems::Item item;
            item.geometry = cwTestGeometry::texturedQuad(kQuadHalfExtent);
            item.streamedTexture = m_source;
            m_itemId = m_render.addItem(item);

            m_backend = new cwRhiTexturedItems;
            frameRenderer()->registerRenderObject(m_render.renderObjectId(), m_backend);
            m_backend->synchronize({&m_render, &m_renderer});
            REQUIRE(CwRhiTexturedItemsTestAccess::hasItem(*m_backend, m_itemId));
        }

        ~StreamedItemFixture()
        {
            frameRenderer()->evictPipelinesFor(m_live.renderPassDescriptor.get());
            frameRenderer()->destroyRenderObject(m_render.renderObjectId());
        }

        cwRhiItemRenderer& renderer() { return m_renderer; }
        cwRhiFrameRenderer* frameRenderer() { return m_renderer.frameRenderer(); }
        const cwRhiTexturedItems& backend() const { return *m_backend; }
        uint32_t itemId() const { return m_itemId; }
        const cwStreamedTexture& source() const { return m_source; }

        //! True when the last frame gathered the item into a draw batch
        bool itemWasGathered() const { return m_itemGathered; }

        //! One frame in renderLiveFrame's order: updateResources (first frame
        //! only), streamResources, gatherScene, drawScene
        void renderFrame()
        {
            QRhiCommandBuffer* cb = nullptr;
            REQUIRE(m_rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);

            frameRenderer()->initialize(cb);

            QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
            const QSize size = m_live.target->pixelSize();

            cwRHIObject::RenderData renderData{cb, &m_renderer, cwSceneUpdate::Flag::None,
                                               m_live.renderPassDescriptor.get(),
                                               m_live.target->sampleCount()};

            frameRenderer()->setupPassRouting(m_live.target.get(), nullptr);

            constexpr int kLiveCameraSlot = 0;
            const cwRhiFrameRenderer::ClipSpaceCamera camera =
                frameRenderer()->stampCamera(batch, m_rhi, renderData, kLiveCameraSlot,
                                             m_live.target.get(), liveProjection(size),
                                             liveView(), 1.0f, size);

            const cwRHIObject::PerPassRenderData perPassRenderData =
                frameRenderer()->buildPerPassRenderData(renderData);

            cwRHIObject::ResourceUpdateData resourceUpdateData{batch, renderData, &perPassRenderData};

            if (!m_backendInitialized) {
                m_backend->initialize(resourceUpdateData);
                m_backend->updateResources(resourceUpdateData);
                m_backendInitialized = true;
            }

            qint64 remainingUploadBytes = renderData.budgets.uploadBudgetBytesPerFrame;
            m_backend->streamResources(resourceUpdateData, remainingUploadBytes);

            std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiFrameRenderer::kPassCount> passBatches;
            frameRenderer()->gatherScene(passBatches, perPassRenderData);

            m_itemGathered = false;
            for (const auto& batches : passBatches) {
                for (const auto& pipelineBatch : batches) {
                    m_itemGathered = m_itemGathered || !pipelineBatch.drawables.isEmpty();
                }
            }

            const cwRhiPostProcessEffect::FrameUniformContext frameContext{
                camera.projectionCorrected, size, 1.0f
            };

            frameRenderer()->drawScene(cb, m_live.target.get(), nullptr, passBatches,
                                       perPassRenderData, batch, frameContext, 0,
                                       QColor::fromRgbF(0.0, 0.0, 0.0, 0.0));

            m_rhi->endOffscreenFrame();
        }

        //! Frames until the item holds a level, or the timeout runs out
        void renderUntilResident()
        {
            QElapsedTimer timer;
            timer.start();
            renderFrame();
            while (CwRhiTexturedItemsTestAccess::residentTopLevel(*m_backend, m_itemId)
                       == CwRhiTexturedItemsTestAccess::noResidentLevel()
                   && timer.elapsed() < kWaitTimeoutMs) {
                QThread::msleep(kFramePauseMs);
                renderFrame();
            }
        }

    private:
        QRhi* const m_rhi;
        QTemporaryDir m_cacheDirectory;
        RenderTarget m_live;
        cwStreamedTexture m_source;
        cwScene m_scene;
        cwRenderTexturedItems m_render;
        cwRhiItemRenderer m_renderer;
        cwRhiTexturedItems* m_backend = nullptr;
        uint32_t m_itemId = 0;
        bool m_backendInitialized = false;
        bool m_itemGathered = false;
    };

    std::unique_ptr<QRhi> makeRhi()
    {
#ifdef Q_OS_MACOS
        QRhiMetalInitParams initParams;
        return std::unique_ptr<QRhi>(QRhi::create(QRhi::Metal, &initParams));
#else
        return {};
#endif
    }

} // namespace

TEST_CASE("Hiding a view releases its streamed textures and showing it re-streams them",
          "[TexturedItemsStreaming][StreamedEviction]")
{
    std::unique_ptr<QRhi> rhi = makeRhi();
    if (!rhi) {
        SKIP("A real QRhi backend is required to watch GPU texture lifetimes; "
             "this test runs on Metal");
    }

    StreamedItemFixture fixture(rhi.get(), QStringLiteral("streamed-eviction"));
    const uint32_t itemId = fixture.itemId();

    fixture.renderUntilResident();

    const int baseLevel = CwRhiTexturedItemsTestAccess::residentTopLevel(fixture.backend(), itemId);
    REQUIRE(baseLevel == cw::residency::pinnedBaseLevel(fixture.source().size));
    REQUIRE(CwRhiTexturedItemsTestAccess::texturePointer(fixture.backend(), itemId) != nullptr);
    REQUIRE(fixture.itemWasGathered());

    const qint64 residentBytes = streamedTextureLedgerBytes();
    const qint64 chainBytes =
        cw::mip::chainBytes(CwRhiTexturedItemsTestAccess::streamTargetFormat(),
                            fixture.source().size, baseLevel);
    REQUIRE(chainBytes > 0);
    REQUIRE(residentBytes >= chainBytes);

    // The hide: what cwRhiViewer's render job reaches on the render thread.
    fixture.renderer().releaseStreamedTextures();

    CHECK(CwRhiTexturedItemsTestAccess::residentTopLevel(fixture.backend(), itemId)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());
    CHECK(CwRhiTexturedItemsTestAccess::requestedTopLevel(fixture.backend(), itemId)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());
    CHECK_FALSE(CwRhiTexturedItemsTestAccess::demotionInFlight(fixture.backend(), itemId));
    CHECK(CwRhiTexturedItemsTestAccess::texturePointer(fixture.backend(), itemId) == nullptr);
    CHECK(streamedTextureLedgerBytes() == residentBytes - chainBytes);

    // Shown again: the very first frame has to draw the item behind the loading
    // texture — it holds none of its own — and ask for the pinned base again.
    fixture.renderFrame();

    CHECK(CwRhiTexturedItemsTestAccess::srbPointer(fixture.backend(), itemId) != nullptr);
    CHECK(fixture.itemWasGathered());
    CHECK(CwRhiTexturedItemsTestAccess::requestedTopLevel(fixture.backend(), itemId) == baseLevel);

    fixture.renderUntilResident();

    CHECK(CwRhiTexturedItemsTestAccess::residentTopLevel(fixture.backend(), itemId) == baseLevel);
    CHECK(CwRhiTexturedItemsTestAccess::texturePointer(fixture.backend(), itemId) != nullptr);
    CHECK(fixture.itemWasGathered());
    CHECK(streamedTextureLedgerBytes() == residentBytes);
}

TEST_CASE("Releasing streamed textures while a load is in flight leaves the item able to recover",
          "[TexturedItemsStreaming][StreamedEviction]")
{
    std::unique_ptr<QRhi> rhi = makeRhi();
    if (!rhi) {
        SKIP("A real QRhi backend is required to watch GPU texture lifetimes; "
             "this test runs on Metal");
    }

    StreamedItemFixture fixture(rhi.get(), QStringLiteral("streamed-eviction-in-flight"));
    const uint32_t itemId = fixture.itemId();

    const qint64 bytesBefore = streamedTextureLedgerBytes();

    // One frame is all it takes for gather() to ask for the pinned base; the
    // load is still running when the view is hidden out from under it.
    fixture.renderFrame();
    REQUIRE(CwRhiTexturedItemsTestAccess::requestedTopLevel(fixture.backend(), itemId)
            != CwRhiTexturedItemsTestAccess::noResidentLevel());

    fixture.renderer().releaseStreamedTextures();

    CHECK(CwRhiTexturedItemsTestAccess::requestedTopLevel(fixture.backend(), itemId)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());
    CHECK(CwRhiTexturedItemsTestAccess::residentTopLevel(fixture.backend(), itemId)
          == CwRhiTexturedItemsTestAccess::noResidentLevel());
    CHECK_FALSE(CwRhiTexturedItemsTestAccess::demotionInFlight(fixture.backend(), itemId));

    // The load that was in flight lands on an item that has forgotten it, and
    // the request the next frames make is what actually brings the level in.
    fixture.renderUntilResident();

    const int baseLevel = CwRhiTexturedItemsTestAccess::residentTopLevel(fixture.backend(), itemId);
    CHECK(baseLevel == cw::residency::pinnedBaseLevel(fixture.source().size));
    CHECK(CwRhiTexturedItemsTestAccess::texturePointer(fixture.backend(), itemId) != nullptr);
    CHECK(fixture.itemWasGathered());
    CHECK(streamedTextureLedgerBytes() > bytesBefore);
}
