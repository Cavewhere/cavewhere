#ifndef CWRHISCENE_H
#define CWRHISCENE_H

//Qt includes
#include <rhi/qrhi.h>
class QRhiCommandBuffer;

//Our includes
#include "cwSceneUpdate.h"
#include <QHash>
#include <QString>
#include <functional>
class cwScene;
class cwRHIObject;
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
    };

    cwSceneUpdate::Flag m_updateFlags = cwSceneUpdate::Flag::None;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_projectionCorrectedMatrix;
    QMatrix4x4 m_viewProjectionMatrix;
    QMatrix4x4 m_viewMatrix;
    float m_devicePixelRatio;

    QRhiBuffer* m_globalUniformBuffer = nullptr;
    QHash<cwRhiPipelineKey, PipelineRecord*> m_pipelineCache;
    QRhiSampler* m_sharedLinearSampler = nullptr;

    void updateGlobalUniformBuffer(QRhiResourceUpdateBatch* batch, QRhi* rhi);
    bool needsUpdate(cwSceneUpdate::Flag flag) const { return (m_updateFlags & flag) == flag; }


};


#endif // CWRHISCENE_H
