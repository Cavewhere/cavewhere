#ifndef CWRHISCENE_H
#define CWRHISCENE_H

//Qt includes
#include <rhi/qrhi.h>
class QRhiCommandBuffer;

//Our includes
#include "cwSceneUpdate.h"
#include "cwRHIObject.h"
#include "cwRhiPipelineTypes.h"
#include "cwEdlParametersData.h"
#include "cwRenderObjectId.h"
#include <QHash>
#include <QList>
#include <QSet>
#include <QSize>
#include <QString>
#include <array>
#include <functional>
#include <memory>
class cwScene;
class cwRenderObject;
class cwRhiItemRenderer;
class cwRhiOffscreenRenderer;
struct cwOffscreenRenderJob;

// Per-render-job inputs to cwRhiScene::gatherScene that change which objects are
// gathered and how they look. Defaults = gather the scene exactly like the live
// frame, which is what the live frame passes. New per-job overrides go here as
// fields rather than as more gatherScene() arguments.
struct cwSceneGatherOptions {
    // Render objects to suppress for this gather only, by stable id (per-job
    // visibility override); empty = honor each object's live isVisible().
    QSet<cwRenderObjectId> hiddenObjectIds;
    // Appearance slot stamped into each GatherContext (0 = live appearance); an
    // object that supports appearance overrides reads it to pick its dynamic
    // UBO offset.
    int appearanceSlot = 0;
};

/**
 * @brief The cwRhiScene class
 *
 * The backend renderer for the scene object. Renders to Qt RHI
 */
class cwRhiScene {
public:
    friend class cwRhiItemRenderer;
    friend struct CwRhiSceneTestAccess;

    // cwRhiOffscreenRenderer drives the offscreen-render queue + readback but leans
    // on this class's draw primitives (gatherScene / drawScene / setupPassRouting /
    // buildPerPassRenderData / anyCloudVisible / ensureEffectInitialized and the
    // global-UBO accessors), so it reaches them through this friend rather than a
    // formal interface. The coupling is a deliberate choice (D1, Option A): with
    // only two consumers of those primitives — the live frame and the offscreen
    // renderer — a friend is cheaper than extracting a shared draw layer, and it
    // leaves the live hot path untouched.
    //
    // Promote that primitive set into a standalone cwRhiFrameRenderer that both
    // compose (dropping this friend) when ANY of these turns true — until then the
    // friend is the right tool:
    //   1. A third consumer of the draw primitives appears (a shadow/depth pass, a
    //      thumbnail renderer, a second offscreen mode). Two callers justify a
    //      friend; the third justifies the class.
    //   2. The friend surface grows past the primitives listed above — the
    //      offscreen renderer starts needing new cwRhiScene internals. The seam is
    //      leaking and wants to become a real interface.
    //   3. The draw layer needs unit-testing without a live scene/window — friend
    //      coupling blocks that; a free cwRhiFrameRenderer is testable in isolation.
    //   4. cwRhiScene's size/responsibilities creep back up after this extraction —
    //      the god-object smell returning despite the split.
    // The friend already names the exact methods cwRhiFrameRenderer would expose, so
    // B is a mechanical promotion of those primitives, not a do-over.
    friend class cwRhiOffscreenRenderer;

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

    cwRhiScene();
    ~cwRhiScene();

    QMatrix4x4 viewMatrix() const { return m_viewMatrix; }
    QMatrix4x4 projectionMatrix() const { return m_projectionCorrectedMatrix; }
    QMatrix4x4 viewProjectionMatrix() const { return m_viewProjectionMatrix; }
    float devicePixelRatio() const { return m_devicePixelRatio; }
    QRhiBuffer* globalUniformBuffer() const { return m_globalUniformBuffer; }

    // Byte stride between camera slots in the global UBO (an aligned
    // sizeof(GlobalUniform)). Render objects bind the global UBO with a dynamic
    // offset of this size so a pass can select the live (slot 0) or offscreen
    // (slot 1) camera without rebuilding the SRB. Valid after initialize().
    quint32 globalUniformBufferStride() const { return m_globalUniformStride; }

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

