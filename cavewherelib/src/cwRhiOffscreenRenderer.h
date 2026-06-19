#ifndef CWRHIOFFSCREENRENDERER_H
#define CWRHIOFFSCREENRENDERER_H

//Qt includes
#include <rhi/qrhi.h>
#include <QList>
#include <QSize>

//Std includes
#include <memory>

//Our includes
#include "cwRhiFrameRenderer.h"   // for cwRhiFrameRenderer::EdlOffscreen and the engine reference
#include "cwOffscreenAtlasGrid.h"
class cwRhiItemRenderer;
class QThread;
struct cwOffscreenRenderParameters;
struct cwOffscreenRenderJob;

/**
 * @brief Drives the generic offscreen-render queue for cwRhiScene.
 *
 * Renders the resident scene from an arbitrary camera into an offscreen target and
 * reads it back into a QImage that fulfils each queued job's promise (hi-res map
 * export, offscreen point-cloud capture, the debug render hook). Render-thread-owned; held by
 * cwRhiScene as a unique_ptr member.
 *
 * It does not re-implement the draw pipeline: it renders the resident scene through
 * cwRhiFrameRenderer's shared primitives (gather / draw / pass routing / global UBO /
 * EDL builder), held by reference. cwRhiScene composes that engine and constructs
 * this renderer with it.
 */
class cwRhiOffscreenRenderer {
public:
    explicit cwRhiOffscreenRenderer(cwRhiFrameRenderer& frame);
    ~cwRhiOffscreenRenderer();

    // Single-owner render-thread type holding GPU resources, a back-ref to the
    // scene, and raw-owning read-back results. Copying or moving any of that would
    // duplicate or strand ownership, so forbid both explicitly.
    Q_DISABLE_COPY_MOVE(cwRhiOffscreenRenderer)

    // Move GUI-side jobs (handed over in cwRhiScene::synchroize while the GUI thread
    // is blocked at the scene-graph sync barrier) onto the render-thread queue,
    // clearing the source list. Render-thread only.
    void enqueue(QList<std::shared_ptr<cwOffscreenRenderJob>>& jobs);

    // True while there is queued work or an in-flight read-back. cwRhiScene::render
    // uses this to decide whether to drain and re-arm another frame.
    bool hasPendingWork() const;

    // Drain one frame's worth of queued work: peel a maximal compatible run off the front
    // and render it either as an atlas batch (one read-back fanned out to every tile) or,
    // for a solitary/incompatible job, the single-tile path. render() re-arms another frame
    // while hasPendingWork(), so the queue drains fully across frames. Render-thread only.
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

    // Colour-only atlas texture for the batch path: many compatible tiles are rendered
    // into the reused scratch (m_target) and copyTexture'd into their own sub-rect here,
    // then the whole atlas is read back once and sliced per job. It is never a render
    // target (no depth, no rpDesc) — purely a copyTexture destination + read-back source,
    // so it carries only UsedAsTransferSource. Cached and reused across frames for the
    // same grid size, like m_target.
    struct AtlasTarget {
        std::unique_ptr<QRhiTexture> color;
        QSize size;

        bool valid() const { return color != nullptr; }
    };

    // A read-back recorded but not yet completed. One read-back fans out to one or more
    // jobs (single-tile path: one; atlas path: one per tile). We track only weak refs to
    // those jobs plus the raw QRhiReadbackResult the completion lambda owns — the per-job
    // sub-rects live solely in that lambda, which does the slicing. The lambda holds the
    // only strong refs to every job, so a still-lockable job is the signal it has not run;
    // shutdown() finishes those promises and reclaims the result. Render-thread only.
    struct InflightOffscreenReadback {
        QRhiReadbackResult* result = nullptr;
        QList<std::weak_ptr<cwOffscreenRenderJob>> jobs;

        // True once the completion lambda has run. It owned the only strong refs to every
        // job and releases them together when destroyed, so they expire as one — checking
        // any job is enough. jobs is never empty (recordReadbackFanout asserts it), so this
        // never reports an unfired read-back as completed (which would leak its result).
        bool completed() const
        {
            return jobs.constFirst().expired();
        }
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

    // Build (or rebuild on size change) the colour-only batch atlas. Cheaper than
    // ensureTarget: no depth, no render-pass descriptor, so no pipeline eviction.
    void ensureAtlas(QRhi* rhi, QSize size);
    void destroyAtlas();

