//Our includes
#include "cwRhiScene.h"
#include "cwRHIObject.h"
#include "cwRenderObject.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwCamera.h"
#include "cwEDLEffect.h"

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

    // Tear down per-pass offscreen targets before the pipeline cache so
    // pipelines keyed on the per-pass rpDesc are released first.
    for (auto& kv : m_passConfigs) {
        destroyPassConfig(kv.second);
    }
    m_passConfigs.clear();

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

    ensurePointCloudPass(rhi, m_viewportSize);

    // The swap-chain rpDesc isn't available until the first render(), so
    // post-process effects are initialized here. sampleCount must match the
    // swap-chain (4× MSAA per cw3dRegionViewer::setSampleCount) — a mismatched
    // pipeline only writes sample 0, producing dim translucent output after
    // resolve.
    if (!m_effectsInitialized && swapchainRPDesc && renderer && renderer->renderTarget()) {
        const int swapchainSampleCount = renderer->renderTarget()->sampleCount();
        for (auto& kv : m_passConfigs) {
            for (auto& fx : kv.second.effects) {
                fx->initialize(rhi, swapchainRPDesc, swapchainSampleCount, m_globalUniformBuffer);
            }
        }
        m_effectsInitialized = true;
    }

    cwRHIObject::RenderData renderData{cb, renderer, m_updateFlags, swapchainRPDesc};

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

    constexpr int kPassCount = static_cast<int>(cwRHIObject::RenderPass::Count);
    using PassBatchesArray = std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount>;
    PassBatchesArray passBatches;

    // Pass order. Passes without an entry in m_passConfigs render straight
    // to the swap-chain; passes with an entry use the offscreen target there.
    const std::array<cwRHIObject::RenderPass, 5> passOrder = {
        cwRHIObject::RenderPass::Background,
        cwRHIObject::RenderPass::PointCloud,
        cwRHIObject::RenderPass::Opaque,
        cwRHIObject::RenderPass::Transparent,
        cwRHIObject::RenderPass::Overlay,
    };

    // Per-pass render-data: gather() reads renderData.renderPassDescriptor
    // when building pipeline keys, so each pass needs its own copy with the
    // pass's rpDesc (offscreen target or swap-chain fallback).
    std::array<cwRHIObject::RenderData, kPassCount> perPassRenderData;
    for (int i = 0; i < kPassCount; ++i) {
        perPassRenderData[i] = renderData;
        const auto pass = static_cast<cwRHIObject::RenderPass>(i);
        auto cfgIt = m_passConfigs.find(pass);
        if (cfgIt != m_passConfigs.end() && cfgIt->second.rpDesc) {
            perPassRenderData[i].renderPassDescriptor = cfgIt->second.rpDesc;
        }
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
    auto drainBatches = [&renderData](QRhiCommandBuffer* cb,
                                      QVector<cwRHIObject::PipelineBatch>& batches,
                                      QRhiGraphicsPipeline*& boundPipeline) {
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
                        drawable.customDraw(renderData);
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
    const int pcIndex = static_cast<int>(RP::PointCloud);
    auto pcCfgIt = m_passConfigs.find(RP::PointCloud);
    const bool runPointCloudOffscreenPass =
        (pcCfgIt != m_passConfigs.end()
         && pcCfgIt->second.target != nullptr
         && !passBatches[pcIndex].isEmpty());

    // Offscreen PointCloud pass must complete before the swap-chain pass so
    // its color+depth textures are sampler-readable when EDL samples them.
    // Qt RHI inserts the necessary barriers.
    if (runPointCloudOffscreenPass) {
        PassConfig& cfg = pcCfgIt->second;
        QRhiGraphicsPipeline* offscreenBound = nullptr;
        cb->beginPass(cfg.target,
                      QColor::fromRgbF(0.0, 0.0, 0.0, 0.0),
                      { 1.0f, 0 },
                      resources);
        cb->setViewport(QRhiViewport(0, 0, cfg.size.width(), cfg.size.height()));
        drainBatches(cb, passBatches[pcIndex], offscreenBound);
        cb->endPass();

        // resources batch has been consumed by beginPass; null it so the
        // swap-chain beginPass below doesn't double-submit.
        resources = nullptr;
    }

    // Swap-chain pass: Background → EDL composite (inline) → Opaque / etc.
    const QColor clearColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);
    cb->beginPass(renderer->renderTarget(), clearColor, { 1.0f, 0 }, resources);

    const QSize outputSize = renderer->renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    QRhiGraphicsPipeline* boundPipeline = nullptr;
    for (cwRHIObject::RenderPass pass : passOrder) {
        if (pass == RP::PointCloud && runPointCloudOffscreenPass) {
            // Cloud batches were drawn offscreen; this slot runs the effect chain.
            PassConfig& cfg = pcCfgIt->second;
            for (auto& fx : cfg.effects) {
                fx->apply(cb, cfg.color, cfg.depth, outputSize);
            }
            // Effects bind their own pipeline + SRB; reset the tracker.
            boundPipeline = nullptr;
            continue;
        }

        drainBatches(cb, passBatches[static_cast<int>(pass)], boundPipeline);
    }

    cb->endPass();
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

void cwRhiScene::ensurePointCloudPass(QRhi* rhi, QSize size)
{
    if (!rhi || size.isEmpty()) {
        return;
    }

    using RP = cwRHIObject::RenderPass;
    auto it = m_passConfigs.find(RP::PointCloud);
    const bool exists = it != m_passConfigs.end();
    const bool needsRebuild = exists && it->second.size != size;

    if (exists && !needsRebuild) {
        return;
    }

    // Move the effect chain off before destroying the rest of the pass; the
    // effects' pipelines are bound to the swap-chain rpDesc (stable across
    // resizes) and their SRBs detect the new color/depth via pointer-change
    // in cwEDLEffect::ensureBindings(), so they survive a rebuild as-is.
    std::vector<std::unique_ptr<cwRhiPostProcessEffect>> savedEffects;
    if (needsRebuild) {
        savedEffects = std::move(it->second.effects);
        destroyPassConfig(it->second);
        m_passConfigs.erase(it);
    }

    PassConfig fresh;
    fresh.size = size;

    fresh.color = rhi->newTexture(QRhiTexture::RGBA8,
                                  size,
                                  1,
                                  QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    if (!fresh.color->create()) {
        delete fresh.color;
        return;
    }

    // D32F where supported; D24S8 elsewhere. Both are sampler-readable on
    // every Qt RHI backend.
    const QRhiTexture::Format depthFormat =
        rhi->isTextureFormatSupported(QRhiTexture::D32F) ? QRhiTexture::D32F
                                                         : QRhiTexture::D24S8;
    fresh.depth = rhi->newTexture(depthFormat, size, 1, QRhiTexture::RenderTarget);
    if (!fresh.depth->create()) {
        delete fresh.color;
        delete fresh.depth;
        return;
    }

    QRhiTextureRenderTargetDescription desc;
    desc.setColorAttachments({ QRhiColorAttachment(fresh.color) });
    desc.setDepthTexture(fresh.depth);

    fresh.target = rhi->newTextureRenderTarget(desc);
    fresh.rpDesc = fresh.target->newCompatibleRenderPassDescriptor();
    if (!fresh.rpDesc) {
        delete fresh.target;
        delete fresh.color;
        delete fresh.depth;
        return;
    }
    fresh.target->setRenderPassDescriptor(fresh.rpDesc);
    if (!fresh.target->create()) {
        delete fresh.target;
        delete fresh.rpDesc;
        delete fresh.color;
        delete fresh.depth;
        return;
    }

    if (needsRebuild) {
        fresh.effects = std::move(savedEffects);
    } else {
        fresh.effects.push_back(std::make_unique<cwEDLEffect>());
    }

    m_passConfigs.emplace(RP::PointCloud, std::move(fresh));
}

void cwRhiScene::destroyPassConfig(PassConfig& cfg)
{
    // Effects must die before the textures they sample (their SRBs reference
    // cfg.color / cfg.depth).
    cfg.effects.clear();
    // Evict pipelines keyed on the rpDesc *before* deleting it — newly
    // allocated rpDescs may reuse the same address and hash-collide with
    // stale cache entries.
    if (cfg.rpDesc) {
        evictPipelinesFor(cfg.rpDesc);
    }
    delete cfg.target;
    delete cfg.rpDesc;
    delete cfg.color;
    delete cfg.depth;
    cfg.target = nullptr;
    cfg.rpDesc = nullptr;
    cfg.color = nullptr;
    cfg.depth = nullptr;
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
            delete record->pipeline;
            delete record->layout;
            delete record;
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
