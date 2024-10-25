#include "cwRhiScraps.h"
#include "cwRenderScraps.h"
#include "cwRhiItemRenderer.h"
#include "cwScrap.h"
#include "cwScene.h"
#include <QFile>
#include <QDebug>

cwRhiScraps::cwRhiScraps()
{
}

cwRhiScraps::~cwRhiScraps()
{
    // Release all resources
    for (auto scrap : m_scraps) {
        delete scrap;
    }
    delete m_pipeline;
    delete m_sampler;
    delete m_globalSrb;
    // delete m_globalSrb;
}

void cwRhiScraps::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized)
        return;

    initializePipeline(data);
    m_resourcesInitialized = true;
}

void cwRhiScraps::initializePipeline(const ResourceUpdateData& data)
{
    QRhi* rhi = data.renderData.cb->rhi();

    // Load shaders
    QShader vs = loadShader(":/shaders/scrap.vert.qsb");
    QShader fs = loadShader(":/shaders/scrap.frag.qsb");

    // Create graphics pipeline
    m_pipeline = rhi->newGraphicsPipeline();
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs }
    });
    m_pipeline->setDepthTest(true);
    m_pipeline->setDepthWrite(true);

    // Create sampler
    m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

    //Shader binding layout, this will be update for each scrap
    m_globalSrb = rhi->newShaderResourceBindings();
    m_globalSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, nullptr), //Global uniform buffer
        // QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, nullptr), //Scrap uniform buffer
        // QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr) //Texture and sampler
    });
    m_globalSrb->create();

    // Input layout
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { sizeof(QVector3D) }, // Vertex positions
        { sizeof(QVector2D) }  // Texture coordinates
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 1, 1, QRhiVertexInputAttribute::Float2, 0 }
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);

    m_pipeline->setRenderPassDescriptor(data.renderData.renderer->renderTarget()->renderPassDescriptor());
    m_pipeline->setShaderResourceBindings(m_globalSrb);
    m_pipeline->create();
}

void cwRhiScraps::synchronize(const SynchronizeData& data)
{
    // Get the cwRenderScraps object
    Q_ASSERT(dynamic_cast<cwRenderScraps*>(data.object) != nullptr);
    cwRenderScraps* renderScraps = static_cast<cwRenderScraps*>(data.object);

    m_renderScraps = renderScraps;
    m_project = renderScraps->project();
    m_futureManagerToken = renderScraps->m_futureManagerToken;
    m_visible = renderScraps->visible();

    // Process pending changes
    auto pendingChanges = renderScraps->m_pendingChanges;
    for (const auto& command : pendingChanges) {
        switch (command.type()) {
        case cwRenderScraps::PendingScrapCommand::AddScrap: {
            cwScrap* scrap = command.scrap();
            cwTriangulatedData geometry = command.triangulatedData();

            // int scrapId = -1;
            if (m_scraps.contains(scrap)) {
                RhiScrap* rhiScrap = m_scraps[scrap];
                rhiScrap->geometry = geometry;
                rhiScrap->resourcesInitialized = false; // Mark for resource update
            } else {
                RhiScrap* rhiScrap = new RhiScrap;
                rhiScrap->geometry = geometry;
                rhiScrap->sampler = m_sampler;
                m_scraps.insert(scrap, rhiScrap);
            }
            // Update geometry intersector if needed

            break;
        }
        case cwRenderScraps::PendingScrapCommand::RemoveScrap: {
            cwScrap* scrap = command.scrap();
            if (m_scraps.contains(scrap)) {
                RhiScrap* rhiScrap = m_scraps.take(scrap);
                // Remove from geometry intersector if needed
                delete rhiScrap;
            }
            break;
        }
        default:
            break;
        }
    }

    renderScraps->m_pendingChanges.clear();
}

