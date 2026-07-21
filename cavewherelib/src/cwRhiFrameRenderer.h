#ifndef CWRHIFRAMERENDERER_H
#define CWRHIFRAMERENDERER_H

//Qt includes
#include <rhi/qrhi.h>
class QRhiCommandBuffer;

//Our includes
#include "cwSceneUpdate.h"
#include "cwRHIObject.h"
#include "cwRhiPipelineTypes.h"
#include "cwEdlParametersData.h"
#include "cwRenderObjectId.h"
#include "cwSceneVisibility.h"
#include <QHash>
#include <QList>
#include <QSet>
#include <QSize>
#include <array>
#include <functional>
#include <memory>
class cwRhiItemRenderer;

// Per-render-job inputs to cwRhiFrameRenderer::gatherScene that change which objects
// are gathered and how they look. Defaults = gather the scene exactly like the live
// frame, which is what the live frame passes. New per-job overrides go here as
// fields rather than as more gatherScene() arguments.
struct cwSceneGatherOptions {
    // Render objects to suppress for this gather only, by stable id (per-job
    // visibility override); empty = honor the frame's visibility snapshot alone.
    QSet<cwRenderObjectId> hiddenObjectIds;
    // Per-object appearance slot stamped into each object's GatherContext, by
    // cwRHIObject pointer. Empty (the live frame) = every object gathers at slot 0
    // (its live appearance). The offscreen renderer fills this after acquiring a
    // transient slot per overridden object; an object absent from the map gathers
    // at slot 0. Keyed by pointer because the resolution from id already happened
    // (the renderer needed the cwAppearanceSlotted to acquire), so the gather loop
    // does a cheap pointer lookup rather than re-hashing ids.
    QHash<const cwRHIObject*, int> appearanceSlotForObject;
};

/**
 * @brief The cwRhiFrameRenderer class
 *
 * The shared GPU draw engine for the scene. Owns the render-object registry, the
 * pipeline cache, the global camera UBO, the EDL composite machinery, and the
 * per-pass draw primitives, and renders the resident scene into a target.
 *
 * It is composed (by value) by cwRhiScene — the QQuick item backend, which drives
 * the front-end sync and per-frame orchestration through this class's public API —
 * and referenced by cwRhiOffscreenRenderer, which renders the same resident scene
 * from arbitrary cameras into offscreen targets. Having a real interface here (rather
 * than a friend back-ref into cwRhiScene) lets a third consumer compose the engine
 * without reaching cwRhiScene internals, and lets the draw layer be unit-tested
 * without a live scene/window.
 */
class cwRhiFrameRenderer {
public:
    // cwRhiPipelineKey and cwRhiPipelineRecord live in cwRhiPipelineTypes.h.

    // Offscreen targets for the EDL composite, engaged only while a point cloud
    // is visible. Background + Opaque render into sceneColor with sceneDepth;
    // the point cloud then renders into cloudColor while *sharing* sceneDepth
    // (loaded via PreserveDepthStencilContents) so the hardware depth test
    // occludes the cloud against scene geometry and leaves the combined depth in
    // the buffer. The EDL effect then composites over(sceneColor, shadedCloud)
    // to the swap chain.
    //
    // All three textures are created at `sampleCount`. When the backend supports
    // multisample textures + per-sample shading and the swap chain is MSAA, they
    // are MSAA: the EDL composite reads them per-sample (EDL_MSAA.frag) and the
    // swap chain's own color resolve at frame end anti-aliases both the geometry
    // and the EDL silhouettes. When MSAA isn't available (or sampleCount == 1)
    // they are 1x and the plain EDL.frag runs — no AA, today's behavior.
    // Owns its GPU resources via unique_ptr so it can't be copied (would alias
    // raw pointers) and a missed teardown can't leak. destroyEdlOffscreen() still
    // drives the *ordered* release (effect's SRB before its textures; pipeline
    // eviction before the rpDescs) — declaration order also makes the implicit
    // destructor a safe backstop (effect, declared last, is destroyed first).
    struct EdlOffscreen {
        std::unique_ptr<QRhiTexture> sceneColor;
        std::unique_ptr<QRhiTexture> cloudColor;
        std::unique_ptr<QRhiTexture> depth;            // shared by both targets
        std::unique_ptr<QRhiTextureRenderTarget> sceneTarget;
        std::unique_ptr<QRhiTextureRenderTarget> cloudTarget;
        std::unique_ptr<QRhiRenderPassDescriptor> sceneRpDesc;
        std::unique_ptr<QRhiRenderPassDescriptor> cloudRpDesc;
        QSize size;
        int requestedSampleCount = 1;   // swap-chain count this was built for
        int sampleCount = 1;            // effective MSAA samples (1 = no AA fallback)
        std::unique_ptr<cwRhiPostProcessEffect> effect;

