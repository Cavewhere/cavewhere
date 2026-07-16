#ifndef CWRHIGRIDPLANE_H
#define CWRHIGRIDPLANE_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderGridPlane.h"
#include "cwRhiFrameRenderer.h"

//Qt includes
#include <rhi/qrhi.h>
class QRhi;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiShaderResourceBindings;
#include <QColor>
#include <QMatrix4x4>


class cwRHIGridPlane : public cwRHIObject {
public:
    cwRHIGridPlane();
    virtual ~cwRHIGridPlane();

    virtual void initialize(const ResourceUpdateData &data) override;
    virtual void synchronize(const SynchronizeData& data) override;
    virtual void updateResources(const ResourceUpdateData&) override;
    virtual void render(const RenderData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

private:
    //Shader layout — view/projection comes from the shared global UBO (binding 0);
    //this per-object block (binding 1) carries only the grid's own matrices.
    struct UniformData {
        float modelMatrix[16]; // grid placement
        float scaleMatrix[16]; // scale-only matrix used for contour sampling
    };

    struct FragmentUniformData {
        float lineColor[4];
    };

    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_uniformBuffer = nullptr;
    QRhiBuffer* m_fragmentUniformBuffer = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;
    cwRhiFrameRenderer* m_frame = nullptr;

    cwTracked<QMatrix4x4> m_modelMatrix;
    cwTracked<QMatrix4x4> m_scaleMatrix;
    cwTracked<QColor> m_color;
    bool m_resourcesInitialized = false;

    void initializeResources(const ResourceUpdateData& data);
    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;
};

#endif // CWRHIGRIDPLANE_H
