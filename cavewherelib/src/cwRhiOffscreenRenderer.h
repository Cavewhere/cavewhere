#ifndef CWRHIOFFSCREENRENDERER_H
#define CWRHIOFFSCREENRENDERER_H

//Qt includes
#include <rhi/qrhi.h>
#include <QList>
#include <QSize>

//Std includes
#include <memory>

//Our includes
#include "cwRhiScene.h"   // for cwRhiScene::EdlOffscreen and the friend back-ref
class cwRhiItemRenderer;
class QThread;
struct cwOffscreenRenderJob;

/**
 * @brief Drives the generic offscreen-render queue for cwRhiScene.
 *
 * Renders the resident scene from an arbitrary camera into an offscreen target and
 * reads it back into a QImage that fulfils each queued job's promise (hi-res map
 * export, the sink classifier, the debug render hook). Render-thread-owned; held by
 * cwRhiScene as a unique_ptr member.
 *
 * It does not re-implement the draw pipeline: it reaches cwRhiScene's shared
 * primitives (gather / draw / pass routing / global UBO / EDL builder) through a
 * back-pointer, granted by `friend class cwRhiOffscreenRenderer;` in cwRhiScene. See
 * the tripwire note at that friend declaration for when this coupling should be
 * promoted to a standalone cwRhiFrameRenderer that both compose.
 */
class cwRhiOffscreenRenderer {
public:
    explicit cwRhiOffscreenRenderer(cwRhiScene* scene);
    ~cwRhiOffscreenRenderer();

    // Single-owner render-thread type holding GPU resources, a back-ref to the
    // scene, and raw-owning read-back results. Copying or moving any of that would
    // duplicate or strand ownership, so forbid both explicitly.
    Q_DISABLE_COPY_MOVE(cwRhiOffscreenRenderer)

    // Renders drained per frame. Must be 1: every render targets the shared
    // m_target and the read-back resolves against it at frame end, so batching two
    // renders into one frame makes both read-backs return the last tile (the
    // repeated-tile bug). render() re-arms a frame while hasPendingWork(), so a
    // large batch still drains fully — one tile per frame. (Reintroducing real
    // batching would require a distinct read-back texture per job.) Public so tests
    // can assert the one-render-per-frame bound.
    static constexpr int kOffscreenRendersPerFrame = 1;

    // Move GUI-side jobs (handed over in cwRhiScene::synchroize while the GUI thread
    // is blocked at the scene-graph sync barrier) onto the render-thread queue,
    // clearing the source list. Render-thread only.
    void enqueue(QList<std::shared_ptr<cwOffscreenRenderJob>>& jobs);

    // True while there is queued work or an in-flight read-back. cwRhiScene::render
    // uses this to decide whether to drain and re-arm another frame.
    bool hasPendingWork() const;

    // Render one queued job (see kOffscreenRendersPerFrame): draw the resident
    // scene into the offscreen target from the job's camera and enqueue a texture
    // read-back that fulfils the job's promise on completion. Render-thread only.
    void drainPending(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer);

    // Finish any straggler promises (so their QFutures can't hang when the QRhi is
    // torn down mid-flight), reclaim the read-backs their completion lambdas would
    // have freed, then release the offscreen GPU resources. Called from ~cwRhiScene
    // BEFORE the pipeline cache is freed, because the offscreen targets' rpDescs key
    // pipelines that must be evicted first. Idempotent: the destructor calls it
    // again as a backstop, so cleanup can't be lost if the explicit call is ever
    // dropped, and a second call is a no-op.
    void shutdown();

private:
    // Generic offscreen render target: an RGBA8 colour + depth texture at the
    // requested output size and sample count. The resident scene renders here —
    // flat, or as the final composite target of the EDL path (m_edl) when a cloud is
    // visible. When sampleCount > 1 the colour is an MSAA texture resolved into a 1x
    // `resolve` texture at end-of-pass (so anti-aliased edges match the live view);
    // at 1x there is no resolve and `color` itself is the read-back source. The
    // read-back source carries UsedAsTransferSource (an MSAA texture cannot).
    struct OffscreenTarget {
        std::unique_ptr<QRhiTexture> color;     // render-target colour (MSAA when sampleCount > 1)
        std::unique_ptr<QRhiTexture> resolve;   // 1x resolve + read-back source; null at 1x
        std::unique_ptr<QRhiTexture> depth;
        std::unique_ptr<QRhiTextureRenderTarget> target;
        std::unique_ptr<QRhiRenderPassDescriptor> rpDesc;
        QSize size;
        int sampleCount = 1;

        bool valid() const { return target != nullptr; }
        // The texture read back into the QImage: the resolved 1x colour under MSAA,
        // else the (already 1x) colour itself.
        QRhiTexture* readbackTexture() const { return resolve ? resolve.get() : color.get(); }
    };

    // Read-backs recorded but not yet completed. Each entry pairs a weak ref to the
    // job with the raw QRhiReadbackResult the completion lambda owns. shutdown()
    // finishes any still-pending promise and reclaims the result the lambda would
    // otherwise have freed. A non-expired weak ref is the signal that the lambda has
    // not run yet — it holds the only other strong ref to the job — so shutdown
    // finishes/deletes only those entries. Render-thread only.
    struct InflightOffscreenReadback {
        std::weak_ptr<cwOffscreenRenderJob> job;
        QRhiReadbackResult* result = nullptr;
    };

    // The queue and in-flight read-back list are unsynchronized; their safety rests
    // on every mutator running on the one render thread (enqueue is reached from
    // cwRhiScene::synchroize while the GUI thread is blocked, drainPending from
    // render()). This records the first render-thread to touch them and asserts every
    // later touch matches — catching a future caller that wanders onto another thread.
    void assertRenderThread();

    // Build (or rebuild on output-size or sample-count change) the offscreen render
    // target. @a sampleCount is the already capability-gated effective count.
    void ensureTarget(QRhi* rhi, QSize size, int sampleCount);
    // Release the offscreen target's GPU resources, evicting pipelines keyed on its
    // render-pass descriptor first (same ordering rule as the EDL offscreen).
    void destroyTarget();

    cwRhiScene* m_scene;   // back-ref to the owner; not owned

    bool m_didShutdown = false;   // guards shutdown() so the dtor backstop is a no-op

    // First render-thread to touch the queue/in-flight list; the reference for the
    // single-thread assertion (see assertRenderThread).
    QThread* m_renderThread = nullptr;

    QList<std::shared_ptr<cwOffscreenRenderJob>> m_queue;
    OffscreenTarget m_target;
    // EDL composite buffers for the offscreen render, sized to the request and
    // always 1x (no MSAA on the offscreen path). Separate from cwRhiScene's live
    // m_edlOffscreen because an offscreen request's size differs from the live
    // viewport and both are live within the same frame. Its effect composites into
    // m_target; its own EdlOffscreen::effectOutputRpDesc tracks the rpDesc that
    // effect was last initialized against so a target rebuild re-inits it.
    cwRhiScene::EdlOffscreen m_edl;
    QList<InflightOffscreenReadback> m_inflightReadbacks;
    // Render-thread counter held by shared_ptr so a read-back completion lambda can
    // decrement it without capturing `this` (the renderer may be torn down before
    // the read-back fires).
    std::shared_ptr<int> m_outstandingReadbacks = std::make_shared<int>(0);
};

#endif // CWRHIOFFSCREENRENDERER_H