        // What the effect's pipeline was last initialized for, owned per-instance so
        // the live and offscreen EDLs each track their own init key (no shared
        // bool/int that drifts between them). ensureEffectInitialized re-inits when
        // the output target it composites into (rpDesc + that target's sample count)
        // or the input sample count it reads (== this->sampleCount, which selects the
        // 1x vs MSAA shader variant) changes. The null/0 sentinels on a freshly built
        // struct force the first init; they are preserved across an effect-reuse
        // resize so a pure resize doesn't needlessly rebuild the pipeline.
        QRhiRenderPassDescriptor* effectOutputRpDesc = nullptr;
        int effectOutputSampleCount = 0;
        int effectInputSampleCount = 0;

        bool valid() const { return sceneTarget && cloudTarget && effect != nullptr; }
    };

    // A raw camera mapped into the active backend's clip space: the projection
    // adjusted by clipSpaceCorrMatrix() and the combined view-projection. The single
    // source of the clip-space convention for both the live frame (updateGlobalUniform
    // Buffer) and the offscreen path (cwRhiOffscreenRenderer), which feed the global
    // UBO from these.
    struct ClipSpaceCamera {
        QMatrix4x4 projectionCorrected;
        QMatrix4x4 viewProjection;
    };
    static ClipSpaceCamera clipSpaceCorrectedCamera(QRhi* rhi,
                                                    const QMatrix4x4& projection,
                                                    const QMatrix4x4& view);

    struct GlobalUniform {
        float viewProjectionMatrix[16];
        float viewMatrix[16];
        float projectionMatrix[16];
        float devicePixelRatio;
        float _pad0;       // align viewportSize on a vec2 boundary
        float viewportSize[2];
    };

    // D32F where supported; D24S8 elsewhere — both sampler-readable on every Qt RHI
    // backend, so offscreen depth attachments use either interchangeably. Shared by
    // the live EDL offscreen and the offscreen renderer's target.
    static QRhiTexture::Format preferredDepthFormat(QRhi* rhi);

    static constexpr int kPassCount = static_cast<int>(cwRHIObject::RenderPass::Count);

    // The global UBO holds kGlobalCameraSlotCount aligned camera slots: slot 0 is the
    // live on-screen camera (written by updateGlobalUniformBuffer); slots 1..N are the
    // offscreen cameras (written by cwRhiOffscreenRenderer). A solitary offscreen render
    // uses slot 1; an atlas batch uses one distinct slot per tile, so the K tiles drawn
    // into a single command buffer each read their own camera. A shared slot would
    // collapse — every pass reads the slot's final value at GPU-execution time, so all
    // tiles would render the last tile's camera (the repeated-tile bug). N is the
    // per-frame atlas-batch cap; the UBO stays tiny (one stride per slot) so 256 costs
    // ~64 KB. m_globalUniformStride is the aligned per-slot size and the offset of slot 1.
    // cwRHIObject::kOffscreenBatchAppearanceSlots mirrors this value (one appearance slot
    // per tile, same as the camera) — keep the two in sync.
    static constexpr int kOffscreenBatchCameraSlots = 256;
    static constexpr int kGlobalCameraSlotCount = 1 + kOffscreenBatchCameraSlots;

    cwRhiFrameRenderer();
    ~cwRhiFrameRenderer();

    // Single-owner render-thread engine holding raw-owning GPU resources (pipeline
    // records, UBO, sampler, the RHI object registry). Copying or moving any of that
    // would duplicate or strand ownership, so forbid both explicitly.
    Q_DISABLE_COPY_MOVE(cwRhiFrameRenderer)

    QRhiBuffer* globalUniformBuffer() const { return m_globalUniformBuffer; }

    // Byte stride between camera slots in the global UBO (an aligned
    // sizeof(GlobalUniform)). Render objects bind the global UBO with a dynamic
    // offset of this size so a pass can select the live (slot 0) or offscreen
    // (slot 1) camera without rebuilding the SRB. Valid after initialize().
    quint32 globalUniformBufferStride() const { return m_globalUniformStride; }

