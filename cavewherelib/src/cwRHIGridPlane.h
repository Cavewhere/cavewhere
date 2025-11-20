#ifndef CWRHIGRIDPLANE_H
#define CWRHIGRIDPLANE_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderGridPlane.h"
#include "cwRhiScene.h"

//Qt includes
#include <rhi/qrhi.h>
class QRhi;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiShaderResourceBindings;
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
    //Shader layout
    struct UniformData {
        float mvpMatrix[16];
        float modelMatrix[16]; // scale-only matrix used for contour sampling
    };

    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_uniformBuffer = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;
    cwRhiScene* m_scene = nullptr;
    cwRhiScene::PipelineRecord* m_pipelineRecord = nullptr;
    cwRhiPipelineKey m_pipelineKey;
    bool m_hasPipelineKey = false;

    cwTracked<QMatrix4x4> m_modelMatrix;
    cwTracked<QMatrix4x4> m_scaleMatrix;
    bool m_resourcesInitialized = false;

    void initializeResources(const ResourceUpdateData& data);
    void releasePipeline();
    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderTarget* target) const;
};

#endif // CWRHIGRIDPLANE_H
