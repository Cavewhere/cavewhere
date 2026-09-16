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
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QtMath>
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
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <vector>

//Our includes
#include "cwAppearanceOverride.h"
#include "cwDiskCacher.h"
#include "cwPointCloudAppearance.h"
#include "cwPointOctree.h"
#include "cwPointOctreeManifest.h"
#include "cwProfileLog.h"
#include "cwPointOctreeBuilder.h"
#include "cwPointOctreeSampler.h"
#include "cwPointOctreeSelection.h"
#include "cwPointOctreeSource.h"
#include "cwRHIObject.h"
#include "cwRHIPointCloud.h"
#include "cwRenderBudgets.h"
#include "cwRenderFrameStats.h"
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
#include "cwSceneVisibility.h"

#include "CwRhiPointCloudTestAccess.h"
#include "ProfileLogCapture.h"

using Access = CwRhiPointCloudTestAccess;
using NodeState = CwRhiPointCloudTestAccess::NodeState;

namespace {

    //! The value of @a key in one profile line, or -1 where the line has none
    qint64 fieldOf(const QString& line, const QString& key)
    {
        const QString prefix = key + QLatin1Char('=');
        const QStringList fields = line.split(QLatin1Char(' '));
        for (const QString& field : fields) {
            if (field.startsWith(prefix)) {
                return field.mid(prefix.size()).toLongLong();
            }
        }
        return -1;
    }

    constexpr int kRootIndex = 0;
    constexpr int kRootLevel = 0;
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
    //Small enough that the camera holds one stretch of the passage at a time
    constexpr float kChurnOrthoHeight = 20.0f;
    constexpr double kChurnNearAlong = 0.25;
    constexpr double kChurnFarAlong = 0.75;
    //Room for one sprite's width when checking where the drawn points landed
    constexpr double kSpriteMarginPx = 4.0;

    //Zoomed in enough that the root's sample spacing is several pixels wide,
    //which is where a sprite narrower than the spacing shows holes. The cut is
    //held at the root by raising the screen-space error to its ceiling.
    constexpr float kCoarseOrthoHeight = 48.0f;
    //The widest run of unlit pixels a covered surface may show
    constexpr int kMaxUnlitRunPx = 2;
    //A sprite sized off the spacing has to light far more than a 1 px one
    constexpr int kCoveredLitMultiple = 3;
    //The shader's lower clamp on gl_PointSize
    constexpr double kMinSpritePx = 1.0;
    //A coverage the wheel can double while both sizes stay clear of the
    //shader's one-pixel floor, so the growth is the rule's and not the clamp's
    constexpr float kWheelCoverage = 2.0f;
    //Rasterization rounds a sprite to whole pixels and the halo is read off a
    //pixel grid, so every sprite bound carries a pixel of slack
    constexpr double kSpriteTolerancePx = 1.0;
    //Some backends light more than the gl_PointSize they are handed — Metal
    //rasterizes a sprite 1.25 sides wide, measured against the shader's 64 px
    //ceiling — so every upper bound on a sprite allows for it
    constexpr double kSpriteRasterFactor = 1.25;
    //One byte, so cw::residency::takeFromBudget lets exactly the one node it
    //always allows through and the cut refines a node at a time
    constexpr qint64 kOneNodePerFrameUploadBytes = 1;