    // Latest EDL tuning, staged from cwRhiScene::synchroize via setEdlParameters and
    // applied to the offscreen effect at render time (the live effect is fed directly
    // in setEdlParameters). Read by the offscreen renderer per job.
    const EdlParametersData& edlParameters() const { return m_edlParameters; }

    cwRhiPipelineRecord* acquirePipeline(const cwRhiPipelineKey& key,
                                    QRhi* rhi,
                                    const std::function<cwRhiPipelineRecord*(QRhi*)>& createFn);
    void releasePipeline(cwRhiPipelineRecord* record);

    // Free a record's GPU objects and the record itself. The caller is
    // responsible for removing it from m_pipelineCache first.
    static void destroyPipelineRecord(cwRhiPipelineRecord* record);

    QRhiSampler* sharedLinearClampSampler(QRhi* rhi);

    // Evict (and delete) any cached cwRhiPipelineRecord whose pipeline key references
    // `descriptor`. Call this immediately *before* destroying a
    // QRhiRenderPassDescriptor that pipelines have been built against — without
    // it the cache holds dangling-pointer keys that hash-collide with newly
    // allocated descriptors at the same address.
    void evictPipelinesFor(QRhiRenderPassDescriptor* descriptor);

    const QHash<cwRhiPipelineKey, cwRhiPipelineRecord*>& pipelineCache() const { return m_pipelineCache; }

    // Create the global camera UBO. Idempotent — called from cwRhiScene::initialize,
    // which the QQuick backend may invoke more than once.
    void initialize(QRhiCommandBuffer* cb);

    // Front-end sync inputs, staged from cwRhiScene::synchroize (the only holder of
    // the cwScene). setUpdateFlags replaces the frame's dirty flags; syncCameraValues
    // pulls the camera state gated on those flags; setEdlParameters stages the tuning
    // and feeds the live offscreen effect.
    void setUpdateFlags(cwSceneUpdate::Flag flags) { m_updateFlags = flags; }
    void syncCameraValues(float devicePixelRatio,
                          const QMatrix4x4& viewMatrix,
                          const QMatrix4x4& projectionMatrix);
    void setEdlParameters(const EdlParametersData& parameters);

    // The frame's visibility truth: one immutable snapshot of the scene's
    // visibility store, captured per sync at the barrier (the GUI thread is
    // blocked, so the read is race-free) and read by every render-side gate —
    // gatherScene's object gate, the cloud/atlas checks, and per-object readers
    // (cwRhiTexturedItems item gating, cwRHILinePlot's mask upload). A default
    // snapshot (never synced) reads as everything-visible.
    void setVisibilitySnapshot(cwVisibilitySnapshot snapshot) { m_visibility = std::move(snapshot); }
    const cwVisibilitySnapshot& visibilitySnapshot() const { return m_visibility; }

    // Render-object registry mutation, driven by cwRhiScene::synchroize from the
    // front-end cwScene's add/remove/update queues. registerRenderObject frees any
    // cwRHIObject already mapped under @a id before remapping (the issue #512 re-add
    // guard); destroyRenderObject frees and unmaps; markForResourceUpdate queues an
    // object for the next render's updateResources.
    cwRHIObject* renderObjectForId(cwRenderObjectId id) const { return m_rhiObjectLookup.value(id); }
    void registerRenderObject(cwRenderObjectId id, cwRHIObject* rhiObject);
    void destroyRenderObject(cwRenderObjectId id);
    void markForResourceUpdate(cwRHIObject* rhiObject) { m_rhiNeedResourceUpdate.append(rhiObject); }

    // The live render objects, in gather order. The offscreen renderer iterates these
    // for per-frame appearance-slot recycling and id resolution.
    const QList<cwRHIObject*>& renderObjects() const { return m_rhiObjects; }

    // True when any visible object draws real geometry into the PointCloud pass
    // — the signal to engage the EDL composite. Settled before any object builds
    // a pipeline so the pass routing is fixed for the frame.
    bool anyCloudVisible() const;

    // The ids of currently-visible objects that can't be drawn in an atlas batch
    // (cwRHIObject::precludesAtlasBatching) — billboards, whose inline MVP UBO would
    // collapse across batched tiles. The offscreen renderer batches a job only when
    // the job suppresses all of these (hiddenObjectIds), so a billboard-free job
    // (e.g. a thumbnail) still atlases while a billboard-drawing one renders alone.
    // Empty in the common no-billboard case.
    QSet<cwRenderObjectId> atlasIncompatibleVisibleObjectIds() const;

