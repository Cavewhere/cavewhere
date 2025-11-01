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
    delete m_srb;

    releasePipeline();
}

void cwRHIGridPlane::initialize(const ResourceUpdateData& data)
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
    gridPlane->m_modelMatrix.resetChanged();
    gridPlane->m_scaleMatrix.resetChanged();
}

void cwRHIGridPlane::updateResources(const ResourceUpdateData & data)
{
    if(cwSceneUpdate::isFlagSet(data.renderData.updateFlag, cwSceneUpdate::Flag::ViewMatrix)
        || cwSceneUpdate::isFlagSet(data.renderData.updateFlag, cwSceneUpdate::Flag::ProjectionMatrix)
        || m_modelMatrix.isChanged()
        || m_scaleMatrix.isChanged()
        )
    {
        auto mvp = data.renderData.renderer->viewProjectionMatrix() * m_modelMatrix.value();

        // Update the buffer with m_mvpMatrix
        data.resourceUpdateBatch->updateDynamicBuffer(
            m_uniformBuffer,
            offsetof(UniformData, mvpMatrix),
            sizeof(UniformData::mvpMatrix),
            mvp.constData()
            );

        if(m_modelMatrix.isChanged() || m_scaleMatrix.isChanged()) {
            // Update the buffer with the scale-only matrix for grid shading
            data.resourceUpdateBatch->updateDynamicBuffer(
                m_uniformBuffer,
                offsetof(UniformData, modelMatrix),
                sizeof(UniformData::modelMatrix),
                m_scaleMatrix.value().constData()
                );
            if (m_modelMatrix.isChanged()) {
                m_modelMatrix.resetChanged();
            }
            if (m_scaleMatrix.isChanged()) {
                m_scaleMatrix.resetChanged();
            }
        }
    }
}

void cwRHIGridPlane::render(const RenderData& data)
{
    // if (!ensurePipeline(data)) {
    //     return;
    // }

    // if (!m_pipelineRecord || !m_pipelineRecord->pipeline) {
    //     return;
    // }

    // data.cb->setGraphicsPipeline(m_pipelineRecord->pipeline);
    // data.cb->setShaderResources(m_srb);
    // const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer, 0);
    // data.cb->setVertexInput(0, 1, &vertexInput);
    // data.cb->draw(4);
}

bool cwRHIGridPlane::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (!isVisible()) {
        return false;
    }

    if (context.renderPass != RenderPass::Opaque) {
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
    const quint64 pipelineKey = quint64(quintptr(pipeline));
    state.sortKey = (quint64(context.objectOrder) << 32) | (pipelineKey & 0xffffffffu);

    auto& batch = acquirePipelineBatch(batches, state);
    cwRHIObject::Drawable drawable;
    drawable.type = cwRHIObject::Drawable::Type::NonIndexed;
    drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_vertexBuffer, 0));
    drawable.vertexCount = 4;
    drawable.bindings = m_srb;

    batch.drawables.append(drawable);
    return true;
}

void cwRHIGridPlane::releasePipeline()
{
    if (m_pipelineRecord && m_scene) {
        m_scene->releasePipeline(m_pipelineRecord);
    }
    m_pipelineRecord = nullptr;
    m_hasPipelineKey = false;
}

bool cwRHIGridPlane::ensurePipeline(const RenderData& data)
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

            QShader vs = loadShader(":/shaders/grid.vert.qsb");
            QShader fs = loadShader(":/shaders/grid.frag.qsb");

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
            blendState.enable = true;
            blendState.srcColor = QRhiGraphicsPipeline::SrcAlpha;
            blendState.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            blendState.srcAlpha = QRhiGraphicsPipeline::One;
            blendState.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
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

    if (!ensureShaderResources(rhi)) {
        return false;
    }

    return true;
}

bool cwRHIGridPlane::ensureShaderResources(QRhi* rhi)
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

    if (!rhi || !m_uniformBuffer) {
        return false;
    }

    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_uniformBuffer)
    });
    m_srb->create();

    if (m_pipelineRecord && m_pipelineRecord->layout) {
        Q_ASSERT(m_pipelineRecord->layout->isLayoutCompatible(m_srb));
    }

    return true;
}

cwRhiPipelineKey cwRHIGridPlane::buildPipelineKey(QRhiRenderTarget* target) const
{
    cwRhiPipelineKey key;
    key.renderPass = target ? target->renderPassDescriptor() : nullptr;
    key.sampleCount = target ? target->sampleCount() : 1;
    key.vertexShader = QStringLiteral(":/shaders/grid.vert.qsb");
    key.fragmentShader = QStringLiteral(":/shaders/grid.frag.qsb");
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::Alpha);
    key.depthTest = 1;
    key.depthWrite = 1;
    key.globalBinding = 0;
    key.perDrawBinding = 0xFF;
    key.textureBinding = 0xFF;
    key.globalStages = 0x1; // vertex stage
    key.perDrawStages = 0;
    key.textureStages = 0;
    key.hasPerDraw = 0;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::TriangleStrip);
    return key;
}
