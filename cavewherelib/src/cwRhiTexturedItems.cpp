#include "cwRhiTexturedItems.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiItemRenderer.h"
#include "cwScrap.h"
#include "cwScene.h"

//Our includes
#include <QFile>
#include <QDebug>
#include <QPainter>

cwRhiTexturedItems::cwRhiTexturedItems()
{
}

cwRhiTexturedItems::~cwRhiTexturedItems()
{
    // Release all resources
    for (auto scrap : m_items) {
        delete scrap;
    }
    delete m_pipeline;
    delete m_sharedData.m_sampler;
    delete m_sharedData.m_loadingTexture;
    delete m_globalSrb;
    // delete m_globalSrb;
}

void cwRhiTexturedItems::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized)
        return;

    initializePipeline(data);
    m_resourcesInitialized = true;
}

void cwRhiTexturedItems::initializePipeline(const ResourceUpdateData& data)
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
    m_pipeline->setCullMode(QRhiGraphicsPipeline::Back);

    // Create sampler
    m_sharedData.m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                                                  QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sharedData.m_sampler->create();

    // Create the loading texture here
    // Create the loading texture
    {
        // Create an image with text "Loading"
        QImage image(256, 256, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 48));
        painter.drawText(image.rect(), Qt::AlignCenter, "Loading");
        painter.end();

        // Create QRhiTexture
        // m_sharedScrapData.m_loadingTexture = rhi->newTexture(QRhiTexture::RGBA8, image.size(), 1, QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
        m_sharedData.m_loadingTexture = rhi->newTexture(QRhiTexture::RGBA8, image.size(), 1, QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
        m_sharedData.m_loadingTexture->create();

        // Upload the texture data
        data.resourceUpdateBatch->uploadTexture(m_sharedData.m_loadingTexture, image);
        data.resourceUpdateBatch->generateMips(m_sharedData.m_loadingTexture);
    }


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
        sizeof(QVector3D) + sizeof(QVector2D), // Vertex positions and Texture coordinates
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, sizeof(QVector3D) /*offset*/ }
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);

    m_pipeline->setRenderPassDescriptor(data.renderData.renderer->renderTarget()->renderPassDescriptor());
    m_pipeline->setShaderResourceBindings(m_globalSrb);
    m_pipeline->create();
}

void cwRhiTexturedItems::synchronize(const SynchronizeData& data)
{
    // Get the cwRenderScraps object
    Q_ASSERT(dynamic_cast<cwRenderTexturedItems*>(data.object) != nullptr);
    cwRenderTexturedItems* renderItems = static_cast<cwRenderTexturedItems*>(data.object);

    m_renderScraps = renderItems;

    // m_futureManagerToken = renderScraps->m_futureManagerToken;
    m_visible = renderItems->isVisible();

    // Process pending changes
    auto pendingChanges = renderItems->m_pendingChanges;
    for (const auto& command : pendingChanges) {
        const auto id = command.id();

        switch (command.type()) {
        case cwRenderTexturedItems::PendingCommand::Add: {
            Item* item = [this, id](){
                if (m_items.contains(id)) {
                    return m_items[id];
                } else {
                    auto item = new Item;
                    m_items.insert(id, item);
                    return item;
                }
            }();

            // Update the data
            item->geometry = command.item().geometry;
            item->image = command.item().texture;
            item->geometryNeedsUpdate = true;
            item->textureNeedsUpdate = true;

            break;
        }
        case cwRenderTexturedItems::PendingCommand::Remove: {
            if (m_items.contains(id)) {
                Item* item = m_items.take(id);
                // Remove from geometry intersector if needed
                delete item;
            }
            break;
        }
        case cwRenderTexturedItems::PendingCommand::UpdateGeometry: {
            if(m_items.contains(id)) {
                Item* item = m_items[id];
                item->geometry = command.item().geometry;
                item->geometryNeedsUpdate = true;
            }
            break;
        }
        case cwRenderTexturedItems::PendingCommand::UpdateTexture: {
            if(m_items.contains(id)) {
                Item* item = m_items[id];
                item->image = command.item().texture;
                item->textureNeedsUpdate = true;
            }
            break;
        }
        default:
            break;
        }
    }

    renderItems->m_pendingChanges.clear();
}

