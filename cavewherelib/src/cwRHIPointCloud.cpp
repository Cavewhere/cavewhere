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
#include "cwRhiAttributeFormat.h"
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
    for (QRhiBuffer* buffer : m_vertexBuffers) {
        delete buffer;
    }
    delete m_perCloudUBO;
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
    // The input layout is built from the geometry on the first non-empty
    // updateResources call (we don't know the attribute set yet). UBOs and
    // SRBs that don't depend on geometry are created on demand below.
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
    const cwGeometry& geometry = value.geometry;
    const auto bufferViews = geometry.vertexBuffers();

    if (geometry.vertexCount() == 0 || bufferViews.isEmpty()) {
        m_data.resetChanged();
        return;
    }

    auto* rhi = data.renderData.cb->rhi();
    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

    // Build the input layout from the geometry the first time we see a
    // non-empty geometry. The geometry's attribute set is treated as
    // immutable for the lifetime of this RHI object.
    if (!m_layoutBuilt) {
        m_inputLayout = buildRhiInputLayout(geometry);
        m_layoutBuilt = true;
    }

    // Allocate one QRhiBuffer per cwGeometry vertex buffer. Interleaved
    // geometry has 1 buffer (current LAZ case); Separated would have N.
    if (m_vertexBuffers.size() != bufferViews.size()) {
        for (QRhiBuffer* buffer : m_vertexBuffers) {
            delete buffer;
        }
        m_vertexBuffers.assign(bufferViews.size(), nullptr);
        m_vertexBufferCapacities.assign(bufferViews.size(), 0);
    }

    // Immutable buffers must be recreated on size change; uploads are batched
    // once per geometry change (not per frame), so the rebuild cost is fine.
    for (qsizetype i = 0; i < bufferViews.size(); ++i) {
        const QByteArray* data = bufferViews[i].data;
        const qsizetype byteSize = data->size();
        if (!m_vertexBuffers[i] || m_vertexBufferCapacities[i] != byteSize) {
            delete m_vertexBuffers[i];
            m_vertexBuffers[i] = rhi->newBuffer(QRhiBuffer::Immutable,
                                                QRhiBuffer::VertexBuffer,
                                                quint32(byteSize));
            m_vertexBuffers[i]->create();
            m_vertexBufferCapacities[i] = byteSize;
        }
        batch->uploadStaticBuffer(m_vertexBuffers[i], data->constData());
    }

    // Per-cloud uniform — world-space point radius derived from the cloud's
    // measured mean spacing. * 0.5 because the spacing is *between* points, and
    // a radius of half that just covers the gap to a neighbor. The shader
    // applies a further `gapFudge` constant on top to compensate for clustering.
    const float worldRadius = value.meanSpacingXY * 0.5f;

    if (!m_perCloudUBO) {
        const quint32 size = rhi->ubufAligned(sizeof(PerCloudUniform));
        m_perCloudUBO = rhi->newBuffer(QRhiBuffer::Dynamic,
                                       QRhiBuffer::UniformBuffer,
                                       size);
        m_perCloudUBO->create();
    }

    if (m_lastUploadedWorldRadius != worldRadius) {
        const PerCloudUniform uniform{ worldRadius, {0.0f, 0.0f, 0.0f} };
        batch->updateDynamicBuffer(m_perCloudUBO, 0, sizeof(PerCloudUniform), &uniform);
        m_lastUploadedWorldRadius = worldRadius;
    }

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

    if (!m_pipelineRecord || !m_pipelineRecord->pipeline || m_vertexBuffers.isEmpty()) {
        return;
    }

    data.cb->setGraphicsPipeline(m_pipelineRecord->pipeline);
    data.cb->setShaderResources(m_srb);

    QVarLengthArray<QRhiCommandBuffer::VertexInput, 4> inputs;
    inputs.reserve(m_vertexBuffers.size());
    for (QRhiBuffer* buffer : m_vertexBuffers) {
        inputs.append(QRhiCommandBuffer::VertexInput(buffer, 0));
    }
    data.cb->setVertexInput(0, inputs.size(), inputs.constData());
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
    if (!pipeline || m_vertexBuffers.isEmpty() || !m_srb) {
        return false;
    }

    cwRHIObject::PipelineState state;
    state.pipeline = pipeline;
    state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, pipeline);

    auto& batch = acquirePipelineBatch(batches, state);
    cwRHIObject::Drawable drawable;
    drawable.type = cwRHIObject::Drawable::Type::NonIndexed;
    for (QRhiBuffer* buffer : m_vertexBuffers) {
        drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(buffer, 0));
    }
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

    // m_inputLayout is built from the geometry in the first non-empty
    // updateResources call — until then, the pipeline can't be created.
    if (!m_layoutBuilt) {
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
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, nullptr),
                QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, nullptr),
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
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, renderer->globalUniformBuffer()),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, m_perCloudUBO),
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
    key.perDrawBinding = 1;
    key.textureBinding = 0xFF;
    key.globalStages = 0x1;
    key.perDrawStages = 0x1;
    key.textureStages = 0;
    key.hasPerDraw = 1;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Points);
    return key;
}
