#include "cwRHIGridPlane.h"
#include "cwRenderGridPlane.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"

cwRHIGridPlane::cwRHIGridPlane()
{
}

cwRHIGridPlane::~cwRHIGridPlane()
{
    delete m_vertexBuffer;
    delete m_uniformBuffer;
    delete m_pipeline;
    delete m_srb;
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

    // Load shaders
    QShader vs = loadShader(":/shaders/grid.vert.qsb");
    QShader fs = loadShader(":/shaders/grid.frag.qsb");


    // Create shader resource bindings
    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_uniformBuffer)
    });
    m_srb->create();

    // Create graphics pipeline
    m_pipeline = rhi->newGraphicsPipeline();

    QRhiGraphicsPipeline::TargetBlend blendState;
    blendState.enable = true;

    m_pipeline->setTargetBlends({ blendState });
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs }
    });

    m_pipeline->setDepthTest(true);
    m_pipeline->setDepthWrite(true);

    // Input layout
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 3 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 }
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setRenderPassDescriptor(data.renderData.renderer->renderTarget()->renderPassDescriptor()); // You need to provide the render pass descriptor
    m_pipeline->create();
}

void cwRHIGridPlane::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderGridPlane*>(data.object) != nullptr);
    cwRenderGridPlane* gridPlane = static_cast<cwRenderGridPlane*>(data.object);

    m_modelMatrix = gridPlane->m_modelMatrix;
    gridPlane->m_modelMatrix.resetChanged();
}

void cwRHIGridPlane::updateResources(const ResourceUpdateData & data)
{
    if(cwSceneUpdate::isFlagSet(data.renderData.updateFlag, cwSceneUpdate::Flag::ViewMatrix)
        || cwSceneUpdate::isFlagSet(data.renderData.updateFlag, cwSceneUpdate::Flag::ProjectionMatrix)
        || m_modelMatrix.isChanged()
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

        if(m_modelMatrix.isChanged()) {
            // Update the buffer with m_modelMatrix
            data.resourceUpdateBatch->updateDynamicBuffer(
                m_uniformBuffer,
                offsetof(UniformData, modelMatrix),
                sizeof(UniformData::modelMatrix),
                m_modelMatrix.value().constData()
                );
            m_modelMatrix.resetChanged();
        }
    }
}

void cwRHIGridPlane::render(const RenderData& data)
{
    data.cb->setGraphicsPipeline(m_pipeline);
    data.cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer, 0);
    data.cb->setVertexInput(0, 1, &vertexInput);
    data.cb->draw(4);
}
