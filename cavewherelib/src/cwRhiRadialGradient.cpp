#include "cwRhiRadialGradient.h"
#include "cwRenderRadialGradient.h"
#include "cwRhiItemRenderer.h"

cwRhiRadialGradient::cwRhiRadialGradient()
{
}

cwRhiRadialGradient::~cwRhiRadialGradient()
{
    delete m_pipeline;
    delete m_srb;
    delete m_vertexBuffer;
    delete m_uniformBuffer;
}

void cwRhiRadialGradient::initialize(const ResourceUpdateData& data)
{
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

    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_uniformBuffer)
    });
    m_srb->create();

    m_pipeline = rhi->newGraphicsPipeline();
    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setTargetBlends({ QRhiGraphicsPipeline::TargetBlend() });
    // m_pipeline->setSampleCount(data.renderData.renderer->sampleCount());
    m_pipeline->setDepthTest(false);
    m_pipeline->setDepthWrite(false);
    m_pipeline->setCullMode(QRhiGraphicsPipeline::None);
    m_pipeline->setVertexInputLayout(m_inputLayout);
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

    // Load shaders
    QShader vs = loadShader(":/shaders/radial_gradient.vert.qsb");
    QShader fs = loadShader(":/shaders/radial_gradient.frag.qsb");

    m_pipeline->setShaderStages( {{ QRhiShaderStage::Vertex, vs },
                                 { QRhiShaderStage::Fragment, fs }});
    m_pipeline->setRenderPassDescriptor(data.renderData.renderer->renderTarget()->renderPassDescriptor());
    m_pipeline->create();
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
    data.resourceUpdateBatch->updateDynamicBuffer(m_uniformBuffer, 0, sizeof(UniformData), &m_uniformData);
}

void cwRhiRadialGradient::render(const RenderData& data)
{
    data.cb->setGraphicsPipeline(m_pipeline);
    data.cb->setShaderResources(m_srb);
    const QRhiCommandBuffer::VertexInput vertexBindings[] = {
        { m_vertexBuffer, 0 }
    };
    data.cb->setVertexInput(0, 1, vertexBindings);
    data.cb->draw(4);
}
