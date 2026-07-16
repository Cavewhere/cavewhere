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
#include "cwBillboardHandle.h"

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

    // The billboard layer is owned by the scene, so a scene change means a
    // different layer: drop the old reference and (re)fetch from the new scene.
    m_renderBillboards = nullptr;
    ensureRenderBillboards();

    repositionBillboards();
    emit sceneChanged();
}

/**
 * @brief cwBillboardOverlayItem::ensureRenderBillboards
 *
 * Lazily fetches the scene-owned billboard layer once both the window (from the
 * scene graph) and the scene are available. The overlay's content renders as
 * depth-occluded billboards through it instead of as 2D overlay children.
 */
void cwBillboardOverlayItem::ensureRenderBillboards() {
    if(m_renderBillboards != nullptr) {
        return;
    }

    QQuickWindow* quickWindow = window();
    if(quickWindow == nullptr || m_scene.isNull()) {
        return;
    }

    // One billboard layer per scene, shared with every other overlay so all
    // billboards sort back-to-front together (#538).
    m_renderBillboards = m_scene->billboardLayer();
    m_renderBillboards->setWindow(quickWindow);
}

void cwBillboardOverlayItem::itemChange(ItemChange change, const ItemChangeData& value) {
    if(change == ItemSceneChange) {
        if(m_renderBillboards != nullptr) {
            // The view moved to a different window (value.window is null when it
            // left one); rebind the billboards and their subscenes to it.
            m_renderBillboards->setWindow(value.window);
        } else {
            ensureRenderBillboards();
        }
        repositionBillboards();
    }
    QQuickItem::itemChange(change, value);
}

void cwBillboardOverlayItem::requestBillboardRender() {
    if(m_renderBillboards != nullptr) {
        m_renderBillboards->update();
    }
}

void cwBillboardOverlayItem::setBillboard(cwBillboardHandle& handle, QQuickItem* content,
                                          const QVector3D& worldPosition, float depthBias) {
    if(m_renderBillboards == nullptr || content == nullptr) {
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
