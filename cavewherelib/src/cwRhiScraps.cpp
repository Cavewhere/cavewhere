#include "cwRhiScraps.h"
#include "cwRenderScraps.h"
#include "cwRhiItemRenderer.h"
#include "cwScrap.h"
#include "cwScene.h"

//Our includes
#include <QFile>
#include <QDebug>
#include <QPainter>

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
    delete m_sharedScrapData.m_sampler;
    delete m_sharedScrapData.m_loadingTexture;
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
    m_pipeline->setSampleCount(data.renderData.renderer->renderTarget()->sampleCount());

    // Create sampler
    m_sharedScrapData.m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sharedScrapData.m_sampler->create();

    // Create the loading texture here
    // Create the loading texture
    auto createLoadingTexture = [this, rhi, &data]() {
        // Create an image with text "Loading"
        QImage image(256, 256, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 48));
        painter.drawText(image.rect(), Qt::AlignCenter, "Loading");
        painter.end();

        // Create QRhiTexture
        m_sharedScrapData.m_loadingTexture = rhi->newTexture(QRhiTexture::RGBA8, image.size(), 1, QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
        m_sharedScrapData.m_loadingTexture->create();

        // Upload the texture data
        data.resourceUpdateBatch->uploadTexture(m_sharedScrapData.m_loadingTexture, image);
        data.resourceUpdateBatch->generateMips(m_sharedScrapData.m_loadingTexture);
    };
    createLoadingTexture();


    //Shader binding layout, this will be update for each scrap
    m_globalSrb = rhi->newShaderResourceBindings();
    m_globalSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, nullptr), //Global uniform buffer
        // QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, nullptr), //Scrap uniform buffer
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr) //Texture and sampler
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
    // m_futureManagerToken = renderScraps->m_futureManagerToken;
    m_visible = renderScraps->isVisible();

    // Process pending changes
    auto pendingChanges = renderScraps->m_pendingChanges;
    for (const auto& command : pendingChanges) {
        switch (command.type()) {
        case cwRenderScraps::PendingScrapCommand::AddScrap: {
            cwScrap* scrap = command.scrap();

            RhiScrap* rhiScrap = [this, scrap](){
                if (m_scraps.contains(scrap)) {
                    return m_scraps[scrap];
                } else {
                    auto rhiScrap = new RhiScrap;
                    m_scraps.insert(scrap, rhiScrap);
                    return rhiScrap;
                }
            }();

            // Update the data
            rhiScrap->geometry = command.triangulatedData();
            rhiScrap->geometryNeedsUpdate = true;
            rhiScrap->textureNeedsUpdate = true;

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
        case cwRenderScraps::PendingScrapCommand::UpdateScrapTexture: {
            cwScrap* scrap = command.scrap();
            if(m_scraps.contains(scrap)) {
                RhiScrap* rhiScrap = m_scraps[scrap];
                rhiScrap->geometry = command.triangulatedData();
                rhiScrap->textureNeedsUpdate = true;
            }
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
            scrap->initializeResources(data, m_sharedScrapData);
        }

        if(scrap->geometryNeedsUpdate) {
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
            scrap->geometryNeedsUpdate = false;
        }

        if(scrap->textureNeedsUpdate) {
            const auto imageData = scrap->geometry.croppedImageData();

            auto imageDataSize = [&imageData]() {
                return imageData.image.size();
            };

            auto uploadTextureData = [batch, scrap, &imageData]() {
                Q_ASSERT(imageData.image.size() == scrap->texture->pixelSize());
                batch->uploadTexture(scrap->texture, imageData.image);
                batch->generateMips(scrap->texture);
            };

            auto createTexture = [scrap, &data, &imageData, uploadTextureData, this](const QSize& size){
                if(!size.isEmpty()) {
                    scrap->texture = data.renderData.cb->rhi()->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
                    scrap->texture->create();
                    uploadTextureData();

                    //Update the bindings
                    scrap->createShadeResourceBindings(data, m_sharedScrapData);
                }
            };

            if(scrap->texture == nullptr) {
                createTexture(imageDataSize());
            } else {
                //Texture already exists, update the texture
                auto size = imageDataSize();
                if(scrap->texture->pixelSize() != size) {
                    scrap->texture->deleteLater();
                    scrap->texture = nullptr;
                    createTexture(size);
                } else {
                    //Just upload new data
                    uploadTextureData();
                }
            }
            scrap->textureNeedsUpdate = false;
        }
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

void cwRhiScraps::RhiScrap::initializeResources(const ResourceUpdateData& data, const SharedScrapData& sharedData)
{

    QRhi* rhi = data.renderData.cb->rhi();

    // Create buffers if they don't exist
    vertexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    vertexBuffer->create();

    texCoordBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    texCoordBuffer->create();

    indexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, 0);
    indexBuffer->create();


    createShadeResourceBindings(data, sharedData);

    resourcesInitialized = true;
}


void cwRhiScraps::RhiScrap::createShadeResourceBindings(const ResourceUpdateData& data, const SharedScrapData &sharedData)
{
    QRhi* rhi = data.renderData.cb->rhi();
    if(srb) {
        srb->deleteLater();
    }

    auto currentTexture = texture ? texture : sharedData.m_loadingTexture;

    srb = rhi->newShaderResourceBindings();
    srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, data.renderData.renderer->globalUniformBuffer()),
        // QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, uniformBuffer),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, currentTexture, sharedData.m_sampler)
    });
    srb->create();
}
