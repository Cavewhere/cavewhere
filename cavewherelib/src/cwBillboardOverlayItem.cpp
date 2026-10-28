/**************************************************************************
**
**    Copyright (C) 2024 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBillboardOverlayItem.h"
#include "cwScene.h"
#include "cwCamera.h"
#include "cwRenderBillboards.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"

//Qt includes
#include <QQuickWindow>

cwBillboardOverlayItem::cwBillboardOverlayItem(QQuickItem* parent) :
    cwBillboardOverlayItem(QString(), parent)
{
}

cwBillboardOverlayItem::cwBillboardOverlayItem(const QString& layerKey, QQuickItem* parent) :
    QQuickItem(parent),
    m_layerKey(layerKey)
{
    connect(this, &QQuickItem::visibleChanged, this, &cwBillboardOverlayItem::onVisibleChanged);
}

void cwBillboardOverlayItem::onVisibleChanged()
{
    if(isVisible()) {
        invalidateDeclutter();
        repositionBillboards();
    } else if(cwLabelDeclutter* declutter = labelDeclutter()) {
        // Later tiers refill the space this overlay gave up.
        declutter->schedulePass();
    }
}

cwBillboardOverlayItem::~cwBillboardOverlayItem()
{
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->removeParticipant(this);
        declutter->schedulePass();
    }

    // The scene's layer outlives this overlay; release the prototypes so the
    // layer drops its subscenes and textures of them.
    if(!m_spriteLayer.isNull()) {
        m_spriteLayer->setSprites({});
    }
}

QString cwBillboardOverlayItem::layerKey() const {
    return m_layerKey;
}

void cwBillboardOverlayItem::setLayerKey(const QString& layerKey) {
    if(m_layerKey == layerKey) {
        return;
    }

    // The old key's sprite layer drops its subscenes and textures of the
    // prototypes, which the new key's layer takes over.
    if(!m_spriteLayer.isNull()) {
        m_spriteLayer->setSprites({});
        m_spriteLayer = nullptr;
    }
    m_spritesEdited = true;

    m_layerKey = layerKey;
    m_renderSpriteBillboards = nullptr;
    m_renderTextBillboards = nullptr;
    m_extraTextLayers.clear();

    pushSpritesToLayer();
    invalidateDeclutter();
    repositionBillboards();
    emit layerKeyChanged();
}

QQmlListProperty<QQuickItem> cwBillboardOverlayItem::sprites() {
    return QQmlListProperty<QQuickItem>(this, nullptr,
                                        &cwBillboardOverlayItem::appendSprite,
                                        &cwBillboardOverlayItem::spriteCount,
                                        &cwBillboardOverlayItem::spriteAt,
                                        &cwBillboardOverlayItem::clearSprites);
}

void cwBillboardOverlayItem::appendSprite(QQmlListProperty<QQuickItem>* list, QQuickItem* sprite) {
    auto overlay = static_cast<cwBillboardOverlayItem*>(list->object);
    overlay->m_sprites.append(sprite);
    if(overlay->m_sprites.size() == 1 && sprite != nullptr) {
        connect(sprite, &QQuickItem::widthChanged, overlay, &cwBillboardOverlayItem::spriteSizeChanged);
        connect(sprite, &QQuickItem::heightChanged, overlay, &cwBillboardOverlayItem::spriteSizeChanged);
    }
    overlay->spritesEdited();
}

qsizetype cwBillboardOverlayItem::spriteCount(QQmlListProperty<QQuickItem>* list) {
    return static_cast<cwBillboardOverlayItem*>(list->object)->m_sprites.size();
}

QQuickItem* cwBillboardOverlayItem::spriteAt(QQmlListProperty<QQuickItem>* list, qsizetype index) {
    return static_cast<cwBillboardOverlayItem*>(list->object)->m_sprites.at(index);
}

void cwBillboardOverlayItem::clearSprites(QQmlListProperty<QQuickItem>* list) {
    auto overlay = static_cast<cwBillboardOverlayItem*>(list->object);
    if(!overlay->m_sprites.isEmpty() && !overlay->m_sprites.at(0).isNull()) {
        QQuickItem* sizeSource = overlay->m_sprites.at(0);
        disconnect(sizeSource, &QQuickItem::widthChanged, overlay, &cwBillboardOverlayItem::spriteSizeChanged);
        disconnect(sizeSource, &QQuickItem::heightChanged, overlay, &cwBillboardOverlayItem::spriteSizeChanged);
    }
    overlay->m_sprites.clear();
    overlay->spritesEdited();
}

void cwBillboardOverlayItem::spritesEdited() {
    m_spritesEdited = true;
    // QML assigns a list as a clear plus one append per item; while the
    // component is being created, componentComplete hands over the whole list once.
    if(!isComponentComplete()) {
        return;
    }
    pushSpritesToLayer();
    emit spritesChanged();
    emit spriteSizeChanged();
}

/**
 * @brief cwBillboardOverlayItem::spriteSize
 * @return Slot 0's size in logical pixels; empty without a slot.
 */
