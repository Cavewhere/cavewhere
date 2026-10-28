/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderSpriteBillboards.h"
#include "cwProfileLog.h"
#include "cwQuickItemLayerTexture.h"
#include "cwQuickItemSubscene.h"
#include "cwRHIObject.h"
#include "cwRenderMaterialState.h"
#include "cwRhiFrameRenderer.h"
#include "cwRhiItemRenderer.h"

//Qt includes
#include <QElapsedTimer>
#include <QImage>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtMath>
#include <rhi/qrhi.h>

//Std includes
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace {

// The atlas slot cap. One row of cells, so the atlas stays well under every
// backend's 2D texture limit at icon sizes.
constexpr int kMaxSprites = 16;

// The fixed TriangleStrip the vertex shader expands per instance: 4 corners
// selected from gl_VertexIndex, no corner vertex buffer.
constexpr quint32 kVerticesPerQuad = 4;

// Shader binding points, shared with SpriteBillboard.vert and .frag.
constexpr int kGlobalCameraBinding = 0;
constexpr int kSpriteBlockBinding = 1;
constexpr int kSpriteTextureBinding = 2;

// Per-instance attribute offsets into cwRenderSpriteBillboards::Instance, which
// uploads verbatim, so its layout is pinned to what SpriteBillboard.vert reads.
using SpriteInstance = cwRenderSpriteBillboards::Instance;
constexpr quint32 kWorldPositionOffset = offsetof(SpriteInstance, worldPosition);
constexpr quint32 kDepthBiasOffset = offsetof(SpriteInstance, depthBias);
constexpr quint32 kOpacityOffset = offsetof(SpriteInstance, opacity);
constexpr quint32 kSpriteIndexOffset = offsetof(SpriteInstance, spriteIndex);
static_assert(kWorldPositionOffset == 0, "iWorldPosition is Float3 at offset 0");
static_assert(kDepthBiasOffset == 3 * sizeof(float), "iDepthBias follows the three position floats");
static_assert(kOpacityOffset == kDepthBiasOffset + sizeof(float), "iOpacity follows iDepthBias");
static_assert(kSpriteIndexOffset == kOpacityOffset + sizeof(float), "iSpriteIndex follows iOpacity");
static_assert(sizeof(SpriteInstance) == kSpriteIndexOffset + sizeof(uint32_t),
              "Instance must match the vertex input layout with no padding");

// The vertex shader's size-mode selector (a float in the std140 block).
constexpr float kSizeModeWorldSized = 0.0f;
constexpr float kSizeModeScreenConstant = 1.0f;

// Mirrors SpriteBlock in SpriteBillboard.vert.
struct SpriteBlock {
    float spriteSizePx[2];
    float sizeMode;
    float slotCount;
    float anchor[2];
};

float sizeModeSelector(cwBillboardSizeMode sizeMode)
{
    return sizeMode == cwBillboardSizeMode::WorldSized ? kSizeModeWorldSized
                                                       : kSizeModeScreenConstant;
}

// Resizes a dynamic per-instance buffer to fit and uploads the values.
template<typename T>
void uploadPerInstance(QRhiBuffer* buffer, QRhiResourceUpdateBatch* batch, const QVector<T>& perInstance)
{
    const int bufferSize = perInstance.size() * int(sizeof(T));
    if (bufferSize > 0) {
        if (buffer->size() != bufferSize) {
            buffer->setSize(bufferSize);
            buffer->create();
        }
        batch->updateDynamicBuffer(buffer, 0, bufferSize, perInstance.constData());
    }
}

// The device-pixel size a layer renders logicalSize at, as cwQuickItemLayerTexture
// sizes its texture, so an atlas cell and a slot's texture match.
QSize cellPixelSize(const QSizeF& logicalSize, float devicePixelRatio)
{
    return QSize(qCeil(logicalSize.width() * devicePixelRatio),
                 qCeil(logicalSize.height() * devicePixelRatio));
}

// Render-thread backend: one instanced draw of every visible billboard, sampling
// an atlas that holds a copy of each slot's layer texture.
class cwRHISpriteBillboards : public cwRHIObject
{
public:
    ~cwRHISpriteBillboards() override
    {
        teardownWindowResources();
        delete m_instanceBuffer;
        delete m_spriteBlockBuffer;
        // m_pipelines releases its held pipeline references on destruction.
    }

    void initialize(const ResourceUpdateData& data) override
    {
        if (m_resourcesInitialized) {
            return;
        }

        QRhi* rhi = data.renderData.cb->rhi();

        m_instanceBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 0);
        m_instanceBuffer->create();

        const auto spriteBlockSize = rhi->ubufAligned(sizeof(SpriteBlock));
        m_spriteBlockBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, spriteBlockSize);
        m_spriteBlockBuffer->create();

        // One binding stepping per instance, laid out as Instance.
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { sizeof(cwRenderSpriteBillboards::Instance), QRhiVertexInputBinding::PerInstance }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float3, kWorldPositionOffset },  // iWorldPosition
            { 0, 1, QRhiVertexInputAttribute::Float, kDepthBiasOffset },       // iDepthBias
            { 0, 2, QRhiVertexInputAttribute::Float, kOpacityOffset },         // iOpacity
            { 0, 3, QRhiVertexInputAttribute::UInt, kSpriteIndexOffset }       // iSpriteIndex
        });
        m_inputLayout = inputLayout;

        m_resourcesInitialized = true;
    }

    void synchronize(const SynchronizeData& data) override
    {
        auto* renderObject = static_cast<cwRenderSpriteBillboards*>(data.object);

        QQuickWindow* window = renderObject->window();
        if (window != m_window) {
            bindWindow(window);
        }

        // Snapshot while the GUI thread is blocked; nothing past this point
        // dereferences a QQuickItem or the window.
        QVector<cwRenderSpriteBillboards::Instance> instances = renderObject->buildInstances();
        if (instances != m_instances) {
            m_instances = std::move(instances);
            m_instancesDirty = true;
        }

        const QSizeF spriteSize = renderObject->spriteSize();
        const cwBillboardSizeMode sizeMode = renderObject->sizeMode();
        const QPointF anchor = renderObject->anchor();
        if (spriteSize != m_spriteSize || sizeMode != m_sizeMode || anchor != m_anchor) {
            m_spriteSize = spriteSize;
            m_sizeMode = sizeMode;
            m_anchor = anchor;
            m_spriteBlockDirty = true;
        }

        if (m_window.isNull() || !m_window->isSceneGraphInitialized()) {
            releaseAtlas();
            return;
        }

        const float devicePixelRatio = float(m_window->effectiveDevicePixelRatio());
        const int slotCount = renderObject->spriteCount();

        m_slots.resize(size_t(slotCount));
        for (int i = 0; i < slotCount; ++i) {
            SlotTexture& slot = m_slots.at(size_t(i));
            if (!slot.layer) {
                slot.layer = std::make_unique<cwQuickItemLayerTexture>(m_window, renderObject);
                if (!slot.layer->isValid()) {
                    releaseAtlas();
                    return;
                }
            }
            // A slot without nodes (a destroyed prototype) leaves its cell as it was.
            slot.layer->setRootNodeHandle(renderObject->spriteRootNodeHandle(i));
            slot.layer->setContentSize(m_spriteSize, devicePixelRatio);
        }

        const QSize cellSize = cellPixelSize(m_spriteSize, devicePixelRatio);
        if (cellSize != m_pendingCellSize || slotCount != m_pendingSlotCount) {
            m_pendingCellSize = cellSize;
            m_pendingSlotCount = slotCount;
            m_atlasDirty = true;
        }
        if (m_atlas == nullptr && slotCount > 0 && !cellSize.isEmpty()) {
            m_atlasDirty = true;
        }
    }

    void updateResources(const ResourceUpdateData& data) override
    {
        QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

        if (m_atlasDirty) {
            recreateAtlas(data.renderData.cb->rhi(), batch);
            m_atlasDirty = false;
        }

        if (m_instancesDirty && m_instanceBuffer) {
            uploadPerInstance(m_instanceBuffer, batch, m_instances);
            m_instancesDirty = false;
        }

        if (m_spriteBlockDirty && m_spriteBlockBuffer) {
            SpriteBlock block;
            block.spriteSizePx[0] = float(m_spriteSize.width());
            block.spriteSizePx[1] = float(m_spriteSize.height());
            block.sizeMode = sizeModeSelector(m_sizeMode);
            block.slotCount = float(m_atlasSlotCount);
            block.anchor[0] = float(m_anchor.x());
            block.anchor[1] = float(m_anchor.y());
            batch->updateDynamicBuffer(m_spriteBlockBuffer, 0, sizeof(SpriteBlock), &block);
            m_spriteBlockDirty = false;
        }
    }

    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override
    {
        if (context.renderPass != RenderPass::Overlay || m_instances.isEmpty() || m_atlas == nullptr) {
            return false;
        }

        const bool profile = context.liveFrame && lcProfileRender().isDebugEnabled();
        QElapsedTimer timer;
        if (profile) {
            timer.start();
        }

        const RenderData& renderData = *context.renderData;
        cwRhiItemRenderer* renderer = renderData.renderer;
        if (!renderer || !renderer->rhi()) {
            return false;
        }

        copySlotsIntoAtlas(context.resourceUpdateBatch);

        if (!ensurePipeline(renderData, m_atlas)) {
            return false;
        }

        auto* pipeline = m_pipelineRecord ? m_pipelineRecord->pipeline : nullptr;
        if (!pipeline || !m_instanceBuffer || !m_spriteBlockBuffer || !m_srb) {
            return false;
        }

        PipelineState state;
        state.pipeline = pipeline;
        state.sortKey = makeSortKey(context.objectOrder, pipeline);

        auto& batch = acquirePipelineBatch(batches, state);
        Drawable drawable;
        drawable.type = Drawable::Type::NonIndexed;
        drawable.vertexBindings.append(QRhiCommandBuffer::VertexInput(m_instanceBuffer, 0));
        drawable.vertexCount = kVerticesPerQuad;
        drawable.instanceCount = static_cast<quint32>(m_instances.size());
        drawable.bindings = m_srb;
        drawable.globalCameraBinding = kGlobalCameraBinding;
        batch.drawables.append(drawable);

        if (profile) {
            cw::profile::write(lcProfileRender(),
                               QStringLiteral("spriteBillboards instances=%1 gatherUs=%2")
                                   .arg(m_instances.size())
                                   .arg(cw::profile::elapsedUs(timer)));
        }
        return true;
    }