    //Far enough to the side that the frame's frustum misses the root cube
    constexpr float kLookAwayDistance = 5000.0f;

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
                                 const QVector<QVector3D>& points, int corruptNode = -1)
    {
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

    OctreeCache buildOctreeCache(const QString& cacheRoot, const QString& tag,
                                 int corruptNode = -1)
    {
        return buildOctreeCache(cacheRoot, tag,
                                passagePoints(kPassagePointCount, kPassageSeed), corruptNode);
    }

    //! A regular grid in the z = 0 plane, @a side meters across and @a step
    //! meters apart. Every level of the sampler's grid subsample of a plane is
    //! again a plane, so the cut's spacing is the whole story about the gaps on
    //! screen and any unlit pixel inside the square is a hole.
    QVector<QVector3D> planePoints(double side, double step)
    {
        const int perAxis = int(side / step) + 1;
        const double half = side * 0.5;

        QVector<QVector3D> points;
        points.reserve(qsizetype(perAxis) * perAxis);
        for (int row = 0; row < perAxis; row++) {
            for (int column = 0; column < perAxis; column++) {
                points.append(QVector3D(float(column * step - half),
                                        float(row * step - half), 0.0f));
            }
        }
        return points;
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

    //! The perspective projection that puts the same world height on screen at
    //! the camera's distance as orthoProjection(@a height) does, so a test can
    //! swap cameras without retuning what it expects to measure.
    QMatrix4x4 perspectiveProjection(float height)
    {
        const double verticalFieldOfView =
            2.0 * std::atan(double(height) * 0.5 / double(kEyeDistance));
        QMatrix4x4 projection;
        projection.perspective(float(qRadiansToDegrees(verticalFieldOfView)), 1.0f,
                               kOrthoNear, kOrthoFar);
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

    // Bytes on the ledger in some other category for as long as it is alive —
    // a note's texture, as far as the point clouds' budget share is concerned.
    class LedgerHold {
    public:
        LedgerHold(cwRenderMemoryLedger::Category category,
                   cwRenderMemoryLedger::Residency residency, qint64 bytes) :
            m_category(category), m_residency(residency), m_bytes(bytes)
        {
            cwRenderMemoryLedger::instance()->adjust(m_category, m_residency, m_bytes);
        }

        ~LedgerHold()
        {
            cwRenderMemoryLedger::instance()->adjust(m_category, m_residency, -m_bytes);
        }

    private:
        const cwRenderMemoryLedger::Category m_category;
        const cwRenderMemoryLedger::Residency m_residency;
        const qint64 m_bytes;
    };

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

            start();
        }

        //! A fixture over an octree somebody else built — one the builder wrote
        //! out of a real LAZ file, say — drawn into a target @a targetDimension
        //! pixels on a side, since the viewport is what the screen-space error
        //! rule measures against.
        PointCloudFixture(QRhi* rhi, const OctreeCache& cache, int targetDimension) :
            m_rhi(rhi),
            m_live(makeRenderTarget(rhi, QSize(targetDimension, targetDimension)))
        {
            m_cache = cache;
            REQUIRE(m_cache.source.manifest->nodes.size() > cw::octree::kChildCount);

            start();
        }

        ~PointCloudFixture()
        {
            frameRenderer()->evictPipelinesFor(m_live.renderPassDescriptor.get());
            if (m_secondRender) {
                frameRenderer()->destroyRenderObject(m_secondRender->renderObjectId());
            }
            frameRenderer()->destroyRenderObject(m_render.renderObjectId());
        }

        //! A second cloud over the same cache in the same scene, so the two
        //! share the frame's point and byte budgets
        void addSecondCloud()
        {
            m_secondRender = std::make_unique<RenderCloud>();
            m_secondRender->setScene(&m_scene);
            m_secondRender->setOctree(m_cache.source);
            m_secondBackend = static_cast<cwRHIPointCloud*>(m_secondRender->createRHIObject());
            frameRenderer()->registerRenderObject(m_secondRender->renderObjectId(),
                                                  m_secondBackend);
            m_secondBackend->synchronize({m_secondRender.get(), &m_renderer});
        }

        const cwRHIPointCloud& secondBackend() const { return *m_secondBackend; }

        cwRenderObjectId secondObjectId() const { return m_secondRender->renderObjectId(); }

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

        void synchronize()
        {
            m_backend->synchronize({&m_render, &m_renderer});
            m_synchronizedSinceFrame = true;
        }

        //! Production runs updateResources() only on the frames the render
        //! object changed — cwRhiScene::syncRenderObject marks it for one —
        //! so a test that wants the frames between can hold renderFrame() to
        //! the same rule.
        void setResourceUpdateAfterSyncOnly(bool afterSyncOnly)
        {
            m_resourceUpdateAfterSyncOnly = afterSyncOnly;
        }

        void setOrthoHeight(float height) { m_orthoHeight = height; }

        //! Draws the following frames through perspectiveProjection() instead
        //! of orthoProjection(), at the same world height.
        void setPerspective(bool perspective) { m_perspective = perspective; }

        //! Renders the following frames the way an export job with an
        //! appearance override for this cloud does: a transient slot, uploaded
        //! just-in-time, bound in place of the live slot 0.
        void setAppearanceOverride(const cwPointCloudAppearance& appearance)
        {
            m_appearanceOverride = appearance;
        }

        void clearAppearanceOverride() { m_appearanceOverride.reset(); }

        //! Moves what renderFrame() looks at, so two cameras can hold
        //! different parts of the cloud
        void setViewOffset(const QVector3D& offset) { m_viewOffset = offset; }

        //! Points renderFrame()'s camera far enough off the cloud that the
        //! frame's frustum misses its world bounds, the way a pan off screen does
        void lookAway() { setViewOffset(QVector3D(kLookAwayDistance, 0.0f, 0.0f)); }

        //! Takes the cloud out of the frame's visibility snapshot, the way a
        //! LAZ layer toggled off does
        void setCloudVisible(bool visible)
        {
            cwSceneVisibility* visibility = m_scene.visibility();
            visibility->setObjectVisible(m_render.renderObjectId(), visible);
            frameRenderer()->setVisibilitySnapshot(visibility->snapshot());
        }

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
        //! only), streamResources, gatherScene, drawScene. @a options is what an
        //! offscreen export job would pass — the live frame's defaults otherwise.
        void renderFrame(const cwSceneGatherOptions& options = {})
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
                                             m_live.target.get(), projection(m_orthoHeight),
                                             viewAt(m_cache.center + m_viewOffset), 1.0f, size);

            const cwRHIObject::PerPassRenderData perPassRenderData =
                frameRenderer()->buildPerPassRenderData(renderData);

            cwRHIObject::ResourceUpdateData resourceUpdateData{batch, renderData,
                                                               &perPassRenderData};

            if (!m_backendInitialized) {
                m_backend->initialize(resourceUpdateData);
                m_backendInitialized = true;
            }
            if (m_synchronizedSinceFrame || !m_resourceUpdateAfterSyncOnly) {
                m_backend->updateResources(resourceUpdateData);
                m_synchronizedSinceFrame = false;
            }

            qint64 remainingUploadBytes = renderData.budgets.uploadBudgetBytesPerFrame;
            m_backend->streamResources(resourceUpdateData, remainingUploadBytes);

            if (m_secondBackend) {
                if (!m_secondInitialized) {
                    m_secondBackend->initialize(resourceUpdateData);
                    m_secondInitialized = true;
                }
                m_secondBackend->updateResources(resourceUpdateData);
                m_secondBackend->streamResources(resourceUpdateData, remainingUploadBytes);
            }

            cwSceneGatherOptions gatherOptions = options;
            if (m_appearanceOverride.has_value()) {
                m_backend->flushRetiredAppearanceResources();
                m_backend->resetFrameAppearanceSlots();
                const int slot = m_backend->acquireAppearanceSlot(m_rhi, batch);
                REQUIRE(slot != cwAppearanceSlotted::kNoAppearanceSlot);
                m_backend->uploadAppearance(batch, slot,
                                            cwAppearanceOverride(*m_appearanceOverride));
                gatherOptions.appearanceSlotForObject.insert(m_backend, slot);
            }

            std::array<QVector<cwRHIObject::PipelineBatch>,
                       cwRhiFrameRenderer::kPassCount> passBatches;
            frameRenderer()->gatherScene(passBatches, perPassRenderData, gatherOptions);

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

        //! The pixels of the last frame that are not the clear color, in target
        //! pixel coordinates
        QVector<QPoint> litPixels() const
        {
            QVector<QPoint> lit;
            if (m_colorSize.isEmpty()) {
                return lit;
            }

            constexpr int kChannelsPerPixel = 4;
            const auto* pixels = reinterpret_cast<const uchar*>(m_colorPixels.constData());
            for (int y = 0; y < m_colorSize.height(); y++) {
                for (int x = 0; x < m_colorSize.width(); x++) {
                    const int offset = (y * m_colorSize.width() + x) * kChannelsPerPixel;
                    const bool drawn = pixels[offset] != 0 || pixels[offset + 1] != 0
                                       || pixels[offset + 2] != 0;
                    if (drawn) {
                        lit.append(QPoint(x, y));
                    }
                }
            }
            return lit;
        }

        QSize colorSize() const { return m_colorSize; }

        //! The last frame's readback as an image, for dumping a case that
        //! failed. The readback is RGBA8 and the copy owns its bytes.
        QImage frameImage() const
        {
            if (m_colorSize.isEmpty()) {
                return QImage();
            }
            return QImage(reinterpret_cast<const uchar*>(m_colorPixels.constData()),
                          m_colorSize.width(), m_colorSize.height(),
                          QImage::Format_RGBA8888).copy();
        }

        //! The pixels of the last frame that are not the clear color, in the
        //! [-1, 1] square of the target, x and y folded to their magnitudes so
        //! the check does not depend on which way the backend flips y.
        QVector<QPointF> litPixelsInNdc() const
        {
            QVector<QPointF> ndc;
            const QVector<QPoint> lit = litPixels();
            ndc.reserve(lit.size());
            for (const QPoint& pixel : lit) {
                ndc.append(QPointF(
                    std::abs((pixel.x() + 0.5) / m_colorSize.width() * 2.0 - 1.0),
                    std::abs((pixel.y() + 0.5) / m_colorSize.height() * 2.0 - 1.0)));
            }
            return ndc;
        }

        //! Frames until nothing is queued, in flight or waiting to be uploaded
        void renderUntilQuiet()
        {
            QElapsedTimer timer;
            timer.start();
            renderFrame();
            while (streamingWork() && timer.elapsed() < kWaitTimeoutMs) {
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

        //! True while either cloud still has something in flight
        bool streamingWork() const
        {
            return Access::hasStreamingWork(*m_backend)
                   || (m_secondBackend && Access::hasStreamingWork(*m_secondBackend));
        }

        //! The nodes the primary cloud holds right now
        QSet<int> residentNodes() const
        {
            QSet<int> resident;
            for (int i = 0; i < Access::nodeCount(*m_backend); i++) {
                if (Access::nodeState(*m_backend, i) == NodeState::Resident) {
                    resident.insert(i);
                }
            }
            return resident;
        }

    private:
        //! Everything both constructors do once the cache and the target are in
        //! place: publish the octree, build the back end and register it.
        void start()
        {
            m_gpuBaseline = ledgerBytes(cwRenderMemoryLedger::Residency::Gpu);
            m_cpuBaseline = ledgerBytes(cwRenderMemoryLedger::Residency::Cpu);

            m_render.setScene(&m_scene);
            m_render.setOctree(m_cache.source);

            m_backend = static_cast<cwRHIPointCloud*>(m_render.createRHIObject());
            frameRenderer()->registerRenderObject(m_render.renderObjectId(), m_backend);
            synchronize();
        }

        QMatrix4x4 projection(float height) const
        {
            return m_perspective ? perspectiveProjection(height) : orthoProjection(height);
        }

        QRhi* const m_rhi;
        QTemporaryDir m_cacheDirectory;
        RenderTarget m_live;
        OctreeCache m_cache;
        cwScene m_scene;
        RenderCloud m_render;
        mutable cwRhiItemRenderer m_renderer;
        cwRHIPointCloud* m_backend = nullptr;
        std::unique_ptr<RenderCloud> m_secondRender;
        cwRHIPointCloud* m_secondBackend = nullptr;
        bool m_secondInitialized = false;
        bool m_backendInitialized = false;
        bool m_synchronizedSinceFrame = false;
        bool m_resourceUpdateAfterSyncOnly = false;
        int m_drawableCount = 0;
        float m_orthoHeight = kFarOrthoHeight;
        bool m_perspective = false;
        std::optional<cwPointCloudAppearance> m_appearanceOverride;
        QVector3D m_viewOffset;
        bool m_readbackEnabled = false;
        QByteArray m_colorPixels;
        QSize m_colorSize;
        qint64 m_gpuBaseline = 0;
        qint64 m_cpuBaseline = 0;
    };

    //! The longest run of unlit pixels between the first and last lit pixel of
    //! @a row, or -1 where the row has nothing lit on it. litPixels() walks
    //! rows in order, so a row's columns arrive ascending.
    int longestUnlitRun(const QVector<QPoint>& lit, int row)
    {
        int previousColumn = -1;
        int longest = -1;
        for (const QPoint& pixel : lit) {
            if (pixel.y() != row) {
                continue;
            }

            longest = previousColumn < 0
                ? 0
                : std::max(longest, pixel.x() - previousColumn - 1);
            previousColumn = pixel.x();
        }
        return longest;
    }

    //! The side, in target pixels, the shader gives a world-space length
    //! @a radius under renderFrame()'s camera of world height @a orthoHeight.
    //! gl_PointSize is a side length, and the projection puts @a orthoHeight
    //! meters across kTargetDimension pixels.
    double expectedSpriteSidePx(double radius, float orthoHeight)
    {
        return radius * kTargetDimension / double(orthoHeight);
    }

    //! The side, in target pixels, PointCloud.vert sizes a sprite to wherever
    //! the cut keeps up: the coverage's share of the spacing the cut refines
    //! to. That spacing projects to @a thresholdPx pixels, so the term is
    //! already in pixels and needs no camera. Where the cut is behind, the
    //! spacing really drawn is wider and the sprites go with it.
    double spriteSidePx(double spacingCoverage, double thresholdPx)
    {
        return spacingCoverage * thresholdPx;
    }

    //! The largest a sprite of side @a sidePx may measure once rasterized.
    double spriteSideUpperBoundPx(double sidePx)
    {
        return sidePx * kSpriteRasterFactor + kSpriteTolerancePx;
    }

    //! The largest the sprite spriteSidePx() describes may measure once
    //! rasterized.
    double spriteSideUpperBoundPx(double spacingCoverage, double thresholdPx)
    {
        return spriteSideUpperBoundPx(spriteSidePx(spacingCoverage, thresholdPx));
    }

    //! The projected spacing the cloud's cut is refining to, as the CPU hands
    //! it to the shader.
    double refineThresholdPx(const PointCloudFixture& fixture)
    {
        return Access::liveAppearanceUniform(fixture.backend()).sseThresholdPx;
    }

    //! The world spacing @a node's sprites floor against, as the CPU hands it
    //! to the shader: the finest spacing the last live frame drew under it.
    double nodeFloorSpacing(const PointCloudFixture& fixture, int node)
    {
        return Access::nodeFloorSpacing(fixture.backend(), node);
    }

    //! The finest spacing the last live frame drew anywhere in the cloud. The
    //! root is an ancestor of every drawn node, so its own floor is the whole
    //! cloud's.
    double drawnFloorSpacing(const PointCloudFixture& fixture)
    {
        return nodeFloorSpacing(fixture, kRootIndex);
    }

    //! The deepest level @a drawnNodes draws in @a node's subtree, @a node
    //! itself included — the spacing its sprites have to close.
    int finestDrawnLevelUnder(const cwPointOctreeManifest& manifest,
                              const QVector<int>& drawnNodes,
                              int node)
    {
        const QVector<int>& parents = manifest.parents();
        int finest = manifest.nodes.at(node).level;
        for (int under : drawnNodes) {
            for (int ancestor = under; ancestor >= 0; ancestor = parents.at(ancestor)) {
                if (ancestor == node) {
                    finest = std::max(finest, manifest.nodes.at(under).level);
                    break;
                }
            }
        }
        return finest;
    }

    //! A drawn node with nothing drawn under it while the frame draws something
    //! finer elsewhere, or -1 when every drawn node sits at the finest level on
    //! screen. It is the node the per-node floor exists for.
    int starvedDrawnNode(const cwPointOctreeManifest& manifest, const QVector<int>& drawnNodes)
    {
        int finest = kRootLevel;
        for (int node : drawnNodes) {
            finest = std::max(finest, manifest.nodes.at(node).level);
        }

        for (int node : drawnNodes) {
            const int level = manifest.nodes.at(node).level;
            if (level < finest && finestDrawnLevelUnder(manifest, drawnNodes, node) == level) {
                return node;
            }
        }
        return -1;
    }

    //! Frames until the cloud draws a starved node, which it returns, or -1
    //! when the timeout runs out first. Which node the streamer holds back is
    //! down to how the frames and the decode threads interleave, so the frame
    //! that shows one is waited for rather than counted to.
    int renderUntilStarvedNode(PointCloudFixture& fixture)
    {
        QElapsedTimer timer;
        timer.start();
        for (;;) {
            const int node = starvedDrawnNode(fixture.manifest(),
                                              Access::drawnNodes(fixture.backend()));
            if (node >= 0 || timer.elapsed() >= kWaitTimeoutMs) {
                return node;
            }

            QThread::msleep(kFramePauseMs);
            fixture.renderFrame();
        }
    }

    //! The deepest level the cloud's last cut asked for.
    int finestSelectedLevel(const PointCloudFixture& fixture)
    {
        int finest = kRootLevel;
        for (int level : Access::selectedLevels(fixture.backend())) {
            finest = std::max(finest, level);
        }
        return finest;
    }

    //! The deepest level the cloud's last frame drew, which lags the cut while
    //! the children behind it are still streaming.
    int finestDrawnLevel(const PointCloudFixture& fixture)
    {
        int finest = kRootLevel;
        for (int level : Access::drawnLevels(fixture.backend())) {
            finest = std::max(finest, level);
        }
        return finest;
    }

    //! The topmost lit row of @a lit, or -1 for a frame that lit nothing.
    int topmostLitRow(const QVector<QPoint>& lit)
    {
        int topmost = -1;
        for (const QPoint& pixel : lit) {
            topmost = topmost < 0 ? pixel.y() : std::min(topmost, pixel.y());
        }
        return topmost;
    }

    //! How many levels of the tree the cloud's last cut draws at once. The cut
    //! is additive, so a refined region carries its ancestors along with it.
    int selectedLevelCount(const PointCloudFixture& fixture)
    {
        QSet<int> levels;
        for (int level : Access::selectedLevels(fixture.backend())) {
            levels.insert(level);
        }
        return levels.size();
    }

    //! The side, in target pixels, of the sprites the cloud draws at
    //! @a spacingCoverage.
    //!
    //! A sprite is centered on its point, so the lit region reaches half a
    //! sprite past the highest point the cut draws. The same cut rendered at
    //! the shader's one-pixel floor says where that highest point is, and the
    //! two top edges differ by the halo. Measured off the top edge rather than
    //! a run along a row because at this zoom neighboring sprites merge into
    //! runs many sprites long, and the passage misses the frame's center row.
    double drawnSpriteSidePx(PointCloudFixture& fixture, float spacingCoverage)
    {
        // Coverage zero sizes every sprite to nothing, which the shader clamps
        // up to its one-pixel floor — the reference the halo is measured from.
        fixture.render().setSpacingCoverage(0.0f);
        fixture.synchronize();
        fixture.renderFrame();
        const int floorTop = topmostLitRow(fixture.litPixels());
        REQUIRE(floorTop >= 0);

        fixture.render().setSpacingCoverage(spacingCoverage);
        fixture.synchronize();
        fixture.renderFrame();
        const int top = topmostLitRow(fixture.litPixels());
        REQUIRE(top >= 0);

        return kMinSpritePx + 2.0 * (floorTop - top);
    }

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

    // Renders until a view inflated to @a inflation has stepped all the way back
    // to 1, giving it the probe frames the governor needs for every step.
    void renderUntilRelaxed(PointCloudFixture& fixture, double inflation)
    {
        const int steps = int(std::ceil(std::log(inflation)
                                        / std::log(cw::octree::kSseInflationStep)));
        const int mostFrames = 2 * cw::octree::kSseRelaxProbeFrames * std::max(1, steps);
        for (int i = 0; i < mostFrames && Access::sseInflation(fixture.backend()) > 1.0; i++) {
            fixture.renderFrame();
        }
    }

    // ---- Hole metric ---------------------------------------------------
    //
    // A point cloud read as a surface is holed wherever a sprite is narrower
    // than the gap to the next point of the cut. Zooming out drops the cut a
    // level at a time, doubling that gap in one step, and these helpers put a
    // number on what the drop costs, measured off the rendered pixels.

    //! The controlled fixture: a plane 64 m across sampled every 10 cm, which
    //! is finer than the deepest level the sweeps reach, so every level the cut
    //! stops at is a full grid and any unlit pixel inside the square is a hole.
    constexpr double kPlaneSide = 64.0;
    constexpr double kPlaneStep = 0.1;

    //! The reference render the tile's mask comes from: the cut refined as far
    //! as the octree goes, with sprites twice the spacing so the only pixels it
    //! leaves unlit are gaps in the data itself.
    constexpr float kReferenceSpacingCoverage = 2.0f;
    constexpr double kReferenceScreenSpaceErrorPx = cw::budgets::kMinScreenSpaceErrorPx;
    //! The reference's own sprites spill half their side past a real gap, so the
    //! mask is eroded by that much plus a pixel of rasterization slack
    constexpr int kReferenceErosionSlackPx = 1;

    //! PointCloud.vert's ceiling on gl_PointSize (:45)
    constexpr double kMaxSpritePx = 64.0;

    //! 2 % a step, which puts several scales between neighboring level
    //! transitions without the sweep costing more frames than it is worth
    constexpr double kSweepStepFraction = 1.02;
    //! The 2000 px sweep reads back 16 MB a frame, so it steps coarser
    constexpr double kStarvedStepFraction = 1.15;

    constexpr float kPlaneSweepLowHeight = 30.0f;
    constexpr float kPlaneSweepHighHeight = 120.0f;
    constexpr float kTileSweepLowHeight = 120.0f;
    constexpr float kTileSweepHighHeight = 800.0f;
    constexpr float kStarvedSweepLowHeight = 300.0f;
    constexpr float kStarvedSweepHighHeight = 3000.0f;

    //! With frustum culling the tile's cut at 256 px stays far under a million
    //! points at every scale, so the starved variant needs a viewport the
    //! screen-space error rule refines for
    constexpr int kStarvedTargetDimension = 2000;
    constexpr qint64 kStarvedPointBudget = 1000000;

    //! Small enough that residency lags the cut by more than the one step the
    //! transient variant gives it
    constexpr qint64 kTransientUploadBudgetBytes = 256 * 1024;
    constexpr int kTransientFramesPerStep = 2;

    //! The pass criteria, quiet variant only, and they are a baseline rather
    //! than a target: the plan asked for 5 % holes and 2 px, and the first run
    //! measured 49 % and a hole that crosses the whole target at the scale
    //! where the cut drops from level 2 to level 1. These numbers are the
    //! measurement plus a little slack, so a sizing change that helps moves
    //! them down and one that regresses trips them.
    //!
    //! The plane's holes are not blobs: a sprite one pixel wide on a grid 1.4
    //! pixels apart leaves unlit lattice lines that run the width of the
    //! target, so its largest hole is the target itself at almost every scale.
    //! The side is recorded for the plane and bounded only on the tile, whose
    //! terrain breaks the lattice up into holes a reader can picture.
    constexpr double kMaxHoleFraction = 0.55;
    constexpr double kMaxHoleFractionJump = 0.52;

    //! The coverage the "sizing rule alone" plane sweep runs at: one sprite per
    //! cell, near enough, which is where an irregular cut still shows the
    //! lattice and the sweep has something to measure. It is the old default,
    //! and the sweep at it peaks at 0.485 — which is why the default moved.
    constexpr float kBareSpacingCoverage = 0.75f;

    //! The same plane at the default coverage, where sprites overlap their
    //! neighbors by half a cell: 0.016 worst, a thirtieth of what the bare
    //! coverage leaves, and the same number is the largest step-to-step jump
    //! because the whole curve is flat until one level transition. Measured,
    //! with room for the pixel rounding a different rasterizer would do.
    constexpr double kDefaultCoverageMaxHoleFraction = 0.02;
    constexpr double kDefaultCoverageMaxHoleFractionJump = 0.02;

    //! The same criteria on the tile, which is measured against a reference
    //! render rather than a rectangle and carries the residual the erosion
    //! leaves behind, so its baseline stands on its own. At the default
    //! coverage the tile measures 0.00025 worst over a 1 px hole, so these are
    //! the measured values with a few times their own size in slack.
    constexpr double kTileMaxHoleFraction = 0.002;
    constexpr int kTileMaxHoleSidePx = 4;
    constexpr double kTileMaxHoleFractionJump = 0.002;

    //! Where a failed case writes its frame, when it is set
    const char* const kFrameDumpEnvironmentVariable = "CAVEWHERE_HOLE_METRIC_DUMP_DIR";
    const char* const kTileLazEnvironmentVariable = "CAVEWHERE_HOLE_METRIC_LAZ";

    //! A per-pixel flag over a target-sized grid: which pixels are lit, or which
    //! ones the hole count is entitled to look at.
    struct PixelMask {
        QSize size;
        std::vector<char> flags;

        PixelMask() = default;

        explicit PixelMask(QSize size) :
            size(size), flags(size_t(std::max(0, size.width() * size.height())), 0)
        {
        }

        bool at(int x, int y) const
        {
            return x >= 0 && y >= 0 && x < size.width() && y < size.height()
                   && flags.at(size_t(y) * size_t(size.width()) + size_t(x)) != 0;
        }

        void setAt(int x, int y, bool flag)
        {
            flags[size_t(y) * size_t(size.width()) + size_t(x)] = flag ? 1 : 0;
        }

        qsizetype count() const
        {
            return qsizetype(std::count(flags.begin(), flags.end(), char(1)));
        }
    };

    //! The last frame's lit pixels as a mask.
    PixelMask litMask(const PointCloudFixture& fixture)
    {
        PixelMask mask(fixture.colorSize());
        for (const QPoint& pixel : fixture.litPixels()) {
            mask.setAt(pixel.x(), pixel.y(), true);
        }
        return mask;
    }

    //! @a mask with every pixel within @a radius of an unset one cleared, run as
    //! two one-dimensional passes so the cost follows the radius rather than its
    //! square.
    PixelMask eroded(const PixelMask& mask, int radius)
    {
        if (radius <= 0) {
            return mask;
        }

        PixelMask rows(mask.size);
        for (int y = 0; y < mask.size.height(); y++) {
            for (int x = 0; x < mask.size.width(); x++) {
                bool keep = true;
                for (int offset = -radius; offset <= radius && keep; offset++) {
                    keep = mask.at(x + offset, y);
                }
                rows.setAt(x, y, keep);
            }
        }

        PixelMask columns(mask.size);
        for (int y = 0; y < mask.size.height(); y++) {
            for (int x = 0; x < mask.size.width(); x++) {
                bool keep = true;
                for (int offset = -radius; offset <= radius && keep; offset++) {
                    keep = rows.at(x, y + offset);
                }
                columns.setAt(x, y, keep);
            }
        }
        return columns;
    }

    struct HoleStats {
        double fraction = 0.0;
        int count = 0;
        int maxSidePx = 0;
    };

    //! What @a lit leaves unlit inside @a mask: the share of the mask that is
    //! holed, how many 4-connected holes there are, and the longest side of the
    //! largest one's bounding box, which is the number a reader can picture.
    HoleStats holeStats(const PixelMask& lit, const PixelMask& mask)
    {
        HoleStats stats;
        const qsizetype maskCount = mask.count();
        if (maskCount <= 0) {
            return stats;
        }

        const int width = mask.size.width();
        const int height = mask.size.height();
        std::vector<char> visited(size_t(width) * size_t(height), 0);
        const auto isHole = [&](int x, int y) {
            return mask.at(x, y) && !lit.at(x, y);
        };

        qsizetype holeCount = 0;
        std::vector<QPoint> stack;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const size_t seed = size_t(y) * size_t(width) + size_t(x);
                if (visited.at(seed) != 0 || !isHole(x, y)) {
                    continue;
                }

                stats.count++;
                visited[seed] = 1;
                stack.push_back(QPoint(x, y));

                int left = x;
                int right = x;
                int top = y;
                int bottom = y;
                while (!stack.empty()) {
                    const QPoint pixel = stack.back();
                    stack.pop_back();
                    holeCount++;
                    left = std::min(left, pixel.x());
                    right = std::max(right, pixel.x());
                    top = std::min(top, pixel.y());
                    bottom = std::max(bottom, pixel.y());

                    const std::array<QPoint, 4> neighbors {
                        QPoint(pixel.x() - 1, pixel.y()), QPoint(pixel.x() + 1, pixel.y()),
                        QPoint(pixel.x(), pixel.y() - 1), QPoint(pixel.x(), pixel.y() + 1)
                    };
                    for (const QPoint& neighbor : neighbors) {
                        if (neighbor.x() < 0 || neighbor.y() < 0
                            || neighbor.x() >= width || neighbor.y() >= height) {
                            continue;
                        }
                        const size_t index =
                            size_t(neighbor.y()) * size_t(width) + size_t(neighbor.x());
                        if (visited.at(index) != 0 || !isHole(neighbor.x(), neighbor.y())) {
                            continue;
                        }
                        visited[index] = 1;
                        stack.push_back(neighbor);
                    }
                }

                stats.maxSidePx = std::max(stats.maxSidePx,
                                           std::max(right - left, bottom - top) + 1);
            }
        }

        stats.fraction = double(holeCount) / double(maskCount);
        return stats;
    }

    //! The side, in target pixels, PointCloud.vert gives the sprites of a node
    //! whose floor is @a floorSpacing — the very rule under test, read off the
    //! block the shader is handed rather than guessed from the camera. Sizing
    //! is per node, so a coarse node held back by the streamer draws wider
    //! sprites than a refined one of the same cloud.
    double nodeSpriteSidePx(const PointCloudFixture& fixture, float orthoHeight,
                            double floorSpacing)
    {
        const CwRhiPointCloudTestAccess::PerCloudUniform uniform =
            Access::liveAppearanceUniform(fixture.backend());
        const double pixelsPerMeter = fixture.colorSize().height() / double(orthoHeight);
        const double coverage = double(uniform.spacingCoverage);
        const double sizePx = std::max(coverage * floorSpacing * pixelsPerMeter,
                                       coverage * double(uniform.sseThresholdPx));
        return std::clamp(sizePx, kMinSpritePx, kMaxSpritePx);
    }

    //! The widest floor the last frame drew, which is the node with the least
    //! under it — the largest sprite anywhere in the cloud.
    double widestDrawnFloorSpacing(const PointCloudFixture& fixture)
    {
        double widest = 0.0;
        for (int node : Access::drawnNodes(fixture.backend())) {
            widest = std::max(widest, double(nodeFloorSpacing(fixture, node)));
        }
        return widest;
    }

    //! How far a mask has to pull back from an edge before the half sprite that
    //! spills over it stops reading as data: half of the cloud's largest
    //! sprite, plus a pixel for the rounding rasterization does.
    int maskInsetPx(const PointCloudFixture& fixture, float orthoHeight)
    {
        const double sidePx =
            nodeSpriteSidePx(fixture, orthoHeight, widestDrawnFloorSpacing(fixture));
        return int(std::ceil(sidePx * 0.5)) + kReferenceErosionSlackPx;
    }

    //! The finest subtree's sprite side over the on-screen gap of the finest
    //! level the last frame drew: the geometric coverage the sizing rule
    //! reaches there, on the CPU. One means the sprites just meet. Coarser
    //! nodes sit at their own wider floor, so they cover their own gaps.
    double geometricCoverageRatio(const PointCloudFixture& fixture, float orthoHeight)
    {
        const double pixelsPerMeter = fixture.colorSize().height() / double(orthoHeight);
        const double gapPx = fixture.manifest().spacing(finestDrawnLevel(fixture))
                             * pixelsPerMeter;
        if (gapPx <= 0.0) {
            return 0.0;
        }
        return nodeSpriteSidePx(fixture, orthoHeight, drawnFloorSpacing(fixture)) / gapPx;
    }

    //! The rectangle the cloud's data covers on the target, shrunk by @a
    //! shrinkPx so the half-covered edge row does not read as a band of holes.
    PixelMask footprintMask(const PointCloudFixture& fixture, float orthoHeight, int shrinkPx)
    {
        const QSize size = fixture.colorSize();
        const QPointF bounds = fixture.dataBoundsInNdc(orthoHeight);
        const double halfWidth = bounds.x() * size.width() * 0.5 - shrinkPx;
        const double halfHeight = bounds.y() * size.height() * 0.5 - shrinkPx;
        const double centerX = size.width() * 0.5;
        const double centerY = size.height() * 0.5;

        PixelMask mask(size);
        for (int y = 0; y < size.height(); y++) {
            for (int x = 0; x < size.width(); x++) {
                const bool inside = std::abs(x + 0.5 - centerX) <= halfWidth
                                    && std::abs(y + 0.5 - centerY) <= halfHeight;
                mask.setAt(x, y, inside);
            }
        }
        return mask;
    }

    //! Writes the fixture's last frame under the dump directory, when one is
    //! named. Nothing happens otherwise, so a passing run leaves no files.
    void dumpFrame(const PointCloudFixture& fixture, const QString& name)
    {
        const QByteArray directory = qgetenv(kFrameDumpEnvironmentVariable);
        if (directory.isEmpty()) {
            return;
        }

        const QString path = QDir(QString::fromLocal8Bit(directory))
                                 .filePath(QStringLiteral("%1-%2.png")
                                               .arg(name)
                                               .arg(QCoreApplication::applicationPid()));
        fixture.frameImage().save(path);
    }

    //! The pixels a render of @a cache at each of @a heights is entitled to
    //! light: the densest render the octree can give, eroded by half its own
    //! sprite. Built on a fixture of its own so the sweep it feeds starts from
    //! the residency its own steps left behind.
    QVector<PixelMask> referenceMasks(QRhi* rhi, const OctreeCache& cache,
                                      int targetDimension, const QVector<float>& heights)
    {
        PointCloudFixture fixture(rhi, cache, targetDimension);
        fixture.setReadbackEnabled(true);

        cwRenderBudgets budgets = fixture.budgets();
        budgets.screenSpaceErrorPx = kReferenceScreenSpaceErrorPx;
        fixture.setBudgets(budgets);

        fixture.render().setSpacingCoverage(kReferenceSpacingCoverage);
        fixture.synchronize();

        QVector<PixelMask> masks;
        masks.reserve(heights.size());
        for (float height : heights) {
            fixture.setOrthoHeight(height);
            fixture.renderUntilQuiet();

            masks.append(eroded(litMask(fixture), maskInsetPx(fixture, height)));
        }
        return masks;
    }

    //! Heights from @a low to @a high, ascending, each @a fraction times the one
    //! before — the direction the pop is seen, and the one that leaves no
    //! coarser level resident to hide it.
    QVector<float> zoomOutHeights(float low, float high, double fraction)
    {
        QVector<float> heights;
        for (double height = low; height <= high; height *= fraction) {
            heights.append(float(height));
        }
        return heights;
    }

    //! The octree of a real LAZ tile, built once per process. The file is a
    //! 1 km USGS lidar tile — USGS_LPC_WY_FEMA_East_2019_D19_w1145n2340.laz,
    //! 5.5 M points, downloaded from the USGS 3DEP lidar catalog — and it is
    //! too big to check in, so the path comes from CAVEWHERE_HOLE_METRIC_LAZ
    //! and the tile cases skip without it.
    struct TileCache {
        QTemporaryDir directory {
            QDir::temp().filePath(QStringLiteral("cwHoleMetric-%1-XXXXXX")
                                      .arg(QCoreApplication::applicationPid()))
        };
        OctreeCache cache;
        qint64 buildMilliseconds = 0;
        bool built = false;
    };

    const TileCache& usgsTileCache()
    {
        static TileCache tile;
        static bool attempted = false;
        if (attempted) {
            return tile;
        }
        attempted = true;

        const QString lazPath = QString::fromLocal8Bit(qgetenv(kTileLazEnvironmentVariable));
        if (lazPath.isEmpty() || !QFileInfo::exists(lazPath)) {
            return tile;
        }

        REQUIRE(tile.directory.isValid());

        //An empty frame CS leaves the tile in its own Albers meters
        const cwPointOctreeBuilder::Request request {
            .path = lazPath,
            .cacheRootPath = tile.directory.path()
        };

        QElapsedTimer timer;
        timer.start();
        QFuture<cwPointOctreeBuilder::Result> future = cwPointOctreeBuilder::build(request);
        future.waitForFinished();
        tile.buildMilliseconds = timer.elapsed();

        REQUIRE(future.resultCount() == 1);
        const cwPointOctreeBuilder::Result result = future.result();
        REQUIRE_FALSE(result.hasError());

        auto manifest = std::make_shared<cwPointOctreeManifest>(result.value());

        // The tile's own coordinates run to 2.3 million meters, where a float
        // holds a quarter of a meter — coarser than a pixel at the scales the
        // sweep measures. Node payloads are quantized against bounds derived
        // from rootMin, so sliding rootMin slides the whole cloud onto the
        // origin without touching a single cached byte.
        const QVector3D center = (manifest->bboxMin + manifest->bboxMax) * 0.5f;
        manifest->rootMin -= center;
        manifest->bboxMin -= center;
        manifest->bboxMax -= center;

        for (const cwPointOctreeNode& node : std::as_const(manifest->nodes)) {
            tile.cache.largestNodeBytes = std::max(tile.cache.largestNodeBytes, node.byteSize);
        }
        tile.cache.center = QVector3D();
        tile.cache.source = cwPointOctreeSource(tile.directory.path(), lazPath,
                                                manifest->fingerprint, manifest);
        tile.built = true;
        return tile;
    }

    //! One line of the sweep's CSV. The release build compiles qDebug out, and
    //! Catch2 prints an INFO only for a case that failed, so the curve — the
    //! point of the whole exercise — goes straight to stdout.
    void writeCsvLine(const QString& line)
    {
        std::cout << line.toStdString() << std::endl;
    }

    enum class StepPolicy {
        Quiet,  //!< settle at every scale, so the curve is the sizing rule alone
        Frames  //!< a couple of bare frames, so the curve carries the transient
    };

    struct SweepRecord {
        float height = 0.0f;
        int frame = 0;
        int finestSelectedLevel = 0;
        int finestDrawnLevel = 0;
        double holeFraction = 0.0;
        int holeCount = 0;
        int maxHoleSidePx = 0;
        double geometricCoverage = 0.0;
        double sseInflation = 1.0;
        bool pointCapped = false;
        int residentNodes = 0;
    };

    //! One zoom-out sweep of @a fixture over @a heights, one record a frame.
    //! @a masks says which pixels each scale is entitled to light; an empty one
    //! asks for the data's own footprint rectangle, which is all a plane needs.
    QVector<SweepRecord> sweep(PointCloudFixture& fixture, const QString& variant,
                               const QVector<float>& heights, StepPolicy policy,
                               int framesPerStep, const QVector<PixelMask>& masks)
    {
        REQUIRE_FALSE(heights.isEmpty());
        REQUIRE((masks.isEmpty() || masks.size() == heights.size()));

        fixture.setReadbackEnabled(true);

        //Settle at the tightest scale so the deepest level is resident before
        //the first step out
        fixture.setOrthoHeight(heights.first());
        fixture.renderUntilQuiet();

        writeCsvLine(QStringLiteral("holeCsv,variant,height,frame,finestSelected,finestDrawn,"
                                    "holeFraction,holeCount,maxHoleSide,geometricCoverage,"
                                    "sseInflation,pointCapped,residentNodes"));

        QVector<SweepRecord> records;
        for (int step = 0; step < heights.size(); step++) {
            const float height = heights.at(step);
            fixture.setOrthoHeight(height);

            const int frames = policy == StepPolicy::Quiet ? 1 : framesPerStep;
            if (policy == StepPolicy::Quiet) {
                fixture.renderUntilQuiet();
            }

            for (int frame = 0; frame < frames; frame++) {
                fixture.renderFrame();

                SweepRecord record;
                record.height = height;
                record.frame = frame;
                record.finestSelectedLevel = finestSelectedLevel(fixture);
                record.finestDrawnLevel = finestDrawnLevel(fixture);
                record.geometricCoverage = geometricCoverageRatio(fixture, height);
                record.sseInflation = Access::sseInflation(fixture.backend());
                record.pointCapped = Access::pointCapped(fixture.backend());
                record.residentNodes = Access::residentCount(fixture.backend());

                const PixelMask mask =
                    masks.isEmpty()
                        ? footprintMask(fixture, height, maskInsetPx(fixture, height))
                        : masks.at(step);
                const HoleStats stats = holeStats(litMask(fixture), mask);
                record.holeFraction = stats.fraction;
                record.holeCount = stats.count;
                record.maxHoleSidePx = stats.maxSidePx;

                writeCsvLine(QStringLiteral("holeCsv,%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12")
                           .arg(variant)
                           .arg(double(height), 0, 'f', 2)
                           .arg(record.frame)
                           .arg(record.finestSelectedLevel)
                           .arg(record.finestDrawnLevel)
                           .arg(record.holeFraction, 0, 'f', 5)
                           .arg(record.holeCount)
                           .arg(record.maxHoleSidePx)
                           .arg(record.geometricCoverage, 0, 'f', 3)
                           .arg(record.sseInflation, 0, 'f', 3)
                           .arg(record.pointCapped ? 1 : 0)
                           .arg(record.residentNodes));

                records.append(record);
            }
        }
        return records;
    }

    //! The worst frame of @a records by hole fraction, for the message a failed
    //! criterion carries.
    SweepRecord worstRecord(const QVector<SweepRecord>& records)
    {
        SweepRecord worst;
        for (const SweepRecord& record : records) {
            if (record.holeFraction >= worst.holeFraction) {
                worst = record;
            }
        }
        return worst;
    }

    //! The largest step in hole fraction between neighboring scales of
    //! @a records, which is what a level transition shows up as.
    double largestHoleFractionJump(const QVector<SweepRecord>& records)
    {
        double largest = 0.0;
        for (int i = 1; i < records.size(); i++) {
            largest = std::max(largest, records.at(i).holeFraction
                                            - records.at(i - 1).holeFraction);
        }
        return largest;
    }

    //! The plane octree, built once per process — 410 k points is a second of
    //! sampling that no sweep needs to pay twice.
    const OctreeCache& planeCache()
    {
        static QTemporaryDir directory {
            QDir::temp().filePath(QStringLiteral("cwHoleMetricPlane-%1-XXXXXX")
                                      .arg(QCoreApplication::applicationPid()))
        };
        static OctreeCache cache = [] {
            REQUIRE(directory.isValid());
            return buildOctreeCache(directory.path(), QStringLiteral("hole-metric-plane"),
                                    planePoints(kPlaneSide, kPlaneStep));
        }();
        return cache;
    }

    //! The tile octree, or a skip when CAVEWHERE_HOLE_METRIC_LAZ names nothing.
    const OctreeCache& usgsTileCacheOrSkip()
    {
        const TileCache& tile = usgsTileCache();
        if (!tile.built) {
            SKIP("Set CAVEWHERE_HOLE_METRIC_LAZ to a USGS lidar tile to measure "
                 "holes on real data");
        }
        writeCsvLine(QStringLiteral("holeBuild,usgsTile,%1,%2")
                         .arg(tile.buildMilliseconds)
                         .arg(tile.cache.source.manifest->pointCount));
        return tile.cache;
    }

    int largestHoleSidePx(const QVector<SweepRecord>& records)
    {
        int largest = 0;
        for (const SweepRecord& record : records) {
            largest = std::max(largest, record.maxHoleSidePx);
        }
        return largest;
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

    const qint64 coldestBytes = Access::nodeBytes(fixture.backend(), resident.at(0)).size();
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

TEST_CASE("A cut the budget share cannot hold coarsens, and a probe relaxes it again",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("sse-inflation"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    REQUIRE(Access::residentCount(fixture.backend()) > 2);
    REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);

    // A budget share nothing fits in: the cut's bytes are over it however much
    // residency is given back, so the only way down is a coarser cut.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = 1;
    fixture.setBudgets(budgets);
    fixture.setOrthoHeight(kFarOrthoHeight);

    constexpr int kInflatingFrames = 2;
    for (int i = 0; i < kInflatingFrames; i++) {
        fixture.renderFrame();
    }

    CHECK(Access::residentCount(fixture.backend()) == 1);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);

    const double coarsened = Access::sseInflation(fixture.backend());
    CHECK(coarsened > 1.0);

    // Room to spare again. The step down waits for a probe frame, which
    // re-selects the cut one step finer and finds it fits.
    budgets.gpuBudgetBytes = cw::budgets::kDefaultGpuBudgetBytes;
    fixture.setBudgets(budgets);

    renderUntilRelaxed(fixture, coarsened);

    CHECK(Access::sseInflation(fixture.backend()) == 1.0);
}

