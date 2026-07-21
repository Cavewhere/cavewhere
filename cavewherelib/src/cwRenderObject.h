/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBJECT_H
#define CWGLOBJECT_H

//Qt includes
#include <QObject>
#include <QOpenGLFunctions>
#include <QPointer>
#include <QSet>
#include <QFuture>
#include <QMatrix4x4>
class QOpenGLShaderProgram;

//Our includes
class cwCamera;
class cwShaderDebugger;
class cwRhiViewer;
class cwScene;
class cwUpdateDataCommand;
class cwRHIObject;
class cwRhiItemRenderer;
class cwSceneVisibility;
class cwGeometryItersecter;
class cwGeometry;
#include "cwScene.h"
#include "cwRenderObjectId.h"
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwRenderObject : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)

    friend class cwRhiScene;
    friend class cwScene;

public:
    cwRenderObject(QObject* parent = nullptr);
    ~cwRenderObject();

    void setScene(cwScene *scene);
    cwScene *scene() const;

    // Stable, process-unique identity assigned at construction. Used instead of
    // the raw `this` pointer to correlate a render object with its cwRHIObject
    // across the cwScene / cwRhiScene queues: a freed object's address can be
    // reused by a new object, but ids never repeat (issue #512).
    cwRenderObjectId renderObjectId() const { return m_renderObjectId; }

    cwGeometryItersecter* geometryItersecter() const;

    // The scene's visibility store, or null before scene attach. Facade
    // setters publish through this; pre-attach state lands via
    // updateVisibility() when cwScene::addItem seeds the store.
    cwSceneVisibility* sceneVisibility() const;

    cwCamera* camera() const;

    void update();

    bool isVisible() const;
    void setVisible(bool newVisible);

signals:
    void sceneChange();

    void visibleChanged();

protected:
    virtual cwRHIObject* createRHIObject() { return nullptr; }
    // virtual cwSceneUpdate::Flag updateOnFlags() const { return cwSceneUpdate::Flag::None; }

    // Publish this object's full authoring visibility state into the scene's
    // store. cwScene::addItem calls it at attach, making it the single seed
    // path for state set before the scene was wired. Idempotent — republishing
    // an unchanged state is a store no-op. Overrides publish their sub-item /
    // mask state on top of the base's object flag.
    virtual void updateVisibility();

    // Register geometry with the scene's intersecter for picking and manage its
    // pick-ready gate (issue #505 Phase 4, plans/PICKER_MUTATION_PIPELINE_PLAN.html).
    // These two are the only path a render object hands geometry to, or takes
    // it back from, the intersecter — the gate machinery is entirely private.
    //
    // Geometry is not pickable until its sub-BVH publishes, so registerPickable
    // keeps object.key().id render-hidden across that window and drops the hide
    // once the intersecter reports it ready — upholding "rendered ⇒ pickable".
    // First-publish-only is free: the intersecter returns an already-finished
    // readiness future for a same-key replacement of a published Key, so a
    // geometry edit keeps the old geometry shown (eventual consistency) instead
    // of blinking. unregisterPickable removes the geometry and drops the gate so
    // a later re-register of the same Key re-arms.
    //
    // Neither method publishes visibility: the caller calls updateVisibility()
    // once its full state (per-item flags, line masks) is assembled, because
    // that is identity state which must publish even with no geometry registered
    // (e.g. a born-hidden item, issue #579). Removal-specific store scrubbing
    // (removeSub) stays with the multi-item owner that knows its sub entries.
    //
    // subPickGateOpen: for multi-item owners (textured items) to AND into the
    //   per-sub-item visibility they publish in updateVisibility().
    // setPickGateHidesObject: single-key owners (point cloud, line plot) call
    //   this once so an armed gate hides the whole object at the object level.
    void registerPickable(uint64_t subId, cwGeometry geometry,
                          const QMatrix4x4& modelMatrix = QMatrix4x4(),
                          float pickRadius = 0.0f);
    void unregisterPickable(uint64_t subId);
    bool subPickGateOpen(uint64_t subId) const { return !m_armedPickGates.contains(subId); }
    void setPickGateHidesObject(bool hides) { m_pickGateHidesObject = hides; }

private:
    // Release a single gate once its readiness future finished; republishes
    // this object's visibility so the now-pickable geometry becomes visible.
    void onPickReady(uint64_t subId);

    // Object-level visibility to publish: authoring visibility ANDed with the
    // object-level gate (only for single-key owners; see setPickGateHidesObject).
    bool effectiveObjectVisible() const;

    // Sever the back-pointer to the scene without touching it. cwScene calls this on
    // each render-object child while destructing, so that when ~QObject later deletes
    // those children, ~cwRenderObject sees a null scene and skips the removeItem()
    // that would otherwise reach into the half-destroyed scene (see ~cwScene()).
    void detachFromScene() { m_scene = nullptr; }

    cwScene* m_scene;

    const cwRenderObjectId m_renderObjectId;

    bool m_visible = true;

    // Pick-ready gate state (see registerPickable). m_armedPickGates holds the
    // sub-ids currently hidden while their readiness future is pending; each
    // entry is dropped when that future finishes (onPickReady) or the geometry
    // is removed (unregisterPickable). First-publish-only lives in the
    // intersecter now — it hands back an already-finished future for a
    // republished Key — so the owner keeps no "has published" record.
    QSet<uint64_t> m_armedPickGates;
    bool m_pickGateHidesObject = false;
};



/**
 * @brief cwRenderObject::scene
 * @returns The scene that is resposible for this object
 */
inline cwScene *cwRenderObject::scene() const
{
    return m_scene;
}

inline bool cwRenderObject::isVisible() const
{
    return m_visible;
}




#endif // CWGLOBJECT_H
