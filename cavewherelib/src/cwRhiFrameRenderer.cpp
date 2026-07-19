//Our includes
#include "cwRhiFrameRenderer.h"
#include "cwRHIObject.h"
#include "cwRhiItemRenderer.h"
#include "cwEDLEffect.h"
#include "cwRenderingSettings.h"

#include <algorithm>
#include <array>
#include <cstring>

size_t qHash(const cwRhiPipelineKey& key, size_t seed) noexcept
{
    seed = qHash(quintptr(key.renderPass), seed);
    seed = qHash(key.sampleCount, seed);
    seed = qHash(key.vertexShader, seed);
    seed = qHash(key.fragmentShader, seed);
    seed = qHash(key.cullMode, seed);
    seed = qHash(key.frontFace, seed);
    seed = qHash(key.blendMode, seed);
    seed = qHash(key.depthTest, seed);
    seed = qHash(key.depthWrite, seed);
    seed = qHash(key.globalBinding, seed);
    seed = qHash(key.perDrawBinding, seed);
    seed = qHash(key.textureBinding, seed);
    seed = qHash(key.globalStages, seed);
    seed = qHash(key.perDrawStages, seed);
    seed = qHash(key.textureStages, seed);
    seed = qHash(key.hasPerDraw, seed);
    seed = qHash(key.topology, seed);
    return seed;
}

namespace {
cwRHIObject::PipelineBatch& ensurePipelineBatch(QVector<cwRHIObject::PipelineBatch>& batches,
                                                const cwRHIObject::PipelineState& state)
{
    for (auto& existing : batches) {
        if (existing.state.pipeline == state.pipeline &&
            existing.state.sortKey == state.sortKey) {
            return existing;
        }
    }

    batches.append({state, {}});
    return batches.last();
}

// Draw order for the scene's passes. The no-cloud path draws these straight to
// its single target; the EDL composite path splits them across the scene / cloud
// / composite passes. Shared by the live frame and the offscreen render.
constexpr std::array<cwRHIObject::RenderPass, 5> kPassOrder = {
    cwRHIObject::RenderPass::Background,
    cwRHIObject::RenderPass::PointCloud,
    cwRHIObject::RenderPass::Opaque,
    cwRHIObject::RenderPass::Transparent,
    cwRHIObject::RenderPass::Overlay,
};
}

QRhiTexture::Format cwRhiFrameRenderer::preferredDepthFormat(QRhi* rhi)
{
    return rhi->isTextureFormatSupported(QRhiTexture::D32F) ? QRhiTexture::D32F
                                                            : QRhiTexture::D24S8;
}

cwRhiFrameRenderer::ClipSpaceCamera cwRhiFrameRenderer::clipSpaceCorrectedCamera(QRhi* rhi,
                                                                const QMatrix4x4& projection,
                                                                const QMatrix4x4& view)
{
    ClipSpaceCamera camera;
    camera.projectionCorrected = rhi->clipSpaceCorrMatrix() * projection;
    camera.viewProjection = camera.projectionCorrected * view;
    return camera;
}

cwRhiFrameRenderer::cwRhiFrameRenderer() = default;

cwRhiFrameRenderer::~cwRhiFrameRenderer()
{
    for(auto rhiObject : std::as_const(m_rhiObjects)) {
        delete rhiObject;
    }
    // Clear the tracking lists before tearing down the EDL offscreen below:
    // destroyEdlOffscreen calls evictPipelinesFor, which iterates m_rhiObjects to
    // purge their pipeline caches — the objects are gone now, so a stale entry would
    // be a use-after-free. Their destructors already released every pipeline reference.
    m_rhiObjects.clear();
    m_rhiObjectsToInitilize.clear();
    m_rhiNeedResourceUpdate.clear();

    delete m_globalUniformBuffer;

    // Tear down the live EDL offscreen before the pipeline cache so pipelines keyed
    // on its rpDescs are released first.
    destroyEdlOffscreen(m_edlOffscreen);

    for (auto record : std::as_const(m_pipelineCache)) {
        delete record->pipeline;
        delete record->layout;
        delete record;
    }
    m_pipelineCache.clear();

    delete m_sharedLinearSampler;
    m_sharedLinearSampler = nullptr;
}

void cwRhiFrameRenderer::initialize(QRhiCommandBuffer *cb)
{
    //This function is called multiple times, make sure bufferes that are initilized
    //here are done so only once, or shaders will not update correctly.

    if(m_globalUniformBuffer == nullptr) {
        auto rhi = cb->rhi();
        // Aligned per-slot stride (portable UBO size + valid dynamic-offset
        // granularity). The buffer holds kGlobalCameraSlotCount such slots; render
        // objects bind it with a dynamic offset that selects the live or offscreen
        // camera. Slot 0 (live) lives at offset 0; stampCamera writes each slot.
        m_globalUniformStride = rhi->ubufAligned(sizeof(GlobalUniform));
        m_globalUniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                               m_globalUniformStride * kGlobalCameraSlotCount);
        m_globalUniformBuffer->create();
    }
}

