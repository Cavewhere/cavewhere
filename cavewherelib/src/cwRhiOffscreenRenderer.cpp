//Our includes
#include "cwRhiOffscreenRenderer.h"
#include "cwRhiFrameRenderer.h"
#include "cwRHIObject.h"
#include "cwAppearanceSlotted.h"
#include "cwAppearanceOverride.h"
#include "cwRhiItemRenderer.h"
#include "cwEDLEffect.h"
#include "cwOffscreenRenderJob.h"

//Qt includes
#include <rhi/qrhi.h>
#include <QThread>

//Std includes
#include <array>
#include <cstring>

namespace {
    // Atlas-batch tuning (see plans/OFFSCREEN_ATLAS_BATCHING_PLAN §5). The grid is the
    // smallest of: the backend's TextureSizeMax, this byte budget (atlas colour + the one
    // scratch's colour+depth), and the per-frame tile cap. The tile cap is
    // cwRhiFrameRenderer::kOffscreenBatchCameraSlots — the UBO camera-slot count is the real
    // per-frame bound (one slot per batched tile) and doubles as the GPU-work cap that
    // keeps a single command buffer from stalling long enough to trip a watchdog (the
    // Phase 0 spike ran 256 passes + copies in ~20 ms safely).
    constexpr qint64 kAtlasByteBudget = qint64(128) * 1024 * 1024;   // 128 MB
    // The scratch carries a depth texture; preferredDepthFormat is 24- or 32-bit, so 4
    // bytes/pixel is the budget term (a small over-estimate for a 16-bit depth).
    constexpr int kScratchDepthBytesPerPixel = 4;
    // Global-UBO camera slot 0 is the live on-screen camera; the offscreen cameras start at
    // slot 1 (a solitary render uses it; batch tile i uses kFirstOffscreenCameraSlot + i).
    constexpr int kFirstOffscreenCameraSlot = 1;
    // Bytes per pixel of the read-back source; always RGBA8 (the scratch colour/resolve and
    // the atlas alike), enforced by the format guard in readbackView.
    constexpr int kRgba8BytesPerPixel = 4;

    // Wrap a completed RGBA8 read-back as a non-owning QImage over its transient buffer,
    // or a null image on a short/empty/wrong-format result. The buffer (held by the
    // QRhiReadbackResult) must outlive every use of the returned view.
    QImage readbackView(const QRhiReadbackResult& readback)
    {
        const QSize pixelSize = readback.pixelSize;
        const qsizetype expectedBytes =
            qsizetype(pixelSize.width()) * pixelSize.height() * kRgba8BytesPerPixel;
        if (readback.format != QRhiTexture::RGBA8 || pixelSize.isEmpty()
            || readback.data.size() < expectedBytes) {
            return QImage();
        }
        return QImage(reinterpret_cast<const uchar*>(readback.data.constData()),
                      pixelSize.width(), pixelSize.height(), QImage::Format_RGBA8888);
    }
}

cwRhiOffscreenRenderer::cwRhiOffscreenRenderer(cwRhiFrameRenderer& frame) :
    m_frame(frame)
{
}

cwRhiOffscreenRenderer::~cwRhiOffscreenRenderer()
{
    // Backstop: ~cwRhiScene calls shutdown() explicitly (while the scene is still
    // valid) so this is normally a guarded no-op, but it guarantees GPU resources
    // and stranded promises are reclaimed even if that explicit call is ever lost.
    shutdown();
}

void cwRhiOffscreenRenderer::assertRenderThread()
{
    QThread* current = QThread::currentThread();
    if (!m_renderThread) {
        m_renderThread = current; // first render-thread touch sets the reference
    }
    Q_ASSERT(current == m_renderThread);
}

void cwRhiOffscreenRenderer::enqueue(QList<std::shared_ptr<cwOffscreenRenderJob>>& jobs)
{
    assertRenderThread();
    if (jobs.isEmpty()) {
        return;
    }
    for (auto& job : jobs) {
        m_queue.append(std::move(job));
    }
    jobs.clear();
}

