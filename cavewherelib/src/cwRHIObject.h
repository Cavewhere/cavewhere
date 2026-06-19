#ifndef CWRHIOBJECT_H
#define CWRHIOBJECT_H

//Qt includes
#include "cwScene.h"
#include "cwRhiPipelineSet.h"
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;

#include <rhi/qrhi.h>
#include <functional>
#include <QMatrix4x4>
#include <QSize>
#include <QVector>

//Our includes
class cwRhiItemRenderer;
class cwRenderObject;

class cwRHIObject {

public:
    virtual ~cwRHIObject() {}

    struct RenderData {
        QRhiCommandBuffer* cb;
        cwRhiItemRenderer* renderer;
        cwSceneUpdate::Flag updateFlag;
        // Active pass's render-pass descriptor — offscreen when the pass routes
        // through an EDL offscreen target in cwRhiScene, swap-chain otherwise.
        // Back-ends key their pipelines on this so they rebuild when a pass
        // switches targets.
        QRhiRenderPassDescriptor* renderPassDescriptor = nullptr;
        // Active pass's MSAA sample count. The swap chain is multisampled, but
        // the EDL offscreen targets are 1x, so a pass routed offscreen needs a
        // 1x pipeline. Back-ends key their pipelines on this; it must match the
        // sample count of the render target the pass actually draws into.
        int sampleCount = 1;
    };

    struct ResourceUpdateData {
        QRhiResourceUpdateBatch* resourceUpdateBatch;
        RenderData renderData;
    };

    struct SynchronizeData {
        cwRenderObject* object;
        cwRhiItemRenderer* renderer;
    };

    //For rendering
    enum class RenderPass : int {
        Background = 0,   // radial gradient — was implicit; now an explicit named pass
        PointCloud,       // LAZ point clouds — offscreen target + EDL composite
        Opaque,           // line plot, grid, compass, scraps
        Transparent,
        Overlay,
        ShadowMap,
        Count
    };

    // Per-object appearance-slot budget (ceiling) for offscreen render overrides.
    // Slot 0 is the live appearance; the remaining slots hold per-job appearance
    // overrides for offscreen jobs in flight (the payload travels on the job, see
    // cwOffscreenRenderParameters::appearanceOverrides; the offscreen renderer
    // resolves it to a slot, and Drawable::appearanceBinding carries the offset). An
    // appearance-slotted object (cwAppearanceSlotted) grows its per-object UBO on
    // demand up to this ceiling, not to it — steady state is one slot. Intentionally
    // smaller than the camera-slot budget (cwRhiScene::kOffscreenBatchCameraSlots):
    // a multi-view job shares ONE appearance slot across all its camera views, so a
    // batch needs far fewer appearance slots than camera slots.
    static constexpr int kOffscreenBatchAppearanceSlots = 32;
    static constexpr int kAppearanceSlotCount = 1 + kOffscreenBatchAppearanceSlots;

    struct GatherContext {
        const RenderData* renderData;
        RenderPass renderPass;
        quint32 objectOrder = 0;
        // Per-object appearance slot this render job selects (0 = the live
        // appearance in slot 0; higher slots = a per-job override the offscreen
        // renderer acquired and uploaded for this object before gathering). The
        // scene stamps it from cwSceneGatherOptions::appearanceSlotForObject; an
        // object's gather() turns it into Drawable::appearanceUniformOffset via its
        // own per-slot stride. The live frame always passes 0.
        int appearanceSlot = 0;
    };

    struct Drawable {
        enum class Type {
            Indexed,
            NonIndexed,
            Custom
        };

        Type type = Type::Indexed;
        QVector<QRhiCommandBuffer::VertexInput> vertexBindings;
        QRhiBuffer* indexBuffer = nullptr;
        QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt32;
        quint32 indexOffset = 0;
        qint32 vertexOffset = 0;
        quint32 indexCount = 0;
        quint32 vertexCount = 0;
        quint32 instanceCount = 1;
        quint32 firstInstance = 0;
        QRhiShaderResourceBindings* bindings = nullptr;
        std::function<void(const RenderData&)> customDraw;