QSizeF cwBillboardOverlayItem::spriteSize() const {
    if(m_sprites.isEmpty() || m_sprites.at(0).isNull()) {
        return QSizeF();
    }
    const QQuickItem* sprite = m_sprites.at(0);
    return QSizeF(sprite->width(), sprite->height());
}

void cwBillboardOverlayItem::pushSpritesToLayer() {
    if(m_sprites.isEmpty() && m_spriteLayer.isNull()) {
        return;
    }

    cwRenderSpriteBillboards* layer = ensureRenderSpriteBillboards();
    if(layer == nullptr) {
        return;
    }

    if(m_spriteLayer == layer && !m_spritesEdited) {
        return;
    }

    if(!m_spriteLayer.isNull() && m_spriteLayer != layer) {
        m_spriteLayer->setSprites({});
    }
    m_spriteLayer = layer;
    m_spritesEdited = false;

    // A destroyed sprite drops out, so the layer sees a list without nulls.
    QList<QQuickItem*> sprites;
    sprites.reserve(m_sprites.size());
    for(const QPointer<QQuickItem>& sprite : std::as_const(m_sprites)) {
        if(!sprite.isNull()) {
            sprites.append(sprite.data());
        }
    }
    layer->setSprites(sprites);
}

void cwBillboardOverlayItem::componentComplete() {
    QQuickItem::componentComplete();
    spritesEdited();
}

cwCamera* cwBillboardOverlayItem::camera() const {
    return m_camera;
}

/**
 * @brief cwBillboardOverlayItem::setCamera
 *
 * The overlay reprojects its billboards on every camera change (view or
 * projection), so the camera's change signals drive repositionBillboards().
 * Disconnects the old camera first so reassigning the camera doesn't accumulate
 * connections and leave a stale camera still firing relayouts.
 */
void cwBillboardOverlayItem::setCamera(cwCamera* camera) {
    if(m_camera == camera) {
        return;
    }

    if(!m_camera.isNull()) {
        disconnect(m_camera, &cwCamera::viewMatrixChanged, this, nullptr);
        disconnect(m_camera, &cwCamera::projectionChanged, this, nullptr);
    }

    m_camera = camera;

    if(!m_camera.isNull()) {
        connect(m_camera, &cwCamera::viewMatrixChanged, this, &cwBillboardOverlayItem::repositionBillboards);
        connect(m_camera, &cwCamera::projectionChanged, this, &cwBillboardOverlayItem::repositionBillboards);
    }

    invalidateDeclutter();
    repositionBillboards();
    emit cameraChanged();
}

cwScene* cwBillboardOverlayItem::scene() const {
    return m_scene;
}

cwLabelDeclutter::Tier cwBillboardOverlayItem::declutterTier() const {
    return m_declutterTier;
}

