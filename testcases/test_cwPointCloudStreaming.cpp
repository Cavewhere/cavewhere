// Tests for the streamed point cloud renderer: the cut a camera asks for, the
// nodes that stream in behind it, the budgets that bound them, and the release
// a hidden view triggers. Nothing here needs a LAZ file — the octree is built
// straight out of the sampler and written into an ordinary cwDiskCacher, which
// is exactly what the builder leaves behind.

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QColor>
#include <QRay3D>
#include <QtEndian>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QSize>
#include <QTemporaryDir>
#include <QThread>
#include <QVector3D>
#include <QVector4D>

//Qt RHI
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <random>

//Our includes
#include "cwDiskCacher.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwPointOctreeSampler.h"
#include "cwPointOctreeSelection.h"
#include "cwPointOctreeSource.h"
#include "cwRHIObject.h"
#include "cwRHIPointCloud.h"
#include "cwRenderBudgets.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderPointCloud.h"
#include "cwRhiFrameRenderer.h"
#include "cwRhiItemRenderer.h"
#include "cwCamera.h"
#include "cwGeometryItersecter.h"
#include "cwProjection.h"
#include "cwScene.h"
#include "cwScenePick.h"
#include "cwSceneUpdate.h"

#include "CwRhiPointCloudTestAccess.h"

using Access = CwRhiPointCloudTestAccess;
using NodeState = CwRhiPointCloudTestAccess::NodeState;

namespace {

    constexpr int kRootIndex = 0;
    constexpr int kTargetDimension = 256;
    constexpr int kWaitTimeoutMs = 30000;
    constexpr int kFramePauseMs = 2;

    //Enough points, packed tightly enough, that the sampler builds real levels
    constexpr int kPassagePointCount = 30000;
    constexpr quint32 kPassageSeed = 20260912;
    constexpr int kLeafMaxPoints = 2000;

    constexpr double kTubeLength = 120.0;
    constexpr double kTubeRadius = 3.0;
    constexpr double kTubeBend = 25.0;
    constexpr double kTubeWaveAmplitude = 6.0;
    constexpr double kTubeWaveCount = 3.0;
    constexpr double kTwoPi = 6.283185307179586;
    constexpr double kRootPadding = 1.02;

    //An orthographic box this tall makes the root's spacing project under the
    //1.5 px threshold, so the far camera's cut is the root alone
    constexpr float kFarOrthoHeight = 400.0f;
    //...and this one makes it project well over, so the cut refines
    constexpr float kCloseOrthoHeight = 16.0f;
    //Room for one sprite's width when checking where the drawn points landed
    constexpr double kSpriteMarginPx = 4.0;

    constexpr float kOrthoNear = 1.0f;
    constexpr float kOrthoFar = 4000.0f;
    constexpr float kEyeDistance = 800.0f;

    int s_loadWarningCount = 0;
    QtMessageHandler s_previousMessageHandler = nullptr;

    void countLoadWarnings(QtMsgType type, const QMessageLogContext& context,
                           const QString& message)
    {
        if (type == QtWarningMsg && message.contains(QStringLiteral("failed to load"))) {
            s_loadWarningCount++;
        }

        if (s_previousMessageHandler) {
            s_previousMessageHandler(type, context, message);
        }
    }

    //Counts the loader's warnings for as long as it is alive
    class LoadWarningCounter {
    public:
        LoadWarningCounter()
        {
            s_loadWarningCount = 0;
            s_previousMessageHandler = qInstallMessageHandler(&countLoadWarnings);
        }

        ~LoadWarningCounter()
        {
            qInstallMessageHandler(s_previousMessageHandler);
            s_previousMessageHandler = nullptr;
        }

        int count() const { return s_loadWarningCount; }
    };

    //A bent, wavy tube standing in for a cave passage
    QVector<QVector3D> passagePoints(int count, quint32 seed)
    {
        std::mt19937 generator(seed);
        std::uniform_real_distribution<double> unit(0.0, 1.0);

        QVector<QVector3D> points;
        points.reserve(count);

        for (int i = 0; i < count; i++) {
            const double along = unit(generator);
            const double around = unit(generator) * kTwoPi;

            const double x = along * kTubeLength;
            const double y = kTubeBend * along * along;
            const double z = kTubeWaveAmplitude * std::sin(kTwoPi * kTubeWaveCount * along);

            points.append(QVector3D(float(x + kTubeRadius * std::cos(around) * 0.25),
                                    float(y + kTubeRadius * std::cos(around)),
                                    float(z + kTubeRadius * std::sin(around))));
        }

        return points;
    }

    struct OctreeCache {
        cwPointOctreeSource source;
        qint64 largestNodeBytes = 0;
        QVector3D center;
    };

