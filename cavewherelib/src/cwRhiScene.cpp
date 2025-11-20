//Our includes
#include "cwRhiScene.h"
#include "cwRHIObject.h"
#include "cwRenderObject.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwCamera.h"

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
    seed = qHash(key.colorAttachmentCount, seed);
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

    for (auto record : std::as_const(m_pipelineCache)) {
        delete record->pipeline;
        delete record->layout;
        delete record;
    }
    m_pipelineCache.clear();

    releaseTransparentResources();
    delete m_transparent.fullscreenQuad;
    m_transparent.fullscreenQuad = nullptr;

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

QRhiRenderTarget* cwRhiScene::renderTargetForPass(cwRHIObject::RenderPass pass) const
{
    const int idx = passIndex(pass);
    if (idx < 0 || idx >= m_passStates.size()) {
        return nullptr;
    }
    return m_passStates[idx].target;
}

int cwRhiScene::colorAttachmentCountForPass(cwRHIObject::RenderPass pass) const
{
    const int idx = passIndex(pass);
    if (idx < 0 || idx >= m_passStates.size()) {
        return 1;
    }
    return m_passStates[idx].colorAttachmentCount;
}

void cwRhiScene::render(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    auto renderData = cwRHIObject::RenderData({cb, renderer, this, m_updateFlags});

    auto rhi = cb->rhi();
    preparePassStates(rhi, renderer, cb);

    QRhiResourceUpdateBatch* resources = rhi->nextResourceUpdateBatch();
    auto resourceUpdateData = cwRHIObject::ResourceUpdateData({resources, renderData});

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

    //For future implementations
    // const std::array<cwRHIObject::RenderPass, 3> passOrder = {
    //     cwRHIObject::RenderPass::Opaque,
    //     cwRHIObject::RenderPass::Transparent,
    //     cwRHIObject::RenderPass::Overlay
    // };

    const std::array<cwRHIObject::RenderPass, 3> passOrder = {
        cwRHIObject::RenderPass::Opaque,
        cwRHIObject::RenderPass::Transparent,
        cwRHIObject::RenderPass::Overlay
    };

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
            const cwRHIObject::GatherContext context { &renderData, pass, objectOrder };
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

    const QColor clearColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0);
    QRhiRenderTarget* opaqueTarget = renderer->renderTarget();
    const QSize outputSize = opaqueTarget ? opaqueTarget->pixelSize() : QSize();

    if (opaqueTarget) {
        cb->beginPass(opaqueTarget, clearColor, { 1.0f, 0 }, resources);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
        executeBatches(cb, renderData, passBatches[passIndex(cwRHIObject::RenderPass::Opaque)]);
        cb->endPass();
    }

    const bool hasTransparent = !passBatches[passIndex(cwRHIObject::RenderPass::Transparent)].isEmpty();
    if (hasTransparent && m_transparent.renderTarget) {
        cb->beginPass(m_transparent.renderTarget,
                      QColor::fromRgbF(0.0, 0.0, 0.0, 0.0),
                      { 1.0f, 0 },
                      nullptr);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
        executeBatches(cb, renderData, passBatches[passIndex(cwRHIObject::RenderPass::Transparent)]);
        cb->endPass();
    }

    auto& overlayBatches = passBatches[passIndex(cwRHIObject::RenderPass::Overlay)];
    const bool needsFinalPass = (hasTransparent && m_transparent.compositePipeline && m_transparent.compositeBindings)
                                || !overlayBatches.isEmpty();

    if (needsFinalPass && opaqueTarget) {
        cb->beginPass(opaqueTarget,
                      clearColor,
                      { 1.0f, 0 },
                      nullptr,
                      QRhiCommandBuffer::BeginPassFlag::ExternalContent);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

        if (hasTransparent && m_transparent.compositePipeline && m_transparent.compositeBindings && m_transparent.fullscreenQuad) {
            cb->setGraphicsPipeline(m_transparent.compositePipeline);
            cb->setShaderResources(m_transparent.compositeBindings);
            const QRhiCommandBuffer::VertexInput vbuf(m_transparent.fullscreenQuad, 0);
            cb->setVertexInput(0, 1, &vbuf);
            cb->draw(4);
        }

        executeBatches(cb, renderData, overlayBatches);
        cb->endPass();
    }
}

int cwRhiScene::passIndex(cwRHIObject::RenderPass pass)
{
    return static_cast<int>(pass);
}