void cwBillboardOverlayItem::setDeclutterTier(cwLabelDeclutter::Tier tier) {
    if(m_declutterTier == tier) {
        return;
    }

    const bool wasParticipating = labelDeclutter() != nullptr;
    leaveDeclutter();
    m_declutterTier = tier;
    joinDeclutter();
    emit declutterTierChanged();

    if(wasParticipating && labelDeclutter() == nullptr) {
        repositionBillboards();
    }
}

cwLabelDeclutter* cwBillboardOverlayItem::labelDeclutter() const {
    if(m_declutterTier == cwLabelDeclutter::NoTier || m_scene.isNull()) {
        return nullptr;
    }
    return m_scene->labelDeclutter();
}

void cwBillboardOverlayItem::joinDeclutter() {
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->addParticipant(this);
        declutter->schedulePass();
    }
}

void cwBillboardOverlayItem::leaveDeclutter() {
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->removeParticipant(this);
        declutter->schedulePass();
    }
}

void cwBillboardOverlayItem::invalidateDeclutter() {
    if(cwLabelDeclutter* declutter = labelDeclutter()) {
        declutter->invalidate();
    }
}

void cwBillboardOverlayItem::claimDeclutterSpace(cwCollisionRectGrid&) {
}

QSize cwBillboardOverlayItem::declutterCellSize() const {
    return QSize();
}

void cwBillboardOverlayItem::setScene(cwScene* scene) {
    if(m_scene == scene) {
        return;
    }

    leaveDeclutter();
    m_scene = scene;
    joinDeclutter();

    // The billboard layers are owned by the scene, so a scene change means
    // different layers: drop the old references. Each is fetched again from the
    // new scene the next time a billboard is pushed.
    m_renderBillboards = nullptr;
    m_renderSpriteBillboards = nullptr;
    m_renderTextBillboards = nullptr;
    m_extraTextLayers.clear();

    pushSpritesToLayer();
    invalidateDeclutter();
    repositionBillboards();
    emit sceneChanged();
}

/**
 * @brief cwBillboardOverlayItem::ensureRenderBillboards
 *
 * Lazily fetches the scene-owned item layer once both the window (from the
 * scene graph) and the scene are available. Called from setBillboard(), so only
 * an overlay that draws QQuickItem content creates the per-item renderers.
 */
cwRenderBillboards* cwBillboardOverlayItem::ensureRenderBillboards() {
    if(m_renderBillboards != nullptr) {
        return m_renderBillboards;
    }

    QQuickWindow* quickWindow = window();
    if(quickWindow == nullptr || m_scene.isNull()) {
        return nullptr;
    }

    // One item layer per scene, shared with every other overlay so all its
    // billboards sort back-to-front together (#538).
    m_renderBillboards = m_scene->billboardLayer();
    m_renderBillboards->setWindow(quickWindow);
    return m_renderBillboards;
}

void cwBillboardOverlayItem::itemChange(ItemChange change, const ItemChangeData& value) {
    if(change == ItemSceneChange) {
        // The view moved to a different window (value.window is null when it
        // left one); rebind the layers it has fetched to it. Layers it has yet
        // to fetch bind on their first use.
        if(m_renderBillboards != nullptr) {
            m_renderBillboards->setWindow(value.window);
        }
        if(m_renderSpriteBillboards != nullptr) {
            m_renderSpriteBillboards->setWindow(value.window);
        }
        if(m_renderTextBillboards != nullptr) {
            m_renderTextBillboards->setWindow(value.window);
        }
        for(const QPointer<cwRenderTextBillboards>& layer : std::as_const(m_extraTextLayers)) {
            if(!layer.isNull()) {
                layer->setWindow(value.window);
            }
        }
        pushSpritesToLayer();
        invalidateDeclutter();
        repositionBillboards();
    }
    QQuickItem::itemChange(change, value);
}