TEST_CASE("Residency churning at the budget leaves the cut alone", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("churn-no-inflation"));

    // Two windows a third of the passage apart. Each cut is a handful of nodes
    // over its own stretch, so holding both costs more than either.
    const auto stretchOffset = [&fixture](double along) {
        return QVector3D(float(along * kTubeLength),
                         float(kTubeBend * along * along),
                         0.0f) - fixture.center();
    };
    const QVector3D nearEnd = stretchOffset(kChurnNearAlong);
    const QVector3D farEnd = stretchOffset(kChurnFarAlong);

    fixture.setOrthoHeight(kChurnOrthoHeight);
    fixture.setViewOffset(nearEnd);
    fixture.renderUntilQuiet();
    const qint64 nearCutBytes = Access::selectedBytes(fixture.backend());

    fixture.setViewOffset(farEnd);
    fixture.renderUntilQuiet();
    const qint64 farCutBytes = Access::selectedBytes(fixture.backend());

    const qint64 bothBytes = fixture.gpuBytes();
    const qint64 largestCutBytes = std::max(nearCutBytes, farCutBytes);
    REQUIRE(largestCutBytes > 0);
    REQUIRE(bothBytes > largestCutBytes);

    // A share that holds the larger cut with room over, but not both stretches
    // at once: LRU gives back what the camera left behind, frame after frame.
    const qint64 share = largestCutBytes + (bothBytes - largestCutBytes) / 2;
    const qint64 outsideBytes = totalGpuBytes() - fixture.gpuBytes();
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = outsideBytes + share;
    fixture.setBudgets(budgets);

    constexpr int kAlternatingFrames = 200;
    int nodesLeftResidency = 0;
    for (int i = 0; i < kAlternatingFrames; i++) {
        const QSet<int> before = fixture.residentNodes();
        fixture.setViewOffset(i % 2 == 0 ? nearEnd : farEnd);
        fixture.renderFrame();
        nodesLeftResidency += int((before - fixture.residentNodes()).size());

        REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);
    }

    CHECK(nodesLeftResidency > 0);
}

