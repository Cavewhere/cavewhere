/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderObject.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "cwGeometryItersecter.h"
#include "cwPickingLog.h"
#include "asyncfuture.h"

//Qt includes
#include <QFile>
#include <QAtomicInteger>

namespace {
    // Monotonic source of cwRenderObject ids. Atomic so ids stay unique even if a
    // render object is ever constructed off the main thread; relaxed ordering is
    // enough — we need value uniqueness, not any happens-before relationship.
    // Starts at 1 so 0 can read as "no render object".
    QAtomicInteger<quint64> g_nextRenderObjectId = 1;
}

cwRenderObject::cwRenderObject(QObject* parent) :
    QObject(parent),
    m_scene(nullptr),
    m_renderObjectId(static_cast<cwRenderObjectId>(g_nextRenderObjectId.fetchAndAddRelaxed(1)))
{
}

cwRenderObject::~cwRenderObject()
{
    // A render object may be deleted while still attached to a live scene (a direct
    // `delete`, not the setScene(nullptr) path). Scrub its pending entry and picker
    // geometry now, or the scene keeps a dangling pointer to drain (#491) and the
    // intersecter dereferences freed geometry on the next pick (follow-up 6). When
    // the scene is itself being torn down it has already called detachFromScene() on
    // us, so m_scene is null here and this no-ops — see ~cwScene().
    if (m_scene != nullptr) {
        m_scene->removeItem(this);
    }
}

void cwRenderObject::setScene(cwScene *scene)
{
    if(m_scene != scene) {
        if(m_scene != nullptr) {
            m_scene->removeItem(this);
        }

        m_scene = scene;

        if(m_scene != nullptr) {
            m_scene->addItem(this);
        }

        emit sceneChange();
    }
}

 cwCamera* cwRenderObject::camera() const {
    return m_scene == nullptr ? nullptr : m_scene->camera();
}

 void cwRenderObject::update()
 {
     if(m_scene == nullptr) { return; }
     m_scene->updateItem(this);
 }

/**
 * @brief cwRenderObject::geometryItersecter
 * @return The current geometry intersector
 */
 cwGeometryItersecter *cwRenderObject::geometryItersecter() const
{
    return m_scene == nullptr ? nullptr : m_scene->geometryItersecter();
}

cwSceneVisibility *cwRenderObject::sceneVisibility() const
{
    return m_scene == nullptr ? nullptr : m_scene->visibility();
}

void cwRenderObject::setVisible(bool newVisible)
{
    if (m_visible == newVisible)
        return;
    m_visible = newVisible;
    if (auto* visibility = sceneVisibility()) {
        visibility->setObjectVisible(m_renderObjectId, effectiveObjectVisible());
    }
    update();
    emit visibleChanged();
}

void cwRenderObject::updateVisibility()
{
    if (auto* visibility = sceneVisibility()) {
        visibility->setObjectVisible(m_renderObjectId, effectiveObjectVisible());
    }
}

bool cwRenderObject::effectiveObjectVisible() const
{
    // A single-key owner (point cloud, line plot) hides the whole object while
    // its one gate is armed; a multi-item owner leaves the object flag alone
    // and gates per sub-item instead.
    if (m_pickGateHidesObject && !m_armedPickGates.isEmpty()) {
        return false;
    }
    return m_visible;
}

void cwRenderObject::registerPickable(uint64_t subId, cwGeometry geometry,
                                      const QMatrix4x4& modelMatrix, float pickRadius)
{
    auto* intersecter = geometryItersecter();
    if (intersecter == nullptr) {
        // No scene wired yet — nothing to register or gate. The caller still
        // publishes identity visibility, and a re-register after scene attach
        // arms the gate. This log is the breadcrumb the #505 "geometry never
        // reaches the picker" investigations rely on.
        qCDebug(lcPick).nospace()
            << "registerPickable id=" << subId
            << " skipped: no geometryItersecter (scene wiring not ready)";
        return;
    }

    QFuture<void> pickReady = intersecter->addObject(
        cwGeometryItersecter::Object(this, subId, std::move(geometry), modelMatrix, pickRadius));
    if (pickReady.isFinished()) {
        // Already pickable — a same-key replacement of an already-published Key
        // (or a registration that didn't take). Don't hide; keep the gate open.
        m_armedPickGates.remove(subId);
        return;
    }
    if (m_armedPickGates.contains(subId)) {
        // Already hidden and observing an earlier pending registration for this
        // sub-id; that observation will release it.
        return;
    }
    m_armedPickGates.insert(subId);
    AsyncFuture::observe(pickReady).context(this, [this, subId]() {
        onPickReady(subId);
    });
}

void cwRenderObject::unregisterPickable(uint64_t subId)
{
    if (auto* intersecter = geometryItersecter()) {
        intersecter->removeObject(m_renderObjectId, subId);
    }
    // Drop the gate so a later re-register of the same Key (a fresh pending
    // future) re-arms. No visibility publish here: the caller republishes once
    // it has scrubbed any owner-specific store entry.
    m_armedPickGates.remove(subId);
}

void cwRenderObject::onPickReady(uint64_t subId)
{
    if (!m_armedPickGates.remove(subId)) {
        // Released meanwhile (the geometry was removed before it went ready).
        return;
    }
    updateVisibility();
    update();
}