bool cwRhiOffscreenRenderer::hasPendingWork() const
{
    return !m_queue.isEmpty() || *m_outstandingReadbacks > 0;
}

void cwRhiOffscreenRenderer::shutdown()
{
    if (m_didShutdown) {
        return; // already torn down (the dtor backstop, or a repeated call)
    }
    m_didShutdown = true;

    // Drop any unstarted jobs; destroying the promises finishes their futures so
    // consumers don't hang.
    m_queue.clear();

    // Read-backs recorded but not yet completed will never fire now the QRhi is
    // going away, so their completion lambdas (the only other owner) would leave the
    // promise unfinished and the QFuture hung, and the QRhiReadbackResult they own
    // leaked. A lambda that already ran released its holder (weak ref expired) and
    // freed its result, so skip those — no double finish, no double free. A live weak
    // ref means the lambda has not run: finish the promise and reclaim the read-back
    // it would have deleted.
    for (const auto& inflight : std::as_const(m_inflightReadbacks)) {
        bool lambdaPending = false;
        for (const auto& weakJob : inflight.jobs) {
            if (auto job = weakJob.lock()) {
                job->promise.finish();
                lambdaPending = true; // a lockable job means the lambda has not run
            }
        }
        // Only a not-yet-fired read-back still owns its result; a completed one's lambda
        // already freed it (and all its jobs have expired, so we took none of them).
        if (lambdaPending) {
            delete inflight.result;
        }
    }
    m_inflightReadbacks.clear();
    // The reclaimed lambdas will never run their `--(*counter)`, so the counter would stay
    // elevated forever; zero it so hasPendingWork() reflects the now-empty state.
    *m_outstandingReadbacks = 0;

    // Tear down the offscreen targets before the scene's pipeline cache (caller
    // ordering) so pipelines keyed on their rpDescs are released first.
    m_frame.destroyEdlOffscreen(m_edl);
    destroyTarget();
    destroyAtlas();
}

void cwRhiOffscreenRenderer::ensureTarget(QRhi* rhi, QSize size, int sampleCount)
{
    if (!rhi || size.isEmpty()) {
        return;
    }
    if (m_target.valid() && m_target.size == size && m_target.sampleCount == sampleCount) {
        return;
    }

    destroyTarget();

    OffscreenTarget fresh;
    fresh.size = size;
    fresh.sampleCount = sampleCount;

    const bool msaa = sampleCount > 1;

    // Under MSAA the colour is a multisample texture (can't be a transfer source)
    // resolved into a separate 1x `resolve` texture that carries the transfer-source
    // flag for read-back; at 1x the colour is itself the 1x read-back source.
    if (msaa) {
        fresh.color.reset(rhi->newTexture(QRhiTexture::RGBA8, size, sampleCount,
                                          QRhiTexture::RenderTarget));
        fresh.resolve.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                            QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    } else {
        fresh.color.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                                          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    }
    fresh.depth.reset(rhi->newTexture(cwRhiFrameRenderer::preferredDepthFormat(rhi), size, sampleCount,
                                      QRhiTexture::RenderTarget));
    if (!fresh.color->create() || !fresh.depth->create()
        || (fresh.resolve && !fresh.resolve->create())) {
        return; // fresh frees its partial resources on scope exit
    }

    // A resolve texture on the colour attachment makes end-of-pass resolve the MSAA
    // samples into it (the live path relies on the swap chain's own resolve instead).
    QRhiColorAttachment colorAttachment(fresh.color.get());
    if (fresh.resolve) {
        colorAttachment.setResolveTexture(fresh.resolve.get());
    }
    QRhiTextureRenderTargetDescription desc;
    desc.setColorAttachments({ colorAttachment });
    desc.setDepthTexture(fresh.depth.get());
    fresh.target.reset(rhi->newTextureRenderTarget(desc));
    if (!fresh.target) {
        return; // fresh frees its partial resources on scope exit
    }
    fresh.rpDesc.reset(fresh.target->newCompatibleRenderPassDescriptor());
    if (!fresh.rpDesc) {
        return;
    }
    fresh.target->setRenderPassDescriptor(fresh.rpDesc.get());
    if (!fresh.target->create()) {
        return;
    }

    m_target = std::move(fresh);
}

