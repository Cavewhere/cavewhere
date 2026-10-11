/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRHIPASSAGETUBE_H
#define CWRHIPASSAGETUBE_H

// Our includes
#include "cwRHIObject.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderPassageTube.h"
#include "cwRhiFrameRenderer.h"

// Qt includes
#include <QVector4D>

class QRhiBuffer;
class QRhiShaderResourceBindings;
class QRhiResourceUpdateBatch;

// Draws the passage tube as an indexed triangle list with culling off, so the
// walls light from inside and outside: in the opaque pass, or alpha blended
// without depth writes in the transparent pass when the tube is translucent
// (its triangles are not depth sorted, so overlapping walls blend in draw
// order). Hidden trip ranges are dropped by compacting the index buffer
// whenever the published mask changes. The wireframe overlay is a second
// pipeline in line polygon mode, available on backends that support
// QRhi::NonFillPolygonMode.
class cwRHIPassageTube : public cwRHIObject
{
public:
    cwRHIPassageTube() = default;
    ~cwRHIPassageTube() override;

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;
    std::optional<QBox3D> worldBounds() const override;
    void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor) override;

private:
    // std140 layout of TubeBlock in PassageTube.frag, padded to 16 bytes.
    struct TubeUniform {
        float color[4];
        float ambient;
        float yUp;
        float pad[2];
        float clipPlane[4];
        float clipEnabled;
        float clipPad[3];
    };

    enum class PipelineKind {
        Fill,
        TranslucentFill,
        Wireframe
    };

    void uploadVertices(QRhi* rhi, QRhiResourceUpdateBatch* batch);
    void uploadVisibleIndices(QRhi* rhi, QRhiResourceUpdateBatch* batch);
    void uploadUniform(QRhi* rhi, QRhiResourceUpdateBatch* batch);
    void reportGpuBytes();

    cwRhiPipelineRecord* acquirePipeline(const RenderData& data, PipelineKind kind);
    void ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer,
                               const cwRhiPipelineRecord* record);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount,
                                      PipelineKind kind) const;

    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_indexBuffer = nullptr;
    QRhiBuffer* m_uniformBuffer = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    // m_pipelineRecord (base) holds the fill pipeline for the pass being gathered.
    cwRhiPipelineRecord* m_wireframeRecord = nullptr;

    // Indices the compacted buffer holds, and the store entry version it was
    // compacted from. m_indicesStale forces a recompaction after new geometry.
    quint32 m_visibleIndexCount = 0;
    quint64 m_uploadedMaskVersion = 0;
    bool m_indicesStale = true;

    cwLedgeredBytes m_gpuBytes {cwRenderMemoryLedger::Category::PassageTubeGeometry,
                                cwRenderMemoryLedger::Residency::Gpu};

    cwTracked<cwRenderPassageTube::GeometryState> m_geometry;
    bool m_wireframe = false;
    float m_opacity = 1.0f;
    QVector4D m_clipPlane;
    bool m_clipEnabled = false;
    bool m_uniformStale = true;
};

#endif // CWRHIPASSAGETUBE_H
