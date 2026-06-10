//Our includes
#include "cwRhiScene.h"
#include "cwRHIObject.h"
#include "cwRenderObject.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwEDLSettings.h"
#include "cwCamera.h"
#include "cwEDLEffect.h"
#include "cwRenderingSettings.h"

#include <algorithm>
#include <array>

uint qHash(const cwRhiPipelineKey& key, uint seed) noexcept
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
}

cwRhiScene::~cwRhiScene()
{
    for(auto rhiObject : std::as_const(m_rhiObjects)) {
        delete rhiObject;
    }
    delete m_globalUniformBuffer;

    // Tear down the EDL offscreen before the pipeline cache so pipelines keyed
    // on its rpDescs are released first.
    destroyEdlOffscreen();

    for (auto record : std::as_const(m_pipelineCache)) {
        delete record->pipeline;
        delete record->layout;
        delete record;
    }
    m_pipelineCache.clear();

    delete m_sharedLinearSampler;
    m_sharedLinearSampler = nullptr;
}

void cwRhiScene::initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    //This function is called multiple times, make sure bufferes that are initilized
    //here are done so only once, or shaders will not update correctly.

    if(m_globalUniformBuffer == nullptr) {
        auto rhi = cb->rhi();
        auto size = rhi->ubufAligned(sizeof(GlobalUniform)); //Makes the uniform buffer size portable
        m_globalUniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
        m_globalUniformBuffer->create();
    }
}

void cwRhiScene::synchroize(cwScene *scene, cwRhiItemRenderer *renderer)
{
    m_updateFlags = scene->m_updateFlags;
    scene->m_updateFlags = cwSceneUpdate::Flag::None;

    //Update camera uniform buffer
    if(scene->camera()) {

        if(needsUpdate(cwSceneUpdate::Flag::DevicePixelRatio)) {
            m_devicePixelRatio = scene->camera()->devicePixelRatio();
        }

        if(needsUpdate(cwSceneUpdate::Flag::ViewMatrix)) {
            m_viewMatrix = scene->camera()->viewMatrix();
        }

        if(needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix)) {
            m_projectionMatrix = scene->camera()->projectionMatrix();
        }
    }

    // Hand the latest tuning to the effect, which stages it on its own cwTracked
    // and re-derives the UBO values next render only if it changed. The effect is
    // created lazily in render() and then persists, so feeding it here (the only
    // place we hold the cwScene) covers it from the second frame onward. The cast
    // is safe — ensureEdlOffscreen only ever creates a cwEDLEffect; the unique_ptr
    // is base-typed solely so cwRhiScene.h need not pull in the heavy header.
    if (m_edlOffscreen.effect) {
        static_cast<cwEDLEffect*>(m_edlOffscreen.effect.get())->setParameters(scene->edl()->parameters());
    }

    //Add new rendering object
    if(!scene->m_newRenderObjects.isEmpty()) {
        for(auto object : std::as_const(scene->m_newRenderObjects)) {
            auto rhiObject = object->createRHIObject();
            // qDebug() << "Creating rhiObject:" << object << "->" << rhiObject;
            m_rhiObjects.append(rhiObject);
            m_rhiObjectsToInitilize.append(rhiObject);
            m_rhiObjectLookup[object] = rhiObject;

            scene->m_toUpdateRenderObjects.insert(object);
        }
        scene->m_newRenderObjects.clear();
    }

    //Remove old rendering objects
    if(!scene->m_toDeleteRenderObjects.isEmpty()) {
        for(auto object : std::as_const(scene->m_toDeleteRenderObjects)) {
            auto rhiObject = m_rhiObjectLookup[object];
            if(rhiObject != nullptr) {
                auto it = std::remove_if(m_rhiObjects.begin(), m_rhiObjects.end(),
                                         [rhiObject](const cwRHIObject* currentRhiObject) {
                                             if (currentRhiObject == rhiObject) {
                                                 delete currentRhiObject;
                                                 return true; // Marks for removal
                                             }
                                             return false;
                                         });
                m_rhiObjects.erase(it);
                m_rhiObjectLookup.remove(object);
            }

        }
        scene->m_toDeleteRenderObjects.clear();
    }

    //Update rendering objects
    for(auto object : std::as_const(scene->m_toUpdateRenderObjects)) {
        auto rhiObject = m_rhiObjectLookup[object];
        if(rhiObject) {
            rhiObject->setVisible(object->isVisible());
            rhiObject->synchronize({object, renderer});
            m_rhiNeedResourceUpdate.append(rhiObject);
        }
    }
    scene->m_toUpdateRenderObjects.clear();

}

