#include "cwRHILinePlot.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwRenderLinePlot.h"
#include "cwRhiFrameRenderer.h"
#include "cwRenderMaterialState.h"
#include <QFile>
#include <QDebug>

namespace {

// The fixed TriangleStrip the vertex shader expands per instance: 4 corners
// selected from gl_VertexIndex, no corner vertex buffer.
constexpr quint32 kVerticesPerQuad = 4;

} // namespace

cwRHILinePlot::cwRHILinePlot()
{
}

cwRHILinePlot::~cwRHILinePlot()
{
    delete m_segmentBuffer;
    delete m_typeBuffer;
    delete m_visibilityBuffer;
    delete m_srb;
    // m_pipelines releases its held pipeline references on destruction.
}

void cwRHILinePlot::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized)
        return;

    initializeResources(data);
    m_resourcesInitialized = true;
}

void cwRHILinePlot::initializeResources(const ResourceUpdateData& data)
{
    auto rhi = data.renderData.cb->rhi();

    m_segmentBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    m_segmentBuffer->create();

    m_typeBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    m_typeBuffer->create();

    m_visibilityBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    m_visibilityBuffer->create();

    // All bindings step per instance; binding 0 reuses the point-pair layout
    // as one {from, to} per segment, and the uint attributes stay 4-byte
    // aligned across every RHI backend.
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 2 * sizeof(QVector3D), QRhiVertexInputBinding::PerInstance },
        { sizeof(quint32), QRhiVertexInputBinding::PerInstance },
        { sizeof(quint32), QRhiVertexInputBinding::PerInstance }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },                  // iFrom
        { 0, 1, QRhiVertexInputAttribute::Float3, sizeof(QVector3D) },  // iTo
        { 1, 2, QRhiVertexInputAttribute::UInt, 0 },                    // iType
        { 2, 3, QRhiVertexInputAttribute::UInt, 0 }                     // iVisibility
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

        const auto& value = m_data.value();

        if (!value.points.isEmpty()) {
            // The point pairs upload byte-for-byte as the per-instance
            // {from, to} buffer — the stride does the pairing.
            const int segmentBufferSize = value.points.size() * sizeof(QVector3D);
            if (m_segmentBuffer->size() != segmentBufferSize) {
                m_segmentBuffer->setSize(segmentBufferSize);
                m_segmentBuffer->create();
            }
            batch->updateDynamicBuffer(m_segmentBuffer, 0, segmentBufferSize,
                                       value.points.constData());

            const int typeBufferSize = value.segmentTypes.size() * int(sizeof(quint32));
            if (m_typeBuffer->size() != typeBufferSize) {
                m_typeBuffer->setSize(typeBufferSize);
                m_typeBuffer->create();
            }
            batch->updateDynamicBuffer(m_typeBuffer, 0, typeBufferSize,
                                       value.segmentTypes.constData());
        }
    }

    m_data.resetChanged();

    updateVisibilityBuffer(batch);
}

