#ifndef CWRHILINEPLOT_H
#define CWRHILINEPLOT_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderLinePlot.h"
#include "cwRhiFrameRenderer.h"

//Qt includes
#include <QMatrix4x4>
#include <QSize>
#include <QVector3D>
#include <QVector>
class QRhiBuffer;
class QRhiShaderResourceBindings;
class QRhiGraphicsPipeline;
class QRhiResourceUpdateBatch;

// Draws the line plot as instanced screen-space quads: one instance per
// segment, a fixed 4-vertex TriangleStrip expanded in the vertex shader from
// gl_VertexIndex. Native Lines topology clamps to 1 px on Metal/D3D/Vulkan;
// quad expansion is what lets the centerline draw at 2 px and splays at 1 px
// with per-type color, in one pipeline and one draw.
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

    // QRhi resources — all three vertex buffers are per-instance:
    //   binding 0: the segment endpoints, uploaded straight from the point
    //              pairs (stride 2 * vec3 = one {from, to} per instance)
    //   binding 1: uint segment type (0 = centerline, 1 = splay)
    //   binding 2: uint visibility (0 = hidden), dynamic so a keyword toggle
    //              re-uploads only this buffer
    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_segmentBuffer = nullptr;
    QRhiBuffer* m_typeBuffer = nullptr;
    QRhiBuffer* m_visibilityBuffer = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    //The front end data that will be rendered
    cwTracked<cwRenderLinePlot::Data> m_data;

    // Upload gate for the visibility attribute: the store entryVersion and
    // segment count the buffer was last filled for. The mask itself is read
    // from the frame's snapshot at updateResources time — there is no synced
    // RHI-side copy. Count -1 forces the first upload (a real plot has >= 0).
    quint64 m_uploadedMaskVersion = 0;
    qsizetype m_uploadedMaskSegmentCount = -1;

    void updateVisibilityBuffer(QRhiResourceUpdateBatch* batch);

    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;
};

#endif // CWRHILINEPLOT_H