void cwRhiScene::render(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
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
    const bool cloudVisible = std::any_of(
        m_rhiObjects.cbegin(), m_rhiObjects.cend(),
        [](const cwRHIObject* object) {
            return object->isVisible() && object->usesPointCloudPass();
        });

    if (cloudVisible) {
        ensureEdlOffscreen(rhi, m_viewportSize, swapchainSampleCount);
    }
    const bool edlActive = cloudVisible && m_edlOffscreen.valid();

    setupPassRouting(renderer ? renderer->renderTarget() : nullptr, edlActive);

    // The swap-chain rpDesc isn't available until the first render(), so the EDL
    // effect's initialize() — which needs the swap-chain sample count
    // (cwRenderingSettings::sampleCount via cw3dRegionViewer) — is deferred to
    // here. Re-init when the offscreen's effective sample count changes so the
    // effect swaps between the 1x and MSAA shader variants.
    if (swapchainRPDesc && m_edlOffscreen.effect
        && (!m_effectsInitialized || m_effectInputSampleCount != m_edlOffscreen.sampleCount)) {
        m_edlOffscreen.effect->initialize(rhi, swapchainRPDesc, swapchainSampleCount,
                                          m_globalUniformBuffer, m_edlOffscreen.sampleCount);
        m_effectsInitialized = true;
        m_effectInputSampleCount = m_edlOffscreen.sampleCount;
    }

    cwRHIObject::RenderData renderData{cb, renderer, m_updateFlags, swapchainRPDesc, swapchainSampleCount};

    QRhiResourceUpdateBatch* resources = rhi->nextResourceUpdateBatch();
    cwRHIObject::ResourceUpdateData resourceUpdateData{resources, renderData};

    //Upate uniforms for all the objects
    updateGlobalUniformBuffer(resources, rhi);

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

    using PassBatchesArray = std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount>;
    PassBatchesArray passBatches;

    // Pass order for the no-cloud path (drawn straight to the swap chain). The
    // EDL composite path below splits these across the scene/cloud/composite
    // passes instead.
    const std::array<cwRHIObject::RenderPass, 5> passOrder = {
        cwRHIObject::RenderPass::Background,
        cwRHIObject::RenderPass::PointCloud,
        cwRHIObject::RenderPass::Opaque,
        cwRHIObject::RenderPass::Transparent,
        cwRHIObject::RenderPass::Overlay,
    };

    // Per-pass render-data: gather() reads renderData.renderPassDescriptor and
    // .sampleCount when building pipeline keys, so each pass needs its own copy
    // carrying the target it routes into this frame (EDL offscreen or swap chain).
    std::array<cwRHIObject::RenderData, kPassCount> perPassRenderData;
    for (int i = 0; i < kPassCount; ++i) {
        perPassRenderData[i] = renderData;
        const auto pass = static_cast<cwRHIObject::RenderPass>(i);
        perPassRenderData[i].renderPassDescriptor = passRenderPassDescriptor(pass);
        perPassRenderData[i].sampleCount = passSampleCount(pass);
    }

    quint32 objectOrder = 0;
    for (auto object : std::as_const(m_rhiObjects)) {
        if(!object->isVisible()) {
            ++objectOrder;
            continue;
        }

        bool gathered = false;
        for (cwRHIObject::RenderPass pass : passOrder) {
            const int passIndex = static_cast<int>(pass);
            auto& batches = passBatches[passIndex];
            const cwRHIObject::GatherContext context {
                &perPassRenderData[passIndex], pass, objectOrder
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

    // Issue draws for one batches array against the currently-open pass.
    auto drainBatches = [](QRhiCommandBuffer* cb,
                           QVector<cwRHIObject::PipelineBatch>& batches,
                           QRhiGraphicsPipeline*& boundPipeline,
                           const cwRHIObject::RenderData& passRenderData) {
        if (batches.isEmpty()) {
            return;
        }

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
                    if (drawable.bindings) {
                        cb->setShaderResources(drawable.bindings);
                    } else {
                        cb->setShaderResources();
                    }

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
                    if (drawable.bindings) {
                        cb->setShaderResources(drawable.bindings);
                    } else {
                        cb->setShaderResources();
                    }

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
    };

    using RP = cwRHIObject::RenderPass;
    auto passBatchFor = [&](RP pass) -> QVector<cwRHIObject::PipelineBatch>& {
        return passBatches[static_cast<int>(pass)];
    };
    auto passDataFor = [&](RP pass) -> const cwRHIObject::RenderData& {
        return perPassRenderData[static_cast<int>(pass)];
    };

    const QColor clearColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);
    const QSize outputSize = renderer->renderTarget()->pixelSize();

    if (edlActive) {
        EdlOffscreen& edl = m_edlOffscreen;
        const QRhiViewport offscreenViewport(0, 0, edl.size.width(), edl.size.height());

        // Pass A — scene offscreen: Background + Opaque into sceneColor with
        // sceneDepth. Consumes the frame's resource-update batch.
        {
            QRhiGraphicsPipeline* bound = nullptr;
            cb->beginPass(edl.sceneTarget.get(), clearColor, { 1.0f, 0 }, resources);
            cb->setViewport(offscreenViewport);
            drainBatches(cb, passBatchFor(RP::Background), bound, passDataFor(RP::Background));
            drainBatches(cb, passBatchFor(RP::Opaque), bound, passDataFor(RP::Opaque));
            cb->endPass();
            resources = nullptr;
        }

        // Pass B — cloud offscreen: the point cloud into cloudColor while
        // *sharing* sceneDepth (loaded via PreserveDepthStencilContents), so the
        // hardware depth test occludes the cloud against scene geometry and the
        // buffer ends up holding the combined cloud+scene depth.
        {
            QRhiGraphicsPipeline* bound = nullptr;
            cb->beginPass(edl.cloudTarget.get(), clearColor, { 1.0f, 0 }, nullptr);
            cb->setViewport(offscreenViewport);
            drainBatches(cb, passBatchFor(RP::PointCloud), bound, passDataFor(RP::PointCloud));
            cb->endPass();
        }

        // Pass C — swap chain: the EDL composite writes the final scene+cloud
        // pixel (and the combined depth), then Transparent + Overlay draw on top.
        cb->beginPass(renderer->renderTarget(), clearColor, { 1.0f, 0 }, nullptr);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        const cwRhiPostProcessEffect::FrameUniformContext effectFrameCtx{
            m_projectionCorrectedMatrix, m_viewportSize, m_devicePixelRatio
        };
        edl.effect->updateFrameUniforms(effectFrameCtx);
        edl.effect->apply(cb, edl.sceneColor.get(), edl.cloudColor.get(), edl.depth.get(), outputSize);

        QRhiGraphicsPipeline* bound = nullptr;  // effect bound its own pipeline
        drainBatches(cb, passBatchFor(RP::Transparent), bound, passDataFor(RP::Transparent));
        drainBatches(cb, passBatchFor(RP::Overlay), bound, passDataFor(RP::Overlay));
        cb->endPass();
    } else {
        // No cloud: every pass renders straight to the (MSAA) swap chain.
        cb->beginPass(renderer->renderTarget(), clearColor, { 1.0f, 0 }, resources);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        QRhiGraphicsPipeline* boundPipeline = nullptr;
        for (cwRHIObject::RenderPass pass : passOrder) {
            drainBatches(cb, passBatchFor(pass), boundPipeline, passDataFor(pass));
        }
        cb->endPass();
    }
}

void cwRhiScene::updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi)
{
    const bool projectionChanged = needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix);
    const bool viewChanged = needsUpdate(cwSceneUpdate::Flag::ViewMatrix);

    if (projectionChanged || viewChanged) {
        m_projectionCorrectedMatrix = rhi->clipSpaceCorrMatrix() * m_projectionMatrix;
        m_viewProjectionMatrix = m_projectionCorrectedMatrix * m_viewMatrix;

        batch->updateDynamicBuffer(
            m_globalUniformBuffer,
            offsetof(GlobalUniform, viewProjectionMatrix),
            sizeof(GlobalUniform::viewProjectionMatrix),
            m_viewProjectionMatrix.constData()
            );
    }

    if (projectionChanged) {
        batch->updateDynamicBuffer(
            m_globalUniformBuffer,
            offsetof(GlobalUniform, projectionMatrix),
            sizeof(GlobalUniform::projectionMatrix),
            m_projectionCorrectedMatrix.constData()
            );
    }

    if (viewChanged) {
        batch->updateDynamicBuffer(
            m_globalUniformBuffer,
            offsetof(GlobalUniform, viewMatrix),
            sizeof(GlobalUniform::viewMatrix),
            m_viewMatrix.constData()
            );
    }

    if (needsUpdate(cwSceneUpdate::Flag::DevicePixelRatio)) {
        batch->updateDynamicBuffer(
            m_globalUniformBuffer,
            offsetof(GlobalUniform, devicePixelRatio),
            sizeof(GlobalUniform::devicePixelRatio),
            &m_devicePixelRatio
            );
    }

    if (needsUpdate(cwSceneUpdate::Flag::ViewportSize)) {
        const float viewportSize[2] = {
            float(m_viewportSize.width()),
            float(m_viewportSize.height()),
        };
        batch->updateDynamicBuffer(
            m_globalUniformBuffer,
            offsetof(GlobalUniform, viewportSize),
            sizeof(GlobalUniform::viewportSize),
            viewportSize
            );
    }

    m_updateFlags = cwSceneUpdate::Flag::None;
}