    // If the job at the front of the queue is non-renderable — null, cancelled, or
    // empty-sized — resolve it (finish its promise) and drop it, returning true. Returns
    // false when the front is a real render job (or the queue is empty). Callers loop on
    // it to skip past dead jobs without consuming frame budget.
    bool dropLeadingNonRenderable();

    // Draw one job's scene into the reused scratch (m_target) exactly as a standalone
    // render would — ensureTarget, EDL composite when a cloud is visible, pass routing,
    // gather, camera UBO, drawScene. @a size / @a sampleCount are the resolved
    // (capability-gated) render config. @a cameraSlot selects the global-UBO camera slot
    // this tile's camera is written to and drawn from (1 for a solitary render; 1+i for
    // tile i of a batch, so batched tiles don't share a slot). Returns false if the
    // scratch could not be allocated. Shared by the single-tile and atlas-batch paths.
    bool renderJobIntoScratch(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer,
                              const std::shared_ptr<cwOffscreenRenderJob>& job,
                              QSize size, int sampleCount, int cameraSlot);

    // Non-atlas fallback: render @a job into the scratch and read that scratch back into
    // the job's promise (one read-back, one slice). Used for solitary or incompatible
    // jobs, and when the atlas cannot be allocated.
    void renderSingleTile(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer,
                          const std::shared_ptr<cwOffscreenRenderJob>& job,
                          QSize size, int sampleCount);

    // Atlas path (B1): render each job in @a batch into the scratch and copyTexture it
    // into its @a grid sub-rect, then read the atlas back once and fan the slices out to
    // the jobs' promises. Falls back to renderSingleTile per job if the atlas can't be
    // allocated.
    void renderAtlasBatch(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer,
                          const QList<std::shared_ptr<cwOffscreenRenderJob>>& batch,
                          const cwOffscreenAtlasGrid& grid, int sampleCount);

    // Record a single texture read-back that fulfils every job in @a jobs from its
    // matching @a subRects slice on completion; tracks it for shutdown() fan-out. The
    // lists are parallel and equal length.
    void recordReadbackFanout(QRhiCommandBuffer* cb, QRhiTexture* readbackTexture,
                              const QList<std::shared_ptr<cwOffscreenRenderJob>>& jobs,
                              const QList<QRect>& subRects);

    // Per-frame appearance-slot recycle, run before this frame's batch acquires any
    // slots: for every appearance-slotted object, free the GPU resources orphaned by
    // a growth on a PRIOR frame (now submitted) and rewind its override-slot cursor.
    // This is the reliable reclaim site — a static object never runs updateResources,
    // so the flush can't live there (§8.1). Render-thread only.
    void recycleFrameAppearanceSlots();

    // Resolve a job's opaque per-object appearance overrides to transient slots:
    // acquire one slot per overridden cwAppearanceSlotted object, upload its payload
    // just-in-time on @a batch, and return the slot to stamp into that object's
    // GatherContext (cwSceneGatherOptions::appearanceSlotForObject). Objects without
    // an override — or whose back-end isn't appearance-slotted — are absent, so they
    // gather at the live slot 0.
    QHash<const cwRHIObject*, int> resolveAppearanceOverrides(
        QRhi* rhi, QRhiResourceUpdateBatch* batch,
        const cwOffscreenRenderParameters& parameters);

    cwRhiFrameRenderer& m_frame;   // the shared draw engine, owned by cwRhiScene

    bool m_didShutdown = false;   // guards shutdown() so the dtor backstop is a no-op

    // First render-thread to touch the queue/in-flight list; the reference for the
    // single-thread assertion (see assertRenderThread).
    QThread* m_renderThread = nullptr;

    QList<std::shared_ptr<cwOffscreenRenderJob>> m_queue;
    OffscreenTarget m_target;
    AtlasTarget m_atlas;
    // EDL composite buffers for the offscreen render, sized to the request and
    // always 1x (no MSAA on the offscreen path). Separate from the frame renderer's
    // live EDL offscreen because an offscreen request's size differs from the live
    // viewport and both are live within the same frame. Its effect composites into
    // m_target; its own EdlOffscreen::effectOutputRpDesc tracks the rpDesc that
    // effect was last initialized against so a target rebuild re-inits it.
    cwRhiFrameRenderer::EdlOffscreen m_edl;
    QList<InflightOffscreenReadback> m_inflightReadbacks;
    // Render-thread counter held by shared_ptr so a read-back completion lambda can
    // decrement it without capturing `this` (the renderer may be torn down before
    // the read-back fires).
    std::shared_ptr<int> m_outstandingReadbacks = std::make_shared<int>(0);
};

#endif // CWRHIOFFSCREENRENDERER_H