private:
    void bindWindow(QQuickWindow* window)
    {
        teardownWindowResources();
        m_invalidationContext.reset();
        m_window = window;
        if (window != nullptr) {
            // The layer and the bindings that sample it belong to the window's
            // render context and have to go when it does, even if this object
            // outlives it.
            m_invalidationContext = std::make_unique<QObject>();
            QObject::connect(window, &QQuickWindow::sceneGraphInvalidated,
                             m_invalidationContext.get(), [this]() { teardownWindowResources(); },
                             Qt::DirectConnection);
        }
    }

    void teardownWindowResources()
    {
        releaseAtlas();
        delete m_sampler;
        m_sampler = nullptr;
    }

    // Destroying a slot's layer frees the texture it renders, and the SRB samples
    // the atlas, so all of them go together; the next synchronize and gather
    // rebuild them.
    void releaseAtlas()
    {
        m_slots.clear();
        destroyAtlasTexture();
        m_pendingCellSize = QSize();
        m_pendingSlotCount = 0;
    }

    // The SRB samples the atlas, so it goes with it.
    void destroyAtlasTexture()
    {
        delete m_srb;
        m_srb = nullptr;
        m_boundTexture = nullptr;
        delete m_atlas;
        m_atlas = nullptr;
        m_atlasCellSize = QSize();
        m_atlasSlotCount = 0;
        m_spriteBlockDirty = true;
    }

    // One row of cells, cleared to transparent so a cell no slot has filled yet
    // draws nothing. Every slot is copied in again on the next gather.
    void recreateAtlas(QRhi* rhi, QRhiResourceUpdateBatch* batch)
    {
        destroyAtlasTexture();

        const QSize atlasSize(m_pendingCellSize.width() * m_pendingSlotCount, m_pendingCellSize.height());
        if (rhi == nullptr || atlasSize.isEmpty()) {
            return;
        }

        m_atlas = rhi->newTexture(QRhiTexture::RGBA8, atlasSize);
        if (!m_atlas->create()) {
            delete m_atlas;
            m_atlas = nullptr;
            return;
        }
        m_atlasCellSize = m_pendingCellSize;
        m_atlasSlotCount = m_pendingSlotCount;

        QImage transparent(atlasSize, QImage::Format_RGBA8888_Premultiplied);
        transparent.fill(Qt::transparent);
        batch->uploadTexture(m_atlas, transparent);

        for (SlotTexture& slot : m_slots) {
            slot.needsCopy = true;
        }
    }

    // Gather runs before any pass opens, which is where each slot's layer may
    // record its own pass to re-render a changed prototype. The copy of a slot
    // that rendered, still waits for its first copy, or replaced its texture
    // (a resize) is recorded into the frame's batch, which is committed after
    // those passes and before the draw.
    void copySlotsIntoAtlas(QRhiResourceUpdateBatch* batch)
    {
        const int copySlots = qMin(int(m_slots.size()), m_atlasSlotCount);
        for (int i = 0; i < copySlots; ++i) {
            SlotTexture& slot = m_slots.at(size_t(i));
            if (slot.layer->updateTexture()) {
                slot.needsCopy = true;
            }
            QRhiTexture* source = slot.layer->rhiTexture();
            if (source == nullptr || batch == nullptr) {
                continue;
            }
            if (!slot.needsCopy && source == slot.copiedTexture) {
                continue;
            }

            QRhiTextureCopyDescription copy;
            copy.setDestinationTopLeft(QPoint(i * m_atlasCellSize.width(), 0));
            copy.setPixelSize(m_atlasCellSize.boundedTo(source->pixelSize()));
            batch->copyTexture(m_atlas, source, copy);
            slot.needsCopy = false;
            slot.copiedTexture = source;
        }
    }

    bool ensurePipeline(const RenderData& data, QRhiTexture* texture)
    {
        if (!m_resourcesInitialized || !data.renderer) {
            return false;
        }

        QRhi* rhi = data.renderer->rhi();
        auto* target = data.renderer->renderTarget();
        if (!rhi || !target) {
            return false;
        }

        const quint32 globalStride = data.renderer->globalUniformBufferStride();
        const auto key = buildPipelineKey(data.renderPassDescriptor, data.sampleCount);

        auto createFn = [this, key, globalStride](QRhi* localRhi) -> cwRhiPipelineRecord* {
            if (!localRhi) {
                return nullptr;
            }

            auto* record = new cwRhiPipelineRecord;
            record->pipeline = localRhi->newGraphicsPipeline();

            QShader vs = loadShader(QStringLiteral(":/shaders/SpriteBillboard.vert.qsb"));
            QShader fs = loadShader(QStringLiteral(":/shaders/SpriteBillboard.frag.qsb"));
            record->pipeline->setShaderStages({
                { QRhiShaderStage::Vertex, vs },
                { QRhiShaderStage::Fragment, fs }
            });

            // Depth writes on: the fragment shader discards transparent pixels, so
            // the depth left behind follows the marker's silhouette.
            record->pipeline->setDepthTest(true);
            record->pipeline->setDepthWrite(true);
            record->pipeline->setSampleCount(key.sampleCount);
            record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
            record->pipeline->setVertexInputLayout(m_inputLayout);
            record->pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);

            // The layer renders premultiplied, as every scene-graph material does.
            QRhiGraphicsPipeline::TargetBlend blendState;
            blendState.enable = true;
            blendState.srcColor = QRhiGraphicsPipeline::One;
            blendState.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            blendState.srcAlpha = QRhiGraphicsPipeline::One;
            blendState.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            record->pipeline->setTargetBlends({ blendState });

            record->layout = localRhi->newShaderResourceBindings();
            record->layout->setBindings({
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                    kGlobalCameraBinding, QRhiShaderResourceBinding::VertexStage, nullptr, globalStride),
                QRhiShaderResourceBinding::uniformBuffer(
                    kSpriteBlockBinding, QRhiShaderResourceBinding::VertexStage, nullptr),
                QRhiShaderResourceBinding::sampledTexture(
                    kSpriteTextureBinding, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr)
            });
            record->layout->create();

            record->pipeline->setShaderResourceBindings(record->layout);
            record->pipeline->setRenderPassDescriptor(key.renderPass);
            record->pipeline->create();
            return record;
        };

        m_pipelineRecord = m_pipelines.acquire(m_frame, key, [&]() {
            return m_frame->acquirePipeline(key, rhi, createFn);
        });

        if (!m_pipelineRecord) {
            return false;
        }

        return ensureShaderResources(rhi, data.renderer, texture);
    }

    // The SRB is rebuilt when the atlas is recreated or the pipeline layout
    // changed; a slot replacing its layer texture only needs a fresh copy.
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer, QRhiTexture* texture)
    {
        if (!rhi || !renderer || !texture) {
            return false;
        }

        if (m_srb) {
            const bool layoutCompatible = m_pipelineRecord && m_pipelineRecord->layout
                                          && m_pipelineRecord->layout->isLayoutCompatible(m_srb);
            if (layoutCompatible && m_boundTexture == texture) {
                return true;
            }
            delete m_srb;
            m_srb = nullptr;
        }

        if (!m_sampler) {
            m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            m_sampler->create();
        }

        m_srb = rhi->newShaderResourceBindings();
        m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                kGlobalCameraBinding, QRhiShaderResourceBinding::VertexStage,
                renderer->globalUniformBuffer(), renderer->globalUniformBufferStride()),
            QRhiShaderResourceBinding::uniformBuffer(
                kSpriteBlockBinding, QRhiShaderResourceBinding::VertexStage, m_spriteBlockBuffer),
            QRhiShaderResourceBinding::sampledTexture(
                kSpriteTextureBinding, QRhiShaderResourceBinding::FragmentStage, texture, m_sampler)
        });
        m_srb->create();
        m_boundTexture = texture;

        if (m_pipelineRecord && m_pipelineRecord->layout) {
            Q_ASSERT(m_pipelineRecord->layout->isLayoutCompatible(m_srb));
        }
        return true;
    }

    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor, int sampleCount) const
    {
        cwRhiPipelineKey key;
        key.renderPass = renderPassDescriptor;
        key.sampleCount = sampleCount;
        key.vertexShader = QStringLiteral(":/shaders/SpriteBillboard.vert.qsb");
        key.fragmentShader = QStringLiteral(":/shaders/SpriteBillboard.frag.qsb");
        key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
        key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
        key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::PremultipliedAlpha);
        key.depthTest = 1;
        key.depthWrite = 1;
        key.globalBinding = kGlobalCameraBinding;
        key.perDrawBinding = kSpriteBlockBinding;
        key.textureBinding = kSpriteTextureBinding;
        key.globalStages = cwShaderStageMask(cwRenderMaterialState::ShaderStage::Vertex);
        key.perDrawStages = cwShaderStageMask(cwRenderMaterialState::ShaderStage::Vertex);
        key.textureStages = cwShaderStageMask(cwRenderMaterialState::ShaderStage::Fragment);
        key.hasPerDraw = 1;
        key.topology = static_cast<quint8>(QRhiGraphicsPipeline::TriangleStrip);
        return key;
    }

    struct SlotTexture {
        std::unique_ptr<cwQuickItemLayerTexture> layer;
        QRhiTexture* copiedTexture = nullptr;   // the layer texture last copied in; owned by the layer
        bool needsCopy = true;
    };

    QPointer<QQuickWindow> m_window;
    std::unique_ptr<QObject> m_invalidationContext;
    std::vector<SlotTexture> m_slots;

    // The atlas: one row of m_atlasSlotCount cells of m_atlasCellSize device
    // pixels. synchronize records the wanted layout and updateResources applies it.
    QRhiTexture* m_atlas = nullptr;
    QSize m_atlasCellSize;
    int m_atlasSlotCount = 0;
    QSize m_pendingCellSize;
    int m_pendingSlotCount = 0;
    bool m_atlasDirty = false;

    // The GUI snapshot and what has changed since the last upload.
    QVector<cwRenderSpriteBillboards::Instance> m_instances;
    QSizeF m_spriteSize;
    cwBillboardSizeMode m_sizeMode = cwBillboardSizeMode::ScreenConstant;
    QPointF m_anchor = cwRenderSpriteBillboards::kDefaultAnchor;
    bool m_instancesDirty = false;
    bool m_spriteBlockDirty = true;

    bool m_resourcesInitialized = false;
    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_instanceBuffer = nullptr;      // binding 0, one Instance per billboard
    QRhiBuffer* m_spriteBlockBuffer = nullptr;   // SpriteBlock at kSpriteBlockBinding
    QRhiSampler* m_sampler = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;
    QRhiTexture* m_boundTexture = nullptr;       // the atlas m_srb samples
};

} // namespace