void cwRhiFrameRenderer::syncCameraValues(float devicePixelRatio,
                                          const QMatrix4x4& viewMatrix,
                                          const QMatrix4x4& projectionMatrix)
{
    if (needsUpdate(cwSceneUpdate::Flag::DevicePixelRatio)) {
        m_devicePixelRatio = devicePixelRatio;
    }
    if (needsUpdate(cwSceneUpdate::Flag::ViewMatrix)) {
        m_viewMatrix = viewMatrix;
    }
    if (needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix)) {
        m_projectionMatrix = projectionMatrix;
    }
}

void cwRhiFrameRenderer::setEdlParameters(const EdlParametersData& parameters)
{
    // Hand the latest tuning to the effect, which stages it on its own cwTracked
    // and re-derives the UBO values next render only if it changed. The effect is
    // created lazily in render() and then persists, so feeding it here covers it
    // from the second frame onward. The cast is safe — ensureEdlOffscreen only ever
    // creates a cwEDLEffect; the unique_ptr is base-typed solely so the header need
    // not pull in the heavy cwEDLEffect header.
    m_edlParameters = parameters;
    if (m_edlOffscreen.effect) {
        static_cast<cwEDLEffect*>(m_edlOffscreen.effect.get())->setParameters(m_edlParameters);
    }
}

void cwRhiFrameRenderer::registerRenderObject(cwRenderObjectId id, cwRHIObject* rhiObject)
{
    // An already-synced object can reappear here: removing then re-adding it before
    // the next sync cancels its pending delete (cwScene::addItem), so its cwRHIObject
    // is still mapped under the same id. Free it before remapping — overwriting the
    // lookup entry blindly would orphan the old cwRHIObject in m_rhiObjects and render
    // it forever (issue #512).
    if(auto* stale = m_rhiObjectLookup.value(id)) {
        destroyRhiObject(stale);
    }

    rhiObject->setRenderObjectId(id);
    m_rhiObjects.append(rhiObject);
    m_rhiObjectsToInitilize.append(rhiObject);
    m_rhiObjectLookup[id] = rhiObject;
}

void cwRhiFrameRenderer::destroyRenderObject(cwRenderObjectId id)
{
    if(auto* rhiObject = m_rhiObjectLookup.value(id)) {
        destroyRhiObject(rhiObject);
        m_rhiObjectLookup.remove(id);
    }
}

void cwRhiFrameRenderer::destroyRhiObject(cwRHIObject* rhiObject)
{
    if(rhiObject == nullptr) {
        return;
    }

    // Drop every pending reference before the delete — synchroize() can run more
    // than once before render() drains these (no render between syncs in tests),
    // so a stale pointer left here would be dereferenced after free.
    m_rhiObjects.removeOne(rhiObject);
    m_rhiObjectsToInitilize.removeOne(rhiObject);
    m_rhiNeedResourceUpdate.removeOne(rhiObject);
    delete rhiObject;
}

