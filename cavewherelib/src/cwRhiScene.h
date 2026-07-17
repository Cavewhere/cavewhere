#ifndef CWRHISCENE_H
#define CWRHISCENE_H

//Qt includes
#include <rhi/qrhi.h>
class QRhiCommandBuffer;

//Our includes
#include "cwRhiFrameRenderer.h"
#include <memory>
class cwScene;
class cwRenderObject;
class cwRhiItemRenderer;
class cwRhiOffscreenRenderer;

/**
 * @brief The cwRhiScene class
 *
 * The QQuick item backend for the scene. Bridges QQuickRhiItem (via
 * cwRhiItemRenderer) to the rendering engine: it reads the front-end cwScene each
 * sync, drives the per-frame orchestration, and manages the offscreen-render queue.
 *
 * The actual GPU draw engine — the render-object registry, pipeline cache, global
 * camera UBO, EDL composite, and per-pass draw primitives — lives in the composed
 * cwRhiFrameRenderer (m_frame). cwRhiOffscreenRenderer references that engine
 * directly; cwRhiItemRenderer (a friend) hands it to render objects via its
 * frameRenderer() accessor. Nothing reaches the engine through cwRhiScene's own
 * surface, so this class keeps no pipeline/UBO/camera forwards.
 */
class cwRhiScene {
public:
    friend class cwRhiItemRenderer;
    friend struct CwRhiSceneTestAccess;

    cwRhiScene();
    ~cwRhiScene();

private:
    void initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer);
    void synchroize(cwScene* scene, cwRhiItemRenderer* renderer);
    void render(QRhiCommandBuffer *cb, cwRhiItemRenderer* renderer);

    // Push a render object's live state into its registered cwRHIObject (visibility
    // + synchronize + queue a resource update). Shared by the Add and Update arms of
    // synchroize()'s drain.
    void syncRenderObject(cwRenderObject* object, cwRhiItemRenderer* renderer);

    // The shared GPU draw engine, composed by value. cwRhiItemRenderer (a friend)
    // exposes it to render objects via frameRenderer(). Declared before m_offscreen
    // so it outlives the offscreen renderer, which holds a reference to it.
    cwRhiFrameRenderer m_frame;

    // Drives the generic offscreen-render queue (queue + target + offscreen EDL +
    // read-back lifecycle). Owns none of the live frame; it renders the resident scene
    // through a reference to m_frame. Held by unique_ptr so this header needs only a
    // forward declaration of it; constructed in the ctor with m_frame and torn down
    // explicitly via shutdown() from ~cwRhiScene, before m_frame's pipeline cache is
    // freed (its targets' rpDescs key pipelines that must be evicted first).
    std::unique_ptr<cwRhiOffscreenRenderer> m_offscreen;
};


#endif // CWRHISCENE_H
