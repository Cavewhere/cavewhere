#ifndef CWRHILINEPLOT_H
#define CWRHILINEPLOT_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderLinePlot.h"
#include "cwRhiScene.h"

//Qt includes
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector>
class QRhiBuffer;
class QRhiShaderResourceBindings;
class QRhiGraphicsPipeline;
class QRhiTexture;
class QRhiSampler;
class QRhiResourceUpdateBatch;

class cwRHILinePlot : public cwRHIObject
{
public:
    cwRHILinePlot();
    ~cwRHILinePlot();

    virtual void initialize(const ResourceUpdateData& data) override;
    virtual void synchronize(const SynchronizeData& data) override;
    virtual void updateResources(const ResourceUpdateData& data) override;
    virtual void render(const RenderData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

private:
    void initializeResources(const ResourceUpdateData& data);

    bool m_resourcesInitialized = false;

    // QRhi resources
    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_tripIdBuffer = nullptr;
    QRhiBuffer* m_indexBuffer = nullptr;
    // QRhiBuffer* m_uniformBuffer = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    // Per-trip visibility lookup, sampled in the vertex shader. R8, width =
    // trip count, height 1. Recreated only when the trip count changes; a plain
    // visibility toggle re-uploads its (few-KB) contents in place.
    QRhiTexture* m_visibilityTexture = nullptr;
    QRhiSampler* m_visibilitySampler = nullptr;
    int m_visibilityTextureWidth = 0;

    cwRhiScene* m_scene = nullptr;
    cwRhiScene::PipelineRecord* m_pipelineRecord = nullptr;
    cwRhiPipelineKey m_pipelineKey;
    bool m_hasPipelineKey = false;

    //The front end data that will be rendered
    cwTracked<cwRenderLinePlot::Data> m_data;
    cwTracked<QVector<quint8>> m_tripVisibility;

    void updateVisibilityTexture(QRhiResourceUpdateBatch* batch);

    // Update uniform buffer
    // struct UniformData {
    //     float maxZValue;
    //     float minZValue;
    // };

    void releasePipeline();
    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;
};

#endif // CWRHILINEPLOT_H
