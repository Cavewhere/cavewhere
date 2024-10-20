#include "cwRHIGridPlane.h"
#include "cwGLGridPlane.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwCamera.h"
#include <QFile>

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
    m_uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QMatrix4x4) * 2);
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

    updateResources(data);

    // Schedule initial resource updates
    // data.cb->resourceUpdate(initialUpdates);
}

void cwRHIGridPlane::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwGLGridPlane*>(data.object) != nullptr);
    cwGLGridPlane* gridPlane = static_cast<cwGLGridPlane*>(data.object);

    m_modelMatrix = gridPlane->modelMatrix();

    cwCamera* camera = gridPlane->scene()->camera();
    m_mvpMatrix = camera->viewProjectionMatrix() * m_modelMatrix;

    //Should schedule update to the resources
}

void cwRHIGridPlane::updateResources(const ResourceUpdateData & data)
{
    //Shader layout
    struct UniformData {
        float mvpMatrix[16];
        float modelMatrix[16];
    };

    //Fixes clipping issues with directx vs opengl
    auto mvp = data.renderData.cb->rhi()->clipSpaceCorrMatrix() * m_mvpMatrix;

    // Update the buffer with m_mvpMatrix
    data.resourceUpdateBatch->updateDynamicBuffer(
        m_uniformBuffer,
        offsetof(UniformData, mvpMatrix),
        sizeof(UniformData::mvpMatrix),
        mvp.constData()
        );

    // Update the buffer with m_modelMatrix
    data.resourceUpdateBatch->updateDynamicBuffer(
        m_uniformBuffer,
        offsetof(UniformData, modelMatrix),
        sizeof(UniformData::modelMatrix),
        m_modelMatrix.constData()
        );
}


void cwRHIGridPlane::updateUniforms(QRhiResourceUpdateBatch* resourceUpdates)
{

}

void cwRHIGridPlane::render(const RenderData& data)
{
    // if (!m_resourcesInitialized) {
    //     initialize(cb);
    // }

    // auto rhi = data.cb->rhi();

    // QRhiResourceUpdateBatch* resourceUpdates = m_rhi->nextResourceUpdateBatch();
    // updateUniforms(resourceUpdates);

    // qDebug() << "Draw!";

    // data.cb->resourceUpdate(resourceUpdates);
    data.cb->setGraphicsPipeline(m_pipeline);
    data.cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer, 0);
    data.cb->setVertexInput(0, 1, &vertexInput);
    data.cb->draw(4);
}

QByteArray loadShader(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to load shader:" << filename;
        return QByteArray();
    }
    return file.readAll();
}
