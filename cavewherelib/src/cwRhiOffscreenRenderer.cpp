//Our includes
#include "cwRhiOffscreenRenderer.h"
#include "cwRhiScene.h"
#include "cwRHIObject.h"
#include "cwRhiItemRenderer.h"
#include "cwEDLEffect.h"
#include "cwOffscreenRenderJob.h"

//Qt includes
#include <rhi/qrhi.h>
#include <QThread>

//Std includes
#include <array>
#include <cstring>

cwRhiOffscreenRenderer::cwRhiOffscreenRenderer(cwRhiScene* scene) :
    m_scene(scene)
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
        if (auto job = inflight.job.lock()) {
            job->promise.finish();
            delete inflight.result;
        }
    }
    m_inflightReadbacks.clear();

    // Tear down the offscreen targets before the scene's pipeline cache (caller
    // ordering) so pipelines keyed on their rpDescs are released first.
    m_scene->destroyEdlOffscreen(m_edl);
    destroyTarget();
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
    fresh.depth.reset(rhi->newTexture(cwRhiScene::preferredDepthFormat(rhi), size, sampleCount,
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
        m_scene->evictPipelinesFor(m_target.rpDesc.get());
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
        [](const InflightOffscreenReadback& inflight) { return inflight.job.expired(); });

    // At most one *render* per frame. Every render draws into the shared m_target,
    // and a texture read-back recorded into this frame's command buffer resolves
    // against that target's contents at frame end — so two renders into it in one
    // frame would make both read-backs return the second tile's image (the
    // repeated-tile bug). render() re-arms another frame while hasPendingWork(), so
    // the queue still drains fully, just one tile per frame. Non-rendering jobs
    // (cancelled / empty) are resolved without consuming that budget.
    int processed = 0;
    while (processed < kOffscreenRendersPerFrame && !m_queue.isEmpty()) {
        // Defense-in-depth: jobs are always non-null (cwScene::renderOffscreen
        // make_shares them and synchroize moves only non-null pointers), but the
        // queue crosses the GUI→render boundary.
        const std::shared_ptr<cwOffscreenRenderJob>& next = m_queue.first();
        if (!next) {
            m_queue.removeFirst();
            continue;
        }
        // The consumer cancelled the future before we reached it — skip the GPU work
        // and just resolve. Checked before the render so a cancelled job never costs
        // frame budget.
        if (next->promise.isCanceled()) {
            m_queue.takeFirst()->promise.finish();
            continue;
        }
        const QSize size = next->parameters.outputSize;
        if (size.isEmpty()) {
            m_queue.takeFirst()->promise.finish(); // nothing to render
            continue;
        }
        // Capability-gated MSAA count for this request; the target, the offscreen EDL,
        // and the composite effect are all built at this count (1 = no AA, today's
        // behavior). The same gate drives ensureEdlOffscreen, so all three agree.
        const int sampleCount = m_scene->effectiveEdlSampleCount(rhi, next->parameters.sampleCount);

        auto job = m_queue.takeFirst();
        const cwOffscreenRenderParameters& p = job->parameters;
        ++processed;

        ensureTarget(rhi, size, sampleCount);
        if (!m_target.valid()) {
            job->promise.finish(); // can't allocate the target — empty result
            continue;
        }

        // Engage the EDL composite when a point cloud is visible, so the offscreen
        // image matches the live view's eye-dome-lit look (what the sink classifier
        // trains on). The offscreen EDL buffers are sized to the request and MSAA-
        // matched to it (ensureEdlOffscreen gates p.sampleCount to the same effective
        // count as the target above).
        const bool cloudVisible = m_scene->anyCloudVisible();
        if (cloudVisible) {
            m_scene->ensureEdlOffscreen(m_edl, rhi, size, p.sampleCount);
        }
        const bool offscreenEdlActive = cloudVisible && m_edl.valid();

        // The offscreen effect composites into m_target. The helper re-inits whenever
        // that target's rpDesc changes (a size/sample-count-driven rebuild) or the
        // input sample count changes; then keep its tuning fresh each render. The
        // composite writes m_target, so its output sample count is the target's.
        if (offscreenEdlActive && m_edl.effect) {
            m_scene->ensureEffectInitialized(m_edl, rhi, m_target.rpDesc.get(), sampleCount);
            static_cast<cwEDLEffect*>(m_edl.effect.get())->setParameters(m_scene->m_edlParameters);
        }

        // Route passes to the offscreen targets (EDL split when active, else flat into
        // m_target). Objects that resolve their pipeline from the scene's per-frame
        // routing (textured items) then build for the right rpDesc. Mutating the
        // routing here is safe: the live passes are already recorded, and the next
        // live render() resets it at its top.
        m_scene->setupPassRouting(m_target.target.get(),
                                  offscreenEdlActive ? &m_edl : nullptr);

        const cwRHIObject::RenderData offscreenRenderData{
            cb, renderer, cwSceneUpdate::Flag::None, m_target.rpDesc.get(), sampleCount
        };
        const std::array<cwRHIObject::RenderData, cwRhiScene::kPassCount> perPassRenderData =
            m_scene->buildPerPassRenderData(offscreenRenderData);

        std::array<QVector<cwRHIObject::PipelineBatch>, cwRhiScene::kPassCount> passBatches;
        m_scene->gatherScene(passBatches, perPassRenderData);

        // The offscreen camera lives in global-UBO slot 1; write it (and the offscreen
        // viewport metrics) so the pass reads it via the globalUniformBufferStride
        // dynamic offset.
        const cwRhiScene::ClipSpaceCamera clipCamera =
            cwRhiScene::clipSpaceCorrectedCamera(rhi, p.projectionMatrix, p.viewMatrix);

        cwRhiScene::GlobalUniform camera;
        std::memcpy(camera.viewProjectionMatrix, clipCamera.viewProjection.constData(),
                    sizeof(camera.viewProjectionMatrix));
        std::memcpy(camera.viewMatrix, p.viewMatrix.constData(),
                    sizeof(camera.viewMatrix));
        std::memcpy(camera.projectionMatrix, clipCamera.projectionCorrected.constData(),
                    sizeof(camera.projectionMatrix));
        camera.devicePixelRatio = p.devicePixelRatio;
        camera._pad0 = 0.0f;
        camera.viewportSize[0] = float(size.width());
        camera.viewportSize[1] = float(size.height());

        QRhiResourceUpdateBatch* cameraBatch = rhi->nextResourceUpdateBatch();
        cameraBatch->updateDynamicBuffer(m_scene->globalUniformBuffer(),
                                         m_scene->globalUniformBufferStride(),
                                         sizeof(cwRhiScene::GlobalUniform), &camera);

        const cwRhiPostProcessEffect::FrameUniformContext offscreenFrameContext{
            clipCamera.projectionCorrected, size, p.devicePixelRatio
        };

        m_scene->drawScene(cb, m_target.target.get(),
                           offscreenEdlActive ? &m_edl : nullptr,
                           passBatches, perPassRenderData, cameraBatch, offscreenFrameContext,
                           m_scene->globalUniformBufferStride(), p.backgroundColor);

        // Read the colour texture back into the job's promise. The completion runs on
        // the render thread; it holds the job (shared_ptr) and the outstanding-count
        // (shared_ptr<int>) so neither a torn-down scene nor a destroyed caller can
        // dangle. A weak copy is tracked so shutdown() can finish the promise if the
        // QRhi is destroyed before this fires.
        auto* readback = new QRhiReadbackResult;
        auto holder = job;
        m_inflightReadbacks.append({holder, readback});
        auto counter = m_outstandingReadbacks;
        ++(*counter);
        readback->completed = [readback, holder, counter]() {
            const QSize pixelSize = readback->pixelSize;
            const qsizetype expectedBytes = qsizetype(pixelSize.width()) * pixelSize.height() * 4;
            // The read-back source is always created RGBA8 (colour and the MSAA
            // resolve alike), so the wrapper below assumes that layout. Guard on the
            // actual format so a future target-format change fails loud (no result)
            // instead of silently producing swapped/garbled colours.
            if (readback->format == QRhiTexture::RGBA8
                && !pixelSize.isEmpty() && readback->data.size() >= expectedBytes) {
                const QImage view(reinterpret_cast<const uchar*>(readback->data.constData()),
                                  pixelSize.width(), pixelSize.height(),
                                  QImage::Format_RGBA8888);
                holder->promise.addResult(view.copy()); // deep copy: read-back buffer is transient
            }
            // A short/empty/wrong-format read-back still finishes the promise (with no
            // result) so the future resolves rather than hanging.
            holder->promise.finish();
            --(*counter);
            delete readback;
        };

        QRhiResourceUpdateBatch* readbackBatch = rhi->nextResourceUpdateBatch();
        // Read the 1x resolve under MSAA, else the 1x colour directly.
        readbackBatch->readBackTexture(QRhiReadbackDescription(m_target.readbackTexture()),
                                       readback);
        cb->resourceUpdate(readbackBatch);
    }
}
