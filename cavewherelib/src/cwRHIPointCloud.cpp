/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRHIPointCloud.h"
#include "cwRenderMaterialState.h"
#include "cwRenderPointCloud.h"
#include "cwRhiItemRenderer.h"
#include "cwRhiScene.h"
#include "cwScene.h"

// Qt includes
#include <QDebug>
#include <QFile>

cwRHIPointCloud::cwRHIPointCloud()
{
}

cwRHIPointCloud::~cwRHIPointCloud()
{
    delete m_vertexBuffer;
    delete m_srb;
    releasePipeline();
}

void cwRHIPointCloud::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized) {
        return;
    }

    if (!m_scene && data.renderData.renderer) {
        m_scene = data.renderData.renderer->sceneBackend();
    }

    initializeResources(data);
    m_resourcesInitialized = true;
}

void cwRHIPointCloud::initializeResources(const ResourceUpdateData& /*data*/)
{
    // Buffer is created lazily in updateResources once we know the point count.
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(QVector3D) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 }
    });
    m_inputLayout = inputLayout;
}

void cwRHIPointCloud::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderPointCloud*>(data.object) != nullptr);
    auto* pointCloud = static_cast<cwRenderPointCloud*>(data.object);

    m_data = pointCloud->m_data;
    pointCloud->m_data.resetChanged();
}

void cwRHIPointCloud::updateResources(const ResourceUpdateData& data)
{
    if (!m_data.isChanged()) {
        return;
    }

    const auto& value = m_data.value();
    const QByteArray& vertexData = value.geometry.vertexData();
    const qsizetype byteSize = vertexData.size();

    if (byteSize == 0) {
        m_data.resetChanged();
        return;
    }

    auto* rhi = data.renderData.cb->rhi();
    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

    // Immutable buffers must be recreated on size change; uploads are batched
    // once per geometry change (not per frame), so the rebuild cost is fine.
    if (!m_vertexBuffer || m_vertexBufferCapacity != byteSize) {
        delete m_vertexBuffer;
        m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable,
                                        QRhiBuffer::VertexBuffer,
                                        quint32(byteSize));
        m_vertexBuffer->create();
        m_vertexBufferCapacity = byteSize;
    }

    batch->uploadStaticBuffer(m_vertexBuffer, vertexData.constData());

    m_data.resetChanged();
}

void cwRHIPointCloud::render(const RenderData& data)
{
    if (m_data.value().geometry.vertexCount() == 0) {
        return;
    }

    if (!ensurePipeline(data)) {
        return;
    }

    if (!m_pipelineRecord || !m_pipelineRecord->pipeline || !m_vertexBuffer) {
        return;
    }

    data.cb->setGraphicsPipeline(m_pipelineRecord->pipeline);
    data.cb->setShaderResources(m_srb);
    const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer, 0);
    data.cb->setVertexInput(0, 1, &vertexInput);
    data.cb->draw(quint32(m_data.value().geometry.vertexCount()));
}

bool cwRHIPointCloud::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (!isVisible()) {
        return false;
    }

    if (context.renderPass != RenderPass::PointCloud) {
        return false;
    }

    const auto& value = m_data.value();
    const qsizetype vertexCount = value.geometry.vertexCount();
    if (vertexCount == 0) {
        return false;
    }

    const RenderData& renderData = *context.renderData;
    if (!ensurePipeline(renderData)) {
        return false;
    }

    auto* pipeline = m_pipelineRecord ? m_pipelineRecord->pipeline : nullptr;
    if (!pipeline || !m_vertexBuffer || !m_srb) {
        return false;
    }

    cwRHIObject::PipelineState state;
    state.pipeline = pipeline;
    state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, pipeline);

    auto& batch = acquirePipelineBatch(batches, state);
    cwRHIObject::Drawable drawable;
    drawable.type = cwRHIObject::Drawable::Type::NonIndexed;
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_vertexBuffer, 0));
    drawable.vertexCount = quint32(vertexCount);
    drawable.bindings = m_srb;

    batch.drawables.append(drawable);
    return true;
}

