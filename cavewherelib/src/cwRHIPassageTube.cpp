/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRHIPassageTube.h"
#include "cwRenderMaterialState.h"
#include "cwRhiItemRenderer.h"
#include "cwSceneVisibility.h"

// Qt includes
#include <QVector4D>

namespace {

constexpr int kTriangleCornerCount = 3;

constexpr QVector4D kTubeColor(0.62f, 0.60f, 0.55f, 1.0f);
constexpr float kTubeAmbient = 0.35f;

constexpr float kYUpSign = 1.0f;
constexpr float kYDownSign = -1.0f;

// Pulls the wireframe lines in front of the filled triangles they outline.
constexpr int kWireframeDepthBias = -1;
constexpr float kWireframeSlopeScaledDepthBias = -1.0f;

} // namespace

cwRHIPassageTube::~cwRHIPassageTube()
{
    delete m_vertexBuffer;
    delete m_indexBuffer;
    delete m_uniformBuffer;
    delete m_srb;
}

void cwRHIPassageTube::initialize(const ResourceUpdateData& /*data*/)
{
    m_inputLayout.setBindings({ { sizeof(QVector3D) } });
    m_inputLayout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float3, 0 } });
}

void cwRHIPassageTube::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderPassageTube*>(data.object) != nullptr);
    auto* tube = static_cast<cwRenderPassageTube*>(data.object);

    m_geometry = tube->m_geometry;
    m_wireframe = tube->m_wireframe;
    tube->m_geometry.resetChanged();
}

void cwRHIPassageTube::updateResources(const ResourceUpdateData& data)
{
    QRhi* rhi = data.renderData.cb->rhi();
    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

    if (m_geometry.isChanged()) {
        uploadVertices(rhi, batch);
        m_indicesStale = true;
        m_geometry.resetChanged();
    }

    uploadVisibleIndices(rhi, batch);

    if (!m_uniformBuffer) {
        createUniform(rhi, batch);
    }

    reportGpuBytes();
}

void cwRHIPassageTube::uploadVertices(QRhi* rhi, QRhiResourceUpdateBatch* batch)
{
    const QVector<QVector3D>& positions = m_geometry.value().positions;
    const quint32 byteSize = quint32(positions.size() * qsizetype(sizeof(QVector3D)));
    if (byteSize == 0) {
        delete m_vertexBuffer;
        m_vertexBuffer = nullptr;
        return;
    }

    if (!m_vertexBuffer || m_vertexBuffer->size() != byteSize) {
        delete m_vertexBuffer;
        m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, byteSize);
        m_vertexBuffer->create();
    }
    batch->uploadStaticBuffer(m_vertexBuffer, 0, byteSize, positions.constData());
}

// Rebuilds the index buffer from the triangles whose indices are visible in the
// frame's snapshot of the scene visibility store, gated on the entry version so
// only a trip toggle or new geometry pays for it. A triangle owns indices
// [3t, 3t + 3) and trip ranges cover whole triangles, so index 3t decides it.
void cwRHIPassageTube::uploadVisibleIndices(QRhi* rhi, QRhiResourceUpdateBatch* batch)
{
    const cwVisibilitySnapshot& visibility = m_frame->visibilitySnapshot();
    const quint64 entryVersion = visibility.entryVersion(renderObjectId(),
                                                         cwRenderPassageTube::kSubId);
    if (!m_indicesStale && entryVersion == m_uploadedMaskVersion) {
        return;
    }

    const QVector<quint32>& indices = m_geometry.value().indices;
    const QVector<quint8>* mask = visibility.mask(renderObjectId(), cwRenderPassageTube::kSubId);

    QVector<quint32> visibleIndices;
    if (mask) {
        visibleIndices.reserve(indices.size());
        for (qsizetype first = 0; first + kTriangleCornerCount <= indices.size();
             first += kTriangleCornerCount) {
            const bool visible = first >= mask->size()
                                 || mask->at(first) != cwRenderPassageTube::kHidden;
            if (visible) {
                for (int corner = 0; corner < kTriangleCornerCount; ++corner) {
                    visibleIndices.append(indices.at(first + corner));
                }
            }
        }
    } else {
        visibleIndices = indices;
    }

    const quint32 byteSize = quint32(visibleIndices.size() * qsizetype(sizeof(quint32)));
    if (byteSize > 0) {
        if (!m_indexBuffer || m_indexBuffer->size() != byteSize) {
            delete m_indexBuffer;
            m_indexBuffer = rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::IndexBuffer, byteSize);
            m_indexBuffer->create();
        }
        batch->uploadStaticBuffer(m_indexBuffer, 0, byteSize, visibleIndices.constData());
    } else {
        delete m_indexBuffer;
        m_indexBuffer = nullptr;
    }

    m_visibleIndexCount = quint32(visibleIndices.size());
    m_uploadedMaskVersion = entryVersion;
    m_indicesStale = false;
}

