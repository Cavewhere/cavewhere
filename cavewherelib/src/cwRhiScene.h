#ifndef CWRHISCENE_H
#define CWRHISCENE_H

//Qt includes
#include <rhi/qrhi.h>
class QRhiCommandBuffer;

//Our includes
#include "cwSceneUpdate.h"
#include "cwRHIObject.h"
#include <QHash>
#include <QSize>
#include <QString>
#include <array>
#include <functional>
#include <memory>
class cwScene;
class cwRenderObject;
class cwRhiItemRenderer;

struct cwRhiPipelineKey {
    QRhiRenderPassDescriptor* renderPass = nullptr;
    int sampleCount = 1;
    QString vertexShader;
    QString fragmentShader;
    quint8 cullMode = 0;
    quint8 frontFace = 0;
    quint8 blendMode = 0;
    quint8 depthTest = 0;
    quint8 depthWrite = 0;
    quint8 globalBinding = 0;
    quint8 perDrawBinding = 0;
    quint8 textureBinding = 0;
    quint8 globalStages = 0;
    quint8 perDrawStages = 0;
    quint8 textureStages = 0;
    quint8 hasPerDraw = 0;
    quint8 topology = static_cast<quint8>(QRhiGraphicsPipeline::Triangles);
    bool operator==(const cwRhiPipelineKey& other) const = default;
};

uint qHash(const cwRhiPipelineKey& key, uint seed) noexcept;

/**
 * @brief The cwRhiScene class
 *
 * The backend renderer for the scene object. Renders to Qt RHI
 */
class cwRhiScene {
public:
    friend class cwRhiItemRenderer;

    struct PipelineRecord {
        cwRhiPipelineKey key;
        QRhiGraphicsPipeline* pipeline = nullptr;
        QRhiShaderResourceBindings* layout = nullptr;
        quint32 refCount = 0;
    };

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

        bool valid() const { return sceneTarget && cloudTarget && effect != nullptr; }
    };

    ~cwRhiScene();

    QMatrix4x4 viewMatrix() const { return m_viewMatrix; }
    QMatrix4x4 projectionMatrix() const { return m_projectionCorrectedMatrix; }
    QMatrix4x4 viewProjectionMatrix() const { return m_viewProjectionMatrix; }
    float devicePixelRatio() const { return m_devicePixelRatio; }
    QRhiBuffer* globalUniformBuffer() const { return m_globalUniformBuffer; }

    PipelineRecord* acquirePipeline(const cwRhiPipelineKey& key,
                                    QRhi* rhi,
                                    const std::function<PipelineRecord*(QRhi*)>& createFn);
    void releasePipeline(PipelineRecord* record);

    // Free a record's GPU objects and the record itself. The caller is
    // responsible for removing it from m_pipelineCache first.
    static void destroyPipelineRecord(PipelineRecord* record);

    QRhiSampler* sharedLinearClampSampler(QRhi* rhi);

    // Evict (and delete) any cached PipelineRecord whose pipeline key references
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
    const QHash<cwRhiPipelineKey, PipelineRecord*>& pipelineCache() const { return m_pipelineCache; }

private:
    void initialize(QRhiCommandBuffer *cb, cwRhiItemRenderer *renderer);
    void synchroize(cwScene* scene, cwRhiItemRenderer* renderer);
    void render(QRhiCommandBuffer *cb, cwRhiItemRenderer* renderer);

    QList<cwRHIObject*> m_rhiObjectsToInitilize;
    QList<cwRHIObject*> m_rhiObjects;
    QList<cwRHIObject*> m_rhiNeedResourceUpdate;

    //Should only be used in synchroize
    QHash<cwRenderObject*, cwRHIObject*> m_rhiObjectLookup;

    struct GlobalUniform {
        float viewProjectionMatrix[16];
        float viewMatrix[16];
        float projectionMatrix[16];
        float devicePixelRatio;
        float _pad0;       // align viewportSize on a vec2 boundary
        float viewportSize[2];
    };

    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_projectionCorrectedMatrix;
    QMatrix4x4 m_viewProjectionMatrix;
    QMatrix4x4 m_viewMatrix;
    float m_devicePixelRatio = 1.0f;
    QSize m_viewportSize;

    QRhiBuffer* m_globalUniformBuffer = nullptr;
    QHash<cwRhiPipelineKey, PipelineRecord*> m_pipelineCache;
    QRhiSampler* m_sharedLinearSampler = nullptr;
    EdlOffscreen m_edlOffscreen;

    static constexpr int kPassCount = static_cast<int>(cwRHIObject::RenderPass::Count);
    std::array<QRhiRenderPassDescriptor*, kPassCount> m_passRpDesc{};
    std::array<int, kPassCount> m_passSampleCount{};

    void updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi);
    bool needsUpdate(cwSceneUpdate::Flag flag) const { return (m_updateFlags & flag) == flag; }

    // Build (or rebuild on resize / sample-count change) the shared-depth EDL
    // offscreen: sceneColor + cloudColor + shared depth, the two render targets,
    // their rpDescs, and the EDL effect. requestedSampleCount is the swap-chain
    // sample count; the actual textures use the capability-gated effective count
    // (stored in EdlOffscreen::sampleCount). No-op when already valid at the
    // requested size and sample count.
    void ensureEdlOffscreen(QRhi* rhi, QSize size, int requestedSampleCount);

    // Capability-gated effective sample count for the offscreen: the swap-chain
    // count when the backend supports multisample textures + per-sample shading
    // and that count is a supported MSAA level, else 1 (no-AA fallback).
    int effectiveEdlSampleCount(QRhi* rhi, int requestedSampleCount) const;

    // Release the EDL offscreen's GPU resources in dependency order: drop the
    // effect (its SRB references the textures), evict pipelines keyed on the
    // rpDescs, then delete the targets and textures.
    void destroyEdlOffscreen();

    // Fill m_passRpDesc / m_passSampleCount for this frame. Both are read from
    // the same render target per pass, so a pass's sample count is always the
    // sample count of the target it draws into — they can never disagree (a
    // pipeline whose sample count differs from its render target does not render
    // and triggers backend validation errors).
    void setupPassRouting(QRhiRenderTarget* swapchainTarget, bool edlActive);

    // Swap-chain rpDesc isn't available until the first render(), so the EDL
    // effect's initialize() is deferred to then.
    bool m_effectsInitialized = false;

    // Input sample count the EDL effect last built its pipeline for. When the
    // offscreen's sample count changes (user toggled cwRenderingSettings), this
    // no longer matches and the effect is re-initialized to swap the 1x / MSAA
    // shader variant.
    int m_effectInputSampleCount = 0;

    // The device's supported MSAA levels are reported once (render thread → GUI)
    // to cwRenderingSettings so the UI offers only valid sample counts.
    bool m_reportedSupportedSampleCounts = false;
};


#endif // CWRHISCENE_H
