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

// Std includes
#include <limits>

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
    cwRhiPipelineKey buildPipelineKey(QRhiRenderTarget* target,
                                      QRhiRenderPassDescriptor* renderPassDescriptor) const;

    // std140 layout: a single float padded to a 16-byte vec4 slot. Mirrors the
    // PerCloudBlock declaration in PointCloud.vert.
    struct PerCloudUniform {
        float worldRadius = 0.0f;
        float pad[3] = {0.0f, 0.0f, 0.0f};
    };

    bool m_resourcesInitialized = false;

    // m_inputLayout and m_vertexBuffers are built lazily from the geometry on
    // the first non-empty updateResources call. Until then, ensurePipeline
    // returns false and the cloud doesn't draw.
    bool m_layoutBuilt = false;
    QRhiVertexInputLayout m_inputLayout;
    QVector<QRhiBuffer*> m_vertexBuffers;
    QVector<qsizetype> m_vertexBufferCapacities;
    // Per-cloud uniform block (binding 1): world-space point radius derived
    // from the cloud's mean point spacing. NaN sentinel forces the first
    // upload since NaN != worldRadius for any non-NaN worldRadius.
    QRhiBuffer* m_perCloudUBO = nullptr;
    float m_lastUploadedWorldRadius = std::numeric_limits<float>::quiet_NaN();
    QRhiShaderResourceBindings* m_srb = nullptr;
    cwRhiScene* m_scene = nullptr;
    cwRhiScene::PipelineRecord* m_pipelineRecord = nullptr;
    cwRhiPipelineKey m_pipelineKey;
    bool m_hasPipelineKey = false;

    cwTracked<cwRenderPointCloud::Data> m_data;
};

#endif // CWRHIPOINTCLOUD_H