cwRenderSpriteBillboards::cwRenderSpriteBillboards(QObject* parent)
    : cwRenderObject(parent)
{
}

cwRenderSpriteBillboards::~cwRenderSpriteBillboards() = default;

void cwRenderSpriteBillboards::setWindow(QQuickWindow* window)
{
    if (m_window == window) {
        return;
    }
    m_window = window;
    rebuildSubscenes();
    update();
}

QQuickWindow* cwRenderSpriteBillboards::window() const
{
    return m_window;
}

void cwRenderSpriteBillboards::setSprites(const QList<QQuickItem*>& prototypes)
{
    QList<QQuickItem*> accepted;
    accepted.reserve(prototypes.size());
    for (QQuickItem* prototype : prototypes) {
        if (prototype != nullptr) {
            accepted.append(prototype);
        }
    }
    const qsizetype nullCount = prototypes.size() - accepted.size();
    const bool truncated = accepted.size() > kMaxSprites;
    if (truncated) {
        accepted.resize(kMaxSprites);
    }

    if (accepted == sprites()) {
        return;
    }

    if (nullCount > 0) {
        qWarning() << "cwRenderSpriteBillboards: skipped" << nullCount << "null sprite prototype(s)";
    }
    if (truncated) {
        qWarning() << "cwRenderSpriteBillboards:" << prototypes.size() << "sprite prototypes exceed the"
                   << kMaxSprites << "atlas slots; the rest are dropped";
    }

    for (const Slot& slot : m_slots) {
        if (slot.prototype) {
            disconnect(slot.prototype, nullptr, this, nullptr);
        }
    }

    m_slots.clear();
    m_slots.resize(accepted.size());
    for (int i = 0; i < accepted.size(); ++i) {
        m_slots.at(i).prototype = accepted.at(i);
    }
    rebuildSubscenes();

    for (QQuickItem* prototype : std::as_const(accepted)) {
        // Sizes are snapshotted at synchronize and drive each slot's layer, so a
        // resize needs a frame.
        connect(prototype, &QQuickItem::widthChanged, this, &cwRenderSpriteBillboards::prototypeResized,
                Qt::UniqueConnection);
        connect(prototype, &QQuickItem::heightChanged, this, &cwRenderSpriteBillboards::prototypeResized,
                Qt::UniqueConnection);
    }

    warnOnSizeMismatch();
    update();
}