void cwRhiOffscreenRenderer::destroyTarget()
{
    // Evict pipelines keyed on the rpDesc before deleting it — a newly allocated
    // rpDesc may reuse the address and hash-collide with a stale cache entry.
    if (m_target.rpDesc) {
        m_frame.evictPipelinesFor(m_target.rpDesc.get());
    }
    m_target.target.reset();
    m_target.rpDesc.reset();
    m_target.color.reset();
    m_target.resolve.reset();
    m_target.depth.reset();
    m_target.size = QSize();

    // The offscreen EDL effect's pipeline was built against the now-freed target
    // rpDesc; a rebuild may reuse the address, so force a re-init by forgetting it.
    m_edl.effectOutputRpDesc = nullptr;
}

void cwRhiOffscreenRenderer::ensureAtlas(QRhi* rhi, QSize size)
{
    if (!rhi || size.isEmpty()) {
        return;
    }
    if (m_atlas.valid() && m_atlas.size == size) {
        return;
    }
    destroyAtlas();

    AtlasTarget fresh;
    fresh.size = size;
    // Colour-only RGBA8. Never a render target: tiles are copyTexture'd in and the whole
    // atlas is read back, so it needs only UsedAsTransferSource (the read-back source).
    fresh.color.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::UsedAsTransferSource));
    if (!fresh.color->create()) {
        return; // fresh frees its partial resource on scope exit
    }
    m_atlas = std::move(fresh);
}

void cwRhiOffscreenRenderer::destroyAtlas()
{
    // No render-pass descriptor keys a pipeline on the atlas, so unlike destroyTarget
    // there is nothing to evict — just release the texture.
    m_atlas.color.reset();
    m_atlas.size = QSize();
}

bool cwRhiOffscreenRenderer::dropLeadingNonRenderable()
{
    if (m_queue.isEmpty()) {
        return false;
    }
    const std::shared_ptr<cwOffscreenRenderJob>& front = m_queue.first();
    // Defense-in-depth on null: jobs are non-null in practice, but the queue crosses the
    // GUI→render boundary.
    if (!front) {
        m_queue.removeFirst();
        return true;
    }
    if (front->promise.isCanceled() || front->parameters.outputSize.isEmpty()) {
        m_queue.takeFirst()->promise.finish();
        return true;
    }
    return false; // front is a real render job
}