void cwRhiFrameRenderer::renderLiveFrame(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    auto rhi = cb->rhi();

    // The renderer's render target is the authoritative source for viewport
    // pixel size — it accounts for devicePixelRatio, where cwCamera's
    // viewportChanged signal carries pre-DPR units.
    if (renderer && renderer->renderTarget()) {
        const QSize currentSize = renderer->renderTarget()->pixelSize();
        if (currentSize != m_viewportSize) {
            m_viewportSize = currentSize;
            m_updateFlags |= cwSceneUpdate::Flag::ViewportSize;
        }
    }

    QRhiRenderPassDescriptor* swapchainRPDesc =
        (renderer && renderer->renderTarget())
        ? renderer->renderTarget()->renderPassDescriptor()
        : nullptr;

    const int swapchainSampleCount =
        (renderer && renderer->renderTarget()) ? renderer->renderTarget()->sampleCount() : 1;

    // Report the device's supported MSAA levels to cwRenderingSettings (GUI thread)
    // once, so the settings UI can offer only sample counts this backend accepts.
    if (!m_reportedSupportedSampleCounts && rhi) {
        m_reportedSupportedSampleCounts = true;
        const QList<int> supported = rhi->supportedSampleCounts();
        QMetaObject::invokeMethod(cwRenderingSettings::instance(), [supported]() {
            if (cwRenderingSettings::instance()) {
                cwRenderingSettings::instance()->setSupportedSampleCounts(supported);
            }
        }, Qt::QueuedConnection);
    }

    // Decide up front whether the EDL composite path is needed — it reroutes
    // Background + Opaque (and the cloud) through the shared-depth offscreen, so
    // the routing must be settled before any object builds a pipeline this frame.
    const bool cloudVisible = anyCloudVisible();

    if (cloudVisible) {
        ensureEdlOffscreen(m_edlOffscreen, rhi, m_viewportSize, swapchainSampleCount);
    }
    const bool edlActive = cloudVisible && m_edlOffscreen.valid();

    setupPassRouting(renderer ? renderer->renderTarget() : nullptr,
                     edlActive ? &m_edlOffscreen : nullptr);

    // Initialize (or re-init) the live EDL effect for the swap-chain target. The
    // effect needs the swap-chain sample count (cwRenderingSettings::sampleCount via
    // cw3dRegionViewer), so this is deferred to render() where the swap-chain rpDesc
    // first exists. The helper owns the init-key bookkeeping and re-inits only when
    // the output target or the effective input sample count changes.
    if (swapchainRPDesc) {
        ensureEffectInitialized(m_edlOffscreen, rhi, swapchainRPDesc, swapchainSampleCount);
    }

    cwRHIObject::RenderData renderData{cb, renderer, m_updateFlags, swapchainRPDesc, swapchainSampleCount};

    QRhiResourceUpdateBatch* resources = rhi->nextResourceUpdateBatch();

    // Derive the live camera once and stamp it into both the global UBO (slot 0,
    // read by geometry on the GPU) and renderData (read by CPU draws such as
    // cwRenderBillboards). renderData already captured this frame's dirty flags
    // above (the snapshot objects consume), so clear them for the next frame.
    constexpr int kLiveCameraSlot = 0;
    const ClipSpaceCamera liveCamera = stampCamera(
        resources, rhi, renderData, kLiveCameraSlot,
        renderer ? renderer->renderTarget() : nullptr,
        m_projectionMatrix, m_viewMatrix, m_devicePixelRatio, m_viewportSize);
    m_updateFlags = cwSceneUpdate::Flag::None;

    // Built after setupPassRouting and stampCamera so each per-pass copy carries
    // both this frame's routing and the live camera. Shared with updateResources
    // (via resourceUpdateData) and gather() so every pipeline — wherever it is
    // built — keys on the same stamped routing.
    const cwRHIObject::PerPassRenderData perPassRenderData =
        buildPerPassRenderData(renderData);

    cwRHIObject::ResourceUpdateData resourceUpdateData{resources, renderData, &perPassRenderData};

    if(!m_rhiObjectsToInitilize.isEmpty()) {
        for(auto object : std::as_const(m_rhiObjectsToInitilize)) {
            object->initialize(resourceUpdateData);
        }
        m_rhiObjectsToInitilize.clear();
    }

    if(!m_rhiNeedResourceUpdate.isEmpty()) {
        for(auto object : std::as_const(m_rhiNeedResourceUpdate)) {
            object->updateResources(resourceUpdateData);
        }
        m_rhiNeedResourceUpdate.clear();
    }

    std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount> passBatches;

    gatherScene(passBatches, perPassRenderData);

    const QColor clearColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);

    // The live frame always reads camera slot 0 of the global UBO.
    const quint32 kLiveCameraOffset = m_globalUniformStride * quint32(kLiveCameraSlot);

    const cwRhiPostProcessEffect::FrameUniformContext liveFrameContext{
        liveCamera.projectionCorrected, m_viewportSize, m_devicePixelRatio
    };

    drawScene(cb, renderer->renderTarget(), edlActive ? &m_edlOffscreen : nullptr,
              passBatches, perPassRenderData, resources, liveFrameContext,
              kLiveCameraOffset, clearColor);
}

cwRhiFrameRenderer::ClipSpaceCamera cwRhiFrameRenderer::stampCamera(
    QRhiResourceUpdateBatch* batch, QRhi* rhi, cwRHIObject::RenderData& renderData,
    int cameraSlot, QRhiRenderTarget* renderTarget,
    const QMatrix4x4& projection, const QMatrix4x4& view,
    float devicePixelRatio, QSize viewportSize)
{
    const ClipSpaceCamera clip = clipSpaceCorrectedCamera(rhi, projection, view);

    // CPU-side copy for draws that build their own MVP (cwRenderBillboards).
    renderData.renderTarget = renderTarget;
    renderData.viewMatrix = view;
    renderData.projectionMatrix = clip.projectionCorrected;
    renderData.viewProjectionMatrix = clip.viewProjection;
    renderData.devicePixelRatio = devicePixelRatio;

    // The same camera into the global-UBO slot geometry binds on the GPU. One full
    // struct write per slot (the UBO is tiny); the per-field dirty-flag gating the
    // live frame once used saved nothing against the full scene draw it always runs.
    GlobalUniform uniform;
    std::memcpy(uniform.viewProjectionMatrix, clip.viewProjection.constData(),
                sizeof(uniform.viewProjectionMatrix));
    std::memcpy(uniform.viewMatrix, view.constData(), sizeof(uniform.viewMatrix));
    std::memcpy(uniform.projectionMatrix, clip.projectionCorrected.constData(),
                sizeof(uniform.projectionMatrix));
    uniform.devicePixelRatio = devicePixelRatio;
    uniform._pad0 = 0.0f;
    uniform.viewportSize[0] = float(viewportSize.width());
    uniform.viewportSize[1] = float(viewportSize.height());

    batch->updateDynamicBuffer(m_globalUniformBuffer,
                               m_globalUniformStride * quint32(cameraSlot),
                               sizeof(GlobalUniform), &uniform);
    return clip;
}

