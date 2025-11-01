#include "cwRhiTexturedItems.h"

#include "cwRenderTexturedItems.h"
#include "cwRhiItemRenderer.h"

#include <QByteArray>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QtGlobal>
#include <algorithm>
#include <utility>

namespace {
constexpr int kFallbackUniformSize = 16; // minimum to satisfy uniform alignment
}

uint qHash(const cwRhiTexturedItems::PipelineKey& key, uint seed) noexcept
{
    seed = qHash(quintptr(key.renderPass), seed);
    seed = qHash(key.sampleCount, seed);
    seed = qHash(key.vertexShader, seed);
    seed = qHash(key.fragmentShader, seed);
    seed = qHash(key.cullMode, seed);
    seed = qHash(key.frontFace, seed);
    seed = qHash(key.blendMode, seed);
    seed = qHash(key.depthTest, seed);
    seed = qHash(key.depthWrite, seed);
    seed = qHash(key.globalBinding, seed);
    seed = qHash(key.perDrawBinding, seed);
    seed = qHash(key.textureBinding, seed);
    seed = qHash(key.globalStages, seed);
    seed = qHash(key.perDrawStages, seed);
    seed = qHash(key.textureStages, seed);
    seed = qHash(key.hasPerDraw, seed);
    return seed;
}

cwRhiTexturedItems::cwRhiTexturedItems() = default;

cwRhiTexturedItems::~cwRhiTexturedItems()
{
    for (auto item : std::as_const(m_items)) {
        delete item;
    }

    delete m_sharedData.sampler;
    delete m_sharedData.loadingTexture;

    for (auto record : std::as_const(m_pipelineCache)) {
        delete record->pipeline;
        delete record->layout;
        delete record;
    }
    m_pipelineCache.clear();
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
            item->owner = this;
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

        if (!item->owner) {
            item->owner = this;
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
    // if (!m_visible) {
    //     return;
    // }

    // QVector<Item*> drawList;
    // drawList.reserve(m_items.size());
    // for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
    //     Item* item = it.value();
    //     if (!item || !item->visible) {
    //         continue;
    //     }

    //     if (item->numberOfIndices <= 0 || !item->pipelineRecord || !item->pipelineRecord->pipeline || !item->srb) {
    //         continue;
    //     }

    //     drawList.append(item);
    // }

    // if (drawList.isEmpty()) {
    //     return;
    // }

    // std::sort(drawList.begin(), drawList.end(), [](const Item* lhs, const Item* rhs) {
    //     return lhs->pipelineRecord->pipeline < rhs->pipelineRecord->pipeline;
    // });

    // QRhiGraphicsPipeline* boundPipeline = nullptr;
    // for (Item* item : std::as_const(drawList)) {
    //     if (item->pipelineRecord->pipeline != boundPipeline) {
    //         boundPipeline = item->pipelineRecord->pipeline;
    //         data.cb->setGraphicsPipeline(boundPipeline);
    //     }

    //     data.cb->setShaderResources(item->srb);

    //     const QRhiCommandBuffer::VertexInput vbind(item->vertexBuffer, 0);
    //     data.cb->setVertexInput(0, 1, &vbind, item->indexBuffer, 0, QRhiCommandBuffer::IndexUInt32);

    //     data.cb->drawIndexed(item->numberOfIndices);
    // }
}

bool cwRhiTexturedItems::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (!m_visible) {
        return false;
    }

    const auto desiredPass = context.renderPass;
    bool appended = false;

    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        const Item* item = it.value();
        if (!item || !item->visible) {
            continue;
        }

        if (item->numberOfIndices <= 0 ||
            !item->pipelineRecord ||
            !item->pipelineRecord->pipeline ||
            !item->srb ||
            !item->vertexBuffer ||
            !item->indexBuffer) {
            continue;
        }

        if (toRenderPass(item->material.renderPass) != desiredPass) {
            continue;
        }

        cwRHIObject::PipelineState state;
        state.pipeline = item->pipelineRecord->pipeline;
        const quint64 pipelineKey = quint64(quintptr(state.pipeline));
        state.sortKey = (quint64(context.objectOrder) << 32) | (pipelineKey & 0xffffffffu);

        auto& batch = acquirePipelineBatch(batches, state);

        cwRHIObject::Drawable drawable;
        drawable.type = cwRHIObject::Drawable::Type::Indexed;
        drawable.bindings = item->srb;
        drawable.indexBuffer = item->indexBuffer;
        drawable.indexFormat = QRhiCommandBuffer::IndexUInt32;
        drawable.indexCount = static_cast<quint32>(item->numberOfIndices);
        drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(item->vertexBuffer, 0));

        batch.drawables.append(drawable);
        appended = true;
    }

    return appended;
}