void cwRhiOffscreenRenderer::drainPending(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer)
{
    assertRenderThread();
    if (!cb || !renderer) {
        return;
    }
    QRhi* rhi = cb->rhi();
    if (!rhi) {
        return;
    }

    // Drop entries whose read-back has since completed (the lambda freed its own
    // result), keeping the in-flight list bounded. shutdown() walks whatever remains
    // to finish stragglers and reclaim their results.
    m_inflightReadbacks.removeIf(
        [](const InflightOffscreenReadback& inflight) { return inflight.completed(); });

    // Skip past any leading non-renderable jobs (resolving them) so the batch config below
    // is read from a real render job.
    while (dropLeadingNonRenderable()) { }
    if (m_queue.isEmpty()) {
        return;
    }

    // Recycle appearance slots before this frame's batch acquires any: free last
    // frame's growth orphans and rewind each object's override-slot cursor (§8.1).
    recycleFrameAppearanceSlots();

    // Batch config from the leading renderable job. Compatible followers (§5: same output
    // size, same effective sample count, same background) share one atlas + one read-back.
    const cwOffscreenRenderParameters& cfg = m_queue.first()->parameters;
    const QSize size = cfg.outputSize;
    const int sampleCount = m_frame.effectiveEdlSampleCount(rhi, cfg.sampleCount);
    const QColor background = cfg.backgroundColor;

    // The grid the atlas can hold for this tile config, bounded by TextureSizeMax, the
    // byte budget, and the per-frame work cap. An invalid grid (tile too big / budget too
    // small) caps the run at 1, falling back to the single-tile path.
    const cwOffscreenAtlasGrid grid =
        cwOffscreenAtlasGrid::choose(size, rhi->resourceLimit(QRhi::TextureSizeMax),
                                     kAtlasByteBudget, cwRhiFrameRenderer::kOffscreenBatchCameraSlots,
                                     kScratchDepthBytesPerPixel);
    // Billboards can't be atlas-batched: their inline cwItem2DRenderer reuses one MVP
    // UBO per gather, so K tiles in one command buffer would all render the last
    // tile's pose. A job can still atlas as long as it suppresses every visible
    // billboard (hiddenObjectIds) — then no billboard is drawn, so nothing collapses.
    // A job that does draw a billboard renders alone (one tile per command buffer /
    // frame), so each gets its own camera. Thumbnails hide billboards (oversized in a
    // small icon anyway) and batch; a full frame that shows them renders single-tile.
    const QSet<cwRenderObjectId> mustHideToBatch = m_frame.atlasIncompatibleVisibleObjectIds();
    const auto jobCanBatch = [&mustHideToBatch](const cwOffscreenRenderParameters& p) {
        for (cwRenderObjectId id : mustHideToBatch) {
            if (!p.hiddenObjectIds.contains(id)) {
                return false;
            }
        }
        return true;
    };

    const int cap = (grid.isValid() && jobCanBatch(m_queue.first()->parameters))
                        ? grid.capacity() : 1;

    // Peel a compatible run off the front, bounded by one atlas-full. Spillover and the
    // first incompatible job stay queued; render() re-arms another frame while
    // hasPendingWork(). Non-renderable jobs met along the way are resolved for free.
    QList<std::shared_ptr<cwOffscreenRenderJob>> batch;
    while (batch.size() < cap) {
        while (dropLeadingNonRenderable()) { } // a job cancelled mid-queue is resolved here
        if (m_queue.isEmpty()) {
            break;
        }
        const cwOffscreenRenderParameters& fp = m_queue.first()->parameters;
        // The leading job (batch empty) is always taken — cap is already 1 when it
        // draws billboards, so it renders single-tile. A later job that draws
        // billboards can't join, so it breaks the run and leads the next one.
        if (fp.outputSize != size || fp.backgroundColor != background
            || m_frame.effectiveEdlSampleCount(rhi, fp.sampleCount) != sampleCount
            || (!batch.isEmpty() && !jobCanBatch(fp))) {
            break; // incompatible: it starts the next batch on a later frame
        }
        batch.append(m_queue.takeFirst());
    }

    // cap is 1 for an invalid grid, so batch.size() > 1 implies a valid grid; renderAtlas-
    // Batch also self-protects (empty atlasSize -> single-tile fallback).
    if (batch.size() > 1) {
        renderAtlasBatch(cb, renderer, batch, grid, sampleCount);
    } else {
        // Solitary or atlas-ineligible: the proven one-target / one-read-back path.
        for (const auto& job : std::as_const(batch)) {
            renderSingleTile(cb, renderer, job, size, sampleCount);
        }
    }
}