void cwRenderSpriteBillboards::setAnchor(const QPointF& anchor)
{
    if (m_anchor == anchor) {
        return;
    }
    m_anchor = anchor;
    update();
}

QPointF cwRenderSpriteBillboards::anchor() const
{
    return m_anchor;
}

QList<QQuickItem*> cwRenderSpriteBillboards::sprites() const
{
    QList<QQuickItem*> prototypes;
    prototypes.reserve(int(m_slots.size()));
    for (const Slot& slot : m_slots) {
        prototypes.append(slot.prototype.data());
    }
    return prototypes;
}

int cwRenderSpriteBillboards::spriteCount() const
{
    return int(m_slots.size());
}

QSizeF cwRenderSpriteBillboards::spriteSize() const
{
    if (m_slots.empty() || m_slots.front().prototype.isNull()) {
        return QSizeF();
    }
    const QQuickItem* prototype = m_slots.front().prototype;
    return QSizeF(prototype->width(), prototype->height());
}

void cwRenderSpriteBillboards::setSprite(QQuickItem* prototype)
{
    if (prototype != nullptr) {
        setSprites({prototype});
    } else {
        setSprites({});
    }
}

QQuickItem* cwRenderSpriteBillboards::sprite() const
{
    if (m_slots.empty()) {
        return nullptr;
    }
    return m_slots.front().prototype;
}