void cwBillboardOverlayItem::setBillboard(cwBillboardHandle& handle, QQuickItem* content,
                                          const QVector3D& worldPosition, float depthBias) {
    if(content == nullptr || ensureRenderBillboards() == nullptr) {
        return;
    }

    cwRenderBillboards::Billboard billboard;
    billboard.content = content;
    billboard.worldPosition = worldPosition;
    billboard.sizeMode = cwRenderBillboards::SizeMode::ScreenConstant;
    billboard.depthBias = depthBias;

    // Adds on first call, updates after; the handle removes it on teardown.
    handle.set(m_renderBillboards, billboard);
}

cwRenderSpriteBillboards* cwBillboardOverlayItem::ensureRenderSpriteBillboards() {
    if(m_renderSpriteBillboards != nullptr) {
        return m_renderSpriteBillboards;
    }

    QQuickWindow* quickWindow = window();
    if(quickWindow == nullptr || m_scene.isNull()) {
        return nullptr;
    }

    m_renderSpriteBillboards = m_scene->spriteBillboardLayer(m_layerKey);
    m_renderSpriteBillboards->setWindow(quickWindow);
    return m_renderSpriteBillboards;
}

void cwBillboardOverlayItem::setSpriteBillboard(cwSpriteBillboardHandle& handle,
                                                const QVector3D& worldPosition,
                                                float depthBias, float opacity, bool visible,
                                                int spriteIndex) {
    if(ensureRenderSpriteBillboards() == nullptr) {
        return;
    }

    cwRenderSpriteBillboards::SpriteBillboard billboard;
    billboard.worldPosition = worldPosition;
    billboard.depthBias = depthBias;
    billboard.opacity = opacity;
    billboard.visible = visible;
    billboard.spriteIndex = spriteIndex;

    // Adds on first call, updates after; the handle removes it on teardown.
    handle.set(m_renderSpriteBillboards, billboard);
}

cwRenderTextBillboards* cwBillboardOverlayItem::ensureRenderTextBillboards() {
    if(m_renderTextBillboards != nullptr) {
        return m_renderTextBillboards;
    }

    QQuickWindow* quickWindow = window();
    if(quickWindow == nullptr || m_scene.isNull()) {
        return nullptr;
    }

    m_renderTextBillboards = m_scene->textBillboardLayer(m_layerKey);
    m_renderTextBillboards->setWindow(quickWindow);
    return m_renderTextBillboards;
}

cwRenderTextBillboards* cwBillboardOverlayItem::ensureRenderTextBillboards(const QString& key) {
    if(key == m_layerKey) {
        return ensureRenderTextBillboards();
    }

    const QPointer<cwRenderTextBillboards> cached = m_extraTextLayers.value(key);
    if(!cached.isNull()) {
        return cached;
    }

    QQuickWindow* quickWindow = window();
    if(quickWindow == nullptr || m_scene.isNull()) {
        return nullptr;
    }

    cwRenderTextBillboards* layer = m_scene->textBillboardLayer(key);
    layer->setWindow(quickWindow);
    m_extraTextLayers.insert(key, layer);
    return layer;
}

void cwBillboardOverlayItem::setTextBillboard(cwRenderTextBillboards* layer, cwTextBillboardHandle& handle,
                                              const QString& text, const QVector3D& worldPosition,
                                              float depthBias, const QPointF& pixelOffset) {
    if(layer == nullptr) {
        return;
    }

    cwRenderTextBillboards::TextBillboard billboard;
    billboard.text = text;
    billboard.worldPosition = worldPosition;
    billboard.depthBias = depthBias;
    billboard.pixelOffset = pixelOffset;

    // Adds on first call, updates after; the handle removes it on teardown.
    handle.set(layer, billboard);
}

void cwBillboardOverlayItem::setTextBillboard(cwTextBillboardHandle& handle, const QString& text,
                                              const QVector3D& worldPosition, float depthBias,
                                              const QPointF& pixelOffset) {
    setTextBillboard(m_renderTextBillboards, handle, text, worldPosition, depthBias, pixelOffset);
}
