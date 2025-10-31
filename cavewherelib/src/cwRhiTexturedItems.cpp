#include "cwRhiTexturedItems.h"

#include "cwRenderTexturedItems.h"
#include "cwRhiItemRenderer.h"

#include <QByteArray>
#include <QFont>
#include <QImage>
#include <QPainter>

namespace {
constexpr int kFallbackUniformSize = 16; // minimum to satisfy uniform alignment
}

cwRhiTexturedItems::cwRhiTexturedItems() = default;

cwRhiTexturedItems::~cwRhiTexturedItems()
{
    for (auto item : std::as_const(m_items)) {
        delete item;
    }

    delete m_sharedData.sampler;
    delete m_sharedData.loadingTexture;
}

void cwRhiTexturedItems::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized) {
        return;
    }

    QRhi* rhi = data.renderData.cb->rhi();

    m_sharedData.sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                                           QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sharedData.sampler->create();

    {
        QImage image(256, 256, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setPen(Qt::white);
        painter.setFont(QFont(QStringLiteral("Arial"), 48));
        painter.drawText(image.rect(), Qt::AlignCenter, QStringLiteral("Loading"));
        painter.end();

        m_sharedData.loadingTexture = rhi->newTexture(QRhiTexture::RGBA8, image.size(), 1,
                                                      QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
        m_sharedData.loadingTexture->create();
        data.resourceUpdateBatch->uploadTexture(m_sharedData.loadingTexture, image);
        data.resourceUpdateBatch->generateMips(m_sharedData.loadingTexture);
    }

    m_inputLayout.setBindings({
        int(sizeof(QVector3D) + sizeof(QVector2D))
    });
    m_inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, int(sizeof(QVector3D)) }
    });

    m_resourcesInitialized = true;
}

