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
    // Per-vertex visibility, parallel to the position buffer (one 4-byte uint
    // per vertex). Dynamic so a keyword toggle re-uploads only this buffer.
    QRhiBuffer* m_visibilityBuffer = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    cwRhiScene* m_scene = nullptr;
    cwRhiScene::PipelineRecord* m_pipelineRecord = nullptr;
    cwRhiPipelineKey m_pipelineKey;
    bool m_hasPipelineKey = false;

    //The front end data that will be rendered
    cwTracked<cwRenderLinePlot::Data> m_data;
    cwTracked<QVector<quint8>> m_visibility;

    void updateVisibilityBuffer(QRhiResourceUpdateBatch* batch);

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