void cwRhiScene::preparePassStates(QRhi* rhi,
                                   cwRhiItemRenderer* renderer,
                                   QRhiCommandBuffer* cb)
{
    if (!renderer) {
        return;
    }

    auto* target = renderer->renderTarget();
    const int opaqueIdx = passIndex(cwRHIObject::RenderPass::Opaque);
    const int overlayIdx = passIndex(cwRHIObject::RenderPass::Overlay);

    m_passStates[opaqueIdx].target = target;
    m_passStates[opaqueIdx].colorAttachmentCount = 1;
    m_passStates[overlayIdx] = m_passStates[opaqueIdx];

    ensureTransparentResources(rhi, renderer, cb);
}

void cwRhiScene::ensureTransparentResources(QRhi* rhi,
                                            cwRhiItemRenderer* renderer,
                                            QRhiCommandBuffer* cb)
{
    const int transparentIdx = passIndex(cwRHIObject::RenderPass::Transparent);
    if (!renderer || !renderer->renderTarget() || !renderer->depthStencilBuffer()) {
        m_passStates[transparentIdx].target = nullptr;
        releaseTransparentResources();
        return;
    }

    const QSize size = renderer->renderTarget()->pixelSize();
    QRhiRenderBuffer* depthBuffer = renderer->depthStencilBuffer();

    const bool sizeChanged = size != m_transparent.size;
    const bool depthChanged = depthBuffer != m_transparent.depthBuffer;

    if (sizeChanged || depthChanged) {
        releaseTransparentResources();
    }

    if (!m_transparent.renderTarget) {
        if (size.isEmpty()) {
            m_passStates[transparentIdx].target = nullptr;
            return;
        }

        m_transparent.size = size;
        m_transparent.depthBuffer = depthBuffer;

        m_transparent.accumulationTexture = rhi->newTexture(QRhiTexture::RGBA16F,
                                                            size,
                                                            1,
                                                            QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore);
        if (!m_transparent.accumulationTexture || !m_transparent.accumulationTexture->create()) {
            releaseTransparentResources();
            m_passStates[transparentIdx].target = nullptr;
            return;
        }

        QRhiColorAttachment colorAttachment(m_transparent.accumulationTexture);
        QRhiTextureRenderTargetDescription desc(colorAttachment);
        desc.setDepthStencilBuffer(depthBuffer);

        m_transparent.renderTarget = rhi->newTextureRenderTarget(desc);
        m_transparent.renderTarget->setFlags(QRhiTextureRenderTarget::PreserveDepthStencilContents);
        m_transparent.renderPassDesc = m_transparent.renderTarget->newCompatibleRenderPassDescriptor();
        m_transparent.renderTarget->setRenderPassDescriptor(m_transparent.renderPassDesc);
        if (!m_transparent.renderTarget->create()) {
            releaseTransparentResources();
            m_passStates[transparentIdx].target = nullptr;
            return;
        }
    }

    if (cb) {
        ensureFullscreenQuad(rhi, cb);
    }

    m_passStates[transparentIdx].target = m_transparent.renderTarget;
    m_passStates[transparentIdx].colorAttachmentCount = 1;

    ensureCompositePipeline(rhi, renderer);
}

void cwRhiScene::releaseTransparentResources()
{
    delete m_transparent.renderTarget;
    m_transparent.renderTarget = nullptr;
    delete m_transparent.renderPassDesc;
    m_transparent.renderPassDesc = nullptr;
    delete m_transparent.accumulationTexture;
    m_transparent.accumulationTexture = nullptr;
    m_transparent.boundAccumulationTexture = nullptr;
    m_transparent.size = {};
    m_transparent.depthBuffer = nullptr;
    m_passStates[passIndex(cwRHIObject::RenderPass::Transparent)].target = nullptr;
    releaseCompositePipeline();
}