void cwRhiTexturedItems::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderTexturedItems*>(data.object) != nullptr);
    auto* renderItems = static_cast<cwRenderTexturedItems*>(data.object);

    m_renderItems = renderItems;
    m_visible = renderItems->isVisible();

    const auto pendingChanges = renderItems->m_pendingChanges;
    for (const auto& command : pendingChanges) {
        const auto id = command.id();

        switch (command.type()) {
        case cwRenderTexturedItems::PendingCommand::Add: {
            auto* item = new Item;
            item->material = command.item().material;
            item->geometry = command.item().geometry;
            item->geometryNeedsUpdate = !item->geometry.indices().isEmpty();
            item->image = command.item().texture;
            item->textureNeedsUpdate = !item->image.isNull();
            item->uniformBlock = command.item().uniformBlock;
            item->uniformNeedsUpdate = !item->uniformBlock.isEmpty();
            item->visible = command.item().visible;
            item->pipelineNeedsUpdate = true;

            m_items.insert(id, item);
            break;
        }
        case cwRenderTexturedItems::PendingCommand::Remove: {
            auto it = m_items.find(id);
            if (it != m_items.end()) {
                delete it.value();
                m_items.erase(it);
            }
            break;
        }
        case cwRenderTexturedItems::PendingCommand::UpdateGeometry: {
            if (auto* item = m_items.value(id, nullptr)) {
                item->geometry = command.item().geometry;
                item->geometryNeedsUpdate = true;
            }
            break;
        }
        case cwRenderTexturedItems::PendingCommand::UpdateTexture: {
            if (auto* item = m_items.value(id, nullptr)) {
                item->image = command.item().texture;
                item->textureNeedsUpdate = true;
            }
            break;
        }
        case cwRenderTexturedItems::PendingCommand::UpdateMaterial: {
            if (auto* item = m_items.value(id, nullptr)) {
                if (!(item->material == command.item().material)) {
                    item->material = command.item().material;
                    item->pipelineNeedsUpdate = true;
                }
            }
            break;
        }
        case cwRenderTexturedItems::PendingCommand::UpdateUniformBlock: {
            if (auto* item = m_items.value(id, nullptr)) {
                item->uniformBlock = command.item().uniformBlock;
                item->uniformNeedsUpdate = true;
            }
            break;
        }
        case cwRenderTexturedItems::PendingCommand::UpdateVisiblity: {
            if (auto* item = m_items.value(id, nullptr)) {
                item->visible = command.item().visible;
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
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        Item* item = it.value();
        if (!item) {
            continue;
        }

        if (!item->resourcesInitialized) {
            item->initializeResources(data, m_sharedData);
        }

        if (item->geometryNeedsUpdate) {
            item->updateGeometryBuffers(data);
        }

        if (item->textureNeedsUpdate) {
            item->updateTextureResource(data, m_sharedData);
            item->createShaderResourceBindings(data, m_sharedData);
        }

        if (item->uniformNeedsUpdate) {
            item->updateUniformBuffer(data);
            item->createShaderResourceBindings(data, m_sharedData);
        }

        if (item->pipelineNeedsUpdate) {
            item->ensurePipeline(data, m_sharedData, m_inputLayout);
            item->createShaderResourceBindings(data, m_sharedData);
        }
    }
}

void cwRhiTexturedItems::render(const RenderData& data)
{
    if (!m_visible) {
        return;
    }

    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        const Item* item = it.value();
        if (!item || !item->visible) {
            continue;
        }

        if (item->numberOfIndices <= 0 || !item->pipeline || !item->srb) {
            continue;
        }

        data.cb->setGraphicsPipeline(item->pipeline);
        data.cb->setShaderResources(item->srb);

        const QRhiCommandBuffer::VertexInput vbind(item->vertexBuffer, 0);
        data.cb->setVertexInput(0, 1, &vbind, item->indexBuffer, 0, QRhiCommandBuffer::IndexUInt32);

        data.cb->drawIndexed(item->numberOfIndices);
    }
}

cwRhiTexturedItems::Item::Item() = default;

cwRhiTexturedItems::Item::~Item()
{
    delete vertexBuffer;
    delete indexBuffer;
    delete uniformBuffer;
    delete texture;
    delete srb;
    delete pipeline;
    delete canonicalLayout;
}

void cwRhiTexturedItems::Item::initializeResources(const ResourceUpdateData& data, const SharedItemData&)
{
    QRhi* rhi = data.renderData.cb->rhi();

    vertexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
    vertexBuffer->create();

    indexBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, 0);
    indexBuffer->create();

    resourcesInitialized = true;
}

void cwRhiTexturedItems::Item::updateGeometryBuffers(const ResourceUpdateData& data)
{
    if (!vertexBuffer || !indexBuffer) {
        return;
    }

    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;
    const QByteArray vertexData = geometry.vertexData();
    const auto indices = geometry.indices();
    const int indexBytes = indices.size() * int(sizeof(uint32_t));

    if (vertexBuffer->size() != vertexData.size()) {
        vertexBuffer->setSize(vertexData.size());
        vertexBuffer->create();
    }
    if (!vertexData.isEmpty()) {
        batch->updateDynamicBuffer(vertexBuffer, 0, vertexData.size(), vertexData.constData());
    }

    if (indexBuffer->size() != indexBytes) {
        indexBuffer->setSize(indexBytes);
        indexBuffer->create();
    }
    if (indexBytes > 0) {
        batch->updateDynamicBuffer(indexBuffer, 0, indexBytes, indices.constData());
    }

    numberOfIndices = indices.size();
    geometry = {};
    geometryNeedsUpdate = false;
}

void cwRhiTexturedItems::Item::updateTextureResource(const ResourceUpdateData& data, const SharedItemData& sharedData)
{
    auto* rhi = data.renderData.cb->rhi();
    const QSize size = image.size();

    if (!texture || texture->pixelSize() != size) {
        delete texture;
        texture = nullptr;

        if (!size.isEmpty()) {
            texture = rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                      QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
            texture->create();
        }
    }

    if (texture && !image.isNull()) {
        data.resourceUpdateBatch->uploadTexture(texture, image);
        data.resourceUpdateBatch->generateMips(texture);
    }

    image = {};
    textureNeedsUpdate = false;
    Q_UNUSED(sharedData);
}