    // Builds the octree of a synthetic passage and writes its nodes into the
    // cacher under @a cacheRoot, exactly where cwRHIPointCloud's loader looks
    // for them. @a corruptNode, when it names a node, gets a payload one point
    // short of what the manifest promises.
    OctreeCache buildOctreeCache(const QString& cacheRoot, const QString& tag,
                                 int corruptNode = -1)
    {
        const QVector<QVector3D> points = passagePoints(kPassagePointCount, kPassageSeed);

        QBox3D box;
        for (const QVector3D& point : points) {
            box.unite(point);
        }

        const QVector3D extent = box.size();
        const double rootSize = std::max({double(extent.x()), double(extent.y()),
                                          double(extent.z()), 1.0}) * kRootPadding;
        const float half = float(rootSize * 0.5);
        const QVector3D rootMin = box.center() - QVector3D(half, half, half);

        const QVector<cw::octree::SampledNode> sampled =
            cw::octree::buildSubtree(points, cw::octree::Cell{0, 0, 0, 0}, rootMin, rootSize,
                                     kLeafMaxPoints);

        // Tests run as concurrent processes, so the pid keeps two of them from
        // sharing a cache entry.
        const QString fingerprint = QStringLiteral("%1-%2")
                                        .arg(tag).arg(QCoreApplication::applicationPid());
        const QString lazPath = QDir(cacheRoot).filePath(
            QStringLiteral("GIS Layers/%1.laz").arg(fingerprint));

        auto manifest = std::make_shared<cwPointOctreeManifest>();
        manifest->rootMin = rootMin;
        manifest->rootSize = rootSize;
        manifest->pointCount = points.size();
        manifest->bboxMin = box.minimum();
        manifest->bboxMax = box.maximum();
        manifest->meanSpacingXY = float(rootSize / cw::octree::kSampleGridResolution);
        manifest->fingerprint = fingerprint;

        manifest->nodes.reserve(sampled.size());
        for (const cw::octree::SampledNode& node : sampled) {
            cwPointOctreeNode entry;
            entry.level = node.cell.level;
            entry.x = node.cell.x;
            entry.y = node.cell.y;
            entry.z = node.cell.z;
            entry.pointCount = quint32(node.points.size());
            entry.byteSize = qint64(node.points.size()) * cw::octree::kBytesPerPoint;
            entry.children = node.children;
            manifest->nodes.append(entry);
        }

        OctreeCache built;
        built.center = box.center();

        cwDiskCacher cacher{QDir(cacheRoot)};
        for (int i = 0; i < manifest->nodes.size(); i++) {
            QByteArray bytes = cw::octree::quantizeAll(sampled.at(i).points,
                                                       manifest->nodeBounds(i));
            built.largestNodeBytes = std::max(built.largestNodeBytes, qint64(bytes.size()));

            if (i == corruptNode) {
                bytes.chop(cw::octree::kBytesPerPoint);
            }

            cacher.insert(cw::octree::nodeKey(lazPath, fingerprint, manifest->nodeName(i)),
                          bytes);
        }

        built.source = cwPointOctreeSource(cacheRoot, lazPath, fingerprint, manifest);
        return built;
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

    QMatrix4x4 orthoProjection(float height)
    {
        const float half = height * 0.5f;
        QMatrix4x4 projection;
        projection.ortho(-half, half, -half, half, kOrthoNear, kOrthoFar);
        return projection;
    }

    QMatrix4x4 viewAt(const QVector3D& center)
    {
        QMatrix4x4 view;
        view.lookAt(center + QVector3D(0.0f, 0.0f, kEyeDistance), center,
                    QVector3D(0.0f, 1.0f, 0.0f));
        return view;
    }

    qint64 ledgerBytes(cwRenderMemoryLedger::Residency residency)
    {
        return cwRenderMemoryLedger::instance()->bytes(
            cwRenderMemoryLedger::Category::PointCloudGeometry, residency);
    }

    qint64 totalGpuBytes()
    {
        return cwRenderMemoryLedger::instance()->totalBytes(
            cwRenderMemoryLedger::Residency::Gpu);
    }

    //! A ray straight down onto @a point from well above everything, so a pick
    //! along it is a pure horizontal-distance question.
    QRay3D rayThrough(const QVector3D& point)
    {
        constexpr float kRayHeight = 10000.0f;
        return QRay3D(QVector3D(point.x(), point.y(), point.z() + kRayHeight),
                      QVector3D(0.0f, 0.0f, -1.0f));
    }

    //! The distance from @a ray to the closest of @a points.
    float nearestDistanceToRay(const QVector<QVector3D>& points, const QRay3D& ray)
    {
        float nearest = std::numeric_limits<float>::max();
        for (const QVector3D& point : points) {
            const QVector3D onRay = ray.point(ray.projectedDistance(point));
            nearest = std::min(nearest, (point - onRay).length());
        }
        return nearest;
    }

    // cwScene hands the back-end its pick set through createRHIObject, so the
    // fixture builds the back-end the same way rather than newing one itself.
    struct RenderCloud : cwRenderPointCloud {
        using cwRenderPointCloud::createRHIObject;
    };

    // One streamed point cloud drawn into an offscreen target, driven through
    // the same call order cwRhiFrameRenderer::renderLiveFrame uses.
    class PointCloudFixture {
    public:
        PointCloudFixture(QRhi* rhi, const QString& tag, int corruptNode = -1) :
            m_rhi(rhi),
            m_live(makeRenderTarget(rhi, QSize(kTargetDimension, kTargetDimension)))
        {
            REQUIRE(m_cacheDirectory.isValid());

            m_cache = buildOctreeCache(m_cacheDirectory.path(), tag, corruptNode);
            REQUIRE(m_cache.source.manifest->nodes.size() > cw::octree::kChildCount);

            m_gpuBaseline = ledgerBytes(cwRenderMemoryLedger::Residency::Gpu);
            m_cpuBaseline = ledgerBytes(cwRenderMemoryLedger::Residency::Cpu);

            m_render.setScene(&m_scene);
            m_render.setOctree(m_cache.source);

            m_backend = static_cast<cwRHIPointCloud*>(m_render.createRHIObject());
            frameRenderer()->registerRenderObject(m_render.renderObjectId(), m_backend);
            synchronize();
        }

        ~PointCloudFixture()
        {
            frameRenderer()->evictPipelinesFor(m_live.renderPassDescriptor.get());
            frameRenderer()->destroyRenderObject(m_render.renderObjectId());
        }

        cwRhiItemRenderer& renderer() { return m_renderer; }
        cwRhiFrameRenderer* frameRenderer() const { return m_renderer.frameRenderer(); }
        const cwRHIPointCloud& backend() const { return *m_backend; }
        cwRHIPointCloud& mutableBackend() { return *m_backend; }
        cwRenderPointCloud& render() { return m_render; }
        cwScene& scene() { return m_scene; }
        const QVector3D& center() const { return m_cache.center; }

        //! A cwCamera on the same ortho view renderFrame() draws with, for the
        //! pick paths — which work off the plain projection, with no RHI clip
        //! correction.
        void configurePickCamera(cwCamera& camera, float orthoHeight) const
        {
            camera.setViewport(QRect(QPoint(0, 0), m_live.target->pixelSize()));

            const float half = orthoHeight * 0.5f;
            cwProjection projection;
            projection.setOrtho(-half, half, -half, half, kOrthoNear, kOrthoFar);
            camera.setProjection(projection);
            camera.setViewMatrix(viewAt(m_cache.center));
        }
        const OctreeCache& cache() const { return m_cache; }
        const cwPointOctreeManifest& manifest() const { return *m_cache.source.manifest; }

        int drawableCount() const { return m_drawableCount; }
        quint64 frameCounter() const { return m_renderer.frameRenderer()->frameCounter(); }

        qint64 gpuBytes() const
        {
            return ledgerBytes(cwRenderMemoryLedger::Residency::Gpu) - m_gpuBaseline;
        }

        qint64 cpuBytes() const
        {
            return ledgerBytes(cwRenderMemoryLedger::Residency::Cpu) - m_cpuBaseline;
        }

        void synchronize() { m_backend->synchronize({&m_render, &m_renderer}); }

        void setOrthoHeight(float height) { m_orthoHeight = height; }

        void setBudgets(const cwRenderBudgets& budgets)
        {
            frameRenderer()->setBudgets(budgets);
        }

        cwRenderBudgets budgets() const { return frameRenderer()->budgets(); }

        cwRHIObject::RenderData jobRenderData(float orthoHeight)
        {
            cwRHIObject::RenderData data;
            const QSize size = m_live.target->pixelSize();
            const cwRhiFrameRenderer::ClipSpaceCamera camera =
                cwRhiFrameRenderer::clipSpaceCorrectedCamera(m_rhi, orthoProjection(orthoHeight),
                                                             viewAt(m_cache.center));
            data.renderer = &m_renderer;
            data.projectionMatrix = camera.projectionCorrected;
            data.viewProjectionMatrix = camera.viewProjection;
            data.viewportSize = size;
            data.budgets = budgets();
            return data;
        }

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
                                             m_live.target.get(), orthoProjection(m_orthoHeight),
                                             viewAt(m_cache.center), 1.0f, size);

            const cwRHIObject::PerPassRenderData perPassRenderData =
                frameRenderer()->buildPerPassRenderData(renderData);

            cwRHIObject::ResourceUpdateData resourceUpdateData{batch, renderData,
                                                               &perPassRenderData};

            if (!m_backendInitialized) {
                m_backend->initialize(resourceUpdateData);
                m_backendInitialized = true;
            }
            m_backend->updateResources(resourceUpdateData);

            qint64 remainingUploadBytes = renderData.budgets.uploadBudgetBytesPerFrame;
            m_backend->streamResources(resourceUpdateData, remainingUploadBytes);

            std::array<QVector<cwRHIObject::PipelineBatch>,
                       cwRhiFrameRenderer::kPassCount> passBatches;
            frameRenderer()->gatherScene(passBatches, perPassRenderData);

            m_drawableCount = 0;
            for (const auto& batches : passBatches) {
                for (const auto& pipelineBatch : batches) {
                    m_drawableCount += int(pipelineBatch.drawables.size());
                }
            }

            const cwRhiPostProcessEffect::FrameUniformContext frameContext{
                camera.projectionCorrected, size, 1.0f
            };

            frameRenderer()->drawScene(cb, m_live.target.get(), nullptr, passBatches,
                                       perPassRenderData, batch, frameContext, 0,
                                       QColor::fromRgbF(0.0, 0.0, 0.0, 0.0));

            QRhiReadbackResult readback;
            if (m_readbackEnabled) {
                QRhiResourceUpdateBatch* readbackBatch = m_rhi->nextResourceUpdateBatch();
                readbackBatch->readBackTexture(QRhiReadbackDescription(m_live.color.get()),
                                               &readback);
                cb->resourceUpdate(readbackBatch);
            }

            m_rhi->endOffscreenFrame();

            if (m_readbackEnabled) {
                //An offscreen frame is complete when endOffscreenFrame returns
                m_colorPixels = readback.data;
                m_colorSize = readback.pixelSize;
            }
        }