bool cwRhiOffscreenRenderer::renderJobIntoScratch(QRhiCommandBuffer* cb,
                                                  cwRhiItemRenderer* renderer,
                                                  const std::shared_ptr<cwOffscreenRenderJob>& job,
                                                  QSize size, int sampleCount, int cameraSlot)
{
    QRhi* rhi = cb->rhi();
    const cwOffscreenRenderParameters& p = job->parameters;

    ensureTarget(rhi, size, sampleCount);
    if (!m_target.valid()) {
        return false; // can't allocate the scratch
    }

    // Engage the EDL composite when a point cloud is visible, so the offscreen image
    // matches the live view's eye-dome-lit look that offscreen point-cloud captures need. The
    // EDL buffers are sized to the request and MSAA-matched (ensureEdlOffscreen gates
    // p.sampleCount to the same effective count as the target above).
    const bool cloudVisible = m_frame.anyCloudVisible();
    if (cloudVisible) {
        m_frame.ensureEdlOffscreen(m_edl, rhi, size, p.sampleCount);
    }
    const bool offscreenEdlActive = cloudVisible && m_edl.valid();

    // The offscreen effect composites into m_target. The helper re-inits whenever that
    // target's rpDesc changes (a size/sample-count rebuild) or the input sample count
    // changes; then keep its tuning fresh each render.
    if (offscreenEdlActive && m_edl.effect) {
        m_frame.ensureEffectInitialized(m_edl, rhi, m_target.rpDesc.get(), sampleCount);
        // Per-job EDL when the job overrides it; otherwise the scene's live tuning.
        static_cast<cwEDLEffect*>(m_edl.effect.get())
            ->setParameters(p.edlOverride.value_or(m_frame.edlParameters()));
    }

    // Route passes to the offscreen targets (EDL split when active, else flat into
    // m_target). Mutating the routing here is safe: the live passes are already recorded,
    // and the next live render() resets it at its top.
    m_frame.setupPassRouting(m_target.target.get(), offscreenEdlActive ? &m_edl : nullptr);

    cwRHIObject::RenderData offscreenRenderData{
        cb, renderer, cwSceneUpdate::Flag::None, m_target.rpDesc.get(), sampleCount
    };

    // Stamp this tile's camera into both the global UBO (slot @a cameraSlot, read by
    // geometry on the GPU) and offscreenRenderData (read by CPU draws like
    // cwRenderBillboards), from one clip-space derivation — so billboards and geometry
    // share the offscreen camera/target/DPR rather than the live frame's. Batched
    // tiles each take a distinct slot so the K passes in one command buffer don't
    // collapse onto one camera. cameraBatch is consumed by drawScene below.
    QRhiResourceUpdateBatch* cameraBatch = rhi->nextResourceUpdateBatch();
    const cwRhiFrameRenderer::ClipSpaceCamera clipCamera = m_frame.stampCamera(
        cameraBatch, rhi, offscreenRenderData, cameraSlot, m_target.target.get(),
        p.projectionMatrix, p.viewMatrix, p.devicePixelRatio, size);

    const std::array<cwRHIObject::RenderData, cwRhiFrameRenderer::kPassCount> perPassRenderData =
        m_frame.buildPerPassRenderData(offscreenRenderData);

    // Resolve this job's per-object appearance overrides to transient slots,
    // uploading each payload just-in-time. Recorded before drawScene records the
    // pass, so the GPU applies the writes first; a growth here drops the cloud's
    // SRB, which gather() rebuilds against the new buffer via ensureShaderResources.
    QHash<const cwRHIObject*, int> appearanceSlotForObject;
    if (!p.appearanceOverrides.isEmpty()) {
        QRhiResourceUpdateBatch* appearanceBatch = rhi->nextResourceUpdateBatch();
        appearanceSlotForObject = resolveAppearanceOverrides(rhi, appearanceBatch, p);
        cb->resourceUpdate(appearanceBatch);
    }

    std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiFrameRenderer::kPassCount> passBatches;
    m_frame.gatherScene(passBatches, perPassRenderData,
                         {p.hiddenObjectIds, appearanceSlotForObject});

    const cwRhiPostProcessEffect::FrameUniformContext offscreenFrameContext{
        clipCamera.projectionCorrected, size, p.devicePixelRatio
    };

    const quint32 cameraOffset = m_frame.globalUniformBufferStride() * quint32(cameraSlot);
    m_frame.drawScene(cb, m_target.target.get(),
                       offscreenEdlActive ? &m_edl : nullptr,
                       passBatches, perPassRenderData, cameraBatch, offscreenFrameContext,
                       cameraOffset, p.backgroundColor);
    return true;
}

