#include "cwRHIGridPlane.h"
#include "cwRenderGridPlane.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwRenderMaterialState.h"

cwRHIGridPlane::cwRHIGridPlane()
{
}

cwRHIGridPlane::~cwRHIGridPlane()
{
    delete m_vertexBuffer;
    delete m_uniformBuffer;
    delete m_fragmentUniformBuffer;
    delete m_srb;
    // m_pipelines releases its held pipeline references on destruction.
}

void cwRHIGridPlane::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized) {
        return;
    }

    initializeResources(data);
    m_resourcesInitialized = true;
}

void cwRHIGridPlane::initializeResources(const ResourceUpdateData& data)
{
    auto rhi = data.renderData.cb->rhi();

    // Create vertex buffer
    float vertices[] = {
        -1.0f, -1.0f, 0.0f, // Bottom-left
        -1.0f,  1.0f, 0.0f, // Top-left
        1.0f, -1.0f, 0.0f, // Bottom-right
        1.0f,  1.0f, 0.0f  // Top-right
    };

    m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices));
    m_vertexBuffer->create();

    QRhiResourceUpdateBatch* initialUpdates = data.resourceUpdateBatch;
    initialUpdates->uploadStaticBuffer(m_vertexBuffer, vertices);

    // Create uniform buffer
   const auto uniformBufferSize = rhi->ubufAligned(sizeof(UniformData));
    m_uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, uniformBufferSize);
    m_uniformBuffer->create();

    const auto fragmentUniformBufferSize = rhi->ubufAligned(sizeof(FragmentUniformData));
    m_fragmentUniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, fragmentUniformBufferSize);
    m_fragmentUniformBuffer->create();

    // Input layout
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 3 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 }
    });
    m_inputLayout = inputLayout;
}

void cwRHIGridPlane::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderGridPlane*>(data.object) != nullptr);
    cwRenderGridPlane* gridPlane = static_cast<cwRenderGridPlane*>(data.object);

    m_modelMatrix = gridPlane->m_modelMatrix;
    m_scaleMatrix = gridPlane->m_scaleMatrix;
    m_color.setValue(gridPlane->color());
    gridPlane->m_modelMatrix.resetChanged();
    gridPlane->m_scaleMatrix.resetChanged();
}

void cwRHIGridPlane::updateResources(const ResourceUpdateData & data)
{
    // The grid now reads view/projection from the shared global UBO, so its own
    // block holds only placement + scale and updates solely on those changes — no
    // longer on every camera move.
    if(m_modelMatrix.isChanged() || m_scaleMatrix.isChanged())
    {
        data.resourceUpdateBatch->updateDynamicBuffer(
            m_uniformBuffer,
            offsetof(UniformData, modelMatrix),
            sizeof(UniformData::modelMatrix),
            m_modelMatrix.value().constData()
            );

        // The scale-only matrix used for contour sampling in the fragment shader.
        data.resourceUpdateBatch->updateDynamicBuffer(
            m_uniformBuffer,
            offsetof(UniformData, scaleMatrix),
            sizeof(UniformData::scaleMatrix),
            m_scaleMatrix.value().constData()
            );

        m_modelMatrix.resetChanged();
        m_scaleMatrix.resetChanged();
    }

    if (m_color.isChanged()) {
        const QColor& c = m_color.value();
        FragmentUniformData fragmentData;
        fragmentData.lineColor[0] = static_cast<float>(c.redF());
        fragmentData.lineColor[1] = static_cast<float>(c.greenF());
        fragmentData.lineColor[2] = static_cast<float>(c.blueF());
        fragmentData.lineColor[3] = static_cast<float>(c.alphaF());
        data.resourceUpdateBatch->updateDynamicBuffer(
            m_fragmentUniformBuffer,
            0,
            sizeof(FragmentUniformData),
            &fragmentData
            );
        m_color.resetChanged();
    }
}

bool cwRHIGridPlane::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    // The grid is a semi-transparent reference overlay: it must draw *after* the
    // point cloud (and the EDL composite), depth-testing against the combined
    // scene+cloud depth but never writing its own full-plane depth — otherwise
    // its invisible inter-line areas occlude everything behind them.
    if (context.renderPass != RenderPass::Transparent) {
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
    drawable.vertexCount = 4;
    drawable.bindings = m_srb;
    drawable.globalCameraBinding = 0; // slot 0 binds the global camera UBO (dynamic offset)

    batch.drawables.append(drawable);
    return true;
}

bool cwRHIGridPlane::ensurePipeline(const RenderData& data)
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

        QShader vs = loadShader(":/shaders/grid.vert.qsb");
        QShader fs = loadShader(":/shaders/grid.frag.qsb");

        record->pipeline->setShaderStages({
            { QRhiShaderStage::Vertex, vs },
            { QRhiShaderStage::Fragment, fs }
        });

        record->pipeline->setDepthTest(true);
        record->pipeline->setDepthWrite(false);
        record->pipeline->setSampleCount(key.sampleCount);
        record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
        record->pipeline->setVertexInputLayout(m_inputLayout);
        record->pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

        QRhiGraphicsPipeline::TargetBlend blendState;
        blendState.enable = true;
        blendState.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blendState.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blendState.srcAlpha = QRhiGraphicsPipeline::One;
        blendState.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        record->pipeline->setTargetBlends({ blendState });

        record->layout = localRhi->newShaderResourceBindings();
        record->layout->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, nullptr, globalStride),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, nullptr),
            QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, nullptr)
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

bool cwRHIGridPlane::ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer)
{
    if (m_srb) {
        if (m_pipelineRecord && m_pipelineRecord->layout &&
            !m_pipelineRecord->layout->isLayoutCompatible(m_srb)) {
            delete m_srb;
            m_srb = nullptr;
        } else {
            return true;
        }
    }

    auto* globalUniformBuffer = renderer ? renderer->globalUniformBuffer() : nullptr;
    if (!rhi || !globalUniformBuffer || !m_uniformBuffer || !m_fragmentUniformBuffer) {
        return false;
    }

    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, globalUniformBuffer, renderer->globalUniformBufferStride()),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, m_uniformBuffer),
        QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, m_fragmentUniformBuffer)
    });
    m_srb->create();

    if (m_pipelineRecord && m_pipelineRecord->layout) {
        Q_ASSERT(m_pipelineRecord->layout->isLayoutCompatible(m_srb));
    }

    return true;
}

cwRhiPipelineKey cwRHIGridPlane::buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                                  int sampleCount) const
{
    cwRhiPipelineKey key;
    key.renderPass = renderPassDescriptor;
    key.sampleCount = sampleCount;
    key.vertexShader = QStringLiteral(":/shaders/grid.vert.qsb");
    key.fragmentShader = QStringLiteral(":/shaders/grid.frag.qsb");
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::Alpha);
    key.depthTest = 1;
    key.depthWrite = 0;
    key.globalBinding = 0;
    key.perDrawBinding = 0xFF;
    key.textureBinding = 0xFF;
    using Stage = cwRenderMaterialState::ShaderStage;
    key.globalStages = cwShaderStageMask(Stage::Vertex | Stage::Fragment);
    key.perDrawStages = 0;
    key.textureStages = 0;
    key.hasPerDraw = 0;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::TriangleStrip);
    return key;
}