int cwRhiScene::effectiveEdlSampleCount(QRhi* rhi, int requestedSampleCount) const
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

void cwRhiScene::ensureEdlOffscreen(QRhi* rhi, QSize size, int requestedSampleCount)
{
    if (!rhi || size.isEmpty()) {
        return;
    }

    // Cheap early-out before the RHI capability queries in effectiveEdlSampleCount:
    // the offscreen only needs rebuilding when the size or the requested swap-chain
    // sample count changes, both of which are rare relative to the per-frame calls.
    if (m_edlOffscreen.valid() && m_edlOffscreen.size == size
        && m_edlOffscreen.requestedSampleCount == requestedSampleCount) {
        return;
    }

    const int sampleCount = effectiveEdlSampleCount(rhi, requestedSampleCount);

    // Preserve the effect across a resize — its pipeline is keyed on the stable
    // swap-chain rpDesc, and its SRB rebinds to the new textures on the next
    // apply() (cwEDLEffect::ensureBindings detects the pointer change). render()
    // re-initializes the effect when sampleCount changes the 1x / MSAA variant.
    std::unique_ptr<cwRhiPostProcessEffect> savedEffect = std::move(m_edlOffscreen.effect);
    destroyEdlOffscreen();

    EdlOffscreen fresh;
    fresh.size = size;
    fresh.requestedSampleCount = requestedSampleCount;
    fresh.sampleCount = sampleCount;

    // fresh owns its resources via unique_ptr, so any early return below releases
    // the partially-built set automatically; we only need to restore the saved
    // effect so a later frame can retry the allocation.
    auto bail = [&]() {
        m_edlOffscreen.effect = std::move(savedEffect);
    };

    // MSAA color textures can't be transfer sources; the EDL composite only
    // samples them anyway, so the flag is needed only on the 1x path.
    auto colorFlags = QRhiTexture::Flags(QRhiTexture::RenderTarget);
    if (sampleCount == 1) {
        colorFlags |= QRhiTexture::UsedAsTransferSource;
    }
    fresh.sceneColor.reset(rhi->newTexture(QRhiTexture::RGBA8, size, sampleCount, colorFlags));
    fresh.cloudColor.reset(rhi->newTexture(QRhiTexture::RGBA8, size, sampleCount, colorFlags));

    // D32F where supported; D24S8 elsewhere. Both are sampler-readable on every
    // Qt RHI backend. One depth texture is shared by both targets.
    const QRhiTexture::Format depthFormat =
        rhi->isTextureFormatSupported(QRhiTexture::D32F) ? QRhiTexture::D32F
                                                         : QRhiTexture::D24S8;
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
    }

    m_edlOffscreen = std::move(fresh);
}

