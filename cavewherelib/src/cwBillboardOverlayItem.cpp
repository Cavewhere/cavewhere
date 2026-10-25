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
    QQuickItem(parent)
{
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if(isVisible()) {
            repositionBillboards();
        }
    });
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

    m_renderSpriteBillboards = m_scene->spriteBillboardLayer();
    m_renderSpriteBillboards->setWindow(quickWindow);
    return m_renderSpriteBillboards;
}

void cwBillboardOverlayItem::setSpriteBillboard(cwSpriteBillboardHandle& handle,
                                                const QVector3D& worldPosition,
                                                float depthBias, float opacity, bool visible) {
    if(ensureRenderSpriteBillboards() == nullptr) {
        return;
    }

    cwRenderSpriteBillboards::SpriteBillboard billboard;
    billboard.worldPosition = worldPosition;
    billboard.depthBias = depthBias;
    billboard.opacity = opacity;
    billboard.visible = visible;

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

    m_renderTextBillboards = m_scene->textBillboardLayer();
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