void cwRhiTexturedItems::Item::updateUniformBuffer(const ResourceUpdateData& data)
{
    if (!material.wantsPerDrawUniform()) {
        uniformNeedsUpdate = false;
        return;
    }

    QRhi* rhi = data.renderData.cb->rhi();
    QByteArray payload = uniformBlock;
    if (payload.isEmpty()) {
        payload.resize(kFallbackUniformSize);
        payload.fill(0);
    }

    const qsizetype alignedSize = rhi->ubufAligned(payload.size());
    if (!uniformBuffer || uniformBuffer->size() != alignedSize) {
        delete uniformBuffer;
        uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, alignedSize);
        uniformBuffer->create();
    }

    data.resourceUpdateBatch->updateDynamicBuffer(uniformBuffer, 0, payload.size(), payload.constData());
    uniformNeedsUpdate = false;
}

void cwRhiTexturedItems::Item::ensurePipeline(const ResourceUpdateData& data,
                                              const SharedItemData& sharedData,
                                              const QRhiVertexInputLayout& layout)
{
    QRhi* rhi = data.renderData.cb->rhi();
    auto* renderer = data.renderData.renderer;
    auto* renderTarget = renderer->renderTarget();
    auto* currentPass = renderTarget->renderPassDescriptor();
    const int currentSampleCount = renderTarget->sampleCount();

    const bool needsRebuild =
        pipeline == nullptr ||
        canonicalLayout == nullptr ||
        renderPass != currentPass ||
        sampleCount != currentSampleCount;

    if (!needsRebuild && !pipelineNeedsUpdate) {
        return;
    }

    destroyPipeline();

    renderPass = currentPass;
    sampleCount = currentSampleCount;

    if (!material.wantsPerDrawUniform()) {
        delete uniformBuffer;
        uniformBuffer = nullptr;
    }

    const QShader vertex = loadShader(material.vertexShader);
    const QShader fragment = loadShader(material.fragmentShader);

    pipeline = rhi->newGraphicsPipeline();
    pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertex },
        { QRhiShaderStage::Fragment, fragment }
    });
    pipeline->setCullMode(toRhiCullMode(material.cullMode));
    pipeline->setFrontFace(toRhiFrontFace(material.frontFace));
    pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    pipeline->setDepthTest(material.depthTest);
    pipeline->setDepthWrite(material.depthWrite);
    pipeline->setSampleCount(sampleCount);

    if (material.requiresBlending()) {
        auto blend = toBlendState(material);
        pipeline->setTargetBlends({ blend });
    } else {
        pipeline->setTargetBlends({});
    }

    QRhiShaderResourceBinding::StageFlags globalStages = toRhiStages(material.globalUniformStages);
    QRhiShaderResourceBinding::StageFlags textureStages = toRhiStages(material.textureStages);

    QVector<QRhiShaderResourceBinding> layoutBindings;
    layoutBindings.append(QRhiShaderResourceBinding::uniformBuffer(material.globalUniformBinding,
                                                                   globalStages,
                                                                   nullptr));

    if (material.wantsPerDrawUniform()) {
        layoutBindings.append(QRhiShaderResourceBinding::uniformBuffer(material.perDrawUniformBinding,
                                                                       toRhiStages(material.perDrawUniformStages),
                                                                       nullptr));
    }

    layoutBindings.append(QRhiShaderResourceBinding::sampledTexture(material.textureBinding,
                                                                    textureStages,
                                                                    nullptr,
                                                                    sharedData.sampler));

    canonicalLayout = rhi->newShaderResourceBindings();
    canonicalLayout->setBindings(layoutBindings.cbegin(), layoutBindings.cend());
    canonicalLayout->create();

    pipeline->setVertexInputLayout(layout);
    pipeline->setShaderResourceBindings(canonicalLayout);
    pipeline->setRenderPassDescriptor(renderPass);
    pipeline->create();

    pipelineNeedsUpdate = false;
}