int cwRhiFrameRenderer::effectiveEdlSampleCount(QRhi* rhi, int requestedSampleCount) const
{
    if (!rhi || requestedSampleCount <= 1) {
        return 1;
    }
    // MultisampleTexture lets us sample the MSAA offscreen; SampleVariables lets
    // EDL_MSAA.frag run per-sample via gl_SampleID. Without both, fall back to
    // 1x (no AA). The requested count must also be an MSAA level the device
    // offers, or the texture create / pipeline build would fail validation.
    if (!rhi->isFeatureSupported(QRhi::MultisampleTexture)
        || !rhi->isFeatureSupported(QRhi::SampleVariables)
        || !rhi->supportedSampleCounts().contains(requestedSampleCount)) {
        return 1;
    }
    return requestedSampleCount;
}

void cwRhiFrameRenderer::ensureEdlOffscreen(EdlOffscreen& edl, QRhi* rhi, QSize size, int requestedSampleCount)
{
    if (!rhi || size.isEmpty()) {
        return;
    }

    // Cheap early-out before the RHI capability queries in effectiveEdlSampleCount:
    // the offscreen only needs rebuilding when the size or the requested swap-chain
    // sample count changes, both of which are rare relative to the per-frame calls.
    if (edl.valid() && edl.size == size
        && edl.requestedSampleCount == requestedSampleCount) {
        return;
    }

    const int sampleCount = effectiveEdlSampleCount(rhi, requestedSampleCount);

    // Preserve the effect across a resize — its pipeline is keyed on the stable
    // output rpDesc, and its SRB rebinds to the new textures on the next apply()
    // (cwEDLEffect::ensureBindings detects the pointer change). The caller
    // re-initializes the effect when sampleCount changes the 1x / MSAA variant.
    std::unique_ptr<cwRhiPostProcessEffect> savedEffect = std::move(edl.effect);
    QRhiRenderPassDescriptor* savedEffectOutputRpDesc = edl.effectOutputRpDesc;
    const int savedEffectOutputSampleCount = edl.effectOutputSampleCount;
    const int savedEffectInputSampleCount = edl.effectInputSampleCount;
    destroyEdlOffscreen(edl);

    EdlOffscreen fresh;
    fresh.size = size;
    fresh.requestedSampleCount = requestedSampleCount;
    fresh.sampleCount = sampleCount;

    // fresh owns its resources via unique_ptr, so any early return below releases
    // the partially-built set automatically; we only need to restore the saved
    // effect so a later frame can retry the allocation.
    auto bail = [&]() {
        edl.effect = std::move(savedEffect);
    };

    // MSAA color textures can't be transfer sources; the EDL composite only
    // samples them anyway, so the flag is needed only on the 1x path.
    auto colorFlags = QRhiTexture::Flags(QRhiTexture::RenderTarget);
    if (sampleCount == 1) {
        colorFlags |= QRhiTexture::UsedAsTransferSource;
    }
    fresh.sceneColor.reset(rhi->newTexture(QRhiTexture::RGBA8, size, sampleCount, colorFlags));
    fresh.cloudColor.reset(rhi->newTexture(QRhiTexture::RGBA8, size, sampleCount, colorFlags));

    // One depth texture is shared by both targets.
    const QRhiTexture::Format depthFormat = preferredDepthFormat(rhi);
    fresh.depth.reset(rhi->newTexture(depthFormat, size, sampleCount, QRhiTexture::RenderTarget));

    if (!fresh.sceneColor->create() || !fresh.cloudColor->create() || !fresh.depth->create()) {
        bail();
        return;
    }

    // Scene target: Background + Opaque draw here; it writes and (by default)
    // stores the shared depth for the cloud target to load.
    {
        QRhiTextureRenderTargetDescription desc;
        desc.setColorAttachments({ QRhiColorAttachment(fresh.sceneColor.get()) });
        desc.setDepthTexture(fresh.depth.get());
        fresh.sceneTarget.reset(rhi->newTextureRenderTarget(desc));
        fresh.sceneRpDesc.reset(fresh.sceneTarget->newCompatibleRenderPassDescriptor());
        if (!fresh.sceneRpDesc) { bail(); return; }
        fresh.sceneTarget->setRenderPassDescriptor(fresh.sceneRpDesc.get());
        if (!fresh.sceneTarget->create()) { bail(); return; }
    }

    // Cloud target: shares the depth texture and preserves its contents so the
    // hardware depth test occludes the cloud against the scene geometry.
    {
        QRhiTextureRenderTargetDescription desc;
        desc.setColorAttachments({ QRhiColorAttachment(fresh.cloudColor.get()) });
        desc.setDepthTexture(fresh.depth.get());
        fresh.cloudTarget.reset(rhi->newTextureRenderTarget(desc));
        fresh.cloudTarget->setFlags(QRhiTextureRenderTarget::PreserveDepthStencilContents);
        fresh.cloudRpDesc.reset(fresh.cloudTarget->newCompatibleRenderPassDescriptor());
        if (!fresh.cloudRpDesc) { bail(); return; }
        fresh.cloudTarget->setRenderPassDescriptor(fresh.cloudRpDesc.get());
        if (!fresh.cloudTarget->create()) { bail(); return; }
    }

    const bool reusedEffect = static_cast<bool>(savedEffect);
    fresh.effect = reusedEffect ? std::move(savedEffect)
                                : std::make_unique<cwEDLEffect>();
    if (reusedEffect) {
        // The preserved effect's SRB references the now-deleted textures; force a
        // rebind on the next apply() (pointer-identity caching is unsafe across a
        // free+realloc that may reuse the same address).
        fresh.effect->resize(size);
        // Carry the init key forward: the pipeline is keyed on the output rpDesc +
        // sample counts, not texture identity, so a pure resize must not trigger a
        // needless re-init. ensureEffectInitialized still re-inits if the output
        // target or the input sample count actually changed.
        fresh.effectOutputRpDesc = savedEffectOutputRpDesc;
        fresh.effectOutputSampleCount = savedEffectOutputSampleCount;
        fresh.effectInputSampleCount = savedEffectInputSampleCount;
    }

    edl = std::move(fresh);
}

