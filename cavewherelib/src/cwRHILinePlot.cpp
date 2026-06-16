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
    delete m_visibilityBuffer;
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

    // Create buffers — positions plus a parallel per-vertex visibility buffer.
    // Non-indexed line list, so there is no index buffer.
    m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    m_vertexBuffer->create();

    m_visibilityBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    m_visibilityBuffer->create();

    // Input layout: binding 0 = vec3 position, binding 1 = uint visibility
    // (0 = hidden, non-zero = visible). A uint keeps the attribute 4-byte
    // aligned across every RHI backend.
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(QVector3D) },
        { sizeof(quint32) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 1, 1, QRhiVertexInputAttribute::UInt, 0 }
    });
    m_inputLayout = inputLayout;
}

void cwRHILinePlot::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderLinePlot*>(data.object) != nullptr);
    cwRenderLinePlot* linePlot = static_cast<cwRenderLinePlot*>(data.object);

    m_data = linePlot->m_data;
    linePlot->m_data.resetChanged();

    m_visibility = linePlot->m_visibility;
    linePlot->m_visibility.resetChanged();
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
    }

    m_data.resetChanged();

    updateVisibilityBuffer(batch);
}

// Uploads the per-vertex visibility buffer when it changed. The render object
// keeps visibility as one byte per vertex; the GPU attribute is a uint (4-byte
// aligned on every backend), so expand on upload. Only runs on a toggle or a
// new solve, so the small expansion cost is off the hot path.
void cwRHILinePlot::updateVisibilityBuffer(QRhiResourceUpdateBatch* batch)
{
    if (!m_visibilityBuffer) {
        return;
    }

    if (m_visibility.isChanged()) {
        const QVector<quint8>& flags = m_visibility.value();

        // Expand to the 4-byte-aligned uint attribute the shader reads; the
        // range constructor widens each quint8 to quint32.
        const QVector<quint32> expanded(flags.cbegin(), flags.cend());

        const int bufferSize = expanded.size() * sizeof(quint32);
        if (bufferSize > 0) {
            if (m_visibilityBuffer->size() != bufferSize) {
                m_visibilityBuffer->setSize(bufferSize);
                m_visibilityBuffer->create();
            }
            batch->updateDynamicBuffer(m_visibilityBuffer, 0, bufferSize, expanded.constData());
        }
    }

    m_visibility.resetChanged();
}

void cwRHILinePlot::render(const RenderData& data)
{
    const int vertexCount = m_data.value().points.size();
    if (vertexCount == 0) {
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
    const QRhiCommandBuffer::VertexInput vertexInputs[2] = {
        QRhiCommandBuffer::VertexInput(m_vertexBuffer, 0),
        QRhiCommandBuffer::VertexInput(m_visibilityBuffer, 0)
    };
    data.cb->setVertexInput(0, 2, vertexInputs);
    data.cb->draw(vertexCount);
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
    if (value.points.isEmpty()) {
        return false;
    }

    const RenderData& renderData = *context.renderData;
    if (!ensurePipeline(renderData)) {
        return false;
    }

    auto* pipeline = m_pipelineRecord ? m_pipelineRecord->pipeline : nullptr;
    if (!pipeline || !m_vertexBuffer || !m_visibilityBuffer || !m_srb) {
        return false;
    }

    cwRHIObject::PipelineState state;
    state.pipeline = pipeline;
    state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, pipeline);

    auto& batch = acquirePipelineBatch(batches, state);
    cwRHIObject::Drawable drawable;
    drawable.type = cwRHIObject::Drawable::Type::NonIndexed;
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_vertexBuffer, 0));
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_visibilityBuffer, 0));
    drawable.vertexCount = static_cast<quint32>(value.points.size());
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

    const auto key = buildPipelineKey(data.renderPassDescriptor, data.sampleCount);
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

cwRhiPipelineKey cwRHILinePlot::buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                                 int sampleCount) const
{
    cwRhiPipelineKey key;
    key.renderPass = renderPassDescriptor;
    key.sampleCount = sampleCount;
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
    key.globalStages = cwShaderStageMask(cwRenderMaterialState::ShaderStage::Vertex);
    key.perDrawStages = 0;
    key.textureStages = 0;
    key.hasPerDraw = 0;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Lines);
    return key;
}