        void setReadbackEnabled(bool enabled) { m_readbackEnabled = enabled; }

        //! Half the width and height the cloud's data bounds cover in NDC
        //! through the same camera renderFrame() draws with.
        QPointF dataBoundsInNdc(float orthoHeight) const
        {
            const cwRhiFrameRenderer::ClipSpaceCamera camera =
                cwRhiFrameRenderer::clipSpaceCorrectedCamera(m_rhi, orthoProjection(orthoHeight),
                                                             viewAt(m_cache.center));
            const QVector3D minimum = m_cache.source.manifest->bboxMin;
            const QVector3D maximum = m_cache.source.manifest->bboxMax;

            constexpr int kCornerCount = 8;
            QPointF bounds;
            for (int corner = 0; corner < kCornerCount; corner++) {
                const QVector3D point((corner & 1) ? maximum.x() : minimum.x(),
                                      (corner & 2) ? maximum.y() : minimum.y(),
                                      (corner & 4) ? maximum.z() : minimum.z());
                const QVector4D clip = camera.viewProjection * QVector4D(point, 1.0f);
                bounds.setX(std::max(bounds.x(), std::abs(double(clip.x() / clip.w()))));
                bounds.setY(std::max(bounds.y(), std::abs(double(clip.y() / clip.w()))));
            }
            return bounds;
        }