void cwRhiScraps::updateResources(const ResourceUpdateData& data)
{
    auto batch = data.resourceUpdateBatch;

    for (auto it = m_scraps.begin(); it != m_scraps.end(); ++it) {
        RhiScrap* scrap = it.value();
        if (!scrap->resourcesInitialized) {
            scrap->initializeResources(data);
            scrap->resourcesInitialized = true;
        }

        // Update buffers
        const cwTriangulatedData& td = scrap->geometry;

        int vertexBufferSize = td.points().size() * sizeof(QVector3D);
        int texCoordBufferSize = td.texCoords().size() * sizeof(QVector2D);
        int indexBufferSize = td.indices().size() * sizeof(uint);

        if (scrap->vertexBuffer->size() != vertexBufferSize) {
            scrap->vertexBuffer->setSize(vertexBufferSize);
            scrap->vertexBuffer->create();
        }
        batch->updateDynamicBuffer(scrap->vertexBuffer, 0, vertexBufferSize, td.points().constData());

        if (scrap->texCoordBuffer->size() != texCoordBufferSize) {
            scrap->texCoordBuffer->setSize(texCoordBufferSize);
            scrap->texCoordBuffer->create();
        }
        batch->updateDynamicBuffer(scrap->texCoordBuffer, 0, texCoordBufferSize, td.texCoords().constData());

        if (scrap->indexBuffer->size() != indexBufferSize) {
            scrap->indexBuffer->setSize(indexBufferSize);
            scrap->indexBuffer->create();
        }
        batch->updateDynamicBuffer(scrap->indexBuffer, 0, indexBufferSize, td.indices().constData());

        scrap->numberOfIndices = td.indices().size();
    }
}

void cwRhiScraps::render(const RenderData& data)
{
    if (m_scraps.isEmpty() || !m_visible)
        return;

    data.cb->setGraphicsPipeline(m_pipeline);

    for (auto scrap : std::as_const(m_scraps)) {
        // Set up per-scrap resources (textures, etc.)
        Q_ASSERT(m_globalSrb->isLayoutCompatible(scrap->srb)); //if this fails check layout compatibility
        data.cb->setShaderResources(scrap->srb);

        const QRhiCommandBuffer::VertexInput vertexInputs[] = {
            { scrap->vertexBuffer, 0 },
            { scrap->texCoordBuffer, 0 }
        };
        data.cb->setVertexInput(0, 2, vertexInputs, scrap->indexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
        data.cb->drawIndexed(scrap->numberOfIndices);
    }
}

cwRhiScraps::RhiScrap::RhiScrap()
{
}

cwRhiScraps::RhiScrap::~RhiScrap()
{
    delete vertexBuffer;
    delete texCoordBuffer;
    delete indexBuffer;
    delete texture;
    delete srb;

    //Sampler isn't owned by RhiScrap
    //delete sampler;
}

void cwRhiScraps::RhiScrap::initializeResources(const ResourceUpdateData& data)
{

    QRhi* rhi = data.renderData.cb->rhi();

    // Create buffers if they don't exist
    // if (!vertexBuffer) {
        vertexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
        vertexBuffer->create();
    // }

    // if (!texCoordBuffer) {
        texCoordBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
        texCoordBuffer->create();
    // }

    // if (!indexBuffer) {
        indexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, 0);
        indexBuffer->create();
    // }

    // if(!uniformBuffer) {
        // auto uniformBufferSize = rhi->ubufAligned(sizeof(ScrapUniform));
        // uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, uniformBufferSize);
        // uniformBuffer->create();
    // }

    // // Create texture
    // if (!texture) {
    //     cwImage image = data.croppedImage();
    //     if (!image.isNull()) {
    //         texture = rhi->newTexture(QRhiTexture::RGBA8, image.size(), 1, QRhiTexture::UsedWithGenerateMips);
    //         texture->create();

    //         // Update the texture with image data
    //         batch->uploadTexture(texture, image);
    //     }
    // }



    // Create srb
    if (!srb) {
        srb = rhi->newShaderResourceBindings();
        srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, data.renderData.renderer->globalUniformBuffer()),
            // QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, uniformBuffer),
            // QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, texture, sampler)
        });
        srb->create();
    }
}

void cwRhiScraps::RhiScrap::releaseResources()
{

}