        // When >= 0, `bindings` binds the shared global camera UBO at this
        // binding point with a dynamic offset. cwRhiScene supplies the per-pass
        // camera slot offset at draw time (0 for the live frame, the offscreen
        // slot for an offscreen render), so the same SRB renders the object from
        // either camera. -1 (default) means no dynamic global-camera binding and
        // the SRB is bound with no dynamic offsets.
        qint32 globalCameraBinding = -1;

        // When >= 0, `bindings` binds a per-object "appearance" UBO at this
        // binding point with a dynamic offset of appearanceUniformOffset bytes,
        // selecting one slot of that object's multi-slot appearance UBO. Unlike
        // the camera slot (one shared global UBO, offset supplied per render job
        // in drainBatches), the appearance UBO is per object with its own per-slot
        // stride, so the byte offset is computed by the object in gather() — from
        // GatherContext::appearanceSlot times the object's own slot stride — and
        // travels on the drawable. -1 (default) means no dynamic appearance
        // binding. Independent of globalCameraBinding: a drawable may carry
        // neither, either, or both.
        qint32 appearanceBinding = -1;
        quint32 appearanceUniformOffset = 0;
    };

    struct PipelineState {
        QRhiGraphicsPipeline* pipeline = nullptr;
        quint64 sortKey = 0;
        bool operator==(const PipelineState& other) const = default;

    };

    struct PipelineBatch {
        PipelineState state;
        QVector<Drawable> drawables;
    };

    static quint64 makeSortKey(quint32 objectOrder, QRhiGraphicsPipeline* pipeline)
    {
        const quint64 orderPart = quint64(objectOrder) << 32;
        const quint64 pipelinePart = quint64(quintptr(pipeline)) & 0xffffffffull;
        return orderPart | pipelinePart;
    }

    virtual void initialize(const ResourceUpdateData& data) = 0;
    virtual void synchronize(const SynchronizeData& data) = 0;
    virtual void updateResources(const ResourceUpdateData& data) = 0;

    //Older rendering method
    virtual void render(const RenderData& data) = 0;

    //Gather render objects
    virtual bool gather(const GatherContext& context,
                        QVector<PipelineBatch>& batches) {
        Q_UNUSED(context);
        Q_UNUSED(batches);
        return false;
    };

    void setVisible(bool visible) { m_isVisible = visible; }
    bool isVisible() const { return m_isVisible; }

    // True when this object draws into the PointCloud pass with real geometry.
    // cwRhiScene polls this before gathering to decide whether to engage the
    // EDL offscreen-composite path (which reroutes Background + Opaque through
    // a shared-depth offscreen so the cloud silhouette can darken against the
    // scene). Default false; cwRHIPointCloud overrides.
    virtual bool usesPointCloudPass() const { return false; }

    // Drop any pipelines this object has cached against @a descriptor. The scene
    // calls this on every render object just before it tears down a render
    // target (and its render-pass descriptor), so per-object pipeline caches
    // never outlive the descriptor their pipelines were built on. The default
    // purges m_pipelines (the common case); cwRhiTexturedItems overrides it to
    // fan out to its per-item sets.
    virtual void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor)
    {
        m_pipelines.purgeFor(descriptor);
        m_pipelineRecord = nullptr; // re-acquired in the next ensurePipeline
    }

    static QShader loadShader(const QString& name);
private:
    bool m_isVisible = true;


