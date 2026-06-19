/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCENE_H
#define CWSCENE_H

//Qt includes
#include <QObject>
#include <QList>
#include <QQueue>
#include <QQmlEngine>
#include <QHash>
#include <QFuture>
#include <QImage>

//Std includes
#include <memory>

//Our includes
#include "cwSceneUpdate.h"
#include "cwRenderObjectId.h"
#include "CaveWhereLibExport.h"
class cwRenderObject;
class cwRenderBillboards;
class cwCamera;
class cwShaderDebugger;
class cwSceneCommand;
class cwGeometryItersecter;
class cwRhiItemRenderer;
class cwEDLSettings;
struct cwOffscreenRenderParameters;
struct cwOffscreenRenderJob;


/**
 * @brief The cwScene class
 */
class CAVEWHERE_LIB_EXPORT cwScene : public QObject
{
    friend class cwRhiScene;

    Q_OBJECT
    QML_NAMED_ELEMENT(Scene)

    // Live Eye-Dome Lighting tuning. Pulled by cwRhiScene::synchroize() and
    // pushed to the render-thread EDL effect (see cwEDLSettings / EDL.frag).
    Q_PROPERTY(cwEDLSettings* edl READ edl CONSTANT)

public:
    explicit cwScene(QObject *parent = 0);
    virtual ~cwScene();

    cwEDLSettings* edl() const { return m_edl; }

    void addItem(cwRenderObject* item);
    void removeItem(cwRenderObject* item);
    void updateItem(cwRenderObject* item);

    // The one billboard layer shared by every overlay in this scene (leads,
    // station labels, future overlays). Lazily created and parented to the scene
    // the first time it's requested. Sharing one layer means all billboards sort
    // back-to-front together, so an overlapping lead and label can't bite each
    // other (#538). Callers still drive its window via setWindow(); the scene
    // doesn't know the hosting QQuickWindow. Invariant: every overlay sharing
    // this layer must live in the same QQuickWindow — they each push setWindow()
    // (last writer wins), which stays a no-op only while they agree.
    cwRenderBillboards* billboardLayer();

    void setCamera(cwCamera* camera);
    cwCamera *camera() const;

    //For doing intersection tests
    cwGeometryItersecter* geometryItersecter() const;

    void update();

    void releaseResources();

    // Live pointers held in queues that cwRhiScene::synchroize() dereferences.
    int pendingItemCount() const;

    // Render the resident scene from an arbitrary camera into an offscreen image,
    // returning a QFuture that resolves to the result. A GUI-thread consumer
    // (hi-res map export, sink classifier) supplies pure parameters (camera
    // matrices + output size + clear colour + DPR); cwScene owns the QPromise
    // internally and hands back only the read-only future, so the caller can
    // observe or cancel the result but never resolve it. cwRhiScene::synchroize()
    // drains the queued job onto the render thread, which renders into an
    // offscreen target and fulfils the promise from a texture read-back; update()
    // is called so a frame runs and the queue drains. Cancelling the future before
    // the render thread reaches the job skips the GPU work.
    //
    // Must be called on the thread cwScene lives on (the GUI thread). The queue
    // is a bare QList handed to the render thread with no mutex; its safety rests
    // entirely on synchroize() draining it while the GUI thread is blocked at the
    // scene-graph sync barrier. An off-thread caller would race that drain and
    // corrupt the list, so a debug build asserts the calling thread. Consumers on
    // a worker thread must marshal here via a queued connection first.
    QFuture<QImage> renderOffscreen(const cwOffscreenRenderParameters& parameters);

signals:
    void cameraChanged();
    void needsRendering();

private:
    //Items to render
    QList<cwRenderObject*> m_newRenderObjects;
    // Render-object ids (not pointers) queued for delete. The render object is
    // already deleted by the caller by the time synchroize() drains this, so we
    // key on the stable id — a reused address must not masquerade as a still-live
    // entry (issue #512).
    QList<cwRenderObjectId> m_toDeleteRenderObjects;
    QSet<cwRenderObject*> m_toUpdateRenderObjects;

    //For interaction
    cwGeometryItersecter* GeometryItersecter;

    //The main camera for the viewer
    cwCamera* Camera;

    //cwSceneUpdate::Flag flags
    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;

    // Live EDL tuning; owned here, read by cwRhiScene::synchroize().
    cwEDLSettings* m_edl;

    // The shared billboard overlay layer (see billboardLayer()); a render object
    // parented to the scene, created on first request.
    cwRenderBillboards* m_billboardLayer = nullptr;

    // Offscreen render requests queued from the GUI thread, drained onto the
    // render thread by cwRhiScene::synchroize() (a friend). shared_ptr so the
    // promise survives the handoff and the async read-back completion.
    QList<std::shared_ptr<cwOffscreenRenderJob>> m_pendingOffscreenJobs;

};




#endif // CWSCENE_H