        //! The pixels of the last frame that are not the clear color, in the
        //! [-1, 1] square of the target, x and y folded to their magnitudes so
        //! the check does not depend on which way the backend flips y.
        QVector<QPointF> litPixelsInNdc() const
        {
            QVector<QPointF> lit;
            if (m_colorSize.isEmpty()) {
                return lit;
            }

            constexpr int kChannelsPerPixel = 4;
            const auto* pixels = reinterpret_cast<const uchar*>(m_colorPixels.constData());
            for (int y = 0; y < m_colorSize.height(); y++) {
                for (int x = 0; x < m_colorSize.width(); x++) {
                    const int offset = (y * m_colorSize.width() + x) * kChannelsPerPixel;
                    const bool black = pixels[offset] == 0 && pixels[offset + 1] == 0
                                       && pixels[offset + 2] == 0;
                    if (black) {
                        continue;
                    }

                    lit.append(QPointF(
                        std::abs((x + 0.5) / m_colorSize.width() * 2.0 - 1.0),
                        std::abs((y + 0.5) / m_colorSize.height() * 2.0 - 1.0)));
                }
            }
            return lit;
        }

        //! Frames until nothing is queued, in flight or waiting to be uploaded
        void renderUntilQuiet()
        {
            QElapsedTimer timer;
            timer.start();
            renderFrame();
            while (Access::hasStreamingWork(*m_backend) && timer.elapsed() < kWaitTimeoutMs) {
                QThread::msleep(kFramePauseMs);
                renderFrame();
            }
            //One more, so anything uploaded by the last drain gets gathered
            renderFrame();
        }

        //! Frames until @a index is resident, or the timeout runs out
        void renderUntilResident(int index)
        {
            QElapsedTimer timer;
            timer.start();
            renderFrame();
            while (Access::nodeState(*m_backend, index) != NodeState::Resident
                   && timer.elapsed() < kWaitTimeoutMs) {
                QThread::msleep(kFramePauseMs);
                renderFrame();
            }
        }

    private:
        QRhi* const m_rhi;
        QTemporaryDir m_cacheDirectory;
        RenderTarget m_live;
        OctreeCache m_cache;
        cwScene m_scene;
        RenderCloud m_render;
        mutable cwRhiItemRenderer m_renderer;
        cwRHIPointCloud* m_backend = nullptr;
        bool m_backendInitialized = false;
        int m_drawableCount = 0;
        float m_orthoHeight = kFarOrthoHeight;
        bool m_readbackEnabled = false;
        QByteArray m_colorPixels;
        QSize m_colorSize;
        qint64 m_gpuBaseline = 0;
        qint64 m_cpuBaseline = 0;
    };

    //! Every point the cloud has resident, dequantized out of the very mirrors
    //! the pick set was published — the points a pick can reach.
    QVector<QVector3D> residentPoints(const PointCloudFixture& fixture)
    {
        QVector<QVector3D> points;
        for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
            if (Access::nodeState(fixture.backend(), i) != NodeState::Resident) {
                continue;
            }

            const QByteArray bytes = Access::nodeBytes(fixture.backend(), i);
            const QBox3D bounds = fixture.manifest().nodeBounds(i);
            for (qsizetype offset = 0; offset + cw::octree::kBytesPerPoint <= bytes.size();
                 offset += cw::octree::kBytesPerPoint) {
                const char* axes = bytes.constData() + offset;
                constexpr int kAxisBytes = int(sizeof(quint16));
                const cw::octree::QuantizedPoint quantized{
                    qFromLittleEndian<quint16>(axes),
                    qFromLittleEndian<quint16>(axes + kAxisBytes),
                    qFromLittleEndian<quint16>(axes + 2 * kAxisBytes),
                    0};
                points.append(cw::octree::dequantize(quantized, bounds));
            }
        }
        return points;
    }

    // Skips the calling test when the platform has no backend to run it on.
    std::unique_ptr<QRhi> makeRhiOrSkip()
    {
#ifdef Q_OS_MACOS
        QRhiMetalInitParams initParams;
        std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Metal, &initParams));
        if (rhi) {
            return rhi;
        }
#endif
        SKIP("A real QRhi backend is required to watch GPU buffer lifetimes; "
             "this test runs on Metal");
        return {};
    }

} // namespace

TEST_CASE("A point cloud's first frame asks for the root alone and draws nothing",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("first-frame"));
    fixture.setOrthoHeight(kFarOrthoHeight);

    fixture.renderFrame();

    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Requested);
    CHECK(fixture.drawableCount() == 0);

    // Nothing but the root: the far camera's cut is one node, and no other node
    // may be asked for before it.
    for (int i = 1; i < Access::nodeCount(fixture.backend()); i++) {
        CHECK(Access::nodeState(fixture.backend(), i) == NodeState::Absent);
    }

    // The pass has to run from the frame the source arrives, or the root would
    // never be composited.
    CHECK(fixture.backend().usesPointCloudPass());

    fixture.renderUntilResident(kRootIndex);
    fixture.renderFrame();

    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
    CHECK(fixture.drawableCount() == 1);
    CHECK(fixture.gpuBytes() == fixture.manifest().nodes.at(kRootIndex).byteSize);
}