cwRhiTexturedItems::Item::Item() = default;

cwRhiTexturedItems::Item::~Item()
{
    releasePipeline();
    delete vertexBuffer;
    delete indexBuffer;
    delete uniformBuffer;
    delete texture;
    delete srb;
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
    if (!owner) {
        pipelineNeedsUpdate = false;
        return;
    }

    auto* renderer = data.renderData.renderer;
    auto* renderTarget = renderer->renderTarget();
    auto* currentPass = renderTarget->renderPassDescriptor();
    const int currentSampleCount = renderTarget->sampleCount();

    const PipelineKey key = owner->makePipelineKey(currentPass, currentSampleCount, material);

    if (!pipelineNeedsUpdate && pipelineRecord && pipelineRecord->key == key) {
        return;
    }

    releasePipeline();
    pipelineRecord = owner->acquirePipeline(key, material, data.renderData.cb->rhi(), layout, sharedData);
    pipelineNeedsUpdate = false;

    if (!material.wantsPerDrawUniform()) {
        delete uniformBuffer;
        uniformBuffer = nullptr;
        uniformNeedsUpdate = false;
    }
}

void cwRhiTexturedItems::Item::releasePipeline()
{
    if (owner && pipelineRecord) {
        owner->releasePipeline(pipelineRecord);
    }
    pipelineRecord = nullptr;
}