void cwRHIPointCloud::releasePipeline()
{
    if (m_pipelineRecord && m_scene) {
        m_scene->releasePipeline(m_pipelineRecord);
    }
    m_pipelineRecord = nullptr;
    m_hasPipelineKey = false;
}

bool cwRHIPointCloud::ensurePipeline(const RenderData& data)
{
    if (!m_resourcesInitialized) {
        return false;
    }

    if (!m_scene && data.renderer) {
        m_scene = data.renderer->sceneBackend();
    }

    if (!m_scene || !data.renderer) {
        return false;
    }

    QRhi* rhi = data.renderer->rhi();
    auto* target = data.renderer->renderTarget();
    if (!rhi || !target) {
        return false;
    }

    const auto key = buildPipelineKey(target, data.renderPassDescriptor);
    if (!m_hasPipelineKey || !(m_pipelineKey == key)) {
        releasePipeline();

        auto createFn = [this, key](QRhi* localRhi) -> cwRhiScene::PipelineRecord* {
            if (!localRhi) {
                return nullptr;
            }

            auto* record = new cwRhiScene::PipelineRecord;
            record->pipeline = localRhi->newGraphicsPipeline();

            QShader vs = loadShader(":/shaders/PointCloud.vert.qsb");
            QShader fs = loadShader(":/shaders/PointCloud.frag.qsb");

            record->pipeline->setShaderStages({
                { QRhiShaderStage::Vertex, vs },
                { QRhiShaderStage::Fragment, fs }
            });

            record->pipeline->setDepthTest(true);
            record->pipeline->setDepthWrite(true);
            record->pipeline->setSampleCount(key.sampleCount);
            record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
            record->pipeline->setVertexInputLayout(m_inputLayout);
            record->pipeline->setTopology(QRhiGraphicsPipeline::Points);

            QRhiGraphicsPipeline::TargetBlend blendState;
            blendState.enable = false;
            record->pipeline->setTargetBlends({ blendState });

            record->layout = localRhi->newShaderResourceBindings();
            record->layout->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, nullptr)
            });
            record->layout->create();

            record->pipeline->setShaderResourceBindings(record->layout);
            record->pipeline->setRenderPassDescriptor(key.renderPass);
            record->pipeline->create();

            return record;
        };

        m_pipelineRecord = m_scene->acquirePipeline(key, rhi, createFn);
        m_pipelineKey = key;
        m_hasPipelineKey = (m_pipelineRecord != nullptr);

        if (m_srb) {
            delete m_srb;
            m_srb = nullptr;
        }
    }

    if (!m_pipelineRecord) {
        return false;
    }

    if (!ensureShaderResources(rhi, data.renderer)) {
        return false;
    }

    return true;
}

bool cwRHIPointCloud::ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer)
{
    if (!renderer) {
        return false;
    }

    if (m_srb) {
        if (m_pipelineRecord && m_pipelineRecord->layout &&
            !m_pipelineRecord->layout->isLayoutCompatible(m_srb)) {
            delete m_srb;
            m_srb = nullptr;
        } else {
            return true;
        }
    }

    if (!rhi) {
        return false;
    }

    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, renderer->globalUniformBuffer())
    });
    m_srb->create();

    if (m_pipelineRecord && m_pipelineRecord->layout) {
        Q_ASSERT(m_pipelineRecord->layout->isLayoutCompatible(m_srb));
    }

    return true;
}

cwRhiPipelineKey cwRHIPointCloud::buildPipelineKey(QRhiRenderTarget* target,
                                                   QRhiRenderPassDescriptor* renderPassDescriptor) const
{
    cwRhiPipelineKey key;
    key.renderPass = renderPassDescriptor;
    key.sampleCount = target ? target->sampleCount() : 1;
    key.vertexShader = QStringLiteral(":/shaders/PointCloud.vert.qsb");
    key.fragmentShader = QStringLiteral(":/shaders/PointCloud.frag.qsb");
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::None);
    key.depthTest = 1;
    key.depthWrite = 1;
    key.globalBinding = 0;
    key.perDrawBinding = 0xFF;
    key.textureBinding = 0xFF;
    key.globalStages = 0x1;
    key.perDrawStages = 0;
    key.textureStages = 0;
    key.hasPerDraw = 0;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Points);
    return key;
}
