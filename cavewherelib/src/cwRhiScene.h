#ifndef CWRHISCENE_H
#define CWRHISCENE_H

//Qt includes
#include <rhi/qrhi.h>
class QRhiCommandBuffer;

//Our includes
#include "cwRhiFrameRenderer.h"
#include "cwRHIObject.h"
#include "cwRhiPipelineTypes.h"
#include <functional>
#include <memory>
class cwScene;
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
 * cwRhiFrameRenderer (m_frame). cwRhiOffscreenRenderer renders the same resident
 * scene from arbitrary cameras and so references that engine directly. The
 * forwarding accessors below re-export the engine's pipeline/UBO/camera surface to
 * render objects (which reach this class via cwRhiItemRenderer::sceneBackend()) so
 * they need not know about the split.
 */
class cwRhiScene {
public:
    friend class cwRhiItemRenderer;
    friend struct CwRhiSceneTestAccess;

    cwRhiScene();
    ~cwRhiScene();

    // Forwarding accessors onto the composed frame renderer. Render objects reach
    // these through cwRhiItemRenderer::sceneBackend(); the bodies are one-line
    // delegations (inlined) so the split costs nothing on the hot path.
    QMatrix4x4 viewMatrix() const { return m_frame.viewMatrix(); }
    QMatrix4x4 projectionMatrix() const { return m_frame.projectionMatrix(); }
    QMatrix4x4 viewProjectionMatrix() const { return m_frame.viewProjectionMatrix(); }
    float devicePixelRatio() const { return m_frame.devicePixelRatio(); }
    QRhiBuffer* globalUniformBuffer() const { return m_frame.globalUniformBuffer(); }
    quint32 globalUniformBufferStride() const { return m_frame.globalUniformBufferStride(); }

    cwRhiPipelineRecord* acquirePipeline(const cwRhiPipelineKey& key,
                                    QRhi* rhi,
                                    const std::function<cwRhiPipelineRecord*(QRhi*)>& createFn) {
        return m_frame.acquirePipeline(key, rhi, createFn);
    }
    void releasePipeline(cwRhiPipelineRecord* record) { m_frame.releasePipeline(record); }
    QRhiSampler* sharedLinearClampSampler(QRhi* rhi) { return m_frame.sharedLinearClampSampler(rhi); }
    void evictPipelinesFor(QRhiRenderPassDescriptor* descriptor) {
        m_frame.evictPipelinesFor(descriptor);
    }
    QRhiRenderPassDescriptor* passRenderPassDescriptor(cwRHIObject::RenderPass pass) const {
        return m_frame.passRenderPassDescriptor(pass);
    }
    int passSampleCount(cwRHIObject::RenderPass pass) const { return m_frame.passSampleCount(pass); }
    const QHash<cwRhiPipelineKey, cwRhiPipelineRecord*>& pipelineCache() const {
        return m_frame.pipelineCache();
    }

private:
    void initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer);
    void synchroize(cwScene* scene, cwRhiItemRenderer* renderer);
    void render(QRhiCommandBuffer *cb, cwRhiItemRenderer* renderer);

    // The shared GPU draw engine, composed by value so the forwarding accessors above
    // inline. Declared before m_offscreen so it outlives the offscreen renderer, which
    // holds a reference to it.
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
