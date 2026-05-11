/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRHIPOINTCLOUD_H
#define CWRHIPOINTCLOUD_H

// Our includes
#include "cwRHIObject.h"
#include "cwRenderPointCloud.h"
#include "cwRhiScene.h"

// Qt includes
#include <QMatrix4x4>
#include <QVector3D>

class QRhiBuffer;
class QRhiShaderResourceBindings;
class QRhiGraphicsPipeline;
class cwRhiItemRenderer;

class cwRHIPointCloud : public cwRHIObject
{
public:
    cwRHIPointCloud();
    ~cwRHIPointCloud() override;

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    void render(const RenderData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

private:
    void initializeResources(const ResourceUpdateData& data);
    void releasePipeline();
    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderTarget* target) const;

    bool m_resourcesInitialized = false;

    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_vertexBuffer = nullptr;
    qsizetype m_vertexBufferCapacity = 0;
    QRhiShaderResourceBindings* m_srb = nullptr;
    cwRhiScene* m_scene = nullptr;
    cwRhiScene::PipelineRecord* m_pipelineRecord = nullptr;
    cwRhiPipelineKey m_pipelineKey;
    bool m_hasPipelineKey = false;

    cwTracked<cwRenderPointCloud::Data> m_data;
};

#endif // CWRHIPOINTCLOUD_H