TEST_CASE("A closer camera streams the children in under the frame's upload budget",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("upload-budget"));

    constexpr qint64 kUploadBudgetBytes = 32 * 1024;
    cwRenderBudgets budgets;
    budgets.uploadBudgetBytesPerFrame = kUploadBudgetBytes;
    fixture.setBudgets(budgets);
    fixture.setOrthoHeight(kCloseOrthoHeight);

    // Never more than the budget plus the one node takeFromBudget always lets
    // through, so a node larger than the whole budget still makes progress.
    const qint64 mostPerFrame = kUploadBudgetBytes + fixture.cache().largestNodeBytes;

    QElapsedTimer timer;
    timer.start();
    qint64 previous = fixture.gpuBytes();
    do {
        fixture.renderFrame();
        const qint64 current = fixture.gpuBytes();
        CHECK(current - previous <= mostPerFrame);
        previous = current;
        QThread::msleep(kFramePauseMs);
    } while (Access::hasStreamingWork(fixture.backend()) && timer.elapsed() < kWaitTimeoutMs);
    fixture.renderFrame();

    CHECK(Access::residentCount(fixture.backend()) > 1);
    CHECK(fixture.drawableCount() > 1);
}

TEST_CASE("Zooming out cancels the nodes the cut dropped", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("zoom-out"));
    fixture.setOrthoHeight(kCloseOrthoHeight);

    // A CPU cap of one byte lets a single load through at a time, so the rest
    // of the cut is still queued when the camera pulls back.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.cpuBudgetBytes = 1;
    fixture.setBudgets(budgets);

    fixture.renderFrame();

    int requested = 0;
    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        if (Access::nodeState(fixture.backend(), i) == NodeState::Requested) {
            requested++;
        }
    }
    REQUIRE(requested > 1);

    // The far camera wants the root alone; everything else has to stop being
    // asked for rather than landing on a view that no longer wants it.
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderFrame();

    for (int i = 1; i < Access::nodeCount(fixture.backend()); i++) {
        CHECK(Access::nodeState(fixture.backend(), i) != NodeState::Requested);
    }

    // The streamer has to forget them too, not just the node table: drain it
    // and watch that nothing but the root ever lands.
    QElapsedTimer timer;
    timer.start();
    while (Access::hasStreamingWork(fixture.backend()) && timer.elapsed() < kWaitTimeoutMs) {
        QThread::msleep(kFramePauseMs);
        fixture.renderFrame();
        CHECK(Access::residentCount(fixture.backend()) <= 1);
    }

    CHECK_FALSE(Access::hasStreamingWork(fixture.backend()));
    CHECK(Access::residentCount(fixture.backend()) <= 1);
}

TEST_CASE("A GPU budget under the resident total evicts the coldest node and never the root",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("gpu-evict"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    REQUIRE(Access::residentCount(fixture.backend()) > 2);
    REQUIRE(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);

    // Two resident non-root nodes, aged apart, with the root alone in this
    // frame's cut. The planner must take the older of the two and leave the
    // root alone whatever the overshoot.
    QVector<int> resident;
    for (int i = 1; i < Access::nodeCount(fixture.backend()); i++) {
        if (Access::nodeState(fixture.backend(), i) == NodeState::Resident) {
            resident.append(i);
        }
    }
    REQUIRE(resident.size() >= 2);

    // Ages, not frame numbers: the coldest node has never been wanted, the rest
    // fell out of the cut only last frame, and the root is still in it.
    const quint64 frame = fixture.frameCounter();
    REQUIRE(frame > 1);
    Access::setLastDesiredFrame(fixture.mutableBackend(), kRootIndex, frame);
    Access::setLastDesiredFrame(fixture.mutableBackend(), resident.at(0), 0);
    for (int i = 1; i < resident.size(); i++) {
        Access::setLastDesiredFrame(fixture.mutableBackend(), resident.at(i), frame - 1);
    }

    const qint64 coldestBytes = Access::mirrorBytes(fixture.backend(), resident.at(0));
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = totalGpuBytes() - 1;
    fixture.setBudgets(budgets);
    fixture.setOrthoHeight(kFarOrthoHeight);

    const qint64 before = fixture.gpuBytes();
    fixture.renderFrame();

    CHECK(Access::nodeState(fixture.backend(), resident.at(0)) == NodeState::Absent);
    CHECK(Access::bufferPointer(fixture.backend(), resident.at(0)) == nullptr);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
    CHECK(fixture.gpuBytes() == before - coldestBytes);
}

TEST_CASE("A budget nothing can satisfy coarsens the cut, and room to spare relaxes it",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("sse-inflation"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const int residentAtRest = Access::residentCount(fixture.backend());
    REQUIRE(residentAtRest > 2);
    REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);

    // A budget the root alone cannot fit, with the camera pulled back so the
    // cut stops asking for children: everything evictable goes, and the view is
    // still over budget with nothing left to give.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = 1;
    fixture.setBudgets(budgets);
    fixture.setOrthoHeight(kFarOrthoHeight);

    fixture.renderFrame();
    fixture.renderFrame();

    CHECK(Access::residentCount(fixture.backend()) == 1);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
    CHECK(Access::sseInflation(fixture.backend()) > 1.0);

    // Room to spare again: the cut is allowed to get finer, one step per frame.
    const double coarsened = Access::sseInflation(fixture.backend());
    budgets.gpuBudgetBytes = cw::budgets::kDefaultGpuBudgetBytes;
    fixture.setBudgets(budgets);

    fixture.renderFrame();

    CHECK(Access::sseInflation(fixture.backend()) < coarsened);
}

TEST_CASE("A node whose payload does not match the manifest fails and the rest still draw",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    // The first child of the root, so the cloud has plenty left to draw.
    PointCloudFixture fixture(rhi.get(), QStringLiteral("bad-node"), 1);
    fixture.setOrthoHeight(kCloseOrthoHeight);

    LoadWarningCounter warnings;
    fixture.renderUntilQuiet();

    // Once, when it fails — not once a frame for as long as the cut wants it.
    CHECK(warnings.count() == 1);
    CHECK(Access::nodeState(fixture.backend(), 1) == NodeState::Failed);
    CHECK(Access::bufferPointer(fixture.backend(), 1) == nullptr);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
    CHECK(fixture.drawableCount() > 1);

    // A failed node is never asked for again, however many frames go by.
    fixture.renderFrame();
    CHECK(Access::nodeState(fixture.backend(), 1) == NodeState::Failed);
    CHECK(warnings.count() == 1);
}

TEST_CASE("An export waits for its own cut to be resident", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("residency-ready"));
    const cwRHIObject::RenderData job = fixture.jobRenderData(kFarOrthoHeight);

    // Nothing is resident yet, and asking is what puts the job's cut in flight.
    CHECK_FALSE(fixture.mutableBackend().residencyReady(job));
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Requested);

    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilResident(kRootIndex);

    CHECK(fixture.mutableBackend().residencyReady(job));
}

