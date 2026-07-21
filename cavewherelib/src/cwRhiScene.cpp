//Our includes
#include "cwRhiScene.h"
#include "cwRHIObject.h"
#include "cwRenderObject.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "cwEDLSettings.h"
#include "cwCamera.h"
#include "cwOffscreenRenderJob.h"
#include "cwRhiOffscreenRenderer.h"

//Qt includes
#include <QVarLengthArray>

//Std includes
#include <algorithm>
#include <utility>

namespace {
    // Inline capacity for the per-sync drain buffer: the pending-change count between
    // two syncs is small in the steady state, so this keeps the copy off the heap
    // without bounding it (QVarLengthArray spills to the heap past this).
    constexpr int kTypicalPendingChanges = 16;
}

cwRhiScene::cwRhiScene()
    : m_offscreen(std::make_unique<cwRhiOffscreenRenderer>(m_frame))
{
}

cwRhiScene::~cwRhiScene()
{
    // Tear down the offscreen subsystem before m_frame: its shutdown() destroys the
    // offscreen target + EDL, which evict pipelines keyed on their rpDescs (must
    // happen while m_frame's pipeline cache still exists), and finishes any straggler
    // promises so their futures don't hang. m_frame then runs its own teardown
    // (objects, UBO, live EDL, pipeline cache, sampler) as it destructs.
    m_offscreen->shutdown();
    m_offscreen.reset();
}

void cwRhiScene::initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    Q_UNUSED(renderer);
    m_frame.initialize(cb);
}

void cwRhiScene::synchroize(cwScene *scene, cwRhiItemRenderer* renderer)
{
    m_frame.setUpdateFlags(scene->m_updateFlags);
    scene->m_updateFlags = cwSceneUpdate::Flag::None;

    //Update camera uniform buffer (the frame gates each value on its update flags)
    if(scene->camera()) {
        m_frame.syncCameraValues(scene->camera()->devicePixelRatio(),
                                 scene->camera()->viewMatrix(),
                                 scene->camera()->projectionMatrix());
    }

    m_frame.setEdlParameters(scene->edl()->parameters());

    // One visibility snapshot per sync: the GUI thread is blocked at this
    // barrier, so reading the store here is race-free, and everything the frame
    // renders — live or offscreen — reads this immutable copy until the next
    // sync. This replaces the per-object visibility mirrors the RHI side used
    // to hand-sync (cwRHIObject::setVisible, per-item flags, the line-plot
    // mask copy).
    m_frame.setVisibilitySnapshot(scene->visibility()->snapshot());

    //Drain the pending render-object changes. One entry per id names its transition
    //(Add/Update/Delete), so this is a single ordered pass rather than three queues.
    //The map is ordered by id — i.e. construction order — but Adds must register in
    //*add* order or draw order changes (gatherScene bakes registration order into
    //the sort key), so drain by the queued sequence. Deletes/Updates are order-
    //independent, so one sorted pass over everything is correct.
    if(!scene->m_pending.isEmpty()) {
        QVarLengthArray<std::pair<cwRenderObjectId, cwScene::PendingChange>, kTypicalPendingChanges> changes;
        changes.reserve(scene->m_pending.size());
        for(auto it = scene->m_pending.cbegin(); it != scene->m_pending.cend(); ++it) {
            changes.append({it.key(), it.value()});
        }
        scene->m_pending.clear();

        std::sort(changes.begin(), changes.end(),
                  [](const auto& a, const auto& b) {
                      return a.second.sequence < b.second.sequence;
                  });

        for(const auto& [id, change] : changes) {
            switch(change.op) {
            case cwScene::PendingOp::Add: {
                auto rhiObject = change.object->createRHIObject();
                m_frame.registerRenderObject(id, rhiObject);
                //An Add implies a first synchronize, exactly as a following Update
                //would — do it inline so the new object never renders empty a frame.
                syncRenderObject(change.object, renderer);
                break;
            }
            case cwScene::PendingOp::Update:
                syncRenderObject(change.object, renderer);
                break;
            case cwScene::PendingOp::Delete:
                m_frame.destroyRenderObject(id);
                break;
            }
        }
    }

    // Move queued offscreen jobs across to the render thread (GUI is blocked during
    // synchronize, so the handoff is safe). render() drains them via m_offscreen.
    m_offscreen->enqueue(scene->m_pendingOffscreenJobs);
}

void cwRhiScene::syncRenderObject(cwRenderObject* object, cwRhiItemRenderer* renderer)
{
    auto rhiObject = m_frame.renderObjectForId(object->renderObjectId());
    if(rhiObject) {
        rhiObject->synchronize({object, renderer});
        m_frame.markForResourceUpdate(rhiObject);
    }
}

void cwRhiScene::render(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer)
{
    m_frame.renderLiveFrame(cb, renderer);

    // Offscreen renders ride after the live frame's passes (the live swap-chain
    // pass has already ended, so the user's view is fully drawn and untouched).
    // Zero cost when nothing is queued and no read-back is in flight. When work
    // remains, request another frame so it drains across frames and the pending
    // texture read-backs get a chance to complete.
    if (m_offscreen->hasPendingWork()) {
        m_offscreen->drainPending(cb, renderer);
        if (renderer) {
            renderer->requestUpdate();
        }
    }
}
