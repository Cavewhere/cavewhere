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
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if(isVisible()) {
            repositionBillboards();
        }
    });
}

cwBillboardOverlayItem::~cwBillboardOverlayItem()
{
    // The scene's layer outlives this overlay; release the prototypes so the
    // layer drops its subscenes and textures of them.
    if(!m_spriteLayer.isNull()) {
        m_spriteLayer->setSprites({});
    }
}

QString cwBillboardOverlayItem::layerKey() const {
    return m_layerKey;
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

    repositionBillboards();
    emit cameraChanged();
}

cwScene* cwBillboardOverlayItem::scene() const {
    return m_scene;
}

void cwBillboardOverlayItem::setScene(cwScene* scene) {
    if(m_scene == scene) {
        return;
    }

    m_scene = scene;

    // The billboard layers are owned by the scene, so a scene change means
    // different layers: drop the old references. Each is fetched again from the
    // new scene the next time a billboard is pushed.
    m_renderBillboards = nullptr;
    m_renderSpriteBillboards = nullptr;
    m_renderTextBillboards = nullptr;

    pushSpritesToLayer();
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
        pushSpritesToLayer();
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

void cwBillboardOverlayItem::setTextBillboard(cwTextBillboardHandle& handle, const QString& text,
                                              const QVector3D& worldPosition, float depthBias) {
    if(m_renderTextBillboards == nullptr) {
        return;
    }

    cwRenderTextBillboards::TextBillboard billboard;
    billboard.text = text;
    billboard.worldPosition = worldPosition;
    billboard.depthBias = depthBias;

    // Adds on first call, updates after; the handle removes it on teardown.
    handle.set(m_renderTextBillboards, billboard);
}