    // Copy @a base into one RenderData per pass, stamping each with the rpDesc +
    // sample count that pass routes into this frame (from setupPassRouting).
    // gather() reads those when building pipeline keys.
    cwRHIObject::PerPassRenderData buildPerPassRenderData(
        const cwRHIObject::RenderData& base) const;

    // Derive one clip-space camera from @a projection / @a view and stamp it into
    // both places a render job reads its camera: global-UBO camera slot @a cameraSlot
    // (read by geometry on the GPU at draw time) and the CPU-side camera fields of
    // @a renderData (read by CPU draws such as cwRenderBillboards, which build their
    // own MVP). Deriving both from a single source is what keeps the GPU and CPU
    // cameras from drifting — the offscreen-billboard misplacement bug. Also stamps
    // renderData.renderTarget / devicePixelRatio; the caller fills renderData's
    // rpDesc + sample count. @a batch carries the UBO write (consumed by the draw).
    // Returns the clip-space camera for callers that still need the corrected
    // projection (the EDL frame context). Shared by the live frame (slot 0) and each
    // offscreen tile (its own slot), so the two paths can't diverge.
    ClipSpaceCamera stampCamera(QRhiResourceUpdateBatch* batch,
                                QRhi* rhi,
                                cwRHIObject::RenderData& renderData,
                                int cameraSlot,
                                QRhiRenderTarget* renderTarget,
                                const QMatrix4x4& projection,
                                const QMatrix4x4& view,
                                float devicePixelRatio,
                                QSize viewportSize);

    // Build the per-pass pipeline batches for every visible object, using the
    // supplied per-pass render data (which carries the target rpDesc + sample
    // count each pass routes into). Shared by the live frame and the offscreen
    // render so both dispatch the same scene. @a options carries the per-job
    // overrides (default = gather exactly like the live frame, which is what the
    // live frame passes): see cwSceneGatherOptions.
    void gatherScene(std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount>& passBatches,
                     const cwRHIObject::PerPassRenderData& perPassRenderData,
                     const cwSceneGatherOptions& options = {});

    // Draw the gathered scene into finalTarget. When edl is non-null the EDL
    // composite runs (Background+Opaque → edl scene target, point cloud → edl
    // cloud target sharing depth, then edl->effect composites into finalTarget
    // with Transparent+Overlay on top); otherwise every pass draws flat into
    // finalTarget. Shared by the live frame and the offscreen render so both
    // produce the same image. cameraUniformOffset selects the global-UBO camera
    // slot; resources is the update batch consumed by the first pass.
    void drawScene(QRhiCommandBuffer* cb,
                   QRhiRenderTarget* finalTarget,
                   const EdlOffscreen* edl,
                   std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount>& passBatches,
                   const cwRHIObject::PerPassRenderData& perPassRenderData,
                   QRhiResourceUpdateBatch* resources,
                   const cwRhiPostProcessEffect::FrameUniformContext& frameContext,
                   quint32 cameraUniformOffset,
                   const QColor& clearColor);

    // Fill m_passRpDesc / m_passSampleCount for this frame. Both are read from
    // the same render target per pass, so a pass's sample count is always the
    // sample count of the target it draws into — they can never disagree (a
    // pipeline whose sample count differs from its render target does not render
    // and triggers backend validation errors).
    void setupPassRouting(QRhiRenderTarget* finalTarget, const EdlOffscreen* edl);

    // Per-pass routing for the current frame, filled by setupPassRouting and read
    // only by buildPerPassRenderData — the single resolver that stamps routing
    // into the per-pass RenderData every pipeline key derives from. Background,
    // Opaque, and PointCloud route to the EDL offscreen (1x) while a cloud is
    // visible; every other pass routes to the final target.
    QRhiRenderPassDescriptor* passRenderPassDescriptor(cwRHIObject::RenderPass pass) const;
    int passSampleCount(cwRHIObject::RenderPass pass) const;

    // Build (or rebuild on resize / sample-count change) the shared-depth EDL
    // offscreen: sceneColor + cloudColor + shared depth, the two render targets,
    // their rpDescs, and the EDL effect. requestedSampleCount is the swap-chain
    // sample count; the actual textures use the capability-gated effective count
    // (stored in EdlOffscreen::sampleCount). No-op when already valid at the
    // requested size and sample count. Operates on @a edl so the same builder
    // serves both the live m_edlOffscreen and the offscreen renderer's EDL.
    void ensureEdlOffscreen(EdlOffscreen& edl, QRhi* rhi, QSize size, int requestedSampleCount);