TEST_CASE("A camera at rest gives nothing back once the cloud has settled",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("static-no-eviction"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const QSet<int> settled = fixture.residentNodes();
    const qint64 settledBytes = fixture.gpuBytes();
    REQUIRE(settled.size() > 2);

    constexpr int kRestingFrames = 120;
    for (int i = 0; i < kRestingFrames; i++) {
        fixture.renderFrame();
        REQUIRE(fixture.residentNodes() == settled);
        REQUIRE(fixture.gpuBytes() == settledBytes);
    }

    CHECK(Access::sseInflation(fixture.backend()) == 1.0);
}

TEST_CASE("The point budget caps what a frame draws from the first frame on",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("point-budget"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const qint64 uncappedPoints = Access::selectedPoints(fixture.backend());
    const qint64 rootPoints = fixture.manifest().nodes.at(kRootIndex).pointCount;
    REQUIRE(uncappedPoints > 3 * rootPoints);
    REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);

    constexpr qint64 kBudgetShare = 3;
    cwRenderBudgets budgets = fixture.budgets();
    budgets.pointBudget = uncappedPoints / kBudgetShare;
    fixture.setBudgets(budgets);

    constexpr int kFramesToInflate = 6;
    bool inflated = false;
    for (int i = 0; i < kFramesToInflate; i++) {
        fixture.renderFrame();
        REQUIRE(Access::selectedPoints(fixture.backend()) <= budgets.pointBudget);
        inflated = inflated || Access::sseInflation(fixture.backend()) > 1.0;
    }
    CHECK(inflated);

    // The cap holds every frame from the first; the inflation that follows
    // makes the whole cut coarse rather than cutting it off part way, and stops
    // as soon as the coarser cut fits on its own.
    constexpr int kFramesToSettle = 60;
    for (int i = 0; i < kFramesToSettle && Access::pointCapped(fixture.backend()); i++) {
        fixture.renderFrame();
        REQUIRE(Access::selectedPoints(fixture.backend()) <= budgets.pointBudget);
    }
    REQUIRE_FALSE(Access::pointCapped(fixture.backend()));

    constexpr int kHoldingFrames = 3 * cw::octree::kSseRelaxProbeFrames;
    const double held = Access::sseInflation(fixture.backend());
    for (int i = 0; i < kHoldingFrames; i++) {
        fixture.renderFrame();
        REQUIRE(Access::selectedPoints(fixture.backend()) <= budgets.pointBudget);
    }
    CHECK(Access::sseInflation(fixture.backend()) == held);

    // Handing the points back relaxes the cut again.
    budgets.pointBudget = cw::budgets::kDefaultPointBudgetPoints;
    fixture.setBudgets(budgets);

    renderUntilRelaxed(fixture, held);
    CHECK(Access::sseInflation(fixture.backend()) == 1.0);
}

TEST_CASE("Two clouds in one view share the byte and point budgets",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("two-clouds"));
    fixture.addSecondCloud();
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const qint64 cutBytes = Access::selectedBytes(fixture.backend());
    const qint64 cutPoints = Access::selectedPoints(fixture.backend());
    REQUIRE(cutBytes > 0);
    REQUIRE(Access::selectedBytes(fixture.secondBackend()) == cutBytes);
    REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);
    REQUIRE(Access::sseInflation(fixture.secondBackend()) == 1.0);

    const qint64 outsideBytes = totalGpuBytes() - fixture.gpuBytes();
    cwRenderBudgets budgets = fixture.budgets();

    //Room for one cut and half of the other, so the two together do not fit
    constexpr double kOneCutAndAHalf = 1.5;

    SECTION("a share that fits one cut but not both coarsens both") {
        budgets.gpuBudgetBytes = outsideBytes + qint64(cutBytes * kOneCutAndAHalf);
        fixture.setBudgets(budgets);

        fixture.renderFrame();
        fixture.renderFrame();

        CHECK(Access::sseInflation(fixture.backend()) > 1.0);
        CHECK(Access::sseInflation(fixture.secondBackend()) > 1.0);
    }

    SECTION("a textured item's bytes come off both clouds' shares") {
        //Room for both cuts and half a cut over, so neither cloud coarsens
        constexpr double kBothCutsAndAHalf = 2.5;
        budgets.gpuBudgetBytes = outsideBytes + qint64(cutBytes * kBothCutsAndAHalf);
        fixture.setBudgets(budgets);

        fixture.renderFrame();
        fixture.renderFrame();

        REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);
        REQUIRE(Access::sseInflation(fixture.secondBackend()) == 1.0);

        //The same budget, with a note's texture holding the difference
        const LedgerHold texture(cwRenderMemoryLedger::Category::TexturedItemTexture,
                                 cwRenderMemoryLedger::Residency::Gpu,
                                 cutBytes);

        fixture.renderFrame();
        fixture.renderFrame();

        CHECK(Access::sseInflation(fixture.backend()) > 1.0);
        CHECK(Access::sseInflation(fixture.secondBackend()) > 1.0);
    }

    SECTION("the point budget is the two clouds' total, not each cloud's own") {
        budgets.pointBudget = qint64(cutPoints * kOneCutAndAHalf);
        fixture.setBudgets(budgets);

        const qint64 rootPoints = fixture.manifest().nodes.at(kRootIndex).pointCount;
        constexpr int kFramesToShare = 40;
        for (int i = 0; i < kFramesToShare; i++) {
            fixture.renderFrame();
        }

        const qint64 first = Access::selectedPoints(fixture.backend());
        const qint64 second = Access::selectedPoints(fixture.secondBackend());

        //Every cloud draws its root whatever the budget says
        CHECK(first + second <= budgets.pointBudget + 2 * rootPoints);

        //Neither cloud gives way entirely: each keeps at least its even share,
        //so the one that gathers second is not starved down to its root and
        //coarsened forever.
        CHECK(first < cutPoints);
        CHECK(second < cutPoints);
        CHECK(first > rootPoints);
        CHECK(second > rootPoints);
        CHECK(Access::sseInflation(fixture.backend()) < cw::octree::kMaxSseInflation);
        CHECK(Access::sseInflation(fixture.secondBackend()) < cw::octree::kMaxSseInflation);
    }

    SECTION("a cloud that stops gathering hands its share back") {
        budgets.pointBudget = qint64(cutPoints * kOneCutAndAHalf);
        fixture.setBudgets(budgets);

        constexpr int kFramesToShare = 40;
        for (int i = 0; i < kFramesToShare; i++) {
            fixture.renderFrame();
        }

        const qint64 shared = Access::selectedPoints(fixture.backend());
        REQUIRE(shared < cutPoints);

        //The second cloud is hidden for this job, so gatherScene skips it and
        //its entry leaves the table rather than holding points it never draws.
        cwSceneGatherOptions hideSecond;
        hideSecond.hiddenObjectIds.insert(fixture.secondObjectId());

        constexpr int kFramesAlone = 40;
        for (int i = 0; i < kFramesAlone; i++) {
            fixture.renderFrame(hideSecond);
        }

        CHECK(Access::selectedPoints(fixture.backend()) > shared);
    }
}

