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
#include <QQuickItem>
#include <QQuickWindow>
#include <rhi/qrhi.h>

//Std includes
#include <cstddef>
#include <memory>
#include <utility>

namespace {

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
static_assert(kWorldPositionOffset == 0, "iWorldPosition is Float3 at offset 0");
static_assert(kDepthBiasOffset == 3 * sizeof(float), "iDepthBias follows the three position floats");
static_assert(kOpacityOffset == kDepthBiasOffset + sizeof(float), "iOpacity follows iDepthBias");
static_assert(sizeof(SpriteInstance) == kOpacityOffset + sizeof(float),
              "Instance must match the vertex input layout with no padding");

// The vertex shader's size-mode selector (a float in the std140 block).
constexpr float kSizeModeWorldSized = 0.0f;
constexpr float kSizeModeScreenConstant = 1.0f;

// Mirrors SpriteBlock in SpriteBillboard.vert.
struct SpriteBlock {
    float spriteSizePx[2];
    float sizeMode;
    float pad;
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

// Render-thread backend: one instanced draw of every visible billboard, sampling
// the prototype's layer texture.
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
            { 0, 2, QRhiVertexInputAttribute::Float, kOpacityOffset }          // iOpacity
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
        if (spriteSize != m_spriteSize || sizeMode != m_sizeMode) {
            m_spriteSize = spriteSize;
            m_sizeMode = sizeMode;
            m_spriteBlockDirty = true;
        }

        if (m_window.isNull() || !m_window->isSceneGraphInitialized()) {
            releaseLayerTexture();
            return;
        }

        const float devicePixelRatio = float(m_window->effectiveDevicePixelRatio());
        const quintptr rootNodeHandle = renderObject->spriteRootNodeHandle();

        if (rootNodeHandle == 0) {
            // The prototype has no nodes (no sprite, or no window); draw nothing
            // until they exist.
            releaseLayerTexture();
            return;
        }

        if (!m_layerTexture) {
            m_layerTexture = std::make_unique<cwQuickItemLayerTexture>(m_window, renderObject);
            if (!m_layerTexture->isValid()) {
                releaseLayerTexture();
                return;
            }
        }
        m_layerTexture->setRootNodeHandle(rootNodeHandle);
        m_layerTexture->setContentSize(m_spriteSize, devicePixelRatio);
    }

    void updateResources(const ResourceUpdateData& data) override
    {
        QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

        if (m_instancesDirty && m_instanceBuffer) {
            uploadPerInstance(m_instanceBuffer, batch, m_instances);
            m_instancesDirty = false;
        }

        if (m_spriteBlockDirty && m_spriteBlockBuffer) {
            SpriteBlock block;
            block.spriteSizePx[0] = float(m_spriteSize.width());
            block.spriteSizePx[1] = float(m_spriteSize.height());
            block.sizeMode = sizeModeSelector(m_sizeMode);
            block.pad = 0.0f;
            batch->updateDynamicBuffer(m_spriteBlockBuffer, 0, sizeof(SpriteBlock), &block);
            m_spriteBlockDirty = false;
        }
    }

    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override
    {
        if (context.renderPass != RenderPass::Overlay || m_instances.isEmpty() || !m_layerTexture) {
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

        // Gather runs before any pass opens, which is where the layer may record
        // its own pass to re-render a changed prototype.
        m_layerTexture->updateTexture();
        QRhiTexture* texture = m_layerTexture->rhiTexture();
        if (!texture) {
            return false;
        }

        if (!ensurePipeline(renderData, texture)) {
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
        releaseLayerTexture();
        delete m_sampler;
        m_sampler = nullptr;
    }

    // Destroying the layer frees the QRhiTexture the SRB samples, so the SRB
    // goes with it; the next gather rebuilds both.
    void releaseLayerTexture()
    {
        m_layerTexture.reset();
        delete m_srb;
        m_srb = nullptr;
        m_boundTexture = nullptr;
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

    // The SRB is rebuilt when the layer's texture pointer changed (a resize
    // allocates a fresh QRhiTexture) or when the pipeline layout changed.
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

    QPointer<QQuickWindow> m_window;
    std::unique_ptr<QObject> m_invalidationContext;
    std::unique_ptr<cwQuickItemLayerTexture> m_layerTexture;

    // The GUI snapshot and what has changed since the last upload.
    QVector<cwRenderSpriteBillboards::Instance> m_instances;
    QSizeF m_spriteSize;
    cwBillboardSizeMode m_sizeMode = cwBillboardSizeMode::ScreenConstant;
    bool m_instancesDirty = false;
    bool m_spriteBlockDirty = true;

    bool m_resourcesInitialized = false;
    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_instanceBuffer = nullptr;      // binding 0, one Instance per billboard
    QRhiBuffer* m_spriteBlockBuffer = nullptr;   // SpriteBlock at kSpriteBlockBinding
    QRhiSampler* m_sampler = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;
    QRhiTexture* m_boundTexture = nullptr;       // the texture m_srb samples; owned by the layer
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
    rebuildSubscene();
    update();
}

QQuickWindow* cwRenderSpriteBillboards::window() const
{
    return m_window;
}

void cwRenderSpriteBillboards::setSprite(QQuickItem* prototype)
{
    if (m_sprite == prototype) {
        return;
    }

    if (m_sprite) {
        disconnect(m_sprite, nullptr, this, nullptr);
    }
    m_sprite = prototype;
    rebuildSubscene();

    if (m_sprite) {
        // The sprite size is the prototype's size, snapshotted at synchronize, so
        // a resize needs a frame.
        connect(m_sprite, &QQuickItem::widthChanged, this, &cwRenderSpriteBillboards::update);
        connect(m_sprite, &QQuickItem::heightChanged, this, &cwRenderSpriteBillboards::update);
    }
    update();
}

QQuickItem* cwRenderSpriteBillboards::sprite() const
{
    return m_sprite;
}

QSizeF cwRenderSpriteBillboards::spriteSize() const
{
    if (m_sprite.isNull()) {
        return QSizeF();
    }
    return QSizeF(m_sprite->width(), m_sprite->height());
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

void cwRenderSpriteBillboards::rebuildSubscene()
{
    // Subscenes are bound to a window's scene graph and to one item.
    m_spriteSubscene.reset();
    if (m_sprite && m_window) {
        m_spriteSubscene = std::make_unique<cwQuickItemSubscene>(m_sprite, m_window);
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
    QVector<Instance> instances;
    instances.reserve(int(m_billboards.size()));
    for (const auto& [id, billboard] : m_billboards) {
        if (!billboard.visible) {
            continue;
        }
        Instance instance;
        instance.worldPosition = billboard.worldPosition;
        instance.depthBias = billboard.depthBias;
        instance.opacity = billboard.opacity;
        instances.append(instance);
    }
    return instances;
}

quintptr cwRenderSpriteBillboards::spriteRootNodeHandle() const
{
    if (!m_spriteSubscene) {
        return 0;
    }
    quintptr handle = m_spriteSubscene->rootNodeHandle();
    if (handle == 0) {
        // The viewer syncs ahead of the prototype on the frame that referenced
        // it, so build the prototype's nodes now (as QQuickShaderEffectSource
        // does for its source item) instead of waiting for another update.
        m_spriteSubscene->forceSync();
        handle = m_spriteSubscene->rootNodeHandle();
    }
    return handle;
}

cwRHIObject* cwRenderSpriteBillboards::createRHIObject()
{
    return new cwRHISpriteBillboards();
}
