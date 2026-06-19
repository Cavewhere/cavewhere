/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRHIPOINTCLOUD_H
#define CWRHIPOINTCLOUD_H

// Our includes
#include "cwAppearanceSlotted.h"
#include "cwRHIObject.h"
#include "cwRenderPointCloud.h"
#include "cwRhiScene.h"

// Qt includes
#include <QMatrix4x4>
#include <QVector3D>

class cwRhiItemRenderer;

class cwRHIPointCloud : public cwRHIObject, public cwAppearanceSlotted
{
public:
    cwRHIPointCloud();
    ~cwRHIPointCloud() override;

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    void render(const RenderData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;
    bool usesPointCloudPass() const override;
    cwAppearanceSlotted* appearanceSlots() override { return this; }

    // cwAppearanceSlotted: unpack a cwPointCloudAppearance from the opaque payload
    // and write it (world radius) into one slot of m_perCloudUBO.
    void uploadAppearance(QRhiResourceUpdateBatch* batch, int slot,
                          const cwAppearanceOverride& override) override;

private:
    void initializeResources(const ResourceUpdateData& data);
    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;

    // cwAppearanceSlotted: grow m_perCloudUBO to @a slotCount slots, deferring
    // deletion of the prior buffer + SRB (m_retiredBuffers/m_retiredSrbs, flushed
    // next updateResources) so draws already recorded this frame keep valid
    // pointers. Re-writes the live slot 0 onto @a batch.
    void resizeAppearanceSlots(QRhi* rhi, QRhiResourceUpdateBatch* batch,
                               int slotCount) override;

    // Write one PerCloudUniform (just @a worldRadius today) into @a slot.
    void writeAppearanceSlot(QRhiResourceUpdateBatch* batch, int slot, float worldRadius);

    // std140 rounds a uniform block to a multiple of 16 bytes; pad three
    // floats so the C++ struct matches the shader-side block size. Mirrors
    // the PerCloudBlock declaration in PointCloud.vert.
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
    // Per-cloud uniform block (binding 1): world-space sprite radius in meters,
    // one aligned slot per appearance slot, bound with a dynamic offset so an
    // offscreen job can render the cloud at an overridden radius without disturbing
    // the live view (slot 0). Steady state is ONE slot (the live radius); the pool
    // (cwAppearanceSlotted) grows it on demand to the concurrent-override high-water
    // mark, so an interactive session pays one slot per cloud, not kAppearanceSlotCount.
    // m_perCloudStride is the aligned byte size of one slot, also the dynamic-offset
    // granularity.
    QRhiBuffer* m_perCloudUBO = nullptr;
    quint32 m_perCloudStride = 0;
    QRhiShaderResourceBindings* m_srb = nullptr;
    cwRhiScene* m_scene = nullptr;

    // Geometry and render-state tracked independently so a uniform-only
    // change (world radius / point size) never re-stages the vertex buffer —
    // the expensive vertex upload is gated on m_geometry.isChanged().
    cwTracked<cwRenderPointCloud::GeometryState> m_geometry;
    cwTracked<cwRenderPointCloud::RenderState> m_renderState;
};

#endif // CWRHIPOINTCLOUD_H