TEST_CASE("An export job draws its own cut and leaves the live governor alone",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("export-point-budget"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const qint64 uncappedPoints = Access::selectedPoints(fixture.backend());
    const qint64 rootPoints = fixture.manifest().nodes.at(kRootIndex).pointCount;
    REQUIRE(uncappedPoints > 3 * rootPoints);
    REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);

    //A budget the live view cannot draw its cut under
    cwRenderBudgets budgets = fixture.budgets();
    budgets.pointBudget = rootPoints * 2;
    fixture.setBudgets(budgets);

    cwSceneGatherOptions exportJob;
    exportJob.liveFrame = false;

    fixture.renderFrame(exportJob);

    // The export renders once, at the detail its camera asked for: the per-frame
    // point budget is the live view's, and so is the relax probe.
    CHECK_FALSE(Access::pointCapped(fixture.backend()));
    CHECK(Access::selectedPoints(fixture.backend()) == uncappedPoints);
    CHECK(Access::sseInflation(fixture.backend()) == 1.0);
    CHECK(Access::relaxProbeFrame(fixture.backend()) == 0);
    CHECK(Access::desiredBytesRelaxed(fixture.backend()) == -1);

    //The live frame is still governed
    fixture.renderFrame();
    CHECK(Access::selectedPoints(fixture.backend()) <= budgets.pointBudget);
    CHECK(Access::pointCapped(fixture.backend()));
}

TEST_CASE("The relaxed cut is probed on probe frames, not every frame",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("relax-probe-cadence"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const qint64 cutBytes = Access::selectedBytes(fixture.backend());
    REQUIRE(cutBytes > 0);
    REQUIRE(Access::sseInflation(fixture.backend()) == 1.0);

    //A share the cut cannot hold, so the governor inflates
    const qint64 outsideBytes = totalGpuBytes() - fixture.gpuBytes();
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = outsideBytes + cutBytes / 2;
    fixture.setBudgets(budgets);

    constexpr int kFramesToInflate = 4;
    int inflatedFrames = 0;
    for (int i = 0; i < kFramesToInflate; i++) {
        fixture.renderFrame();
        if (Access::sseInflation(fixture.backend()) > 1.0) {
            inflatedFrames++;
        }
    }
    const double inflated = Access::sseInflation(fixture.backend());
    REQUIRE(inflated > 1.0);

    //Room to spare again: the step down waits for a probe frame
    budgets.gpuBudgetBytes = outsideBytes + 4 * cutBytes;
    fixture.setBudgets(budgets);

    constexpr int kFrameCeiling = 4 * cw::octree::kSseRelaxProbeFrames;
    for (int i = 0;
         i < kFrameCeiling && Access::sseInflation(fixture.backend()) >= inflated;
         i++) {
        fixture.renderFrame();
        inflatedFrames++;
    }

    CHECK(Access::sseInflation(fixture.backend()) < inflated);

    //The probe runs once every kSseRelaxProbeFrames frames the view spends inflated
    CHECK(inflatedFrames >= cw::octree::kSseRelaxProbeFrames);

    // One probe walks down as many steps as fit, so a deep inflation comes back
    // in one probe rather than one probe per step.
    CHECK(Access::sseInflation(fixture.backend()) == 1.0);
}

TEST_CASE("A streamed frame publishes the cut and its residency to the render stats",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("frame-stats"));
    fixture.setOrthoHeight(kCloseOrthoHeight);

    // A known baseline, so every count below came from a frame of this cloud.
    cwRenderFrameStats* frameStats = cwRenderFrameStats::instance();
    frameStats->publishPointCloud({});
    REQUIRE(frameStats->pointCloud() == cwRenderFrameStats::PointCloud{});

    // An upload budget well under one node's payload, so nodes finish loading
    // faster than the frames can take them and the wait is visible in the
    // counts rather than over between two frames.
    constexpr qint64 kUploadBudgetBytes = 1;
    const cwRenderBudgets defaultBudgets = fixture.budgets();
    cwRenderBudgets streamingBudgets = defaultBudgets;
    streamingBudgets.uploadBudgetBytesPerFrame = kUploadBudgetBytes;
    fixture.setBudgets(streamingBudgets);

    int mostLoadsInFlight = 0;
    int mostQueued = 0;
    QElapsedTimer timer;
    timer.start();
    fixture.renderFrame();
    while (Access::hasStreamingWork(fixture.backend()) && timer.elapsed() < kWaitTimeoutMs) {
        const cwRenderFrameStats::PointCloud streaming = frameStats->pointCloud();
        CHECK(streaming.residentNodes == Access::residentCount(fixture.backend()));
        // Payloads the upload budget held back are still on their way.
        CHECK(streaming.nodeLoadsInFlight >= Access::readyQueueCount(fixture.backend()));
        mostLoadsInFlight = std::max(mostLoadsInFlight, streaming.nodeLoadsInFlight);
        mostQueued = std::max(mostQueued, Access::readyQueueCount(fixture.backend()));
        fixture.renderFrame();
    }

    CHECK(mostQueued > 0);
    CHECK(mostLoadsInFlight > 0);

    fixture.setBudgets(defaultBudgets);
    fixture.renderUntilQuiet();

    const cwRenderFrameStats::PointCloud quiet = frameStats->pointCloud();
    CHECK(quiet.residentNodes == Access::residentCount(fixture.backend()));
    CHECK(quiet.residentNodes > 1);
    // Everything the cut wants is resident, so the cut and the draw list agree.
    CHECK(quiet.selectedNodes == fixture.drawableCount());
    CHECK(quiet.nodeLoadsInFlight == 0);
    CHECK(quiet.sseInflation == 1.0);

    // The pick mirror holds a CPU copy of every resident node's payload plus
    // the pick index derived from it, so it reads just above the GPU figure.
    CHECK(quiet.pickMirrorBytes == fixture.cpuBytes());
    CHECK(quiet.pickMirrorBytes > fixture.gpuBytes());

    // A budget the cloud cannot fit: the coarser cut the view falls back to is
    // what the HUD's multiplier reports.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = 1;
    fixture.setBudgets(budgets);
    fixture.setOrthoHeight(kFarOrthoHeight);

    fixture.renderFrame();
    fixture.renderFrame();

    const cwRenderFrameStats::PointCloud coarsened = frameStats->pointCloud();
    CHECK(coarsened.sseInflation == Access::sseInflation(fixture.backend()));
    CHECK(coarsened.sseInflation > 1.0);
    CHECK(coarsened.residentNodes == Access::residentCount(fixture.backend()));
    CHECK(coarsened.residentNodes == 1);
}

TEST_CASE("Residency is tracked as it moves, and a settled frame publishes nothing",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("residency-bookkeeping"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    // Every Resident node is in the tracked list exactly once, and no other node
    // is. The walk over every index is what the tracked list stands in for, so
    // the test does the walk the render thread no longer does.
    const auto trackedResidency = [&fixture]() {
        QVector<int> walked;
        for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
            if (Access::nodeState(fixture.backend(), i) == NodeState::Resident) {
                walked.append(i);
            }
        }

        QVector<int> tracked = Access::residentIndices(fixture.backend());
        std::sort(tracked.begin(), tracked.end());

        CHECK(tracked == walked);
        CHECK(Access::residentCount(fixture.backend()) == int(walked.size()));
        return int(walked.size());
    };

    REQUIRE(trackedResidency() > 2);

    cwRenderFrameStats* frameStats = cwRenderFrameStats::instance();
    constexpr int kSettledFrames = 60;

    // A settled cloud draws the same cut out of the same nodes every frame. The
    // live frame still publishes its culling tally; the point cloud, whose
    // numbers have not moved, publishes nothing.
    const quint64 liveRevision = frameStats->revision();
    for (int i = 0; i < kSettledFrames; i++) {
        fixture.renderFrame();
    }
    CHECK(frameStats->revision() - liveRevision == quint64(kSettledFrames));
    CHECK(trackedResidency() > 2);

    // The same frames with nothing else publishing: the stats stand completely
    // still, which is what the HUD watches.
    cwSceneGatherOptions offscreenJob;
    offscreenJob.liveFrame = false;

    const quint64 quietRevision = frameStats->revision();
    for (int i = 0; i < kSettledFrames; i++) {
        fixture.renderFrame(offscreenJob);
    }
    CHECK(frameStats->revision() == quietRevision);

    // Release empties the tracking with the nodes...
    fixture.mutableBackend().releaseStreamedResources();
    CHECK(Access::residentIndices(fixture.backend()).isEmpty());
    CHECK(trackedResidency() == 0);

    // ...and streaming the same cut back in rebuilds it.
    fixture.renderUntilQuiet();
    CHECK(trackedResidency() > 2);
}

TEST_CASE("A constants slot comes off the cold queue rather than a walk of the table",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    // Two windows a third of the passage apart, as in the churn test above:
    // each cut is its own handful of nodes, so holding both costs more slots
    // than holding either.
    const auto stretchOffset = [](const PointCloudFixture& cloud, double along) {
        return QVector3D(float(along * kTubeLength),
                         float(kTubeBend * along * along),
                         0.0f) - cloud.center();
    };

    // What each window draws and what holding both costs, measured with the
    // app's pool depth, so the capped cloud below can be given more slots than
    // either cut needs and fewer than both windows together.
    int largestCut = 0;
    int bothWindows = 0;
    {
        PointCloudFixture probe(rhi.get(), QStringLiteral("cold-queue-probe"));
        probe.setOrthoHeight(kChurnOrthoHeight);

        probe.setViewOffset(stretchOffset(probe, kChurnNearAlong));
        probe.renderUntilQuiet();
        largestCut = probe.drawableCount();

        probe.setViewOffset(stretchOffset(probe, kChurnFarAlong));
        probe.renderUntilQuiet();
        largestCut = std::max(largestCut, probe.drawableCount());
        bothWindows = Access::residentCount(probe.backend());
    }

    // One slot over the larger cut: every frame can draw what it asks for, and
    // the camera coming back to a window it left has to evict for the slots.
    const int slotCount = largestCut + 1;
    REQUIRE(largestCut > 1);
    REQUIRE(bothWindows > slotCount);

    PointCloudFixture fixture(rhi.get(), QStringLiteral("cold-queue"));
    Access::setMaxResidentNodes(fixture.mutableBackend(), slotCount);
    fixture.setOrthoHeight(kChurnOrthoHeight);

    const QVector3D nearEnd = stretchOffset(fixture, kChurnNearAlong);
    const QVector3D farEnd = stretchOffset(fixture, kChurnFarAlong);

    // Every Resident node is in the tracked list exactly once, and no other node
    // is — the walk over every index the render thread no longer does, done
    // here instead. Every slot is either free or held by one of them.
    const auto checkTracking = [&fixture]() {
        QVector<int> walked;
        for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
            if (Access::nodeState(fixture.backend(), i) == NodeState::Resident) {
                walked.append(i);
            }
        }

        QVector<int> tracked = Access::residentIndices(fixture.backend());
        std::sort(tracked.begin(), tracked.end());

        CHECK(tracked == walked);
        CHECK(Access::residentCount(fixture.backend()) == int(walked.size()));
        CHECK(Access::residentCount(fixture.backend())
                  + Access::freeSlotCount(fixture.backend())
              == Access::maxResidentNodes(fixture.backend()));
    };

    const ProfileLogCapture capture(QStringLiteral("cw.profile.render.debug=true"));
    const QString renderPrefix = QStringLiteral("render");

    // The camera alternates between the two windows, so the nodes of the window
    // it left go cold and the nodes of the one it returns to have to come back.
    const auto alternate = [&fixture, &nearEnd, &farEnd]() {
        for (int frame = 0; frame < cw::profile::kProfileBlockFrames; frame++) {
            fixture.setViewOffset(frame % 2 == 0 ? nearEnd : farEnd);
            fixture.renderFrame();
        }
    };

    alternate();
    checkTracking();

    QStringList lines = ProfileLogCapture::linesStartingWith(renderPrefix);
    REQUIRE_FALSE(lines.isEmpty());

    // Slots ran out and the uploads evicted for them — and it cost no walk of
    // residency at all. The byte budget is untouched here, so enforceGpuBudget
    // never runs and residencyStats is the slow branch that used to serve every
    // one of these evictions.
    const QString churned = lines.last();
    CHECK(fieldOf(churned, QStringLiteral("evictions")) > 0);
    CHECK(fieldOf(churned, QStringLiteral("slotEvictUs")) > 0);
    CHECK(fieldOf(churned, QStringLiteral("residencyStatsUs")) == 0);
    CHECK(fieldOf(churned, QStringLiteral("uploads")) > 0);

    CHECK(Access::residentCount(fixture.backend()) <= slotCount);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
    CHECK(fixture.drawableCount() > 0);

    // Entries a node's return to the cut left behind are swept, so a long
    // session's queue stays proportional to residency rather than to the frames
    // that have run.
    constexpr int kColdQueueCeiling = 2048;
    constexpr int kChurnBlocks = 4;
    for (int block = 0; block < kChurnBlocks; block++) {
        alternate();
        CHECK(Access::coldNodeCount(fixture.backend()) < kColdQueueCeiling);
    }
    checkTracking();

    // Half the settled bytes on top of the shallow pool, so enforceGpuBudget
    // releases nodes out of the middle of the tracked list as well: that is the
    // swap-remove's back-pointer, which nothing else here would notice.
    const qint64 cloudBytes = fixture.gpuBytes();
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = totalGpuBytes() - cloudBytes + cloudBytes / 2;
    fixture.setBudgets(budgets);

    alternate();
    checkTracking();
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
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

    // A failed node holds no buffer, so the stats count it with the absent
    // ones rather than the resident ones.
    const cwRenderFrameStats::PointCloud stats = cwRenderFrameStats::instance()->pointCloud();
    CHECK(stats.residentNodes == Access::residentCount(fixture.backend()));
    CHECK(stats.nodeLoadsInFlight == 0);

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

    // The HUD would otherwise keep showing the cut of a view that is gone.
    CHECK(cwRenderFrameStats::instance()->pointCloud() == cwRenderFrameStats::PointCloud{});

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

    // The node table went with it, so the residency tracking beside it has to
    // be empty too, or the next upload would write past the list it indexes.
    CHECK(Access::residentCount(fixture.backend()) == 0);
    CHECK(Access::residentIndices(fixture.backend()).isEmpty());
    CHECK(Access::coldNodeCount(fixture.backend()) == 0);
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
    qint64 uploaded = 0;
    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        if (Access::nodeState(fixture.backend(), i) == NodeState::Resident) {
            mirrored += Access::mirrorBytes(fixture.backend(), i);
            uploaded += Access::nodeBytes(fixture.backend(), i).size();
        }
    }

    // The mirror is an implicit share of the very bytes that were uploaded plus
    // the pick index built over them, so the CPU ledger runs ahead of the GPU
    // one by exactly the indexes.
    REQUIRE(mirrored > uploaded);
    CHECK(fixture.cpuBytes() == mirrored);
    CHECK(fixture.gpuBytes() == uploaded);
}