TEST_CASE("Hiding a view releases every node and the next frame starts from the root",
          "[PointCloudStreaming][StreamedEviction]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("release"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    REQUIRE(Access::residentCount(fixture.backend()) > 1);
    REQUIRE(fixture.gpuBytes() > 0);

    // The hide: what cwRhiViewer's render job reaches on the render thread. It
    // must come back without waiting on the disk reads in flight.
    fixture.renderer().releaseStreamedResources();

    CHECK(Access::residentCount(fixture.backend()) == 0);
    CHECK(fixture.gpuBytes() == 0);
    CHECK(Access::bufferPointer(fixture.backend(), kRootIndex) == nullptr);
    CHECK(Access::sseInflation(fixture.backend()) == 1.0);

    // Shown again: the very first frame asks for the root, and the results the
    // release left in flight land on a table that has forgotten them.
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderFrame();
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Requested);

    fixture.renderUntilResident(kRootIndex);
    fixture.renderFrame();

    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
    CHECK(fixture.drawableCount() == 1);
}

TEST_CASE("Releasing while a load is in flight leaves the cloud able to recover",
          "[PointCloudStreaming][StreamedEviction]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("release-in-flight"));
    fixture.setOrthoHeight(kCloseOrthoHeight);

    // One frame is all it takes for gather() to ask; the loads are still
    // running when the view is hidden out from under them.
    fixture.renderFrame();
    fixture.renderer().releaseStreamedResources();

    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        CHECK(Access::nodeState(fixture.backend(), i) != NodeState::Requested);
    }
    CHECK(fixture.gpuBytes() == 0);

    // The far camera wants the root alone, so every load the release left in
    // flight lands on a node this cut never asked for and is thrown away.
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilQuiet();

    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
    CHECK(Access::residentCount(fixture.backend()) == 1);
    CHECK(fixture.drawableCount() == 1);
}

TEST_CASE("Re-publishing the same octree keeps residency and a new fingerprint drops it",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("republish"));
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilResident(kRootIndex);

    const void* rootBuffer = Access::bufferPointer(fixture.backend(), kRootIndex);
    REQUIRE(rootBuffer != nullptr);

    // The same octree published again — a fresh manifest instance, same
    // identity — must not cost the cloud a single node.
    cwPointOctreeSource same = fixture.cache().source;
    same.manifest = std::make_shared<cwPointOctreeManifest>(*fixture.cache().source.manifest);
    fixture.render().setOctree(same);
    fixture.synchronize();

    CHECK(Access::bufferPointer(fixture.backend(), kRootIndex) == rootBuffer);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);

    // A rebuilt LAZ carries a new fingerprint, and that is a different octree.
    cwPointOctreeSource rebuilt = fixture.cache().source;
    rebuilt.fingerprint = QStringLiteral("rebuilt");
    fixture.render().setOctree(rebuilt);
    fixture.synchronize();

    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Absent);
    CHECK(Access::bufferPointer(fixture.backend(), kRootIndex) == nullptr);
    CHECK(fixture.gpuBytes() == 0);
}

TEST_CASE("The CPU ledger holds exactly the resident nodes' pick mirrors",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("pick-mirror"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    REQUIRE(Access::residentCount(fixture.backend()) > 1);
    REQUIRE_FALSE(Access::hasStreamingWork(fixture.backend()));

    qint64 mirrored = 0;
    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        if (Access::nodeState(fixture.backend(), i) == NodeState::Resident) {
            mirrored += Access::mirrorBytes(fixture.backend(), i);
        }
    }

    // The mirror is an implicit share of the very bytes that were uploaded, so
    // the two ledgers agree once nothing is in flight.
    CHECK(fixture.cpuBytes() == mirrored);
    CHECK(fixture.gpuBytes() == mirrored);
}

TEST_CASE("A budget below the cut coarsens it rather than evicting what it draws",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("budget-churn"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const int residentAtRest = Access::residentCount(fixture.backend());
    REQUIRE(residentAtRest > 2);
    REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);

    // The camera stays put, so everything resident is something this frame's
    // cut wants. Evicting one of those would only re-request and re-upload it
    // next frame — the same node read off disk every frame forever.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = totalGpuBytes() - 1;
    fixture.setBudgets(budgets);

    fixture.renderFrame();

    CHECK(Access::residentCount(fixture.backend()) == residentAtRest);
    CHECK(Access::sseInflation(fixture.backend()) > 1.0);

    // The coarser cut is what frees the memory: the nodes it drops leave the
    // selection and the ordinary eviction path takes them. Nothing grows back.
    constexpr int kMostCoarseningFrames = 12;
    int resident = residentAtRest;
    for (int i = 0; i < kMostCoarseningFrames && resident >= residentAtRest; i++) {
        fixture.renderFrame();
        resident = Access::residentCount(fixture.backend());
        CHECK(resident <= residentAtRest);
    }

    CHECK(resident < residentAtRest);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
}