void cwRenderSpriteBillboards::setSizeMode(cwBillboardSizeMode sizeMode)
{
    if (m_sizeMode == sizeMode) {
        return;
    }
    m_sizeMode = sizeMode;
    update();
}

cwBillboardSizeMode cwRenderSpriteBillboards::sizeMode() const
{
    return m_sizeMode;
}

void cwRenderSpriteBillboards::rebuildSubscenes()
{
    // Subscenes are bound to a window's scene graph and to one item.
    for (Slot& slot : m_slots) {
        slot.subscene.reset();
        if (slot.prototype && m_window) {
            slot.subscene = std::make_unique<cwQuickItemSubscene>(slot.prototype, m_window);
        }
    }
}

void cwRenderSpriteBillboards::prototypeResized()
{
    warnOnSizeMismatch();
    update();
}

void cwRenderSpriteBillboards::warnOnSizeMismatch()
{
    if (m_warnedSizeMismatch) {
        return;
    }
    const QSizeF sharedSize = spriteSize();
    if (sharedSize.isEmpty()) {
        return;
    }
    for (int i = 1; i < int(m_slots.size()); ++i) {
        const QQuickItem* prototype = m_slots.at(i).prototype;
        if (prototype == nullptr) {
            continue;
        }
        const QSizeF slotSize(prototype->width(), prototype->height());
        // An empty slot is one still loading; it is drawn at the shared size too.
        if (!slotSize.isEmpty() && slotSize != sharedSize) {
            qWarning() << "cwRenderSpriteBillboards: sprite slot" << i << "is" << slotSize
                       << "but every slot draws at slot 0's size" << sharedSize;
            m_warnedSizeMismatch = true;
            return;
        }
    }
}

