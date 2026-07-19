#ifndef CWRHILINEPLOT_H
#define CWRHILINEPLOT_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderLinePlot.h"
#include "cwRhiFrameRenderer.h"

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


    //The front end data that will be rendered
    cwTracked<cwRenderLinePlot::Data> m_data;

    // Upload gate for the visibility attribute: the store entryVersion and
    // vertex count the buffer was last filled for. The mask itself is read
    // from the frame's snapshot at updateResources time — there is no synced
    // RHI-side copy. Count -1 forces the first upload (a real plot has >= 0).
    quint64 m_uploadedMaskVersion = 0;
    qsizetype m_uploadedMaskVertexCount = -1;

    void updateVisibilityBuffer(QRhiResourceUpdateBatch* batch);

    // Update uniform buffer
    // struct UniformData {
    //     float maxZValue;
    //     float minZValue;
    // };

    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;
};

#endif // CWRHILINEPLOT_H