void cwRhiTexturedItems::updateResources(const ResourceUpdateData& data)
{
    auto batch = data.resourceUpdateBatch;

    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        Item* item = it.value();

        if (!item->resourcesInitialized) {
            item->initializeResources(data, m_sharedData);
        }

        if(item->geometryNeedsUpdate) {
            // Update buffers

            int vertexBufferSize = item->geometry.vertexData().size();
            int indexBufferSize = item->geometry.indices().size() * sizeof(uint32_t);

            if (item->vertexBuffer->size() != vertexBufferSize) {
                item->vertexBuffer->setSize(vertexBufferSize);
                item->vertexBuffer->create();
            }
            batch->updateDynamicBuffer(item->vertexBuffer, 0, vertexBufferSize, item->geometry.vertexData());

            // if (scrap->texCoordBuffer->size() != texCoordBufferSize) {
            //     scrap->texCoordBuffer->setSize(texCoordBufferSize);
            //     scrap->texCoordBuffer->create();
            // }
            // batch->updateDynamicBuffer(scrap->texCoordBuffer, 0, texCoordBufferSize, td.texCoords().constData());

            if (item->indexBuffer->size() != indexBufferSize) {
                item->indexBuffer->setSize(indexBufferSize);
                item->indexBuffer->create();
            }
            batch->updateDynamicBuffer(item->indexBuffer, 0, indexBufferSize, item->geometry.indices().constData());

            item->numberOfIndices = item->geometry.indices().size();
            item->geometryNeedsUpdate = false;

            //Clear the geometry, since we don't need to store it anymore
            item->geometry = {};
        }

        if(item->textureNeedsUpdate) {
            auto image = item->image;

            //Try rescaling the images
            //Resize images
            if(!image.size().isNull()) {
                // image = image.scaled(1024, 1024, Qt::AspectRatioMode::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation);

                item->memoryUsaged = (double)image.sizeInBytes() / 1024.0 / 1024.0;
                qDebug() << "image size:" << image.size() << item->memoryUsaged << "mb";

                qDebug() << "Total memory used:" << std::accumulate(m_items.begin(), m_items.end(), 0.0, [](double value, const auto& item) {
                    return value + item->memoryUsaged;
                });
            }


            auto uploadTextureData = [batch, item, &image]() {
                Q_ASSERT(image.size() == item->texture->pixelSize());
                batch->uploadTexture(item->texture, image);
                batch->generateMips(item->texture);
            };

            auto createTexture = [item, &data, uploadTextureData, this](const QSize& size){
                if(!size.isEmpty()) {
                    item->texture = data.renderData.cb->rhi()->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
                    // item->texture = data.renderData.cb->rhi()->newTexture(QRhiTexture::RGBA8, size, 1); //, QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
                    item->texture->create();
                    uploadTextureData();

                    //Update the bindings
                    item->createShadeResourceBindings(data, m_sharedData);
                }
            };

            if(item->texture == nullptr) {
                createTexture(image.size());
            } else {
                //Texture already exists, update the texture
                if(item->texture->pixelSize() != image.size()) {
                    item->texture->deleteLater();
                    item->texture = nullptr;
                    createTexture(image.size());
                } else {
                    //Just upload new data
                    uploadTextureData();
                }
            }
            item->textureNeedsUpdate = false;

            //Clear the reference to the image
            item->image = {};
        }
    }
}

void cwRhiTexturedItems::render(const RenderData& data)
{
    if (m_items.isEmpty() || !m_visible)
        return;

    data.cb->setGraphicsPipeline(m_pipeline);

    for (auto scrap : std::as_const(m_items)) {
        // Set up per-scrap resources (textures, etc.)
        Q_ASSERT(m_globalSrb->isLayoutCompatible(scrap->srb)); //if this fails check layout compatibility

        //Make sure the scrap is valid to draw
        if(scrap->numberOfIndices > 0) {
            data.cb->setShaderResources(scrap->srb);

            // const QRhiCommandBuffer::VertexInput vertexInputs[] = {
            //     { scrap->vertexBuffer, 0 },
            //     { scrap->texCoordBuffer, 0 }
            // };

            const QRhiCommandBuffer::VertexInput vbind(scrap->vertexBuffer, 0);

            data.cb->setVertexInput(0, 1, &vbind, scrap->indexBuffer, 0, QRhiCommandBuffer::IndexUInt32);

            // Q_ASSERT(scrap->numberOfIndices > 0); //If this fails the scrap isn't generated correctly
            data.cb->drawIndexed(scrap->numberOfIndices);
        }
    }
}

cwRhiTexturedItems::Item::Item()
{
}

cwRhiTexturedItems::Item::~Item()
{
    delete vertexBuffer;
    // delete texCoordBuffer;
    delete indexBuffer;
    delete texture;
    delete srb;

    //Sampler isn't owned by RhiScrap
    //delete sampler;
}

void cwRhiTexturedItems::Item::initializeResources(const ResourceUpdateData& data, const SharedScrapData& sharedData)
{

    QRhi* rhi = data.renderData.cb->rhi();

    // Create buffers if they don't exist
    vertexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    vertexBuffer->create();

    // texCoordBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    // texCoordBuffer->create();

    indexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, 0);
    indexBuffer->create();


    createShadeResourceBindings(data, sharedData);

    resourcesInitialized = true;
}


void cwRhiTexturedItems::Item::createShadeResourceBindings(const ResourceUpdateData& data, const SharedScrapData &sharedData)
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