    // Capability-gated effective sample count for the offscreen: the swap-chain
    // count when the backend supports multisample textures + per-sample shading
    // and that count is a supported MSAA level, else 1 (no-AA fallback).
    int effectiveEdlSampleCount(QRhi* rhi, int requestedSampleCount) const;

    // Release the EDL offscreen's GPU resources in dependency order: drop the
    // effect (its SRB references the textures), evict pipelines keyed on the
    // rpDescs, then delete the targets and textures.
    void destroyEdlOffscreen(EdlOffscreen& edl);

    // (Re)initialize @a edl's composite effect for the target it writes into:
    // @a outputRpDesc + @a outputSampleCount are that target's render-pass
    // descriptor and sample count; the input sample count is edl.sampleCount. A
    // no-op when the effect is already initialized for the same key (tracked on the
    // EdlOffscreen itself). One helper for both the live render path (output =
    // swap chain) and the offscreen drain (output = the offscreen target), so the
    // two can no longer drift. The swap-chain rpDesc isn't available until the
    // first frame, which is why effect init is deferred to here rather than done
    // when the effect is created.
    void ensureEffectInitialized(EdlOffscreen& edl, QRhi* rhi,
                                 QRhiRenderPassDescriptor* outputRpDesc,
                                 int outputSampleCount);

    // Render the live on-screen frame into the renderer's swap-chain target: detect
    // the viewport size, engage the EDL composite when a cloud is visible, initialize
    // / update every render object, gather the scene, and draw it from camera slot 0.
    // Driven by cwRhiScene::render, which sequences the offscreen drain after it.
    void renderLiveFrame(QRhiCommandBuffer* cb, cwRhiItemRenderer* renderer);

private:
    // Delete an RHI object and purge it from every tracking list, so neither the
    // delete loop nor the issue #512 address-reuse cleanup can leave a dangling
    // pointer behind in m_rhiObjectsToInitilize / m_rhiNeedResourceUpdate.
    void destroyRhiObject(cwRHIObject* rhiObject);

    // Issue draws for one pass's batches against the currently-open render pass.
    // cameraUniformOffset selects which camera slot of the global UBO a draw
    // reads (0 = live frame, m_globalUniformStride = offscreen); it is applied as
    // a dynamic offset only to drawables that declared a globalCameraBinding.
    static void drainBatches(QRhiCommandBuffer* cb,
                             QVector<cwRHIObject::PipelineBatch>& batches,
                             QRhiGraphicsPipeline*& boundPipeline,
                             const cwRHIObject::RenderData& passRenderData,
                             quint32 cameraUniformOffset);

    bool needsUpdate(cwSceneUpdate::Flag flag) const { return (m_updateFlags & flag) == flag; }

    QList<cwRHIObject*> m_rhiObjectsToInitilize;
    QList<cwRHIObject*> m_rhiObjects;
    QList<cwRHIObject*> m_rhiNeedResourceUpdate;

    //Render-thread only: mutated by register/destroyRenderObject (driven from
    //cwRhiScene::synchroize), read by renderObjectForId()/gatherScene() here and by
    //the offscreen renderer's id resolution. Keyed by
    //cwRenderObject::renderObjectId() (stable, never reused) rather than the raw
    //pointer, whose address can be recycled by the allocator (issue #512).
    QHash<cwRenderObjectId, cwRHIObject*> m_rhiObjectLookup;

    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;
    cwVisibilitySnapshot m_visibility;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_viewMatrix;
    float m_devicePixelRatio = 1.0f;
    QSize m_viewportSize;

    QRhiBuffer* m_globalUniformBuffer = nullptr;
    quint32 m_globalUniformStride = 0;

    QHash<cwRhiPipelineKey, cwRhiPipelineRecord*> m_pipelineCache;
    QRhiSampler* m_sharedLinearSampler = nullptr;
    EdlOffscreen m_edlOffscreen;

    EdlParametersData m_edlParameters;

    std::array<QRhiRenderPassDescriptor*, kPassCount> m_passRpDesc{};
    std::array<int, kPassCount> m_passSampleCount{};

    // The device's supported MSAA levels are reported once (render thread → GUI)
    // to cwRenderingSettings so the UI offers only valid sample counts.
    bool m_reportedSupportedSampleCounts = false;
};

#endif // CWRHIFRAMERENDERER_H