void cwRhiScene::destroyEdlOffscreen()
{
    // Effect first — its SRB references the textures below.
    m_edlOffscreen.effect.reset();
    // Evict pipelines keyed on the rpDescs *before* deleting them — a newly
    // allocated rpDesc may reuse the same address and hash-collide with a stale
    // cache entry.
    if (m_edlOffscreen.sceneRpDesc) {
        evictPipelinesFor(m_edlOffscreen.sceneRpDesc.get());
    }
    if (m_edlOffscreen.cloudRpDesc) {
        evictPipelinesFor(m_edlOffscreen.cloudRpDesc.get());
    }
    // Targets before their rpDescs/textures (the proven release order); each
    // reset() both frees and nulls, so there's no field-by-field nulling to drift.
    m_edlOffscreen.sceneTarget.reset();
    m_edlOffscreen.cloudTarget.reset();
    m_edlOffscreen.sceneRpDesc.reset();
    m_edlOffscreen.cloudRpDesc.reset();
    m_edlOffscreen.sceneColor.reset();
    m_edlOffscreen.cloudColor.reset();
    m_edlOffscreen.depth.reset();
    m_edlOffscreen.size = QSize();
}

void cwRhiScene::setupPassRouting(QRhiRenderTarget* swapchainTarget, bool edlActive)
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
        route(static_cast<cwRHIObject::RenderPass>(i), swapchainTarget);
    }

    if (!edlActive) {
        return;
    }

    // Background + Opaque draw into the scene offscreen, the cloud into the cloud
    // offscreen (both 1x); the composite + Transparent + Overlay stay on the
    // swap chain. Each pass's sample count follows its offscreen target.
    using RP = cwRHIObject::RenderPass;
    route(RP::Background, m_edlOffscreen.sceneTarget.get());
    route(RP::Opaque, m_edlOffscreen.sceneTarget.get());
    route(RP::PointCloud, m_edlOffscreen.cloudTarget.get());
}