void cwRhiFrameRenderer::destroyEdlOffscreen(EdlOffscreen& edl)
{
    // Effect first — its SRB references the textures below.
    edl.effect.reset();
    // Evict pipelines keyed on the rpDescs *before* deleting them — a newly
    // allocated rpDesc may reuse the same address and hash-collide with a stale
    // cache entry.
    if (edl.sceneRpDesc) {
        evictPipelinesFor(edl.sceneRpDesc.get());
    }
    if (edl.cloudRpDesc) {
        evictPipelinesFor(edl.cloudRpDesc.get());
    }
    // Targets before their rpDescs/textures (the proven release order); each
    // reset() both frees and nulls, so there's no field-by-field nulling to drift.
    edl.sceneTarget.reset();
    edl.cloudTarget.reset();
    edl.sceneRpDesc.reset();
    edl.cloudRpDesc.reset();
    edl.sceneColor.reset();
    edl.cloudColor.reset();
    edl.depth.reset();
    edl.size = QSize();
}

void cwRhiFrameRenderer::ensureEffectInitialized(EdlOffscreen& edl, QRhi* rhi,
                                         QRhiRenderPassDescriptor* outputRpDesc,
                                         int outputSampleCount)
{
    if (!edl.effect || !rhi || !outputRpDesc) {
        return;
    }

    // Re-initialize the effect's pipeline whenever the target it composites into
    // (rpDesc + that target's sample count) or the offscreen input sample count it
    // reads (edl.sampleCount — the 1x vs MSAA shader variant) changes. The key is
    // stored on the EdlOffscreen, so the live and offscreen effects never share or
    // overwrite each other's state.
    if (edl.effectOutputRpDesc == outputRpDesc
        && edl.effectOutputSampleCount == outputSampleCount
        && edl.effectInputSampleCount == edl.sampleCount) {
        return;
    }

    edl.effect->initialize(rhi, outputRpDesc, outputSampleCount,
                           m_globalUniformBuffer, m_globalUniformStride,
                           edl.sampleCount);
    edl.effectOutputRpDesc = outputRpDesc;
    edl.effectOutputSampleCount = outputSampleCount;
    edl.effectInputSampleCount = edl.sampleCount;
}

bool cwRhiFrameRenderer::anyCloudVisible() const
{
    return std::any_of(
        m_rhiObjects.cbegin(), m_rhiObjects.cend(),
        [this](const cwRHIObject* object) {
            return m_visibility.objectVisible(object->renderObjectId())
                   && object->usesPointCloudPass();
        });
}

QSet<cwRenderObjectId> cwRhiFrameRenderer::atlasIncompatibleVisibleObjectIds() const
{
    QSet<cwRenderObjectId> ids;
    for (auto it = m_rhiObjectLookup.cbegin(); it != m_rhiObjectLookup.cend(); ++it) {
        const cwRHIObject* object = it.value();
        if (object && m_visibility.objectVisible(it.key())
            && object->precludesAtlasBatching()) {
            ids.insert(it.key());
        }
    }
    return ids;
}

cwRHIObject::PerPassRenderData cwRhiFrameRenderer::buildPerPassRenderData(
    const cwRHIObject::RenderData& base) const
{
    cwRHIObject::PerPassRenderData perPass;
    for (int i = 0; i < kPassCount; ++i) {
        perPass[i] = base;
        const auto pass = static_cast<cwRHIObject::RenderPass>(i);
        perPass[i].renderPassDescriptor = passRenderPassDescriptor(pass);
        perPass[i].sampleCount = passSampleCount(pass);
    }
    return perPass;
}