    // Per-pass routing for the current frame: the render-pass descriptor and
    // MSAA sample count a given pass draws into. Filled at the start of render()
    // and queried by render objects that build their pipelines outside gather()
    // (cwRhiTexturedItems) so they key on the correct target. Background,
    // Opaque, and PointCloud route to the EDL offscreen (1x) while a cloud is
    // visible; every other pass routes to the swap chain.
    QRhiRenderPassDescriptor* passRenderPassDescriptor(cwRHIObject::RenderPass pass) const;
    int passSampleCount(cwRHIObject::RenderPass pass) const;
    const QHash<cwRhiPipelineKey, cwRhiPipelineRecord*>& pipelineCache() const { return m_pipelineCache; }

private:
    void initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer);
    void synchroize(cwScene* scene, cwRhiItemRenderer* renderer);
    void render(QRhiCommandBuffer *cb, cwRhiItemRenderer* renderer);

    // Delete an RHI object and purge it from every tracking list, so neither the
    // delete loop nor the issue #512 address-reuse cleanup can leave a dangling
    // pointer behind in m_rhiObjectsToInitilize / m_rhiNeedResourceUpdate.
    void destroyRhiObject(cwRHIObject* rhiObject);

    QList<cwRHIObject*> m_rhiObjectsToInitilize;
    QList<cwRHIObject*> m_rhiObjects;
    QList<cwRHIObject*> m_rhiNeedResourceUpdate;

    //Render-thread only: written in synchroize(), read in synchroize() and
    //gatherScene() (the per-job hidden-id resolution). Keyed by
    //cwRenderObject::renderObjectId() (stable, never reused) rather than the raw
    //pointer, whose address can be recycled by the allocator (issue #512).
    QHash<cwRenderObjectId, cwRHIObject*> m_rhiObjectLookup;

    struct GlobalUniform {
        float viewProjectionMatrix[16];
        float viewMatrix[16];
        float projectionMatrix[16];
        float devicePixelRatio;
        float _pad0;       // align viewportSize on a vec2 boundary
        float viewportSize[2];
    };

    // A raw camera mapped into the active backend's clip space: the projection
    // adjusted by clipSpaceCorrMatrix() and the combined view-projection. The single
    // source of the clip-space convention for both the live frame (updateGlobalUniform
    // Buffer) and the offscreen path (cwRhiOffscreenRenderer, via the friend back-ref),
    // which feed the global UBO from these.
    struct ClipSpaceCamera {
        QMatrix4x4 projectionCorrected;
        QMatrix4x4 viewProjection;
    };
    static ClipSpaceCamera clipSpaceCorrectedCamera(QRhi* rhi,
                                                    const QMatrix4x4& projection,
                                                    const QMatrix4x4& view);

    // D32F where supported; D24S8 elsewhere — both sampler-readable on every Qt RHI
    // backend, so offscreen depth attachments use either interchangeably. Shared by
    // the live EDL offscreen and the offscreen renderer's target.
    static QRhiTexture::Format preferredDepthFormat(QRhi* rhi);

    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_projectionCorrectedMatrix;
    QMatrix4x4 m_viewProjectionMatrix;
    QMatrix4x4 m_viewMatrix;
    float m_devicePixelRatio = 1.0f;
    QSize m_viewportSize;

    QRhiBuffer* m_globalUniformBuffer = nullptr;
    // The global UBO holds kGlobalCameraSlotCount aligned camera slots: slot 0 is the
    // live on-screen camera (written by updateGlobalUniformBuffer); slots 1..N are the
    // offscreen cameras (written by cwRhiOffscreenRenderer). A solitary offscreen render
    // uses slot 1; an atlas batch uses one distinct slot per tile, so the K tiles drawn
    // into a single command buffer each read their own camera. A shared slot would
    // collapse — every pass reads the slot's final value at GPU-execution time, so all
    // tiles would render the last tile's camera (the repeated-tile bug). N is the
    // per-frame atlas-batch cap; the UBO stays tiny (one stride per slot) so 256 costs
    // ~64 KB. m_globalUniformStride is the aligned per-slot size and the offset of slot 1.
    static constexpr int kOffscreenBatchCameraSlots = 256;
    static constexpr int kGlobalCameraSlotCount = 1 + kOffscreenBatchCameraSlots;
    quint32 m_globalUniformStride = 0;

    QHash<cwRhiPipelineKey, cwRhiPipelineRecord*> m_pipelineCache;
    QRhiSampler* m_sharedLinearSampler = nullptr;
    EdlOffscreen m_edlOffscreen;

    // Latest EDL tuning, staged in synchroize() and pushed to the offscreen effect
    // at render time (the live effect is fed directly in synchroize()). Read by the
    // offscreen renderer through the friend back-ref.
    EdlParametersData m_edlParameters;

    // Drives the generic offscreen-render queue (queue + target + offscreen EDL +
    // read-back lifecycle). Owns none of the live frame; it reaches this class's
    // shared draw primitives via the friend back-ref. Held by unique_ptr so this
    // header needs only a forward declaration of it (avoids an include cycle, since
    // it includes cwRhiScene.h for EdlOffscreen); constructed in the ctor with
    // `this` and torn down explicitly via shutdown() from ~cwRhiScene, before the
    // pipeline cache is freed.
    std::unique_ptr<cwRhiOffscreenRenderer> m_offscreen;

    static constexpr int kPassCount = static_cast<int>(cwRHIObject::RenderPass::Count);

    // True when any visible object draws real geometry into the PointCloud pass
    // — the signal to engage the EDL composite. Settled before any object builds
    // a pipeline so the pass routing is fixed for the frame.
    bool anyCloudVisible() const;

    // Copy @a base into one RenderData per pass, stamping each with the rpDesc +
    // sample count that pass routes into this frame (from setupPassRouting).
    // gather() reads those when building pipeline keys.
    std::array<cwRHIObject::RenderData, kPassCount> buildPerPassRenderData(
        const cwRHIObject::RenderData& base) const;

    // Build the per-pass pipeline batches for every visible object, using the
    // supplied per-pass render data (which carries the target rpDesc + sample
    // count each pass routes into). Shared by the live frame and the offscreen
    // render so both dispatch the same scene. @a options carries the per-job
    // overrides (default = gather exactly like the live frame, which is what the
    // live frame passes): see cwSceneGatherOptions.
    void gatherScene(std::array<QVector<cwRHIObject::PipelineBatch>, kPassCount>& passBatches,
                     const std::array<cwRHIObject::RenderData, kPassCount>& perPassRenderData,
                     const cwSceneGatherOptions& options = {});

    // Issue draws for one pass's batches against the currently-open render pass.
    // cameraUniformOffset selects which camera slot of the global UBO a draw
    // reads (0 = live frame, m_globalUniformStride = offscreen); it is applied as
    // a dynamic offset only to drawables that declared a globalCameraBinding.
    static void drainBatches(QRhiCommandBuffer* cb,
                             QVector<cwRHIObject::PipelineBatch>& batches,
                             QRhiGraphicsPipeline*& boundPipeline,
                             const cwRHIObject::RenderData& passRenderData,
                             quint32 cameraUniformOffset);

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
                   const std::array<cwRHIObject::RenderData, kPassCount>& perPassRenderData,
                   QRhiResourceUpdateBatch* resources,
                   const cwRhiPostProcessEffect::FrameUniformContext& frameContext,
                   quint32 cameraUniformOffset,
                   const QColor& clearColor);

    std::array<QRhiRenderPassDescriptor*, kPassCount> m_passRpDesc{};
    std::array<int, kPassCount> m_passSampleCount{};

    void updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi);
    bool needsUpdate(cwSceneUpdate::Flag flag) const { return (m_updateFlags & flag) == flag; }

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
    // EdlOffscreen itself). One helper for both the live render() path (output =
    // swap chain) and the offscreen drain (output = the offscreen target), so the
    // two can no longer drift. The swap-chain rpDesc isn't available until the
    // first render(), which is why effect init is deferred to here rather than done
    // when the effect is created.
    void ensureEffectInitialized(EdlOffscreen& edl, QRhi* rhi,
                                 QRhiRenderPassDescriptor* outputRpDesc,
                                 int outputSampleCount);

    // Fill m_passRpDesc / m_passSampleCount for this frame. Both are read from
    // the same render target per pass, so a pass's sample count is always the
    // sample count of the target it draws into — they can never disagree (a
    // pipeline whose sample count differs from its render target does not render
    // and triggers backend validation errors).
    void setupPassRouting(QRhiRenderTarget* finalTarget, const EdlOffscreen* edl);

    // The device's supported MSAA levels are reported once (render thread → GUI)
    // to cwRenderingSettings so the UI offers only valid sample counts.
    bool m_reportedSupportedSampleCounts = false;
};


#endif // CWRHISCENE_H