void cwRhiOffscreenRenderer::renderSingleTile(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer,
                                              const std::shared_ptr<cwOffscreenRenderJob>& job,
                                              QSize size, int sampleCount)
{
    // Solitary render: the first offscreen camera slot.
    if (!renderJobIntoScratch(cb, renderer, job, size, sampleCount, kFirstOffscreenCameraSlot)) {
        job->promise.finish(); // can't allocate the scratch — empty result
        return;
    }
    // One read-back of the scratch, one slice spanning the whole image (null sub-rect).
    // The read-back source is the 1x resolve under MSAA, else the 1x colour directly.
    recordReadbackFanout(cb, m_target.readbackTexture(), { job }, { QRect() });
}

void cwRhiOffscreenRenderer::renderAtlasBatch(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer,
                                              const QList<std::shared_ptr<cwOffscreenRenderJob>>& batch,
                                              const cwOffscreenAtlasGrid& grid, int sampleCount)
{
    QRhi* rhi = cb->rhi();
    const QSize tileSize = grid.tileSize;

    ensureAtlas(rhi, grid.atlasSize());
    if (!m_atlas.valid()) {
        // Atlas allocation failed; fall back to the proven single-tile path per job.
        for (const auto& job : std::as_const(batch)) {
            renderSingleTile(cb, renderer, job, tileSize, sampleCount);
        }
        return;
    }

    // Pre-grow each overridden object's appearance pool once to this batch's
    // concurrency (§8.1): every job that overrides an object needs its own live
    // slot for the duration of the batch, so the object's pool must hold one slot
    // per such job. Doing it here, before any tile is recorded, means the per-tile
    // acquire never reallocates mid-batch (which would orphan a buffer per tile).
    {
        QHash<cwAppearanceSlotted*, int> overrideCountByObject;
        for (const auto& job : std::as_const(batch)) {
            const auto& overrides = job->parameters.appearanceOverrides;
            for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
                cwRHIObject* object = m_frame.renderObjectForId(it.key());
                if (auto* slotted = object ? object->appearanceSlots() : nullptr) {
                    ++overrideCountByObject[slotted];
                }
            }
        }
        if (!overrideCountByObject.isEmpty()) {
            QRhiResourceUpdateBatch* preGrow = rhi->nextResourceUpdateBatch();
            for (auto it = overrideCountByObject.cbegin();
                 it != overrideCountByObject.cend(); ++it) {
                it.key()->reserveAppearanceSlots(rhi, preGrow, 1 + it.value());
            }
            cb->resourceUpdate(preGrow);
        }
    }

    QList<std::shared_ptr<cwOffscreenRenderJob>> rendered;
    QList<QRect> subRects;
    rendered.reserve(batch.size());
    subRects.reserve(batch.size());

    for (int i = 0; i < batch.size(); ++i) {
        const std::shared_ptr<cwOffscreenRenderJob>& job = batch.at(i);
        // Tile i takes its own offscreen camera slot; the batch is capped at
        // kOffscreenBatchCameraSlots so the slot stays within the UBO.
        if (!renderJobIntoScratch(cb, renderer, job, tileSize, sampleCount,
                                  kFirstOffscreenCameraSlot + i)) {
            job->promise.finish(); // scratch alloc failed for this tile
            continue;
        }
        // Copy this tile out of the reused scratch into its atlas cell before the next
        // tile's pass overwrites the scratch. Recorded between the two render passes, this
        // snapshots the tile in-stream (proven on Metal by the Phase 0 spike). Source is
        // the 1x resolve under MSAA, else the 1x colour.
        const QRect cell = grid.tileRect(i);
        QRhiTextureCopyDescription copyDesc;
        copyDesc.setDestinationTopLeft(cell.topLeft());
        QRhiResourceUpdateBatch* copyBatch = rhi->nextResourceUpdateBatch();
        copyBatch->copyTexture(m_atlas.color.get(), m_target.readbackTexture(), copyDesc);
        cb->resourceUpdate(copyBatch);

        rendered.append(job);
        subRects.append(cell);
    }

    if (rendered.isEmpty()) {
        return; // every tile failed to allocate; each promise already finished
    }

    // One read-back of the whole atlas; the completion slices each cell to its job.
    recordReadbackFanout(cb, m_atlas.color.get(), rendered, subRects);
}

