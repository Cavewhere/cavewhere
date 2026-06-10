#ifndef CWRHIOBJECT_H
#define CWRHIOBJECT_H

//Qt includes
#include "cwScene.h"
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

    struct GatherContext {
        const RenderData* renderData;
        RenderPass renderPass;
        quint32 objectOrder = 0;
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
    virtual void initialize(QRhi* rhi,
                            QRhiRenderPassDescriptor* outputRPDesc,
                            int outputSampleCount,
                            QRhiBuffer* globalUBO,
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
    virtual void apply(QRhiCommandBuffer* cb,
                       QRhiTexture* sceneColor,
                       QRhiTexture* cloudColor,
                       QRhiTexture* depth,
                       QSize outputSize) = 0;
};


#endif // CWRHIOBJECT_H