void cwRhiFrameRenderer::gatherScene(std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount>& passBatches,
                            const cwRHIObject::PerPassRenderData& perPassRenderData,
                            const cwSceneGatherOptions& options)
{
    quint32 objectOrder = 0;
    for (auto object : std::as_const(m_rhiObjects)) {
        // Snapshot gate ANDed with the per-job overlay. Objects carry their own
        // ids now, so the overlay is tested directly — no id→pointer resolve
        // pass; contains() on the live frame's empty set is a cheap no-op.
        const cwRenderObjectId id = object->renderObjectId();
        if (!m_visibility.objectVisible(id) || options.hiddenObjectIds.contains(id)) {
            ++objectOrder;
            continue;
        }

        bool gathered = false;
        for (cwRHIObject::RenderPass pass : kPassOrder) {
            const int passIndex = static_cast<int>(pass);
            auto& batches = passBatches[passIndex];
            const cwRHIObject::GatherContext context {
                &perPassRenderData[passIndex], pass, objectOrder,
                &m_visibility,
                options.appearanceSlotForObject.value(object, 0)
            };
            gathered |= object->gather(context, batches);
        }

        //Generate renderables for older rendering path
        if (!gathered) {
            const int defaultPassIndex = static_cast<int>(cwRHIObject::RenderPass::Opaque);
            const quint64 baseSortKey = (quint64(objectOrder) << 32);
            cwRHIObject::PipelineState state;
            state.pipeline = nullptr;
            state.sortKey = baseSortKey;

            auto& batch = ensurePipelineBatch(passBatches[defaultPassIndex], state);
            cwRHIObject::Drawable draw;
            draw.type = cwRHIObject::Drawable::Type::Custom;
            draw.customDraw = [object](const cwRHIObject::RenderData& data) {
                object->render(data);
            };

            batch.drawables.append(draw);
        }

        ++objectOrder;
    }
}

void cwRhiFrameRenderer::drainBatches(QRhiCommandBuffer* cb,
                              QVector<cwRHIObject::PipelineBatch>& batches,
                              QRhiGraphicsPipeline*& boundPipeline,
                              const cwRHIObject::RenderData& passRenderData,
                              quint32 cameraUniformOffset)
{
    if (batches.isEmpty()) {
        return;
    }

    const auto bindResources = [cb, cameraUniformOffset](const cwRHIObject::Drawable& drawable) {
        if (!drawable.bindings) {
            cb->setShaderResources();
            return;
        }
        // Compose the per-draw dynamic offsets: the shared global camera slot
        // (offset supplied per render job) and the per-object appearance slot
        // (offset baked onto the drawable at gather time). A drawable may carry
        // neither, either, or both. They are appended in ascending binding order
        // (camera binding < appearance binding everywhere they coexist), which
        // some backends require — asserted below rather than re-sorted per draw.
        std::array<QRhiCommandBuffer::DynamicOffset, 2> offsets;
        int count = 0;
        if (drawable.globalCameraBinding >= 0) {
            offsets[count++] = QRhiCommandBuffer::DynamicOffset(drawable.globalCameraBinding,
                                                               cameraUniformOffset);
        }
        if (drawable.appearanceBinding >= 0) {
            offsets[count++] = QRhiCommandBuffer::DynamicOffset(drawable.appearanceBinding,
                                                               drawable.appearanceUniformOffset);
        }
        Q_ASSERT(count < 2 || offsets[0].first < offsets[1].first);
        if (count == 0) {
            cb->setShaderResources(drawable.bindings);
        } else {
            cb->setShaderResources(drawable.bindings, count, offsets.data());
        }
    };

    std::sort(batches.begin(), batches.end(), [](const cwRHIObject::PipelineBatch& a,
                                                 const cwRHIObject::PipelineBatch& b) {
        if (a.state.sortKey != b.state.sortKey) {
            return a.state.sortKey < b.state.sortKey;
        }
        return a.state.pipeline < b.state.pipeline;
    });

    for (const auto& batch : std::as_const(batches)) {
        if (batch.state.pipeline && batch.state.pipeline != boundPipeline) {
            boundPipeline = batch.state.pipeline;
            cb->setGraphicsPipeline(boundPipeline);
        } else if (!batch.state.pipeline) {
            boundPipeline = nullptr;
        }

        for (const auto& drawable : batch.drawables) {
            switch (drawable.type) {
            case cwRHIObject::Drawable::Type::Custom:
                boundPipeline = nullptr;
                if (drawable.customDraw) {
                    drawable.customDraw(passRenderData);
                }
                break;
            case cwRHIObject::Drawable::Type::Indexed: {
                bindResources(drawable);

                if (!drawable.vertexBindings.isEmpty()) {
                    cb->setVertexInput(0,
                                       static_cast<int>(drawable.vertexBindings.size()),
                                       drawable.vertexBindings.constData(),
                                       drawable.indexBuffer,
                                       drawable.indexOffset,
                                       drawable.indexFormat);
                }

                cb->drawIndexed(drawable.indexCount,
                                drawable.instanceCount,
                                drawable.indexOffset,
                                drawable.vertexOffset,
                                drawable.firstInstance);
                break;
            }
            case cwRHIObject::Drawable::Type::NonIndexed: {
                bindResources(drawable);

                if (!drawable.vertexBindings.isEmpty()) {
                    cb->setVertexInput(0,
                                       static_cast<int>(drawable.vertexBindings.size()),
                                       drawable.vertexBindings.constData());
                }

                cb->draw(drawable.vertexCount,
                         drawable.instanceCount,
                         static_cast<quint32>(drawable.vertexOffset),
                         drawable.firstInstance);
                break;
            }
            }
        }
    }
}