void cwRhiTexturedItems::Item::createShaderResourceBindings(const ResourceUpdateData& data,
                                                             const SharedItemData& sharedData)
{
    if (!pipelineRecord || !pipelineRecord->pipeline) {
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
    Q_ASSERT(!pipelineRecord->layout || pipelineRecord->layout->isLayoutCompatible(srb));
}

quint8 cwRhiTexturedItems::toStageMask(cwRenderMaterialState::ShaderStages stages)
{
    using Stage = cwRenderMaterialState::ShaderStage;
    quint8 mask = 0;
    if (stages.testFlag(Stage::Vertex)) {
        mask |= 0x1;
    }
    if (stages.testFlag(Stage::Fragment)) {
        mask |= 0x2;
    }
    return mask;
}

cwRhiTexturedItems::PipelineKey cwRhiTexturedItems::makePipelineKey(QRhiRenderPassDescriptor* renderPass,
                                                                    int sampleCount,
                                                                    const cwRenderMaterialState& material) const
{
    PipelineKey key;
    key.renderPass = renderPass;
    key.sampleCount = sampleCount;
    key.vertexShader = material.vertexShader;
    key.fragmentShader = material.fragmentShader;
    key.cullMode = static_cast<quint8>(material.cullMode);
    key.frontFace = static_cast<quint8>(material.frontFace);
    key.blendMode = static_cast<quint8>(material.blendMode);
    key.depthTest = material.depthTest ? 1 : 0;
    key.depthWrite = material.depthWrite ? 1 : 0;
    key.globalBinding = static_cast<quint8>(material.globalUniformBinding);
    key.perDrawBinding = material.wantsPerDrawUniform() ? static_cast<quint8>(material.perDrawUniformBinding) : 0xFF;
    key.textureBinding = static_cast<quint8>(material.textureBinding);
    key.globalStages = toStageMask(material.globalUniformStages);
    key.perDrawStages = toStageMask(material.perDrawUniformStages);
    key.textureStages = toStageMask(material.textureStages);
    key.hasPerDraw = material.wantsPerDrawUniform() ? 1 : 0;
    return key;
}

cwRhiTexturedItems::PipelineRecord *cwRhiTexturedItems::acquirePipeline(const PipelineKey& key,
                                                                        const cwRenderMaterialState& material,
                                                                        QRhi* rhi,
                                                                        const QRhiVertexInputLayout& layout,
                                                                        const SharedItemData& sharedData)
{
    auto it = m_pipelineCache.find(key);
    if (it != m_pipelineCache.end()) {
        it.value()->refCount += 1;
        return it.value();
    }

    auto* record = new PipelineRecord;
    record->key = key;
    record->refCount = 1;
    record->pipeline = rhi->newGraphicsPipeline();

    QShader vertex = loadShader(material.vertexShader);
    QShader fragment = loadShader(material.fragmentShader);

    record->pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vertex },
        { QRhiShaderStage::Fragment, fragment }
    });
    record->pipeline->setCullMode(toRhiCullMode(material.cullMode));
    record->pipeline->setFrontFace(toRhiFrontFace(material.frontFace));
    record->pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    record->pipeline->setDepthTest(material.depthTest);
    record->pipeline->setDepthWrite(material.depthWrite);
    record->pipeline->setSampleCount(key.sampleCount);

    if (material.requiresBlending()) {
        record->pipeline->setTargetBlends({ toBlendState(material) });
    } else {
        record->pipeline->setTargetBlends({});
    }

    QVector<QRhiShaderResourceBinding> layoutBindings;
    layoutBindings.append(QRhiShaderResourceBinding::uniformBuffer(material.globalUniformBinding,
                                                                   toRhiStages(material.globalUniformStages),
                                                                   nullptr));

    if (material.wantsPerDrawUniform()) {
        layoutBindings.append(QRhiShaderResourceBinding::uniformBuffer(material.perDrawUniformBinding,
                                                                       toRhiStages(material.perDrawUniformStages),
                                                                       nullptr));
    }

    layoutBindings.append(QRhiShaderResourceBinding::sampledTexture(material.textureBinding,
                                                                    toRhiStages(material.textureStages),
                                                                    nullptr,
                                                                    sharedData.sampler));

    record->layout = rhi->newShaderResourceBindings();
    record->layout->setBindings(layoutBindings.cbegin(), layoutBindings.cend());
    record->layout->create();

    record->pipeline->setVertexInputLayout(layout);
    record->pipeline->setShaderResourceBindings(record->layout);
    record->pipeline->setRenderPassDescriptor(key.renderPass);
    record->pipeline->create();

    m_pipelineCache.insert(record->key, record);
    return record;
}

void cwRhiTexturedItems::releasePipeline(PipelineRecord* record)
{
    if (!record) {
        return;
    }

    record->refCount -= 1;
    if (record->refCount > 0) {
        return;
    }

    auto it = m_pipelineCache.find(record->key);
    if (it != m_pipelineCache.end() && it.value() == record) {
        m_pipelineCache.erase(it);
    }

    delete record->pipeline;
    delete record->layout;
    delete record;
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

cwRHIObject::RenderPass cwRhiTexturedItems::toRenderPass(cwRenderMaterialState::RenderPass pass)
{
    using MaterialPass = cwRenderMaterialState::RenderPass;
    switch (pass) {
    case MaterialPass::Opaque:      return cwRHIObject::RenderPass::Opaque;
    case MaterialPass::Transparent: return cwRHIObject::RenderPass::Transparent;
    case MaterialPass::Overlay:     return cwRHIObject::RenderPass::Overlay;
    case MaterialPass::ShadowMap:   return cwRHIObject::RenderPass::ShadowMap;
    }
    return cwRHIObject::RenderPass::Opaque;
}