TEST_CASE("A cut bigger than the budget share coarsens rather than evicting what it draws",
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
    // cut wants and the cut's bytes are over the share by construction.
    // Evicting a selected node would only re-request and re-upload it next
    // frame — the same node read off disk every frame forever.
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

TEST_CASE("Sprites grow to the spacing the cut refines to so it reads as a surface",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("spacing-coverage"));
    fixture.setReadbackEnabled(true);
    fixture.setOrthoHeight(kCoarseOrthoHeight);

    // Hold the cut at the root while the camera is close enough for the root's
    // sample spacing to be several pixels wide — the shape the governor's
    // screen-space-error inflation puts on screen, and where a sprite that
    // knows nothing of the spacing leaves the surface full of holes. The root
    // is the whole cut here, and its spacing is what the raised threshold lets
    // it stop at.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.screenSpaceErrorPx = cw::budgets::kMaxScreenSpaceErrorPx;
    fixture.setBudgets(budgets);

    // At zero coverage every sprite falls to the shader's one-pixel floor, so
    // the coverage is the only thing that can make one bigger.
    fixture.render().setSpacingCoverage(0.0f);
    fixture.synchronize();
    fixture.renderUntilResident(kRootIndex);
    fixture.renderFrame();
    REQUIRE(fixture.drawableCount() == 1);

    const qsizetype bare = fixture.litPixels().size();
    REQUIRE(bare > 0);

    // Every sprite is at the shader's one-pixel floor here, so the root cannot
    // light more pixels than it has points — points that share a pixel, and
    // points the ortho frame leaves off the target, only light fewer.
    const qint64 rootPoints = fixture.manifest().nodes.at(kRootIndex).pointCount;
    CHECK(bare <= rootPoints);

    fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
    fixture.synchronize();
    fixture.renderFrame();
    REQUIRE(fixture.drawableCount() == 1);

    const QVector<QPoint> covered = fixture.litPixels();
    CHECK(covered.size() >= kCoveredLitMultiple * bare);

    // The camera looks at the cloud's center, so the center row of the target
    // crosses the passage. gl_PointSize is a side length, so a sprite spans
    // kDefaultSpacingCoverage of the spacing the cut refines to — past one
    // spacing, so neighboring sprites overlap and the gaps a bare cut shows
    // close, which is what kMaxUnlitRunPx bounds.
    const int centerRow = fixture.colorSize().height() / 2;
    const int run = longestUnlitRun(covered, centerRow);
    INFO("longest unlit run on the center row: " << run);
    CHECK(run >= 0);
    CHECK(run <= kMaxUnlitRunPx);
}

TEST_CASE("The wheel coverage changes every sprite", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("wheel-coverage"));
    fixture.setReadbackEnabled(true);
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    // The cut is refined, so its coarse ancestors are on screen beside its
    // finest level — the case a per-node floor blew up into sprites the wheel
    // could not move.
    REQUIRE(selectedLevelCount(fixture) > 1);

    // Both coverages have to clear the shader's one-pixel clamp, or that clamp
    // — not the wheel — is what the sprites measure and the growth below says
    // nothing.
    const double threshold = refineThresholdPx(fixture);
    REQUIRE(spriteSidePx(kWheelCoverage, threshold) > kMinSpritePx);

    const float doubled = 2.0f * kWheelCoverage;

    fixture.render().setSpacingCoverage(kWheelCoverage);
    fixture.synchronize();
    fixture.renderFrame();
    const qsizetype lit = fixture.litPixels().size();
    REQUIRE(lit > 0);

    fixture.render().setSpacingCoverage(doubled);
    fixture.synchronize();
    fixture.renderFrame();

    CHECK(fixture.litPixels().size() > lit);

    // Doubling the coverage doubles every sprite and no sprite outgrows it: an
    // ancestor sized off its own coarse spacing would measure several times
    // this bound.
    const double side = drawnSpriteSidePx(fixture, doubled);
    INFO("sprite side at twice the wheel's coverage: " << side);
    CHECK(side <= spriteSideUpperBoundPx(double(doubled), threshold));
}

TEST_CASE("Every level on screen shares one sprite size", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("one-sprite-size"));
    fixture.setReadbackEnabled(true);
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    // Several levels at once is the whole premise: the cut is additive, so the
    // refined region carries its coarse ancestors onto the screen with it. An
    // orthographic camera puts every one of them at w = 1, so the spacing rule
    // hands them all the same size and the ancestors cannot blob.
    REQUIRE(selectedLevelCount(fixture) > 1);

    const double refinedThreshold = refineThresholdPx(fixture);

    const double refined = drawnSpriteSidePx(fixture,
                                             cw::pointcloud::kDefaultSpacingCoverage);
    INFO("sprite side on the refined cut: " << refined);
    CHECK(refined <= spriteSideUpperBoundPx(
              cw::pointcloud::kDefaultSpacingCoverage, refinedThreshold));

    // The same camera with the cut held coarse. Every level the cut holds is
    // still drawn, and the one bound that describes them all moves only because
    // the spacing the cut refines to moved.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.screenSpaceErrorPx = cw::budgets::kMaxScreenSpaceErrorPx;
    fixture.setBudgets(budgets);
    fixture.renderUntilQuiet();

    const double coarseThreshold = refineThresholdPx(fixture);
    REQUIRE(coarseThreshold > refinedThreshold);

    const double coarse = drawnSpriteSidePx(fixture,
                                            cw::pointcloud::kDefaultSpacingCoverage);
    INFO("sprite side on the coarse cut: " << coarse);
    CHECK(coarse <= spriteSideUpperBoundPx(
              cw::pointcloud::kDefaultSpacingCoverage, coarseThreshold));
}

TEST_CASE("The spacing floor rises with the threshold the cut refines to",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("floor-falls-back"));
    fixture.setReadbackEnabled(true);
    fixture.setOrthoHeight(kCloseOrthoHeight);

    cwRenderBudgets budgets = fixture.budgets();
    budgets.screenSpaceErrorPx = cw::budgets::kMaxScreenSpaceErrorPx;
    fixture.setBudgets(budgets);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    const double coarseThreshold = refineThresholdPx(fixture);
    const double coarseFloor = spriteSidePx(cw::pointcloud::kDefaultSpacingCoverage,
                                            coarseThreshold);
    const double coarse = drawnSpriteSidePx(fixture,
                                            cw::pointcloud::kDefaultSpacingCoverage);
    INFO("sprite side while the cut is held coarse: " << coarse);
    CHECK(coarse >= coarseFloor - kSpriteTolerancePx);
    CHECK(coarse <= spriteSideUpperBoundPx(coarseFloor));

    // Let the threshold fall back to its default. The cut refines, the spacing
    // it aims for collapses, and every sprite goes with it — which is the whole
    // point of sizing off the rule the cut is following.
    budgets.screenSpaceErrorPx = cw::budgets::kDefaultScreenSpaceErrorPx;
    fixture.setBudgets(budgets);
    fixture.renderUntilQuiet();

    const double refinedThreshold = refineThresholdPx(fixture);
    REQUIRE(refinedThreshold < coarseThreshold);

    const double refined = drawnSpriteSidePx(fixture,
                                             cw::pointcloud::kDefaultSpacingCoverage);
    INFO("sprite side once the threshold fell back: " << refined);
    CHECK(refined < coarse);
    CHECK(refined <= spriteSideUpperBoundPx(
              cw::pointcloud::kDefaultSpacingCoverage, refinedThreshold));
}

TEST_CASE("A refine threshold the view moves reaches the shader",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("cut-reaches-shader"));
    fixture.setReadbackEnabled(true);
    fixture.setOrthoHeight(kCloseOrthoHeight);

    fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);

    cwRenderBudgets budgets = fixture.budgets();
    budgets.screenSpaceErrorPx = cw::budgets::kMaxScreenSpaceErrorPx;
    fixture.setBudgets(budgets);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    const QVector<QPoint> coarse = fixture.litPixels();
    REQUIRE(!coarse.isEmpty());

    // Nothing about the render object changes from here, so production would
    // run updateResources() on none of these frames: the threshold moving is
    // the only thing that can resize the sprites.
    fixture.setResourceUpdateAfterSyncOnly(true);
    budgets.screenSpaceErrorPx = cw::budgets::kDefaultScreenSpaceErrorPx;
    fixture.setBudgets(budgets);
    fixture.renderUntilQuiet();

    const QVector<QPoint> refined = fixture.litPixels();
    CHECK(refined.size() != coarse.size());

    // The same camera and the same threshold, drawn once a change to the
    // coverage and back has forced the slot to be rewritten. The sprites were
    // already this size without it.
    fixture.render().setSpacingCoverage(kWheelCoverage);
    fixture.synchronize();
    fixture.renderFrame();

    fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
    fixture.synchronize();
    fixture.renderFrame();

    const QVector<QPoint> resynced = fixture.litPixels();
    CHECK(resynced.size() == refined.size());
    CHECK(topmostLitRow(resynced) == topmostLitRow(refined));
}

TEST_CASE("The floor holds to what is drawn while the children stream",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("streaming-floor"));

    // One node a frame, so the camera asks for the fine levels long before any
    // of them is on screen. That window is what the drawn floor exists for: the
    // coarse ancestors are all there is to draw, and a floor taken only from
    // the threshold the cut asked for would shrink out from under them and show
    // holes.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.uploadBudgetBytesPerFrame = kOneNodePerFrameUploadBytes;
    fixture.setBudgets(budgets);
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.synchronize();
    fixture.renderUntilResident(kRootIndex);
    fixture.renderFrame();

    const int drawn = finestDrawnLevel(fixture);
    REQUIRE(drawn < finestSelectedLevel(fixture));
    // The root draws over everything finer the frame got to, so its own floor
    // is the finest spacing on screen rather than the root's coarse spacing.
    CHECK(drawnFloorSpacing(fixture) == float(fixture.manifest().spacing(drawn)));

    // The floor is a per-node value, so the nodes the frame left coarse keep
    // their own wide sprites while the refined subtrees shrink. One floor
    // shared by the cloud would pass every check above it.
    const int starved = renderUntilStarvedNode(fixture);
    REQUIRE(starved >= 0);

    const QVector<int> drawnNodes = Access::drawnNodes(fixture.backend());
    const int finest = finestDrawnLevel(fixture);
    for (int node : drawnNodes) {
        const int level = finestDrawnLevelUnder(fixture.manifest(), drawnNodes, node);
        CHECK(nodeFloorSpacing(fixture, node) == float(fixture.manifest().spacing(level)));
    }

    // The starved node has nothing drawn under it while another subtree has
    // refined past it, so its floor stays wider than the finest spacing on
    // screen — otherwise its cell draws holes until its children arrive.
    CHECK(nodeFloorSpacing(fixture, starved) > float(fixture.manifest().spacing(finest)));
}

TEST_CASE("A frame that draws nothing keeps the sprite size it had",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("culled-floor"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    const double refined = drawnFloorSpacing(fixture);
    REQUIRE(refined < fixture.manifest().spacing(kRootLevel));

    // Panned off screen the cloud draws nothing, which says nothing about how
    // large its sprites should be — so the last thing it drew stands.
    fixture.lookAway();
    fixture.renderFrame();

    REQUIRE(Access::selectedLevels(fixture.backend()).isEmpty());
    CHECK(drawnFloorSpacing(fixture) == refined);
}

TEST_CASE("A cloud starts out at its root's spacing", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("root-seed"));
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.synchronize();
    fixture.renderUntilResident(kRootIndex);

    // The root is the only thing drawn, so it stands for its own points alone
    // and floors at its own spacing rather than at a floor of nothing.
    CHECK(drawnFloorSpacing(fixture) == float(fixture.manifest().spacing(kRootLevel)));

    fixture.render().setOctree(cwPointOctreeSource());
    fixture.synchronize();

    CHECK(drawnFloorSpacing(fixture) == 0.0);
}