void cwRHIPassageTube::createUniform(QRhi* rhi, QRhiResourceUpdateBatch* batch)
{
    m_uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                     sizeof(TubeUniform));
    m_uniformBuffer->create();

    const TubeUniform uniform {
        { kTubeColor.x(), kTubeColor.y(), kTubeColor.z(), kTubeColor.w() },
        kTubeAmbient,
        rhi->isYUpInFramebuffer() ? kYUpSign : kYDownSign,
        { 0.0f, 0.0f }
    };
    batch->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(TubeUniform), &uniform);
}

void cwRHIPassageTube::reportGpuBytes()
{
    qint64 bytes = 0;
    for (const QRhiBuffer* buffer : {m_vertexBuffer, m_indexBuffer, m_uniformBuffer}) {
        if (buffer) {
            bytes += buffer->size();
        }
    }
    m_gpuBytes.setBytes(bytes);
}

bool cwRHIPassageTube::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (context.renderPass != RenderPass::Opaque) {
        return false;
    }

    if (m_visibleIndexCount == 0 || !m_vertexBuffer || !m_indexBuffer || !m_uniformBuffer) {
        return false;
    }

    const RenderData& renderData = *context.renderData;
    m_pipelineRecord = acquirePipeline(renderData, PipelineKind::Fill);
    if (!m_pipelineRecord) {
        return false;
    }
    QRhi* rhi = renderData.renderer->rhi();
    ensureShaderResources(rhi, renderData.renderer);

    const auto appendDrawable = [&](QRhiGraphicsPipeline* pipeline) {
        cwRHIObject::PipelineState state;
        state.pipeline = pipeline;
        state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, pipeline);

        cwRHIObject::Drawable drawable;
        drawable.type = cwRHIObject::Drawable::Type::Indexed;
        drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_vertexBuffer, 0));
        drawable.indexBuffer = m_indexBuffer;
        drawable.indexFormat = QRhiCommandBuffer::IndexUInt32;
        drawable.indexCount = m_visibleIndexCount;
        drawable.bindings = m_srb;
        drawable.globalCameraBinding = 0;

        acquirePipelineBatch(batches, state).drawables.append(drawable);
    };

    appendDrawable(m_pipelineRecord->pipeline);

    if (m_wireframe && rhi->isFeatureSupported(QRhi::NonFillPolygonMode)) {
        m_wireframeRecord = acquirePipeline(renderData, PipelineKind::Wireframe);
        if (m_wireframeRecord) {
            appendDrawable(m_wireframeRecord->pipeline);
        }
    }

    return true;
}

std::optional<QBox3D> cwRHIPassageTube::worldBounds() const
{
    const QBox3D& bounds = m_geometry.value().bounds;
    if (bounds.isNull()) {
        return std::nullopt;
    }
    return bounds;
}

