#include "cwRHILinePlot.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwRenderLinePlot.h"
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
    delete m_pipeline;
    delete m_srb;
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

    // Create buffers
    m_vertexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    m_vertexBuffer->create();

    m_indexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, 0);
    m_indexBuffer->create();

    // auto uniformBufferSize = rhi->ubufAligned(sizeof(UniformData));
    // m_uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, uniformBufferSize);
    // m_uniformBuffer->create();

    // Load shaders
    QShader vs = loadShader(":/shaders/LinePlot.vert.qsb");
    QShader fs = loadShader(":/shaders/LinePlot.frag.qsb");

    // Create shader resource bindings
    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, data.renderData.renderer->globalUniformBuffer()),
        // QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, m_uniformBuffer)
    });
    m_srb->create();

    // Create graphics pipeline
    m_pipeline = rhi->newGraphicsPipeline();

    QRhiGraphicsPipeline::TargetBlend blendState;
    blendState.enable = false;

    m_pipeline->setTargetBlends({ blendState });
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs }
    });

    m_pipeline->setDepthTest(true);
    m_pipeline->setDepthWrite(true);
    m_pipeline->setSampleCount(data.renderData.renderer->renderTarget()->sampleCount());

    // Input layout
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(QVector3D) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 }
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setTopology(QRhiGraphicsPipeline::Lines);

    m_pipeline->setShaderResourceBindings(m_srb);
    m_pipeline->setRenderPassDescriptor(data.renderData.renderer->renderTarget()->renderPassDescriptor());
    m_pipeline->create();
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

    data.cb->setGraphicsPipeline(m_pipeline);
    data.cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer, 0);
    data.cb->setVertexInput(0, 1, &vertexInput, m_indexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
    data.cb->drawIndexed(m_data.value().indexes.size());
}