TEST_CASE("The per-cloud uniform carries the view's refine threshold",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("per-cloud-uniform"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.render().setSpacingCoverage(kWheelCoverage);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    const Access::PerCloudUniform live =
        Access::liveAppearanceUniform(fixture.backend());

    // The coverage reaches the shader as the render object holds it. Turning it
    // into a size is the shader's job, one vertex at a time, so the CPU folds
    // nothing here.
    CHECK(live.spacingCoverage == kWheelCoverage);

    // The second value is what the shader turns back into a world spacing at
    // each vertex's depth: the view's screen-space error times this cloud's
    // inflation, which is the projected spacing the cut is refining to.
    CHECK(live.sseThresholdPx
          == float(fixture.budgets().screenSpaceErrorPx
                   * Access::sseInflation(fixture.backend())));

    // The view moving its screen-space error moves the cut and the sprites
    // together, without the render object changing at all.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.screenSpaceErrorPx = cw::budgets::kMaxScreenSpaceErrorPx;
    fixture.setBudgets(budgets);
    fixture.renderUntilQuiet();

    const Access::PerCloudUniform coarse =
        Access::liveAppearanceUniform(fixture.backend());
    CHECK(coarse.sseThresholdPx > live.sseThresholdPx);
    CHECK(coarse.sseThresholdPx
          == float(cw::budgets::kMaxScreenSpaceErrorPx
                   * Access::sseInflation(fixture.backend())));
}

TEST_CASE("An export job leaves the live sprite size alone", "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("export-floor"));
    fixture.setReadbackEnabled(true);
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
    fixture.synchronize();
    fixture.renderFrame();

    const Access::PerCloudUniform live =
        Access::liveAppearanceUniform(fixture.backend());
    const double liveFloor = drawnFloorSpacing(fixture);
    const QVector<QPoint> before = fixture.litPixels();
    REQUIRE(!before.isEmpty());

    // An export of the same cloud from far enough back that its own cut is the
    // root alone. Its gather has to leave the live view's floor where the live
    // frame put it, or the tiles of one export come out at different sizes and
    // the live view redraws at the export camera's size.
    cwSceneGatherOptions exportJob;
    exportJob.liveFrame = false;
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderFrame(exportJob);

    const Access::PerCloudUniform after =
        Access::liveAppearanceUniform(fixture.backend());
    CHECK(after.spacingCoverage == live.spacingCoverage);
    CHECK(after.sseThresholdPx == live.sseThresholdPx);
    CHECK(drawnFloorSpacing(fixture) == liveFloor);

    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderFrame();

    CHECK(fixture.litPixels().size() == before.size());
    CHECK(topmostLitRow(fixture.litPixels()) == topmostLitRow(before));
}

TEST_CASE("An export job's requested coverage reaches the shader",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("export-coverage"));
    fixture.setReadbackEnabled(true);
    fixture.setOrthoHeight(kCoarseOrthoHeight);

    cwRenderBudgets budgets = fixture.budgets();
    budgets.screenSpaceErrorPx = cw::budgets::kMaxScreenSpaceErrorPx;
    fixture.setBudgets(budgets);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    // The live cloud draws at the shader's one-pixel floor: the coverage off.
    fixture.render().setSpacingCoverage(0.0f);
    fixture.synchronize();
    fixture.renderFrame();

    const qsizetype bare = fixture.litPixels().size();
    REQUIRE(bare > 0);

    // The job asks for the coverage back on. The refine
    // threshold the coverage is measured against is the cloud's, not the
    // job's, so the job's sprites come out of the same expression the live
    // ones do.
    cwPointCloudAppearance appearance;
    appearance.spacingCoverage = cw::pointcloud::kDefaultSpacingCoverage;
    fixture.setAppearanceOverride(appearance);
    fixture.renderFrame();

    const qsizetype overridden = fixture.litPixels().size();
    CHECK(overridden > bare);

    fixture.clearAppearanceOverride();
    fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
    fixture.synchronize();
    fixture.renderFrame();

    CHECK(overridden == fixture.litPixels().size());
}

TEST_CASE("Perspective sprites share one size across levels too",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("perspective-size"));
    fixture.setReadbackEnabled(true);
    fixture.setPerspective(true);
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.synchronize();
    fixture.renderUntilQuiet();

    REQUIRE(selectedLevelCount(fixture) > 1);

    const double threshold = refineThresholdPx(fixture);
    const double floorPx = cw::pointcloud::kDefaultSpacingCoverage * threshold;

    // The rule's world spacing and the sprite it sizes both carry the same 1/w,
    // so the two cancel: a sprite the spacing rule sizes measures the same
    // number of pixels at every depth on screen, which is what makes a far tile
    // cover its own coarser cell.
    const double side = drawnSpriteSidePx(fixture,
                                          cw::pointcloud::kDefaultSpacingCoverage);
    INFO("perspective sprite side: " << side);
    CHECK(side <= spriteSideUpperBoundPx(floorPx));
}

TEST_CASE("The cloud's world bounds are the root cube padded by the sprite side",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("world-bounds"));

    const std::optional<QBox3D> bounds = fixture.backend().worldBounds();
    REQUIRE(bounds.has_value());

    const QBox3D root = fixture.manifest().nodeBounds(kRootIndex);
    // The root's sprites are the widest the cloud draws: a cloud framed in the
    // view is drawn no coarser than its root, so the root's own spacing bounds
    // every sprite the spacing rule can ask for.
    const float rootSpacing = float(fixture.manifest().spacing(kRootLevel));
    const float radius = fixture.render().spacingCoverage() * rootSpacing;
    const QVector3D padding(radius, radius, radius);

    CHECK(bounds->minimum() == root.minimum() - padding);
    CHECK(bounds->maximum() == root.maximum() + padding);

    // The wheel is the only thing that moves the padding, so a coverage change
    // has to carry straight through to the box.
    fixture.render().setSpacingCoverage(kWheelCoverage);
    fixture.synchronize();

    const std::optional<QBox3D> wheeledBounds = fixture.backend().worldBounds();
    REQUIRE(wheeledBounds.has_value());

    const float wheeledPad = kWheelCoverage * rootSpacing;
    const QVector3D wheeledPadding(wheeledPad, wheeledPad, wheeledPad);
    CHECK(wheeledBounds->minimum() == root.minimum() - wheeledPadding);
    CHECK(wheeledBounds->maximum() == root.maximum() + wheeledPadding);

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

    // The same pick again, this time reading what the query wrote: the nodes
    // reached the pick set with their indexes, so the query descended group
    // and leaf boxes instead of reading every resident point. Without the
    // index the line would say groups=0 leaves=0 and scan the lot.
    qint64 residentPoints = 0;
    for (int index = 0; index < Access::nodeCount(fixture.backend()); index++) {
        if (Access::nodeState(fixture.backend(), index) == NodeState::Resident) {
            residentPoints +=
                Access::nodeBytes(fixture.backend(), index).size() / cw::octree::kBytesPerPoint;
        }
    }
    REQUIRE(residentPoints > 0);

    cwScenePick::Result refined;
    {
        const ProfileLogCapture capture(QStringLiteral("cw.profile.pick.debug=true"));
        refined = cwScenePick::snappedPoint(
            closeCamera.project(aimedAt), closeCamera, *intersecter, kLinePixelRadius);

        const QStringList picks =
            ProfileLogCapture::linesStartingWith(QStringLiteral("pick kind=exactHit"));
        REQUIRE_FALSE(picks.isEmpty());
        CHECK(fieldOf(picks.constFirst(), QStringLiteral("groups")) > 0);
        CHECK(fieldOf(picks.constFirst(), QStringLiteral("leaves")) > 0);
        CHECK(fieldOf(picks.constFirst(), QStringLiteral("points")) < residentPoints);
    }
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

TEST_CASE("The render profile category reports one line per block of frames",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("profile-render"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const QString renderPrefix = QStringLiteral("render");

    {
        // The category's declared level is the default, and the default has to
        // stay silent: no rule at all here.
        const ProfileLogCapture capture{QString()};
        CHECK_FALSE(lcProfileRender().isDebugEnabled());

        for (int frame = 0; frame < cw::profile::kProfileBlockFrames; frame++) {
            fixture.renderFrame();
        }

        CHECK(ProfileLogCapture::linesStartingWith(renderPrefix).isEmpty());
    }

    const ProfileLogCapture capture(QStringLiteral("cw.profile.render.debug=true"));

    // One frame short of the block: still nothing to say.
    for (int frame = 0; frame < cw::profile::kProfileBlockFrames - 1; frame++) {
        fixture.renderFrame();
    }
    CHECK(ProfileLogCapture::linesStartingWith(renderPrefix).isEmpty());

    fixture.renderFrame();

    const QStringList lines = ProfileLogCapture::linesStartingWith(renderPrefix);
    REQUIRE(lines.size() == 1);

    const QString line = lines.first();

    // The runner reads the line by key, so every key is part of the contract.
    const QStringList expectedKeys = {
        QStringLiteral("frames"), QStringLiteral("streamFrames"),
        QStringLiteral("gatherMeanUs"), QStringLiteral("gatherMaxUs"),
        QStringLiteral("selectNodesMeanUs"), QStringLiteral("selectNodesMaxUs"),
        QStringLiteral("requestLoopMeanUs"), QStringLiteral("requestLoopMaxUs"),
        QStringLiteral("cancelLoopMeanUs"), QStringLiteral("cancelLoopMaxUs"),
        QStringLiteral("streamMeanUs"), QStringLiteral("streamMaxUs"),
        QStringLiteral("uploadMeanUs"), QStringLiteral("uploadMaxUs"),
        QStringLiteral("publishPickMeanUs"), QStringLiteral("publishPickMaxUs"),
        QStringLiteral("publishStatsMeanUs"), QStringLiteral("publishStatsMaxUs"),
        QStringLiteral("enforceBudgetMeanUs"), QStringLiteral("enforceBudgetMaxUs"),
        QStringLiteral("cutMed"), QStringLiteral("cutMax"), QStringLiteral("resident"),
        QStringLiteral("requests"), QStringLiteral("cancels"), QStringLiteral("uploads"),
        QStringLiteral("evictions"), QStringLiteral("pendingLoads"),
        QStringLiteral("sse"), QStringLiteral("gpuMb"), QStringLiteral("budgetMb"),
        QStringLiteral("pointsMed"), QStringLiteral("pointsMax"),
        QStringLiteral("slotEvictUs"), QStringLiteral("residencyStatsUs"),
        QStringLiteral("frameMsMed"), QStringLiteral("frameMsP95"),
        QStringLiteral("frameMsMax")
    };

    // "render frame=N key=value ..." in this order, one space between pairs.
    const QStringList pairs = line.split(QLatin1Char(' '));
    REQUIRE(pairs.size() == expectedKeys.size() + 2);
    CHECK(pairs.at(0) == QStringLiteral("render"));
    CHECK(pairs.at(1).startsWith(QStringLiteral("frame=")));
    for (int i = 0; i < expectedKeys.size(); i++) {
        CHECK(pairs.at(i + 2).startsWith(expectedKeys.at(i) + QLatin1Char('=')));
    }

    CHECK(line.contains(QStringLiteral(" frames=%1 streamFrames=%1 ")
                            .arg(cw::profile::kProfileBlockFrames)));

    // The counters the block reports are the streamer's own state, not numbers
    // the line re-derives.
    const QString resident = QStringLiteral(" resident=%1 ")
                                 .arg(Access::residentCount(fixture.backend()));
    CHECK(line.contains(resident));
}

TEST_CASE("The load profile category reports one line per node load",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    const ProfileLogCapture capture(QStringLiteral("cw.profile.load.debug=true"));

    PointCloudFixture fixture(rhi.get(), QStringLiteral("profile-load"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const QStringList lines = ProfileLogCapture::linesStartingWith(QStringLiteral("load"));
    REQUIRE_FALSE(lines.isEmpty());

    // Every load reports the bytes it read, and those are the bytes the
    // manifest says the node holds.
    QSet<qint64> manifestBytes;
    for (int i = 0; i < fixture.manifest().nodes.size(); i++) {
        manifestBytes.insert(fixture.manifest().nodes.at(i).byteSize);
    }

    for (const QString& line : lines) {
        const QStringList pairs = line.split(QLatin1Char(' '));
        REQUIRE(pairs.size() == 3);
        CHECK(pairs.at(1).startsWith(QStringLiteral("us=")));
        REQUIRE(pairs.at(2).startsWith(QStringLiteral("bytes=")));

        bool read = false;
        const qint64 bytes = pairs.at(2).mid(QStringLiteral("bytes=").size()).toLongLong(&read);
        CHECK(read);
        CHECK(manifestBytes.contains(bytes));
    }
}

TEST_CASE("A cloud that leaves the view drops its cut and settles its loads",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("culled-settles"));

    // A CPU cap of one byte lets a single load through at a time, so the rest
    // of the close cut is still queued when the cloud leaves the view.
    cwRenderBudgets budgets = fixture.budgets();
    budgets.cpuBudgetBytes = 1;
    fixture.setBudgets(budgets);

    //The far cut is the root alone, so the root is all the cloud holds
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilQuiet();
    REQUIRE(Access::residentCount(fixture.backend()) == 1);
    REQUIRE(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);

    //One frame of the close cut, which asks for a great many more nodes
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderFrame();
    REQUIRE(fixture.streamingWork());

    SECTION("panned off screen") {
        fixture.lookAway();
    }

    SECTION("hidden") {
        fixture.setCloudVisible(false);
    }

    //The first frame off screen still publishes the cut the last drawn frame
    //asked for — the nodes that would otherwise stream in behind it
    fixture.renderFrame();
    const int lastDrawnCut = cwRenderFrameStats::instance()->pointCloud().selectedNodes;
    REQUIRE(lastDrawnCut > 1);

    fixture.renderUntilQuiet();

    //The cut nothing draws any more, and the loads it asked for, are both gone
    const cwRenderFrameStats::PointCloud stats =
        cwRenderFrameStats::instance()->pointCloud();
    CHECK(stats.selectedNodes == 0);
    CHECK(stats.nodeLoadsInFlight == 0);
    CHECK(Access::selectedPoints(fixture.backend()) == 0);

    //Only what was already in hand landed: the root, plus at most the one load
    //the CPU cap let through. The cut the cloud left behind never streamed in.
    constexpr int kLoadsInFlightUnderCap = 1;
    const int settled = Access::residentCount(fixture.backend());
    CHECK(settled <= 1 + kLoadsInFlightUnderCap);
    CHECK(settled < lastDrawnCut);
    CHECK(stats.residentNodes == settled);

    //Eviction is untouched, so the root the cloud already held stays, and
    //nothing more arrives frame after frame
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);

    constexpr int kSettledFrames = 20;
    for (int i = 0; i < kSettledFrames; i++) {
        fixture.renderFrame();
    }
    CHECK(Access::residentCount(fixture.backend()) == settled);
    CHECK_FALSE(fixture.streamingWork());
}

TEST_CASE("An export's loads survive the frames the cloud is out of the live view",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("culled-export"));

    //The live view sits far back, where its cut is the root alone, while the
    //job wants the close cut
    fixture.setOrthoHeight(kFarOrthoHeight);
    fixture.renderUntilQuiet();

    const cwRHIObject::RenderData job = fixture.jobRenderData(kCloseOrthoHeight);
    REQUIRE_FALSE(fixture.mutableBackend().residencyReady(job));

    //What the job asked for, flagged for it, asked for exactly once
    QVector<int> exportNodes;
    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        if (Access::exportRequested(fixture.backend(), i)) {
            exportNodes.append(i);
        }
    }
    REQUIRE(exportNodes.size() > 1);

    //...and now the live view pans off the cloud entirely, so every frame from
    //here on runs gatherCulled rather than gather. The job says nothing more.
    fixture.lookAway();

    constexpr int kCulledFrames = 5;
    for (int i = 0; i < kCulledFrames; i++) {
        fixture.renderFrame();
    }

    //The cut the live view dropped took none of the job's loads with it: each
    //one is still in flight, or has already landed
    for (const int index : std::as_const(exportNodes)) {
        const NodeState state = Access::nodeState(fixture.backend(), index);
        CHECK(state != NodeState::Absent);
        if (state == NodeState::Requested) {
            CHECK(Access::exportRequested(fixture.backend(), index));
        }
    }

    //And the job still finishes, off screen the whole way
    QElapsedTimer timer;
    timer.start();
    bool ready = false;
    while (!ready && timer.elapsed() < kWaitTimeoutMs) {
        fixture.renderFrame();
        QThread::msleep(kFramePauseMs);
        ready = fixture.mutableBackend().residencyReady(job);
    }

    CHECK(ready);
    CHECK(Access::residentCount(fixture.backend()) > 1);

    //Once the job's cut has landed, nothing is left flagged for it
    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        CHECK_FALSE(Access::exportRequested(fixture.backend(), i));
    }
}

