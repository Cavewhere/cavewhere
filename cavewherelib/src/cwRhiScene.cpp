//Our includes
#include "cwRhiScene.h"
#include "cwRHIObject.h"
#include "cwRenderObject.h"
#include "cwRhiItemRenderer.h"
#include "cwScene.h"
#include "cwEDLSettings.h"
#include "cwCamera.h"
#include "cwOffscreenRenderJob.h"
#include "cwRhiOffscreenRenderer.h"

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

    //Add new rendering object
    if(!scene->m_newRenderObjects.isEmpty()) {
        for(auto object : std::as_const(scene->m_newRenderObjects)) {
            auto rhiObject = object->createRHIObject();
            // qDebug() << "Creating rhiObject:" << object << "->" << rhiObject;
            m_frame.registerRenderObject(object->renderObjectId(), rhiObject);
            scene->m_toUpdateRenderObjects.insert(object);
        }
        scene->m_newRenderObjects.clear();
    }

    //Remove old rendering objects
    if(!scene->m_toDeleteRenderObjects.isEmpty()) {
        for(cwRenderObjectId id : std::as_const(scene->m_toDeleteRenderObjects)) {
            m_frame.destroyRenderObject(id);
        }
        scene->m_toDeleteRenderObjects.clear();
    }

    //Update rendering objects
    for(auto object : std::as_const(scene->m_toUpdateRenderObjects)) {
        auto rhiObject = m_frame.renderObjectForId(object->renderObjectId());
        if(rhiObject) {
            rhiObject->setVisible(object->isVisible());
            rhiObject->synchronize({object, renderer});
            m_frame.markForResourceUpdate(rhiObject);
        }
    }
    scene->m_toUpdateRenderObjects.clear();

    // Move queued offscreen jobs across to the render thread (GUI is blocked during
    // synchronize, so the handoff is safe). render() drains them via m_offscreen.
    m_offscreen->enqueue(scene->m_pendingOffscreenJobs);
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
