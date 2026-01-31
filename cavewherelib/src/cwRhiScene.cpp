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
    auto renderData = cwRHIObject::RenderData({cb, renderer, m_updateFlags});

    auto rhi = cb->rhi();
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

    const std::array<cwRHIObject::RenderPass, 1> passOrder = {
        cwRHIObject::RenderPass::Opaque,
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

    const QColor clearColor = QColor::fromRgbF(0.0, 0.0, 0.0, 0.0); //0.33, 0.66, 1.0, 1.0);
    cb->beginPass(renderer->renderTarget(), clearColor, { 1.0f, 0 }, resources);

    const QSize outputSize = renderer->renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    qDebug() << "Render!";

    QRhiGraphicsPipeline* boundPipeline = nullptr;
    for (cwRHIObject::RenderPass pass : passOrder) {
        const int passIndex = static_cast<int>(pass);
        auto& batches = passBatches[passIndex];
        if (batches.isEmpty()) {
            continue;
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
                qDebug() << "\tbatch depth:" << batch.name << boundPipeline->hasDepthTest() << boundPipeline->hasDepthWrite() << boundPipeline->depthOp();
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

    cb->endPass();
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