void cwRhiScene::ensureCompositePipeline(QRhi* rhi, cwRhiItemRenderer* renderer)
{
    if (!renderer || !renderer->renderTarget() || !m_transparent.accumulationTexture) {
        releaseCompositePipeline();
        return;
    }

    auto* sampler = sharedLinearClampSampler(rhi);
    if (!sampler) {
        return;
    }

    if (!m_transparent.compositeBindings || m_transparent.boundAccumulationTexture != m_transparent.accumulationTexture) {
        delete m_transparent.compositeBindings;
        m_transparent.compositeBindings = nullptr;

        m_transparent.compositeBindings = rhi->newShaderResourceBindings();
        m_transparent.compositeBindings->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0,
                                                      QRhiShaderResourceBinding::FragmentStage,
                                                      m_transparent.accumulationTexture,
                                                      sampler)
        });
        m_transparent.compositeBindings->create();
        m_transparent.boundAccumulationTexture = m_transparent.accumulationTexture;
    }

    QRhiRenderPassDescriptor* finalPassDesc = renderer->renderTarget()->renderPassDescriptor();
    if (m_transparent.compositePipeline && m_transparent.compositePassDescriptor == finalPassDesc) {
        return;
    }

    delete m_transparent.compositePipeline;
    m_transparent.compositePipeline = nullptr;
    m_transparent.compositePassDescriptor = finalPassDesc;

    QRhiGraphicsPipeline* pipeline = rhi->newGraphicsPipeline();
    pipeline->setCullMode(QRhiGraphicsPipeline::None);
    pipeline->setDepthTest(false);
    pipeline->setDepthWrite(false);
    pipeline->setSampleCount(renderer->renderTarget()->sampleCount());
    pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

    QShader vertex = cwRHIObject::loadShader(QStringLiteral(":/shaders/oit_composite.vert.qsb"));
    QShader fragment = cwRHIObject::loadShader(QStringLiteral(":/shaders/oit_composite.frag.qsb"));
    pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertex },
        { QRhiShaderStage::Fragment, fragment }
    });

    QRhiVertexInputLayout layout;
    layout.setBindings({ int(sizeof(float) * 4) });
    layout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, int(sizeof(float) * 2) }
    });

    pipeline->setVertexInputLayout(layout);

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::One;
    blend.srcAlpha = QRhiGraphicsPipeline::Zero;
    blend.dstAlpha = QRhiGraphicsPipeline::One;
    pipeline->setTargetBlends({ blend });

    pipeline->setShaderResourceBindings(m_transparent.compositeBindings);
    pipeline->setRenderPassDescriptor(finalPassDesc);
    pipeline->create();

    m_transparent.compositePipeline = pipeline;
}

void cwRhiScene::releaseCompositePipeline()
{
    delete m_transparent.compositePipeline;
    m_transparent.compositePipeline = nullptr;
    delete m_transparent.compositeBindings;
    m_transparent.compositeBindings = nullptr;
    m_transparent.boundAccumulationTexture = nullptr;
    m_transparent.compositePassDescriptor = nullptr;
}

void cwRhiScene::executeBatches(QRhiCommandBuffer* cb,
                                const cwRHIObject::RenderData& renderData,
                                QVector<cwRHIObject::PipelineBatch>& batches)
{
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

    QRhiGraphicsPipeline* boundPipeline = nullptr;
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
}

void cwRhiScene::ensureFullscreenQuad(QRhi* rhi, QRhiCommandBuffer* cb)
{
    if (m_transparent.fullscreenQuad || !cb) {
        return;
    }

    static const float quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };

    m_transparent.fullscreenQuad = rhi->newBuffer(QRhiBuffer::Immutable,
                                                  QRhiBuffer::VertexBuffer,
                                                  sizeof(quadVertices));
    if (!m_transparent.fullscreenQuad) {
        return;
    }
    m_transparent.fullscreenQuad->create();

    QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(m_transparent.fullscreenQuad, quadVertices);
    cb->resourceUpdate(batch);
}

void cwRhiScene::updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi)
{
    if(needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix) || needsUpdate(cwSceneUpdate::Flag::ViewMatrix)) {
        m_projectionCorrectedMatrix = rhi->clipSpaceCorrMatrix() * m_projectionMatrix;
        m_viewProjectionMatrix = m_projectionCorrectedMatrix * m_viewMatrix;

        batch->updateDynamicBuffer(
            m_globalUniformBuffer,
            offsetof(GlobalUniform, viewProjectionMatrix),
            sizeof(GlobalUniform::viewProjectionMatrix),
            m_viewProjectionMatrix.constData()
            );


        if(needsUpdate(cwSceneUpdate::Flag::ProjectionMatrix)) {
            batch->updateDynamicBuffer(
                m_globalUniformBuffer,
                offsetof(GlobalUniform, projectionMatrix),
                sizeof(GlobalUniform::projectionMatrix),
                m_projectionCorrectedMatrix.constData()
                );
        }

        if(needsUpdate(cwSceneUpdate::Flag::ViewMatrix)) {
            batch->updateDynamicBuffer(
                m_globalUniformBuffer,
                offsetof(GlobalUniform, viewMatrix),
                sizeof(GlobalUniform::viewMatrix),
                m_viewMatrix.constData()
                );
        }
    }

    m_updateFlags = cwSceneUpdate::Flag::None;
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