void cwRhiOffscreenRenderer::recordReadbackFanout(QRhiCommandBuffer* cb, QRhiTexture* readbackTexture,
                                                  const QList<std::shared_ptr<cwOffscreenRenderJob>>& jobs,
                                                  const QList<QRect>& subRects)
{
    Q_ASSERT(jobs.size() == subRects.size());
    Q_ASSERT(!jobs.isEmpty()); // InflightOffscreenReadback::completed() relies on this
    QRhi* rhi = cb->rhi();

    // The completion runs on the render thread; it holds every job (shared_ptr) and the
    // outstanding-count (shared_ptr<int>) so neither a torn-down scene nor a destroyed
    // caller can dangle. Weak copies are tracked so shutdown() can finish the promises if
    // the QRhi is destroyed before this fires.
    auto* readback = new QRhiReadbackResult;
    InflightOffscreenReadback inflight;
    inflight.result = readback;
    inflight.jobs.reserve(jobs.size());
    for (const auto& job : std::as_const(jobs)) {
        inflight.jobs.append(job); // weak ref for shutdown(); the lambda holds the strong ref
    }
    m_inflightReadbacks.append(inflight);

    auto counter = m_outstandingReadbacks;
    ++(*counter);

    const QList<std::shared_ptr<cwOffscreenRenderJob>> holders = jobs;
    const QList<QRect> rects = subRects;
    readback->completed = [readback, holders, rects, counter]() {
        // The read-back source is always RGBA8 (the scratch colour/resolve and the atlas
        // alike); a short/empty/wrong-format result yields a null view, and every promise
        // still finishes (with no result) so no future hangs.
        const QImage image = readbackView(*readback);
        for (int i = 0; i < holders.size(); ++i) {
            if (!image.isNull()) {
                const QRect r = rects.at(i);
                // Null sub-rect = the whole image (single-tile path); else slice the cell.
                // Either branch deep-copies, since the read-back buffer is transient.
                holders.at(i)->promise.addResult(r.isNull() ? image.copy() : image.copy(r));
            }
            holders.at(i)->promise.finish();
        }
        --(*counter);
        delete readback;
    };

    QRhiResourceUpdateBatch* readbackBatch = rhi->nextResourceUpdateBatch();
    readbackBatch->readBackTexture(QRhiReadbackDescription(readbackTexture), readback);
    cb->resourceUpdate(readbackBatch);
}

void cwRhiOffscreenRenderer::recycleFrameAppearanceSlots()
{
    for (cwRHIObject* object : std::as_const(m_frame.renderObjects())) {
        if (auto* slotted = object->appearanceSlots()) {
            // Order: free last frame's orphans (that frame is submitted) THEN rewind
            // the cursor for this frame's acquires.
            slotted->flushRetiredAppearanceResources();
            slotted->resetFrameAppearanceSlots();
        }
    }
}

QHash<const cwRHIObject*, int> cwRhiOffscreenRenderer::resolveAppearanceOverrides(
    QRhi* rhi, QRhiResourceUpdateBatch* batch, const cwOffscreenRenderParameters& parameters)
{
    QHash<const cwRHIObject*, int> appearanceSlotForObject;
    for (auto it = parameters.appearanceOverrides.cbegin();
         it != parameters.appearanceOverrides.cend(); ++it) {
        cwRHIObject* object = m_frame.renderObjectForId(it.key());
        auto* slotted = object ? object->appearanceSlots() : nullptr;
        if (!slotted) {
            continue; // unknown id, or an object that doesn't support overrides
        }
        const int slot = slotted->acquireAppearanceSlot(rhi, batch);
        if (slot == cwAppearanceSlotted::kNoAppearanceSlot) {
            continue; // appearance budget exhausted — leave this object live (slot 0)
        }
        slotted->uploadAppearance(batch, slot, it.value());
        appearanceSlotForObject.insert(object, slot);
    }
    return appearanceSlotForObject;
}