void cwRhiTexturedItems::Item::createShaderResourceBindings(const ResourceUpdateData& data,
                                                             const SharedItemData& sharedData)
{
    if (!pipeline || !canonicalLayout) {
        return;
    }

    QRhi* rhi = data.renderData.cb->rhi();
    auto* renderer = data.renderData.renderer;

    if (srb) {
        delete srb;
        srb = nullptr;
    }

    auto* sampledTexture = texture ? texture : sharedData.loadingTexture;

    QVector<QRhiShaderResourceBinding> bindings;
    bindings.append(QRhiShaderResourceBinding::uniformBuffer(material.globalUniformBinding,
                                                             toRhiStages(material.globalUniformStages),
                                                             renderer->globalUniformBuffer()));

    if (material.wantsPerDrawUniform()) {
        if (!uniformBuffer) {
            QByteArray zero(kFallbackUniformSize, '\0');
            const qsizetype aligned = rhi->ubufAligned(zero.size());
            uniformBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, aligned);
            uniformBuffer->create();
            data.resourceUpdateBatch->updateDynamicBuffer(uniformBuffer, 0, zero.size(), zero.constData());
        }
        bindings.append(QRhiShaderResourceBinding::uniformBuffer(material.perDrawUniformBinding,
                                                                 toRhiStages(material.perDrawUniformStages),
                                                                 uniformBuffer));
    }

    bindings.append(QRhiShaderResourceBinding::sampledTexture(material.textureBinding,
                                                              toRhiStages(material.textureStages),
                                                              sampledTexture,
                                                              sharedData.sampler));

    srb = rhi->newShaderResourceBindings();
    srb->setBindings(bindings.cbegin(), bindings.cend());
    srb->create();
}

void cwRhiTexturedItems::Item::destroyPipeline()
{
    delete pipeline;
    pipeline = nullptr;

    delete canonicalLayout;
    canonicalLayout = nullptr;
}

QRhiShaderResourceBinding::StageFlags cwRhiTexturedItems::toRhiStages(cwRenderMaterialState::ShaderStages stages)
{
    using Stage = cwRenderMaterialState::ShaderStage;
    QRhiShaderResourceBinding::StageFlags flags = {};
    if (stages.testFlag(Stage::Vertex)) {
        flags |= QRhiShaderResourceBinding::VertexStage;
    }
    if (stages.testFlag(Stage::Fragment)) {
        flags |= QRhiShaderResourceBinding::FragmentStage;
    }
    return flags;
}

QRhiGraphicsPipeline::CullMode cwRhiTexturedItems::toRhiCullMode(cwRenderMaterialState::CullMode mode)
{
    switch (mode) {
    case cwRenderMaterialState::CullMode::None:  return QRhiGraphicsPipeline::None;
    case cwRenderMaterialState::CullMode::Front: return QRhiGraphicsPipeline::Front;
    case cwRenderMaterialState::CullMode::Back:  return QRhiGraphicsPipeline::Back;
    }
    return QRhiGraphicsPipeline::Back;
}

QRhiGraphicsPipeline::FrontFace cwRhiTexturedItems::toRhiFrontFace(cwRenderMaterialState::FrontFace face)
{
    return face == cwRenderMaterialState::FrontFace::CW
           ? QRhiGraphicsPipeline::CW
           : QRhiGraphicsPipeline::CCW;
}

QRhiGraphicsPipeline::TargetBlend cwRhiTexturedItems::toBlendState(const cwRenderMaterialState& material)
{
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;

    switch (material.blendMode) {
    case cwRenderMaterialState::BlendMode::None:
        blend.enable = false;
        break;
    case cwRenderMaterialState::BlendMode::Alpha:
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        break;
    case cwRenderMaterialState::BlendMode::Additive:
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::One;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::One;
        break;
    case cwRenderMaterialState::BlendMode::PremultipliedAlpha:
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        break;
    }

    return blend;
}