void cwRHIPassageTube::purgePipelinesFor(QRhiRenderPassDescriptor* descriptor)
{
    cwRHIObject::purgePipelinesFor(descriptor);
    m_wireframeRecord = nullptr;
}

cwRhiPipelineRecord* cwRHIPassageTube::acquirePipeline(const RenderData& data, PipelineKind kind)
{
    if (!data.renderer) {
        return nullptr;
    }

    QRhi* rhi = data.renderer->rhi();
    if (!rhi || !data.renderer->renderTarget()) {
        return nullptr;
    }

    const quint32 globalStride = data.renderer->globalUniformBufferStride();
    const auto key = buildPipelineKey(data.renderPassDescriptor, data.sampleCount, kind);

    auto createFn = [this, key, kind, globalStride](QRhi* localRhi) -> cwRhiPipelineRecord* {
        if (!localRhi) {
            return nullptr;
        }

        auto* record = new cwRhiPipelineRecord;
        record->pipeline = localRhi->newGraphicsPipeline();

        record->pipeline->setShaderStages({
            { QRhiShaderStage::Vertex, loadShader(key.vertexShader) },
            { QRhiShaderStage::Fragment, loadShader(key.fragmentShader) }
        });

        record->pipeline->setDepthTest(true);
        record->pipeline->setDepthWrite(true);
        record->pipeline->setSampleCount(key.sampleCount);
        record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
        record->pipeline->setVertexInputLayout(m_inputLayout);
        record->pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        if (kind == PipelineKind::Wireframe) {
            record->pipeline->setPolygonMode(QRhiGraphicsPipeline::Line);
            record->pipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
            record->pipeline->setDepthBias(kWireframeDepthBias);
            record->pipeline->setSlopeScaledDepthBias(kWireframeSlopeScaledDepthBias);
        }

        QRhiGraphicsPipeline::TargetBlend blendState;
        blendState.enable = false;
        record->pipeline->setTargetBlends({ blendState });

        record->layout = localRhi->newShaderResourceBindings();
        record->layout->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, nullptr, globalStride),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, nullptr)
        });
        record->layout->create();

        record->pipeline->setShaderResourceBindings(record->layout);
        record->pipeline->setRenderPassDescriptor(key.renderPass);
        record->pipeline->create();

        return record;
    };

    return m_pipelines.acquire(m_frame, key, [&]() {
        return m_frame->acquirePipeline(key, rhi, createFn);
    });
}

void cwRHIPassageTube::ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer)
{
    if (m_srb && m_pipelineRecord->layout->isLayoutCompatible(m_srb)) {
        return;
    }

    delete m_srb;
    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, renderer->globalUniformBuffer(), renderer->globalUniformBufferStride()),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_uniformBuffer)
    });
    m_srb->create();
}

cwRhiPipelineKey cwRHIPassageTube::buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                                    int sampleCount,
                                                    PipelineKind kind) const
{
    using Stage = cwRenderMaterialState::ShaderStage;

    cwRhiPipelineKey key;
    key.renderPass = renderPassDescriptor;
    key.sampleCount = sampleCount;
    key.vertexShader = QStringLiteral(":/shaders/PassageTube.vert.qsb");
    // The wireframe variant is PassageTube.frag compiled with WIREFRAME=1; the
    // distinct shader path is also what keeps its line-mode pipeline apart in
    // the cache.
    key.fragmentShader = kind == PipelineKind::Wireframe
                             ? QStringLiteral(":/shaders/PassageTubeWireframe.frag.qsb")
                             : QStringLiteral(":/shaders/PassageTube.frag.qsb");
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::None);
    key.depthTest = 1;
    key.depthWrite = 1;
    key.globalBinding = 0;
    key.perDrawBinding = 1;
    key.textureBinding = 0xFF;
    key.globalStages = cwShaderStageMask(Stage::Vertex);
    key.perDrawStages = cwShaderStageMask(Stage::Fragment);
    key.textureStages = 0;
    key.hasPerDraw = 1;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Triangles);
    return key;
}