cwBillboardId cwRenderSpriteBillboards::addBillboard(const SpriteBillboard& billboard)
{
    const cwBillboardId id = cwBillboardId{m_nextId++};
    m_billboards.emplace(id, billboard);
    update();
    return id;
}

void cwRenderSpriteBillboards::updateBillboard(cwBillboardId id, const SpriteBillboard& billboard)
{
    auto it = m_billboards.find(id);
    if (it == m_billboards.end() || it->second == billboard) {
        return;
    }
    it->second = billboard;
    update();
}

void cwRenderSpriteBillboards::removeBillboard(cwBillboardId id)
{
    if (m_billboards.erase(id) > 0) {
        update();
    }
}

bool cwRenderSpriteBillboards::hasBillboard(cwBillboardId id) const
{
    return m_billboards.find(id) != m_billboards.end();
}

int cwRenderSpriteBillboards::billboardCount() const
{
    return int(m_billboards.size());
}

QVector<cwRenderSpriteBillboards::Instance> cwRenderSpriteBillboards::buildInstances() const
{
    // With no slots there is no atlas and nothing draws, so the bound is moot.
    const int slotCount = spriteCount();
    const bool boundSlots = slotCount > 0;

    QVector<Instance> instances;
    instances.reserve(int(m_billboards.size()));
    for (const auto& [id, billboard] : m_billboards) {
        if (!billboard.visible) {
            continue;
        }
        if (boundSlots && (billboard.spriteIndex < 0 || billboard.spriteIndex >= slotCount)) {
            if (!m_warnedMissingSlot) {
                qWarning() << "cwRenderSpriteBillboards: billboard" << uint32_t(id) << "names sprite slot"
                           << billboard.spriteIndex << "outside the" << slotCount << "declared slots and is left out";
                m_warnedMissingSlot = true;
            }
            continue;
        }
        Instance instance;
        instance.worldPosition = billboard.worldPosition;
        instance.depthBias = billboard.depthBias;
        instance.opacity = billboard.opacity;
        instance.spriteIndex = uint32_t(billboard.spriteIndex);
        instances.append(instance);
    }
    return instances;
}

quintptr cwRenderSpriteBillboards::spriteRootNodeHandle(int slot) const
{
    if (slot < 0 || slot >= spriteCount()) {
        return 0;
    }
    const Slot& entry = m_slots.at(slot);
    if (entry.prototype.isNull() || !entry.subscene) {
        return 0;
    }
    quintptr handle = entry.subscene->rootNodeHandle();
    if (handle == 0) {
        // The viewer syncs ahead of the prototype on the frame that referenced
        // it, so build the prototype's nodes now (as QQuickShaderEffectSource
        // does for its source item) instead of waiting for another update.
        entry.subscene->forceSync();
        handle = entry.subscene->rootNodeHandle();
    }
    return handle;
}

cwRHIObject* cwRenderSpriteBillboards::createRHIObject()
{
    return new cwRHISpriteBillboards();
}