void cwRhiFrameRenderer::drawScene(QRhiCommandBuffer* cb,
                           QRhiRenderTarget* finalTarget,
                           const EdlOffscreen* edl,
                           std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount>& passBatches,
                           const cwRHIObject::PerPassRenderData& perPassRenderData,
                           QRhiResourceUpdateBatch* resources,
                           const cwRhiPostProcessEffect::FrameUniformContext& frameContext,
                           quint32 cameraUniformOffset,
                           const QColor& clearColor)
{
    using RP = cwRHIObject::RenderPass;
    auto passBatchFor = [&](RP pass) -> QVector<cwRHIObject::PipelineBatch>& {
        return passBatches[static_cast<int>(pass)];
    };
    auto passDataFor = [&](RP pass) -> const cwRHIObject::RenderData& {
        return perPassRenderData[static_cast<int>(pass)];
    };

    const QSize outputSize = finalTarget->pixelSize();

    if (edl && edl->valid()) {
        const QRhiViewport offscreenViewport(0, 0, edl->size.width(), edl->size.height());

        // The EDL offscreen colour buffers MUST clear transparent regardless of
        // the caller's background: the composite classifies cloud vs scene by the
        // cloud texture's alpha, so an opaque clear would mark every pixel as
        // cloud and paint the whole scene with the (black) cloud-clear colour.
        const QColor edlClear = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);

        // Pass A — scene offscreen: Background + Opaque into sceneColor with
        // sceneDepth. Consumes the caller's resource-update batch.
        {
            QRhiGraphicsPipeline* bound = nullptr;
            cb->beginPass(edl->sceneTarget.get(), edlClear, { 1.0f, 0 }, resources);
            cb->setViewport(offscreenViewport);
            drainBatches(cb, passBatchFor(RP::Background), bound, passDataFor(RP::Background), cameraUniformOffset);
            drainBatches(cb, passBatchFor(RP::Opaque), bound, passDataFor(RP::Opaque), cameraUniformOffset);
            cb->endPass();
        }

        // Pass B — cloud offscreen: the point cloud into cloudColor while
        // *sharing* sceneDepth (loaded via PreserveDepthStencilContents), so the
        // hardware depth test occludes the cloud against scene geometry and the
        // buffer ends up holding the combined cloud+scene depth.
        {
            QRhiGraphicsPipeline* bound = nullptr;
            cb->beginPass(edl->cloudTarget.get(), edlClear, { 1.0f, 0 }, nullptr);
            cb->setViewport(offscreenViewport);
            drainBatches(cb, passBatchFor(RP::PointCloud), bound, passDataFor(RP::PointCloud), cameraUniformOffset);
            cb->endPass();
        }

        // Pass C — final target: the EDL composite writes the final scene+cloud
        // pixel (and the combined depth), then Transparent + Overlay draw on top.
        cb->beginPass(finalTarget, clearColor, { 1.0f, 0 }, nullptr);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        edl->effect->updateFrameUniforms(frameContext);
        edl->effect->apply(cb, edl->sceneColor.get(), edl->cloudColor.get(), edl->depth.get(),
                           outputSize, cameraUniformOffset);

        QRhiGraphicsPipeline* bound = nullptr;  // effect bound its own pipeline
        drainBatches(cb, passBatchFor(RP::Transparent), bound, passDataFor(RP::Transparent), cameraUniformOffset);
        drainBatches(cb, passBatchFor(RP::Overlay), bound, passDataFor(RP::Overlay), cameraUniformOffset);
        cb->endPass();
    } else {
        // No cloud: every pass renders straight to the final target.
        cb->beginPass(finalTarget, clearColor, { 1.0f, 0 }, resources);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        QRhiGraphicsPipeline* boundPipeline = nullptr;
        for (cwRHIObject::RenderPass pass : kPassOrder) {
            drainBatches(cb, passBatchFor(pass), boundPipeline, passDataFor(pass), cameraUniformOffset);
        }
        cb->endPass();
    }
}

