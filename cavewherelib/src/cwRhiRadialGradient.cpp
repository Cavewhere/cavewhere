#include "cwRhiRadialGradient.h"
#include "cwRenderRadialGradient.h"
#include "cwRhiItemRenderer.h"
#include "cwRenderMaterialState.h"

cwRhiRadialGradient::cwRhiRadialGradient()
{
}

cwRhiRadialGradient::~cwRhiRadialGradient()
{
    delete m_vertexBuffer;
    delete m_uniformBuffer;
    delete m_srb;

    releasePipeline();
}

void cwRhiRadialGradient::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized) {
        return;
    }

    if (!m_scene && data.renderData.renderer) {
        m_scene = data.renderData.renderer->sceneBackend();
    }

    QRhi* rhi = data.renderData.renderer->rhi();

    float vertices[] = {
        -1.0f, -1.0f, // Bottom-left
        1.0f, -1.0f, // Bottom-right
        -1.0f,  1.0f, // Top-left
        1.0f,  1.0f  // Top-right
    };

    m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices));
    m_vertexBuffer->create();

    data.resourceUpdateBatch->uploadStaticBuffer(m_vertexBuffer, vertices);

    auto size = rhi->ubufAligned(sizeof(UniformData));
    m_uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
    m_uniformBuffer->create();

    m_inputLayout.setBindings({
        {2 * sizeof(float)}
    });
    m_inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0}
    });

    m_resourcesInitialized = true;
}

void cwRhiRadialGradient::synchronize(const SynchronizeData& data)
{
    cwRenderRadialGradient* obj = static_cast<cwRenderRadialGradient*>(data.object);
    m_uniformData.center = obj->center();
    m_uniformData.radius = obj->radius();
    m_uniformData.radiusOffset = obj->radiusOffset();

    auto toVector4D = [](const QColor& color) {
        return QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    };

    m_uniformData.color1 = toVector4D(obj->color1());
    m_uniformData.color2 = toVector4D(obj->color2());
}

void cwRhiRadialGradient::updateResources(const ResourceUpdateData& data)
{
    if (!m_scene && data.renderData.renderer) {
        m_scene = data.renderData.renderer->sceneBackend();
    }

    data.resourceUpdateBatch->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(UniformData), &m_uniformData);
}

void cwRhiRadialGradient::render(const RenderData& data)
{
    // if (!ensurePipeline(data)) {
    //     return;
    // }

    // if (!m_pipelineRecord || !m_pipelineRecord->pipeline) {
    //     return;
    // }

    // data.cb->setGraphicsPipeline(m_pipelineRecord->pipeline);
    // data.cb->setShaderResources(m_srb);
    // const QRhiCommandBuffer::VertexInput vertexBindings[] = {
    //     { m_vertexBuffer, 0 }
    // };
    // data.cb->setVertexInput(0, 1, vertexBindings);
    // data.cb->draw(4);
}

bool cwRhiRadialGradient::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (!isVisible()) {
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

    batch.drawables.append(drawable);
    return true;
}

bool cwRhiRadialGradient::ensurePipeline(const RenderData& data)
{
    if (!m_resourcesInitialized || !data.renderer) {
        return false;
    }

    if (!m_scene) {
        m_scene = data.renderer->sceneBackend();
    }
    if (!m_scene) {
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

            QShader vs = loadShader(kVertexShaderPath);
            QShader fs = loadShader(kFragmentShaderPath);
            record->pipeline->setShaderStages({
                { QRhiShaderStage::Vertex, vs },
                { QRhiShaderStage::Fragment, fs }
            });
            record->pipeline->setDepthTest(false);
            record->pipeline->setDepthWrite(false);
            record->pipeline->setSampleCount(key.sampleCount);
            record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
            record->pipeline->setVertexInputLayout(m_inputLayout);
            record->pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
            record->pipeline->setTargetBlends({ QRhiGraphicsPipeline::TargetBlend() });

            record->layout = localRhi->newShaderResourceBindings();
            record->layout->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, nullptr)
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

bool cwRhiRadialGradient::ensureShaderResources(QRhi* rhi)
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

    if (!m_uniformBuffer || !rhi) {
        return false;
    }

    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_uniformBuffer)
    });
    m_srb->create();

    if (m_pipelineRecord && m_pipelineRecord->layout) {
        Q_ASSERT(m_pipelineRecord->layout->isLayoutCompatible(m_srb));
    }

    return true;
}

void cwRhiRadialGradient::releasePipeline()
{
    if (m_pipelineRecord && m_scene) {
        m_scene->releasePipeline(m_pipelineRecord);
    }
    m_pipelineRecord = nullptr;
    m_hasPipelineKey = false;
}

cwRhiPipelineKey cwRhiRadialGradient::buildPipelineKey(QRhiRenderTarget* target) const
{
    cwRhiPipelineKey key;
    key.renderPass = target ? target->renderPassDescriptor() : nullptr;
    key.sampleCount = target ? target->sampleCount() : 1;
    key.vertexShader = QString::fromUtf8(kVertexShaderPath);
    key.fragmentShader = QString::fromUtf8(kFragmentShaderPath);
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::None);
    key.depthTest = 0;
    key.depthWrite = 0;
    key.globalBinding = 0;
    key.perDrawBinding = 0xFF;
    key.textureBinding = 0xFF;
    key.globalStages = 0x2; // Fragment stage
    key.perDrawStages = 0;
    key.textureStages = 0;
    key.hasPerDraw = 0;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::TriangleStrip);
    return key;
}