QRhiRenderPassDescriptor* cwRhiScene::passRenderPassDescriptor(cwRHIObject::RenderPass pass) const
{
    const int i = static_cast<int>(pass);
    if (i < 0 || i >= kPassCount) {
        return nullptr;
    }
    return m_passRpDesc[i];
}

int cwRhiScene::passSampleCount(cwRHIObject::RenderPass pass) const
{
    const int i = static_cast<int>(pass);
    if (i < 0 || i >= kPassCount) {
        return 1;
    }
    return m_passSampleCount[i];
}

cwRhiScene::PipelineRecord* cwRhiScene::acquirePipeline(const cwRhiPipelineKey& key,
                                                        QRhi* rhi,
                                                        const std::function<PipelineRecord*(QRhi*)>& createFn)
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

    PipelineRecord* record = createFn(rhi);
    if (!record) {
        return nullptr;
    }

    record->key = key;
    record->refCount = 1;
    m_pipelineCache.insert(key, record);
    return record;
}

void cwRhiScene::releasePipeline(PipelineRecord* record)
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

void cwRhiScene::destroyPipelineRecord(PipelineRecord* record)
{
    delete record->pipeline;
    delete record->layout;
    delete record;
}

void cwRhiScene::evictPipelinesFor(QRhiRenderPassDescriptor* descriptor)
{
    if (!descriptor) {
        return;
    }

    auto it = m_pipelineCache.begin();
    while (it != m_pipelineCache.end()) {
        if (it.key().renderPass == descriptor) {
            PipelineRecord* record = it.value();
            it = m_pipelineCache.erase(it);
            // Only destroy records nothing references anymore. A record still held
            // by an RHI object (refCount > 0 — e.g. cwRHILinePlot::m_pipelineRecord)
            // is merely orphaned from the cache here; deleting it now would dangle
            // that object's pointer and crash on its next frame. The owning object
            // deletes it via releasePipeline() when it rebuilds for the new render
            // pass (which happens because the sample count / rpDesc changed).
            if (record->refCount == 0) {
                destroyPipelineRecord(record);
            }
        } else {
            ++it;
        }
    }
}

QRhiSampler* cwRhiScene::sharedLinearClampSampler(QRhi* rhi)
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