void cwRhiFrameRenderer::setupPassRouting(QRhiRenderTarget* finalTarget, const EdlOffscreen* edl)
{
    // Read both the rpDesc and the sample count from one target object so a
    // pass's pipeline is always built for the exact target it draws into. The
    // sample count is owned by the target — never tracked separately — because a
    // pipeline/target sample-count mismatch fails to render (backend validation
    // error) rather than degrading gracefully.
    // An unrouted pass is signalled by a null rpDesc; its sample count is left at
    // the unset sentinel 0 (never a legal RHI count). A routed pass always
    // carries its target's real count. Consumers branch on the null rpDesc and
    // fall back to the live target, so the 0 never reaches a pipeline.
    auto route = [&](cwRHIObject::RenderPass pass, QRhiRenderTarget* target) {
        const int i = static_cast<int>(pass);
        m_passRpDesc[i] = target ? target->renderPassDescriptor() : nullptr;
        m_passSampleCount[i] = target ? target->sampleCount() : 0;
    };

    for (int i = 0; i < kPassCount; ++i) {
        route(static_cast<cwRHIObject::RenderPass>(i), finalTarget);
    }

    if (!edl || !edl->valid()) {
        return;
    }

    // Background + Opaque draw into the scene offscreen, the cloud into the cloud
    // offscreen (both 1x); the composite + Transparent + Overlay stay on the
    // final target. Each pass's sample count follows its offscreen target.
    using RP = cwRHIObject::RenderPass;
    route(RP::Background, edl->sceneTarget.get());
    route(RP::Opaque, edl->sceneTarget.get());
    route(RP::PointCloud, edl->cloudTarget.get());
}

QRhiRenderPassDescriptor* cwRhiFrameRenderer::passRenderPassDescriptor(cwRHIObject::RenderPass pass) const
{
    const int i = static_cast<int>(pass);
    if (i < 0 || i >= kPassCount) {
        return nullptr;
    }
    return m_passRpDesc[i];
}

int cwRhiFrameRenderer::passSampleCount(cwRHIObject::RenderPass pass) const
{
    const int i = static_cast<int>(pass);
    if (i < 0 || i >= kPassCount) {
        return 1;
    }
    return m_passSampleCount[i];
}

cwRhiPipelineRecord* cwRhiFrameRenderer::acquirePipeline(const cwRhiPipelineKey& key,
                                                 QRhi* rhi,
                                                 const std::function<cwRhiPipelineRecord*(QRhi*)>& createFn)
{
    auto it = m_pipelineCache.find(key);
    if (it != m_pipelineCache.end()) {
        auto* record = it.value();
        record->refCount += 1;
        return record;
    }

    if (!createFn) {
        return nullptr;
    }

    cwRhiPipelineRecord* record = createFn(rhi);
    if (!record) {
        return nullptr;
    }

    record->key = key;
    record->refCount = 1;
    m_pipelineCache.insert(key, record);
    return record;
}

void cwRhiFrameRenderer::releasePipeline(cwRhiPipelineRecord* record)
{
    if (!record) {
        return;
    }

    if (record->refCount == 0) {
        return;
    }

    record->refCount -= 1;
    if (record->refCount > 0) {
        return;
    }

    auto it = m_pipelineCache.find(record->key);
    if (it != m_pipelineCache.end() && it.value() == record) {
        m_pipelineCache.erase(it);
    }

    destroyPipelineRecord(record);
}

void cwRhiFrameRenderer::destroyPipelineRecord(cwRhiPipelineRecord* record)
{
    delete record->pipeline;
    delete record->layout;
    delete record;
}

void cwRhiFrameRenderer::evictPipelinesFor(QRhiRenderPassDescriptor* descriptor)
{
    if (!descriptor) {
        return;
    }

    // Let render objects drop the references their per-object pipeline caches
    // hold against this descriptor first. Without this they keep the records
    // resident (refCount > 0) and the sweep below can only orphan them — and a
    // newly allocated descriptor reusing this address would then hash-collide
    // with the stale, dangling key. Releasing here frees them outright.
    for (auto* rhiObject : std::as_const(m_rhiObjects)) {
        rhiObject->purgePipelinesFor(descriptor);
    }

    auto it = m_pipelineCache.begin();
    while (it != m_pipelineCache.end()) {
        if (it.key().renderPass == descriptor) {
            cwRhiPipelineRecord* record = it.value();
            it = m_pipelineCache.erase(it);
            // The purge loop above already released every object's reference to
            // records keyed on this descriptor, so a record reaching here at
            // refCount 0 is owned by no one and is freed. A record still at
            // refCount > 0 is held by something that didn't purge (e.g. the EDL
            // effect, or a future object type); only orphan it from the cache so
            // that holder's pointer doesn't dangle.
            if (record->refCount == 0) {
                destroyPipelineRecord(record);
            }
        } else {
            ++it;
        }
    }
}

QRhiSampler* cwRhiFrameRenderer::sharedLinearClampSampler(QRhi* rhi)
{
    if (m_sharedLinearSampler) {
        return m_sharedLinearSampler;
    }

    if (!rhi) {
        return nullptr;
    }

    m_sharedLinearSampler = rhi->newSampler(QRhiSampler::Linear,
                                            QRhiSampler::Linear,
                                            QRhiSampler::Linear,
                                            QRhiSampler::ClampToEdge,
                                            QRhiSampler::ClampToEdge);
    if (m_sharedLinearSampler) {
        m_sharedLinearSampler->create();
    }

    return m_sharedLinearSampler;
}
