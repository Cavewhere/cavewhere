/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwScene.h"
#include "cwRenderObject.h"
#include "cwRenderBillboards.h"
#include "cwDebug.h"
#include "cwRHIObject.h"
#include "cwCamera.h"
#include "cwGeometryItersecter.h"
#include "cwEDLSettings.h"
#include "cwOffscreenRenderParameters.h"
#include "cwOffscreenRenderJob.h"


//Qt includes
#include <rhi/qrhi.h>
#include <QThread>

cwScene::cwScene(QObject *parent) :
    QObject(parent),
    GeometryItersecter(new cwGeometryItersecter(this)),
    Camera(nullptr),
    m_edl(new cwEDLSettings(this))
{
    // An EDL tuning change marks the scene dirty and schedules a repaint;
    // cwRhiScene::synchroize() then pulls the new values across to the effect.
    connect(m_edl, &cwEDLSettings::changed, this, &cwScene::update);
}

cwScene::~cwScene()
{
    // Finish any offscreen jobs that were queued but never handed off to the
    // render thread (e.g. the scene is torn down before the next synchroize), so
    // their futures resolve rather than hang.
    for (const auto& job : std::as_const(m_pendingOffscreenJobs)) {
        if (job) {
            job->promise.finish();
        }
    }
}

/**
 * @brief cwScene::addItem
 * @param item
 *
 * Adds the item to be rendered
 */
void cwScene::addItem(cwRenderObject *item)
{
    // {Add} overwrites any entry pending for this id: an Update is subsumed (an Add
    // already implies a full synchronize), and a Delete left by a remove-before-sync
    // is cancelled. Because the same object re-added keeps its id, its still-mapped
    // cwRHIObject is freed and rebuilt by the register step rather than orphaned
    // (issue #512).
    m_pending.insert(item->renderObjectId(), {PendingOp::Add, item, m_pendingSequence++});
    item->setScene(this);
    item->setParent(this);
    update();
}

cwRenderBillboards* cwScene::billboardLayer()
{
    if (m_billboardLayer == nullptr) {
        m_billboardLayer = new cwRenderBillboards(this);
        // Registers the layer with this scene (and reparents it here via addItem).
        m_billboardLayer->setScene(this);
    }
    return m_billboardLayer;
}

/**
 * @brief cwScene::removeItem
 * @param item
 *
 * Removes the item to be rendered
 */
void cwScene::removeItem(cwRenderObject *item)
{
    if (item == nullptr) {
        return;
    }

    // Drop the picker's entries before the caller deletes `item`: the
    // intersecter keys geometry by raw cwRenderObject* and dereferences it on
    // every pick, so a Node outliving its render object is a use-after-free.
    // Here rather than ~cwRenderObject(), which is too late — callers arrive
    // via setScene(nullptr), so geometryItersecter() is already null by then.
    GeometryItersecter->clear(item);

    // {Delete, null} overwrites whatever was pending for this id and, holding no
    // pointer, cannot dangle when the caller deletes `item` next (issue #491). The
    // id is read while `item` is still alive so the next sync can drop the matching
    // lookup entry without ever dereferencing a freed pointer. The delete is
    // recorded unconditionally: because the entry names the transition, a quiescent
    // object — already synced, nothing pending — is no longer mistaken for "not in
    // the scene" and left to strand its cwRHIObject and GPU buffers (follow-up 7).
    m_pending.insert(item->renderObjectId(), {PendingOp::Delete, nullptr, m_pendingSequence++});
    update();
}

void cwScene::updateItem(cwRenderObject *item)
{
    // A pending Add already implies a full synchronize; a pending Delete must not be
    // resurrected into a dereference of the freed object; a pending Update is
    // idempotent. In every case the existing entry stands, so only a render object
    // with nothing pending needs a fresh Update queued.
    const cwRenderObjectId id = item->renderObjectId();
    if (m_pending.contains(id)) {
        return;
    }
    m_pending.insert(id, {PendingOp::Update, item, m_pendingSequence++});
    update();
}

/**
 * @brief cwScene::setCamera
 * @param camera
 */
void cwScene::setCamera(cwCamera *camera)
{
    if(Camera != camera) {

        if(Camera != nullptr) {
            Camera->disconnect(this);
        }

        Camera = camera;

        if(Camera != nullptr) {
            auto updateFlag = [this](cwSceneUpdate::Flag flag) {
                m_updateFlags |= flag;
                update();
            };

            connect(Camera, &cwCamera::devicePixelRatioChanged, this, [updateFlag]() {
                updateFlag(cwSceneUpdate::Flag::DevicePixelRatio);
            });
            connect(Camera, &cwCamera::viewMatrixChanged, this, [updateFlag]() {
                updateFlag(cwSceneUpdate::Flag::ViewMatrix);
            });
            connect(Camera, &cwCamera::projectionChanged, this, [updateFlag]() {
                updateFlag(cwSceneUpdate::Flag::ProjectionMatrix);
            });

            m_updateFlags = cwSceneUpdate::Flag::DevicePixelRatio
                            | cwSceneUpdate::Flag::ProjectionMatrix
                            | cwSceneUpdate::Flag::ViewMatrix;
        }

        emit cameraChanged();
    }
}

/**
 * @brief cwScene::camera
 * @return
 */
cwCamera *cwScene::camera() const
{
    return Camera;
}

/**
 * @brief cwScene::geometryItersecter
 * @return
 */
cwGeometryItersecter *cwScene::geometryItersecter() const
{
    return GeometryItersecter;
}

QBox3D cwScene::visibleFramingBounds() const
{
    return GeometryItersecter->visibleBoundingBox();
}

/**
 * @brief cwScene::update
 *
 * This will schedule the scene for rendering
 */
void cwScene::update()
{
    emit needsRendering();
}

int cwScene::pendingItemCount() const
{
    // Only entries that still hold a live pointer synchroize() will dereference —
    // Add and Update. A Delete holds none (the object is already freed), so it must
    // not count, or the #491 canary would never fall back to zero after a removal.
    int count = 0;
    for (const auto& change : std::as_const(m_pending)) {
        if (change.object != nullptr) {
            ++count;
        }
    }
    return count;
}

QFuture<QImage> cwScene::renderOffscreen(const cwOffscreenRenderParameters& parameters)
{
    // The queue handoff to the render thread is guarded only by the synchroize()
    // barrier, so this must run on the thread cwScene lives on. Fail loudly in
    // debug rather than silently corrupting the list from a worker thread.
    Q_ASSERT(QThread::currentThread() == thread());

    // cwScene owns the promise; the caller only ever sees the future. Mark the
    // computation started so the future reports state correctly, then hand the
    // job to the render thread (or a teardown path) to addResult()/finish().
    auto job = std::make_shared<cwOffscreenRenderJob>();
    job->parameters = parameters;
    job->promise.start();
    QFuture<QImage> future = job->promise.future();

    m_pendingOffscreenJobs.append(std::move(job));
    update();
    return future;
}