TEST_CASE("An offscreen job's own culled tiles leave the live view's cut alone",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("culled-offscreen-job"));

    //One live frame of the close cut, with the loads it asked for in flight
    cwRenderBudgets budgets = fixture.budgets();
    budgets.cpuBudgetBytes = 1;
    fixture.setBudgets(budgets);

    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderFrame();

    const qint64 livePoints = Access::selectedPoints(fixture.backend());
    REQUIRE(livePoints > 0);

    QVector<int> liveRequests;
    for (int i = 0; i < Access::nodeCount(fixture.backend()); i++) {
        if (Access::nodeState(fixture.backend(), i) == NodeState::Requested) {
            liveRequests.append(i);
        }
    }
    REQUIRE_FALSE(liveRequests.isEmpty());

    //A tile of a tiled capture whose camera misses the cloud. The live view has
    //not moved, so what it asked for is still what it wants.
    cwSceneGatherOptions offscreenJob;
    offscreenJob.liveFrame = false;

    fixture.lookAway();
    fixture.renderFrame(offscreenJob);

    CHECK(Access::selectedPoints(fixture.backend()) == livePoints);
    for (const int index : std::as_const(liveRequests)) {
        //Still in flight, or landed while the tile rendered — never canceled
        CHECK(Access::nodeState(fixture.backend(), index) != NodeState::Absent);
        CHECK_FALSE(Access::exportRequested(fixture.backend(), index));
    }
}

TEST_CASE("A culled cloud republishes its residency once and then stands still",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("culled-republish"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    REQUIRE(Access::residentCount(fixture.backend()) > 1);
    REQUIRE(Access::selectedPoints(fixture.backend()) > 0);

    //The frame the cloud leaves the view drops a cut, so the pick set has
    //something new to hear about
    fixture.lookAway();
    fixture.renderFrame();
    CHECK(Access::residencyChanged(fixture.backend()));

    //Every frame after it changes nothing, so nothing is republished
    constexpr int kStillFrames = 5;
    for (int i = 0; i < kStillFrames; i++) {
        fixture.renderFrame();
        CHECK_FALSE(Access::residencyChanged(fixture.backend()));
    }
}

TEST_CASE("A culled cloud keeps its nodes until the budget takes them",
          "[PointCloudStreaming]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    PointCloudFixture fixture(rhi.get(), QStringLiteral("culled-eviction"));
    fixture.setOrthoHeight(kCloseOrthoHeight);
    fixture.renderUntilQuiet();

    const int drawnResidency = Access::residentCount(fixture.backend());
    REQUIRE(drawnResidency > 2);

    //Off screen under a budget that still fits: eviction has no reason to run
    fixture.lookAway();
    fixture.renderUntilQuiet();
    const int culledResidency = Access::residentCount(fixture.backend());
    CHECK(culledResidency >= drawnResidency);

    //The same cloud, still off screen, under a budget it no longer fits: the
    //nodes go the ordinary way, down to the pinned root
    const qint64 outsideBytes = totalGpuBytes() - fixture.gpuBytes();
    cwRenderBudgets budgets = fixture.budgets();
    budgets.gpuBudgetBytes = outsideBytes + fixture.gpuBytes() / 2;
    fixture.setBudgets(budgets);

    constexpr int kEvictionFrames = 10;
    for (int i = 0; i < kEvictionFrames; i++) {
        fixture.renderFrame();
    }

    CHECK(Access::residentCount(fixture.backend()) < culledResidency);
    CHECK(Access::nodeState(fixture.backend(), kRootIndex) == NodeState::Resident);
}


TEST_CASE("Zooming out until quiet leaves the holes bounded",
          "[PointCloudStreaming][HoleMetric]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    SECTION("a plane at one sprite per cell measures the sizing rule alone") {
        const QVector<float> heights = zoomOutHeights(kPlaneSweepLowHeight,
                                                      kPlaneSweepHighHeight, kSweepStepFraction);

        PointCloudFixture fixture(rhi.get(), planeCache(), kTargetDimension);
        fixture.render().setSpacingCoverage(kBareSpacingCoverage);
        fixture.synchronize();

        const QVector<SweepRecord> records = sweep(fixture, QStringLiteral("quiet-plane"),
                                                   heights, StepPolicy::Quiet, 1, {});

        //Everything resident, the default point budget: the governor has no
        //reason to inflate, so the curve is the sizing rule and nothing else
        for (const SweepRecord& record : records) {
            CHECK(record.sseInflation == 1.0);
            CHECK_FALSE(record.pointCapped);
        }

        const SweepRecord worst = worstRecord(records);
        const int widestHole = largestHoleSidePx(records);
        const double largestJump = largestHoleFractionJump(records);
        if (worst.holeFraction > kMaxHoleFraction) {
            fixture.setOrthoHeight(worst.height);
            fixture.renderUntilQuiet();
            dumpFrame(fixture, QStringLiteral("quiet-plane"));
        }

        INFO("worst hole fraction " << worst.holeFraction << " at H = " << worst.height
             << " m, drawn level " << worst.finestDrawnLevel
             << ", widest hole " << widestHole << " px, largest jump " << largestJump);
        CHECK(worst.holeFraction <= kMaxHoleFraction);
        CHECK(largestJump <= kMaxHoleFractionJump);
    }

    SECTION("a plane at the default coverage all but closes the holes") {
        const QVector<float> heights = zoomOutHeights(kPlaneSweepLowHeight,
                                                      kPlaneSweepHighHeight, kSweepStepFraction);

        PointCloudFixture fixture(rhi.get(), planeCache(), kTargetDimension);
        fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
        fixture.synchronize();

        const QVector<SweepRecord> records = sweep(fixture, QStringLiteral("quiet-plane-default"),
                                                   heights, StepPolicy::Quiet, 1, {});

        //Half again a spacing: neighboring sprites overlap by half a cell at
        //every level the sweep reaches, which all but closes the surface. What
        //is left is the lattice the plane's regular grid leaves on the one
        //level transition the sweep crosses, so the hole side is the target
        //itself and only the fraction and the jump say anything.
        const SweepRecord worst = worstRecord(records);
        INFO("worst hole fraction " << worst.holeFraction << " at H = " << worst.height
             << " m, largest jump " << largestHoleFractionJump(records));
        CHECK(worst.holeFraction <= kDefaultCoverageMaxHoleFraction);
        CHECK(largestHoleFractionJump(records) <= kDefaultCoverageMaxHoleFractionJump);
    }

    SECTION("a USGS tile measured against its own densest render") {
        const OctreeCache& cache = usgsTileCacheOrSkip();
        const QVector<float> heights = zoomOutHeights(kTileSweepLowHeight,
                                                      kTileSweepHighHeight, kSweepStepFraction);
        const QVector<PixelMask> masks = referenceMasks(rhi.get(), cache, kTargetDimension,
                                                        heights);

        PointCloudFixture fixture(rhi.get(), cache, kTargetDimension);
        fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
        fixture.synchronize();

        const QVector<SweepRecord> records = sweep(fixture, QStringLiteral("quiet-tile"),
                                                   heights, StepPolicy::Quiet, 1, masks);

        for (const SweepRecord& record : records) {
            CHECK(record.sseInflation == 1.0);
            CHECK_FALSE(record.pointCapped);
        }

        const SweepRecord worst = worstRecord(records);
        const int widestHole = largestHoleSidePx(records);
        const double largestJump = largestHoleFractionJump(records);
        if (worst.holeFraction > kTileMaxHoleFraction || widestHole > kTileMaxHoleSidePx) {
            fixture.setOrthoHeight(worst.height);
            fixture.renderUntilQuiet();
            dumpFrame(fixture, QStringLiteral("quiet-tile"));
        }

        INFO("worst hole fraction " << worst.holeFraction << " at H = " << worst.height
             << " m, drawn level " << worst.finestDrawnLevel
             << ", widest hole " << widestHole << " px, largest jump " << largestJump);
        CHECK(worst.holeFraction <= kTileMaxHoleFraction);
        CHECK(widestHole <= kTileMaxHoleSidePx);
        CHECK(largestJump <= kTileMaxHoleFractionJump);
    }
}

TEST_CASE("Zooming out a frame at a time records the transient the cut leaves behind",
          "[PointCloudStreaming][HoleMetric]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();

    //A step is two frames and the uploads are throttled, so residency and the
    //drawn spacing both lag the cut the way they do on a live wheel. The curve
    //is read for shape — a spike after every level transition — so nothing here
    //is asserted beyond the sweep having run.
    const auto throttleUploads = [](PointCloudFixture& fixture) {
        cwRenderBudgets budgets = fixture.budgets();
        budgets.uploadBudgetBytesPerFrame = kTransientUploadBudgetBytes;
        fixture.setBudgets(budgets);
    };

    SECTION("a plane") {
        const QVector<float> heights = zoomOutHeights(kPlaneSweepLowHeight,
                                                      kPlaneSweepHighHeight, kSweepStepFraction);

        PointCloudFixture fixture(rhi.get(), planeCache(), kTargetDimension);
        fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
        fixture.synchronize();
        throttleUploads(fixture);

        const QVector<SweepRecord> records =
            sweep(fixture, QStringLiteral("transient-plane"), heights, StepPolicy::Frames,
                  kTransientFramesPerStep, {});
        REQUIRE_FALSE(records.isEmpty());

        const SweepRecord worst = worstRecord(records);
        INFO("worst hole fraction " << worst.holeFraction << " at H = " << worst.height
             << " m, drawn level " << worst.finestDrawnLevel);
        CHECK(worst.holeFraction >= 0.0);
    }

    SECTION("a USGS tile") {
        const OctreeCache& cache = usgsTileCacheOrSkip();
        const QVector<float> heights = zoomOutHeights(kTileSweepLowHeight,
                                                      kTileSweepHighHeight, kSweepStepFraction);
        const QVector<PixelMask> masks = referenceMasks(rhi.get(), cache, kTargetDimension,
                                                        heights);

        PointCloudFixture fixture(rhi.get(), cache, kTargetDimension);
        fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
        fixture.synchronize();
        throttleUploads(fixture);

        const QVector<SweepRecord> records =
            sweep(fixture, QStringLiteral("transient-tile"), heights, StepPolicy::Frames,
                  kTransientFramesPerStep, masks);
        REQUIRE_FALSE(records.isEmpty());

        const SweepRecord worst = worstRecord(records);
        INFO("worst hole fraction " << worst.holeFraction << " at H = " << worst.height
             << " m, drawn level " << worst.finestDrawnLevel);
        CHECK(worst.holeFraction >= 0.0);
    }
}

TEST_CASE("Zooming out under a one million point budget records the starved floor",
          "[PointCloudStreaming][HoleMetric]")
{
    const std::unique_ptr<QRhi> rhi = makeRhiOrSkip();
    const OctreeCache& cache = usgsTileCacheOrSkip();

    //At 256 px the cut in view never reaches a million points, so the cap would
    //quietly measure the quiet variant again; a 2000 px viewport is what asks
    //the selection for the cut a real window does.
    const QVector<float> heights = zoomOutHeights(kStarvedSweepLowHeight,
                                                  kStarvedSweepHighHeight, kStarvedStepFraction);
    const QVector<PixelMask> masks = referenceMasks(rhi.get(), cache, kStarvedTargetDimension,
                                                    heights);

    PointCloudFixture fixture(rhi.get(), cache, kStarvedTargetDimension);
    fixture.render().setSpacingCoverage(cw::pointcloud::kDefaultSpacingCoverage);
    fixture.synchronize();

    cwRenderBudgets budgets = fixture.budgets();
    budgets.pointBudget = kStarvedPointBudget;
    fixture.setBudgets(budgets);

    const QVector<SweepRecord> records = sweep(fixture, QStringLiteral("starved-tile"),
                                               heights, StepPolicy::Quiet, 1, masks);
    REQUIRE_FALSE(records.isEmpty());

    //The cap is the point of the variant: the governor inflates the threshold
    //until the cut fits, and the drawn level is coarser than the rule asked
    //for. Both are recorded rather than bounded until the baseline is known.
    double widestInflation = 1.0;
    int cappedFrames = 0;
    for (const SweepRecord& record : records) {
        widestInflation = std::max(widestInflation, record.sseInflation);
        cappedFrames += record.pointCapped ? 1 : 0;
    }

    const SweepRecord worst = worstRecord(records);
    INFO("worst hole fraction " << worst.holeFraction << " at H = " << worst.height
         << " m, widest inflation " << widestInflation << ", capped frames " << cappedFrames
         << " of " << records.size());
    CHECK(worst.holeFraction >= 0.0);
}
