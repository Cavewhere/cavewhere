#include "cwRHILinePlot.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwRenderLinePlot.h"
#include "cwRhiScene.h"
#include "cwRenderMaterialState.h"
#include <QFile>
#include <QDebug>

cwRHILinePlot::cwRHILinePlot()
{
}

cwRHILinePlot::~cwRHILinePlot()
{
    delete m_vertexBuffer;
    delete m_indexBuffer;
    // delete m_uniformBuffer;
    delete m_srb;
    releasePipeline();
}

void cwRHILinePlot::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized)
        return;

    if (!m_scene && data.renderData.renderer) {
        m_scene = data.renderData.renderer->sceneBackend();
    }

    initializeResources(data);
    m_resourcesInitialized = true;
}

void cwRHILinePlot::initializeResources(const ResourceUpdateData& data)
{
    auto rhi = data.renderData.cb->rhi();

    // Create buffers
    m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    m_vertexBuffer->create();

    m_indexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, 0);
    m_indexBuffer->create();

    // Input layout
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(QVector3D) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 }
    });
    m_inputLayout = inputLayout;
}

void cwRHILinePlot::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderLinePlot*>(data.object) != nullptr);
    cwRenderLinePlot* linePlot = static_cast<cwRenderLinePlot*>(data.object);

    m_data = linePlot->m_data;
    linePlot->m_data.resetChanged();
}

void cwRHILinePlot::updateResources(const ResourceUpdateData& data)
{
    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

    if(m_data.isChanged()) {

        const auto data = m_data.value();

        if (!data.points.isEmpty()) {
            // Update vertex buffer
            int vertexBufferSize = data.points.size() * sizeof(QVector3D);
            if (m_vertexBuffer->size() != vertexBufferSize) {
                m_vertexBuffer->setSize(vertexBufferSize);
                m_vertexBuffer->create();
            }
            batch->updateDynamicBuffer(m_vertexBuffer, 0, vertexBufferSize, data.points.constData());
        }

        if (!data.indexes.isEmpty()) {
            // Update index buffer
            int indexBufferSize = data.indexes.size() * sizeof(unsigned int);
            if (m_indexBuffer->size() != indexBufferSize) {
                m_indexBuffer->setSize(indexBufferSize);
                m_indexBuffer->create();
            }
            batch->updateDynamicBuffer(m_indexBuffer, 0, indexBufferSize, data.indexes.constData());
        }

        // UniformData uniformData;
        // memcpy(uniformData.mvpMatrix, mvp.constData(), sizeof(float) * 16);
        // uniformData.maxZValue = data.maxZValue;
        // uniformData.minZValue = data.minZValue;

        // // Update the buffer with m_mvpMatrix
        // batch->updateDynamicBuffer(
        //     m_uniformBuffer,
        //     offsetof(UniformData, mvpMatrix),
        //     sizeof(UniformData::mvpMatrix),
        //     mvp.constData()
        //     );

        // batch->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(UniformData), &uniformData);
    }

    m_data.resetChanged();
}

void cwRHILinePlot::render(const RenderData& data)
{
    if (m_data.value().indexes.size() == 0) {
        return;
    }

    if (!ensurePipeline(data)) {
        return;
    }

    if (!m_pipelineRecord || !m_pipelineRecord->pipeline) {
        return;
    }

    data.cb->setGraphicsPipeline(m_pipelineRecord->pipeline);
    data.cb->setShaderResources(m_srb);
    const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer, 0);
    data.cb->setVertexInput(0, 1, &vertexInput, m_indexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
    data.cb->drawIndexed(m_data.value().indexes.size());
}

bool cwRHILinePlot::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (!isVisible()) {
        return false;
    }

    if (context.renderPass != RenderPass::Opaque) {
        return false;
    }

    const auto& value = m_data.value();
    if (value.indexes.isEmpty()) {
        return false;
    }

    const RenderData& renderData = *context.renderData;
    if (!ensurePipeline(renderData)) {
        return false;
    }

    auto* pipeline = m_pipelineRecord ? m_pipelineRecord->pipeline : nullptr;
    if (!pipeline || !m_vertexBuffer || !m_indexBuffer || !m_srb) {
        return false;
    }

    cwRHIObject::PipelineState state;
    state.pipeline = pipeline;
    const quint64 pipelineKey = quint64(quintptr(pipeline));
    state.sortKey = (quint64(context.objectOrder) << 32) | (pipelineKey & 0xffffffffu);

    auto& batch = acquirePipelineBatch(batches, state);
    cwRHIObject::Drawable drawable;
    drawable.type = cwRHIObject::Drawable::Type::Indexed;
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_vertexBuffer, 0));
    drawable.indexBuffer = m_indexBuffer;
    drawable.indexFormat = QRhiCommandBuffer::IndexUInt32;
    drawable.indexCount = static_cast<quint32>(value.indexes.size());
    drawable.bindings = m_srb;

    batch.drawables.append(drawable);
    return true;
}

void cwRHILinePlot::releasePipeline()
{
    if (m_pipelineRecord && m_scene) {
        m_scene->releasePipeline(m_pipelineRecord);
    }
    m_pipelineRecord = nullptr;
    m_hasPipelineKey = false;
}

bool cwRHILinePlot::ensurePipeline(const RenderData& data)
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

    const auto key = buildPipelineKey(target);
    if (!m_hasPipelineKey || !(m_pipelineKey == key)) {
        releasePipeline();

        auto createFn = [this, key](QRhi* localRhi) -> cwRhiScene::PipelineRecord* {
            if (!localRhi) {
                return nullptr;
            }

            auto* record = new cwRhiScene::PipelineRecord;
            record->pipeline = localRhi->newGraphicsPipeline();

            QShader vs = loadShader(":/shaders/LinePlot.vert.qsb");
            QShader fs = loadShader(":/shaders/LinePlot.frag.qsb");

            record->pipeline->setShaderStages({
                { QRhiShaderStage::Vertex, vs },
                { QRhiShaderStage::Fragment, fs }
            });

            record->pipeline->setDepthTest(true);
            record->pipeline->setDepthWrite(true);
            record->pipeline->setSampleCount(key.sampleCount);
            record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
            record->pipeline->setVertexInputLayout(m_inputLayout);
            record->pipeline->setTopology(QRhiGraphicsPipeline::Lines);

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

bool cwRHILinePlot::ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer)
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

cwRhiPipelineKey cwRHILinePlot::buildPipelineKey(QRhiRenderTarget* target) const
{
    cwRhiPipelineKey key;
    key.renderPass = target ? target->renderPassDescriptor() : nullptr;
    key.sampleCount = target ? target->sampleCount() : 1;
    key.vertexShader = QStringLiteral(":/shaders/LinePlot.vert.qsb");
    key.fragmentShader = QStringLiteral(":/shaders/LinePlot.frag.qsb");
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::None);
    key.depthTest = 1;
    key.depthWrite = 1;
    key.globalBinding = 0;
    key.perDrawBinding = 0xFF;
    key.textureBinding = 0xFF;
    key.globalStages = 0x1; // vertex stage for camera uniform
    key.perDrawStages = 0;
    key.textureStages = 0;
    key.hasPerDraw = 0;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Lines);
    return key;
}