// Uploads the per-instance visibility attribute from the frame's snapshot of
// the scene visibility store, gated on the entry's store version so unrelated
// visibility churn never re-uploads. The store mask stays per-vertex — it is
// the intersecter's generic pick contract — and both vertices of a segment
// share their value (setRangeVisible spans whole trips), so the upload takes
// byte 2i as segment i's flag. A null mask means all visible and synthesizes
// a full buffer (the shader reads one uint per instance unconditionally).
// Only runs on a toggle or a new solve, so the compression cost is off the
// hot path. A geometry replacement changes the segment count and resets the
// store entry, so both feed the gate.
//
// This reads the frame-global snapshot, not a GatherContext: updateResources
// has no gather context, and the mask buffer is one shared GPU resource — so
// the per-instance mask is per-frame by construction. Per-job overlays
// (cwSceneGatherOptions) hide whole objects only; that split is by design.
void cwRHILinePlot::updateVisibilityBuffer(QRhiResourceUpdateBatch* batch)
{
    if (!m_visibilityBuffer) {
        return;
    }

    const cwVisibilitySnapshot& visibility = m_frame->visibilitySnapshot();
    const cwRenderObjectId id = renderObjectId();
    const quint64 entryVersion = visibility.entryVersion(id, cwRenderLinePlot::kSubId);
    const qsizetype segmentCount = m_data.value().points.size() / 2;
    if (entryVersion == m_uploadedMaskVersion
        && segmentCount == m_uploadedMaskSegmentCount) {
        return;
    }

    const QVector<quint8>* mask = visibility.mask(id, cwRenderLinePlot::kSubId);
    QVector<quint32> perInstance(segmentCount, cwRenderLinePlot::kVisible);
    if (mask) {
        const qsizetype maskedSegments = qMin(segmentCount, mask->size() / 2);
        for (qsizetype segment = 0; segment < maskedSegments; ++segment) {
            perInstance[segment] = mask->at(2 * segment);
        }
    }

    const int bufferSize = perInstance.size() * int(sizeof(quint32));
    if (bufferSize > 0) {
        if (m_visibilityBuffer->size() != bufferSize) {
            m_visibilityBuffer->setSize(bufferSize);
            m_visibilityBuffer->create();
        }
        batch->updateDynamicBuffer(m_visibilityBuffer, 0, bufferSize, perInstance.constData());
    }

    m_uploadedMaskVersion = entryVersion;
    m_uploadedMaskSegmentCount = segmentCount;
}

bool cwRHILinePlot::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
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
    if (!pipeline || !m_segmentBuffer || !m_typeBuffer || !m_visibilityBuffer || !m_srb) {
        return false;
    }

    cwRHIObject::PipelineState state;
    state.pipeline = pipeline;
    state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, pipeline);

    auto& batch = acquirePipelineBatch(batches, state);
    cwRHIObject::Drawable drawable;
    drawable.type = cwRHIObject::Drawable::Type::NonIndexed;
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_segmentBuffer, 0));
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_typeBuffer, 0));
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_visibilityBuffer, 0));
    drawable.vertexCount = kVerticesPerQuad;
    drawable.instanceCount = static_cast<quint32>(value.points.size() / 2);
    drawable.bindings = m_srb;
    drawable.globalCameraBinding = 0; // slot 0 binds the global camera UBO (dynamic offset)

    batch.drawables.append(drawable);
    return true;
}

bool cwRHILinePlot::ensurePipeline(const RenderData& data)
{
    if (!m_resourcesInitialized) {
        return false;
    }

    if (!data.renderer) {
        return false;
    }

    QRhi* rhi = data.renderer->rhi();
    auto* target = data.renderer->renderTarget();
    if (!rhi || !target) {
        return false;
    }

    const quint32 globalStride = data.renderer->globalUniformBufferStride();

    const auto key = buildPipelineKey(data.renderPassDescriptor, data.sampleCount);

    auto createFn = [this, key, globalStride](QRhi* localRhi) -> cwRhiPipelineRecord* {
        if (!localRhi) {
            return nullptr;
        }

        auto* record = new cwRhiPipelineRecord;
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
        record->pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

        QRhiGraphicsPipeline::TargetBlend blendState;
        blendState.enable = false;
        record->pipeline->setTargetBlends({ blendState });

        record->layout = localRhi->newShaderResourceBindings();
        record->layout->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, nullptr, globalStride)
        });
        record->layout->create();

        record->pipeline->setShaderResourceBindings(record->layout);
        record->pipeline->setRenderPassDescriptor(key.renderPass);
        record->pipeline->create();

        return record;
    };

    m_pipelineRecord = m_pipelines.acquire(m_frame, key, [&]() {
        return m_frame->acquirePipeline(key, rhi, createFn);
    });

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
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, renderer->globalUniformBuffer(), renderer->globalUniformBufferStride())
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
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::TriangleStrip);
    return key;
}