TEST_CASE("An export whose camera differs from the live view still becomes ready",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("export-cut"));

    // The live view sits far back, where its cut is the root alone, while the
    // job wants the close cut. Every node the job asks for is one the live
    // gather would otherwise cancel out from under it.
    fixture.setOrthoHeight(kFarOrthoHeight);
    const cwRHIObject::RenderData job = fixture.jobRenderData(kCloseOrthoHeight);

    QElapsedTimer timer;
    timer.start();
    bool ready = fixture.mutableBackend().residencyReady(job);
    while (!ready && timer.elapsed() < kWaitTimeoutMs) {
        QVector<NodeState> before;
        before.reserve(Access::nodeCount(fixture.backend()));
        for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
            before.append(Access::nodeState(fixture.backend(), i));
        }

        fixture.renderFrame();

        // A load the job is waiting on must survive the live frame: dropped
        // back to Absent it starts over, and on a slow disk it never lands.
        for (int i = 0; i < before.size(); i++) {
            const bool dropped = before.at(i) == NodeState::Requested
                                 && Access::nodeState(fixture.backend(), i) == NodeState::Absent;
            CHECK_FALSE(dropped);
        }

        QThread::msleep(kFramePauseMs);
        ready = fixture.mutableBackend().residencyReady(job);
    }

    CHECK(ready);
    CHECK(Access::residentCount(fixture.backend()) > 1);

    // Once a node lands, the live cut is free to let it go again.
    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        CHECK_FALSE(Access::exportRequested(fixture.backend(), i));
    }
}

TEST_CASE("Payloads the upload budget holds back stay bounded", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("ready-queue"));
    fixture.setOrthoHeight(kCloseOrthoHeight);

    constexpr qint64 kUploadBudgetBytes = 32 * 1024;
    cwRenderBudgets budgets = fixture.budgets();
    budgets.uploadBudgetBytesPerFrame = kUploadBudgetBytes;
    budgets.cpuBudgetBytes = 2 * fixture.cache().largestNodeBytes;
    fixture.setBudgets(budgets);

    // A payload is off every ledger between the streamer handing it over and
    // the upload taking it, so what waits must stay inside what the CPU cap
    // would have allowed: the cap, plus the one payload the streamer always
    // lets through.
    const qint64 mostQueued = budgets.cpuBudgetBytes + fixture.cache().largestNodeBytes;

    QElapsedTimer timer;
    timer.start();
    do {
        fixture.renderFrame();
        CHECK(Access::readyQueueBytes(fixture.backend()) <= mostQueued);
        QThread::msleep(kFramePauseMs);
    } while (Access::hasStreamingWork(fixture.backend()) && timer.elapsed() < kWaitTimeoutMs);

    CHECK(Access::residentCount(fixture.backend()) > 1);
    CHECK(Access::readyQueueBytes(fixture.backend()) == 0);
}

TEST_CASE("The drawn points land inside the cloud the manifest describes",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("readback"));
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.setReadbackEnabled(true);
    fixture.renderUntilResident(kRootIndex);
    fixture.renderFrame();

    REQUIRE(fixture.drawableCount() == 1);

    // uint16 positions, the node's origin and step in a per-instance vertex
    // attribute: get the layout or the dequantization wrong and the root's
    // points land somewhere other than where the manifest puts the cloud —
    // usually right off the target.
    const QVector<QPointF> lit = fixture.litPixelsInNdc();
    CHECK_FALSE(lit.isEmpty());

    QPointF drawn;
    for (const QPointF& pixel : lit) {
        drawn.setX(std::max(drawn.x(), pixel.x()));
        drawn.setY(std::max(drawn.y(), pixel.y()));
    }

    const QPointF bounds = fixture.dataBoundsInNdc(kFarOrthoHeight);
    const double margin = kSpriteMarginPx * 2.0 / kTargetDimension;
    CHECK(drawn.x() <= bounds.x() + margin);
    CHECK(drawn.y() <= bounds.y() + margin);
}

TEST_CASE("The cloud's world bounds are the root cube padded by the sprite radius",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("world-bounds"));

    const std::optional<QBox3D> bounds = fixture.backend().worldBounds();
    REQUIRE(bounds.has_value());

    const QBox3D root = fixture.manifest().nodeBounds(kRootIndex);
    const float radius = fixture.render().worldRadius();
    const QVector3D padding(radius, radius, radius);

    CHECK(bounds->minimum() == root.minimum() - padding);
    CHECK(bounds->maximum() == root.maximum() + padding);

    // Cleared, the cloud has nothing to draw and nothing to composite.
    fixture.render().clear();
    fixture.synchronize();

    CHECK_FALSE(fixture.backend().worldBounds().has_value());
    CHECK_FALSE(fixture.backend().usesPointCloudPass());
    CHECK(Access::residentCount(fixture.backend()) == 0);
}

