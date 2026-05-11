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
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
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

    // Per-pass configuration. A default-constructed PassConfig means "render
    // directly to the swap-chain with no post-processing." Passes that need
    // their own offscreen color+depth populate the texture/target/rpDesc
    // fields and (optionally) an effect chain.
    struct PassConfig {
        QRhiTexture* color = nullptr;
        QRhiTexture* depth = nullptr;
        QRhiTextureRenderTarget* target = nullptr;
        QRhiRenderPassDescriptor* rpDesc = nullptr;
        QSize size;
        std::vector<std::unique_ptr<cwRhiPostProcessEffect>> effects;
        bool clearOnBegin = true;
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
    QRhiSampler* sharedLinearClampSampler(QRhi* rhi);

    // Evict (and delete) any cached PipelineRecord whose pipeline key references
    // `descriptor`. Call this immediately *before* destroying a
    // QRhiRenderPassDescriptor that pipelines have been built against — without
    // it the cache holds dangling-pointer keys that hash-collide with newly
    // allocated descriptors at the same address.
    void evictPipelinesFor(QRhiRenderPassDescriptor* descriptor);

    // Read-only access used by tests and (later) the EDL compositor.
    // std::unordered_map (not QHash) because PassConfig holds move-only
    // unique_ptr effects; QHash requires CopyConstructible values.
    using PassConfigMap = std::unordered_map<cwRHIObject::RenderPass, PassConfig>;
    const PassConfigMap& passConfigs() const { return m_passConfigs; }
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
    PassConfigMap m_passConfigs;

    void updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi);
    bool needsUpdate(cwSceneUpdate::Flag flag) const { return (m_updateFlags & flag) == flag; }

    void ensurePointCloudPass(QRhi* rhi, QSize size);

    // Release a PassConfig's GPU resources in dependency order: drop effects
    // (their SRBs reference cfg.color/depth), evict pipelines keyed on rpDesc,
    // then delete the target chain. Leaves cfg in a default-constructed state.
    void destroyPassConfig(PassConfig& cfg);

    // Swap-chain rpDesc isn't available until the first render(), so the
    // post-process effects' initialize() is deferred to then.
    bool m_effectsInitialized = false;


};


#endif // CWRHISCENE_H