protected:    
    static PipelineBatch& acquirePipelineBatch(QVector<PipelineBatch>& batches,
                                               const PipelineState& state)
    {
        for (auto& existing : batches) {
            if (existing.state.pipeline == state.pipeline &&
                existing.state.sortKey == state.sortKey) {
                return existing;
            }
        }

        batches.append(PipelineBatch{state, {}});
        return batches.last();
    }

    // Per-render-target pipeline cache: one resident pipeline per render target
    // this object draws into (live swap chain + any offscreen targets), so
    // live↔offscreen toggling never rebuilds them. m_pipelineRecord is the
    // record for the pass currently being gathered/rendered (set in the
    // subclass's ensurePipeline). cwRhiTexturedItems keeps a set per item
    // instead and leaves these unused.
    cwRhiPipelineSet m_pipelines;
    cwRhiPipelineRecord* m_pipelineRecord = nullptr;

};


// Post-process effect run on a pass's output. Each PassConfig may carry a
// chain of these; chains longer than one currently require ping-pong
// intermediates not yet implemented.
//
// Effects emit their draw inline inside an already-open render pass — the
// scene begins the pass against the output target (typically the swap-chain),
// invokes each effect's apply(), and ends the pass after the rest of the
// scene-level draws. This keeps the pass count to one for the swap-chain.
class cwRhiPostProcessEffect {
public:
    // Frame-level state that effects may need to precompute their own uniforms.
    // Provided by cwRhiScene once per frame before apply(). Effects that don't
    // care leave the default updateFrameUniforms() no-op.
    struct FrameUniformContext {
        QMatrix4x4 projectionMatrix;  // already pre-multiplied by clipSpaceCorrMatrix
        QSize viewportSize;
        float devicePixelRatio;
    };

    virtual ~cwRhiPostProcessEffect() = default;

    // outputRPDesc is the render-pass descriptor the effect writes to
    // (typically the swap-chain's, for the last effect in a chain).
    // outputSampleCount must match the destination render target's sample
    // count — mismatched MSAA pipelines silently write only sample 0 on
    // Metal/Vulkan, producing dim translucent output after resolve.
    // inputSampleCount is the sample count of the offscreen textures the effect
    // reads: > 1 selects a per-sample (sampler2DMS) path, 1 the plain sampler2D
    // path. May be called again with a different inputSampleCount to swap paths.
    // globalUBOStride is the byte stride between camera slots in globalUBO; the
    // effect binds the global block with a dynamic offset so apply() can select
    // the live (offset 0) or offscreen camera slot, matching the camera the
    // depth buffer was rendered with.
    virtual void initialize(QRhi* rhi,
                            QRhiRenderPassDescriptor* outputRPDesc,
                            int outputSampleCount,
                            QRhiBuffer* globalUBO,
                            quint32 globalUBOStride,
                            int inputSampleCount) = 0;

    // Called when an input texture is recreated (e.g. on swap-chain resize) so
    // the effect can rebuild SRBs that reference it.
    virtual void resize(QSize outputSize) { Q_UNUSED(outputSize); }

    // Push per-frame CPU-side state. Called from cwRhiScene before apply() so
    // the effect can derive UBO contents (e.g. projection-dependent constants)
    // without re-reading them from the global UBO (which lives in GPU memory).
    virtual void updateFrameUniforms(const FrameUniformContext&) {}

    // Emit pipeline+SRB+draw into `cb` (which must be inside an open beginPass
    // against the output target, typically the swap chain). The effect is the
    // final composite: `sceneColor` holds Background + Opaque drawn at 1x,
    // `cloudColor` holds the point cloud (opaque where present, transparent
    // elsewhere), and `depth` is the combined cloud+scene depth they share.
    // cameraUniformOffset selects the global-UBO camera slot (0 = live frame,
    // the per-slot stride = offscreen) so the depth linearization uses the same
    // camera the depth buffer was rendered with.
    virtual void apply(QRhiCommandBuffer* cb,
                       QRhiTexture* sceneColor,
                       QRhiTexture* cloudColor,
                       QRhiTexture* depth,
                       QSize outputSize,
                       quint32 cameraUniformOffset) = 0;
};


#endif // CWRHIOBJECT_H