TEST_CASE("A streamed point cloud is picked through the nodes it has resident",
          "[PointCloudStreaming][PointOctreePick]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("pick"));
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilResident(kRootIndex);

    cwGeometryItersecter* intersecter = fixture.scene().geometryItersecter();

    // The cloud registers a pick provider, so it is pickable with no BVH built
    // and no pick gate to wait on.
    const cwGeometryItersecter::Key key{fixture.render().renderObjectId(), 0};
    REQUIRE(intersecter->isObjectPickReady(key));
    REQUIRE_FALSE(intersecter->visibleBoundingBox().isNull());

    // Aim at a point of the source cloud (the generator is deterministic, so
    // this is one of the points the octree was built from). The root's own
    // sampling keeps one point per grid cell, and the pick radius is that cell
    // size — watertight — so the ray lands on whatever the root kept nearby.
    cwCamera farCamera;
    fixture.configurePickCamera(farCamera, kFarOrthoHeight);
    const QVector3D aimedAt = passagePoints(kPassagePointCount, kPassageSeed).constFirst();
    const QPointF screenPoint = farCamera.project(aimedAt);

    constexpr double kLinePixelRadius = 6.0;
    const cwScenePick::Result rootOnly = cwScenePick::snappedPoint(
        screenPoint, farCamera, *intersecter, kLinePixelRadius);
    REQUIRE(rootOnly.hit);
    CHECK_FALSE(rootOnly.snappedToStation);

    const QBox3D bounds = intersecter->boundingBox(key);
    CHECK(bounds.contains(rootOnly.world));

    // Refine: the children stream in and the pick now lands on the finer
    // sampling, still inside the cloud.
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();
    const int refinedCount = Access::residentCount(fixture.backend());
    REQUIRE(refinedCount > 1);

    cwCamera closeCamera;
    fixture.configurePickCamera(closeCamera, kCloseOrthoHeight);
    const cwScenePick::Result refined = cwScenePick::snappedPoint(
        closeCamera.project(aimedAt), closeCamera, *intersecter, kLinePixelRadius);
    CHECK(refined.hit);

    // Evict the children: a GPU budget of nothing takes every node the current
    // cut does not want, and the root is pinned so it survives. The pick has to
    // keep resolving against it — that is the presence invariant.
    fixture.setOrthoHeight(kFarOrthoHeight);
    cwRenderBudgets starved;
    starved.gpuBudgetBytes = 0;
    fixture.setBudgets(starved);
    fixture.renderFrame();
    fixture.renderFrame();

    CHECK(Access::residentCount(fixture.backend()) < refinedCount);
    REQUIRE(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);

    const cwScenePick::Result afterEviction = cwScenePick::snappedPoint(
        screenPoint, farCamera, *intersecter, kLinePixelRadius);
    CHECK(afterEviction.hit);
    CHECK(bounds.contains(afterEviction.world));
}

TEST_CASE("The intersecter frames a streamed cloud before a node has landed",
          "[PointCloudStreaming][PointOctreePick]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    // No frame has run, so nothing is resident — but the root bounds published
    // by the source change are what a reset view frames.
    PointCloudFixture fixture(rhi.get(), QStringLiteral("framing"));
    REQUIRE(Access::residentCount(fixture.backend()) == 0);

    cwGeometryItersecter* intersecter = fixture.scene().geometryItersecter();
    const cwGeometryItersecter::Key key{fixture.render().renderObjectId(), 0};

    const QBox3D rootBounds = fixture.manifest().nodeBounds(kRootIndex);
    CHECK(intersecter->boundingBox(key) == rootBounds);
    CHECK(intersecter->visibleBoundingBox() == rootBounds);

    // Framing it is not the same as picking it: there are no points yet.
    CHECK_FALSE(intersecter->intersectsDetailed(rayThrough(rootBounds.center())).hit());
}

TEST_CASE("A pick reaches exactly the cloud's mean point spacing",
          "[PointCloudStreaming][PointOctreePick]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("radius"));
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilResident(kRootIndex);

    cwGeometryItersecter* intersecter = fixture.scene().geometryItersecter();
    const QVector<QVector3D> resident = residentPoints(fixture);
    REQUIRE_FALSE(resident.isEmpty());

    const float radius = fixture.manifest().meanSpacingXY
                         * cwRenderPointCloud::PointPickRadiusScale;
    REQUIRE(radius > 0.0f);

    // Dead on a resident point: zero off the ray, so it is a hit at any radius.
    const QVector3D target = resident.constFirst();
    REQUIRE(intersecter->intersectsDetailed(rayThrough(target)).hit());

    // Now step sideways until no resident point is within the radius of the
    // ray any more. That ray must miss, and the step it took to get there
    // pins the radius: a wider one would still reach the point just left
    // behind.
    constexpr float kStepFraction = 0.1f;
    const float step = radius * kStepFraction;
    const float limit = fixture.manifest().nodeBounds(kRootIndex).size().x();
    float offset = 0.0f;
    while (offset < limit
           && nearestDistanceToRay(resident, rayThrough(target + QVector3D(offset, 0.0f, 0.0f)))
              <= radius) {
        offset += step;
    }
    REQUIRE(offset < limit);
    REQUIRE(offset > 0.0f);

    CHECK(intersecter->intersectsDetailed(
              rayThrough(target + QVector3D(offset - step, 0.0f, 0.0f))).hit());
    CHECK_FALSE(intersecter->intersectsDetailed(
                    rayThrough(target + QVector3D(offset, 0.0f, 0.0f))).hit());
}

TEST_CASE("Releasing a view's streamed resources empties what picks see",
          "[PointCloudStreaming][PointOctreePick]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("release"));
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilResident(kRootIndex);

    cwGeometryItersecter* intersecter = fixture.scene().geometryItersecter();
    const QVector<QVector3D> resident = residentPoints(fixture);
    REQUIRE_FALSE(resident.isEmpty());

    const QRay3D ray = rayThrough(resident.constFirst());
    REQUIRE(intersecter->intersectsDetailed(ray).hit());

    // A hidden view drops its nodes, and with them the mirrors the pick set
    // was holding a share of — so nothing is left to pick.
    fixture.mutableBackend().releaseStreamedResources();
    REQUIRE(Access::residentCount(fixture.backend()) == 0);
    CHECK(Access::mirrorBytes(fixture.backend(), kRootIndex) == 0);
    CHECK_FALSE(intersecter->intersectsDetailed(ray).hit());

    // The cloud is still there to frame, it just has nothing resident.
    CHECK(intersecter->visibleBoundingBox() == fixture.manifest().nodeBounds(kRootIndex));
}
