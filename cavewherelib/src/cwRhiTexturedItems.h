#ifndef CWRHITEXTUREDITEMS_H
#define CWRHITEXTUREDITEMS_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiPipelineSet.h"
#include "cwRhiFrameRenderer.h"
#include <QMatrix4x4>

class cwRhiTexturedItems : public cwRHIObject
{
public:
    cwRhiTexturedItems();
    ~cwRhiTexturedItems();

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;
    void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor) override;
    std::optional<QBox3D> worldBounds() const override;

private:
    struct SharedItemData {
        QRhiTexture* loadingTexture = nullptr;
    };

    struct Item {
        Item();
        ~Item();

        QRhiBuffer* vertexBuffer = nullptr;
        QRhiBuffer* indexBuffer = nullptr;
        QRhiBuffer* uniformBuffer = nullptr;
        QRhiTexture* texture = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
        // One pipeline per render target this item draws into, kept resident so
        // live↔offscreen toggling never rebuilds them. pipelineRecord is the
        // record for the pass currently being gathered/rendered. The srb above
        // is render-pass-independent and shared across all of them.
        cwRhiPipelineSet pipelines;
        cwRhiPipelineRecord* pipelineRecord = nullptr;

        int numberOfIndices = 0;
        QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt32;

        cwGeometry geometry;
        // Whichever texture representation is waiting to upload; both are
        // dropped once the upload is recorded.
        QImage image;
        cwCompressedTexture compressedTexture;
        QByteArray uniformBlock;
        cwRenderMaterialState material;
        QMatrix4x4 modelMatrix;

        // localBounds comes from the Position attribute, worldBounds from
        // localBounds through modelMatrix. An item with invalid bounds always
        // draws rather than risking a wrong cull.
        QBox3D localBounds;
        QBox3D worldBounds;
        bool boundsValid = false;

        bool resourcesInitialized = false;
        bool geometryNeedsUpdate = false;
        bool textureNeedsUpdate = false;
        bool uniformNeedsUpdate = false;
        bool pipelineNeedsUpdate = true;
        bool modelMatrixNeedsUpdate = true;

        cwLedgeredBytes geometryBytes {cwRenderMemoryLedger::Category::TexturedItemGeometry,
                                       cwRenderMemoryLedger::Residency::Gpu};
        cwLedgeredBytes textureBytes {cwRenderMemoryLedger::Category::TexturedItemTexture,
                                      cwRenderMemoryLedger::Residency::Gpu};

        cwRhiTexturedItems* owner = nullptr;

        void initializeResources(const ResourceUpdateData &data, const SharedItemData &sharedData);
        void ensurePipeline(const RenderData& renderData, const SharedItemData &sharedData, const QRhiVertexInputLayout& layout);
        void updateGeometryBuffers(const ResourceUpdateData& data);
        void updateTextureResource(const ResourceUpdateData& data, const SharedItemData &sharedData);
        //! Uploads compressedTexture; false means the backend rejected the
        //! format or the texture and the caller must use the QImage path
        bool uploadCompressedTexture(const ResourceUpdateData& data);
        void updateUniformBuffer(const ResourceUpdateData& data);
        void createShaderResourceBindings(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor);
        QByteArray buildPerDrawUniformPayload() const;
        void updateBoundsFromGeometry();
        void updateWorldBounds();
    };

    //! Adds this object's item counts to the frame's culled/total tally
    void tallyCullingStats(const GatherContext& context) const;

    QHash<uint32_t, Item*> m_items;
    bool m_resourcesInitialized = false;
    SharedItemData m_sharedData;
    QRhiVertexInputLayout m_inputLayout;

    static QRhiShaderResourceBinding::StageFlags toRhiStages(cwRenderMaterialState::ShaderStages stages);
    static QRhiGraphicsPipeline::CullMode toRhiCullMode(cwRenderMaterialState::CullMode mode);
    static QRhiGraphicsPipeline::FrontFace toRhiFrontFace(cwRenderMaterialState::FrontFace face);
    static QRhiGraphicsPipeline::TargetBlend toBlendState(const cwRenderMaterialState& material);
    static quint8 toStageMask(cwRenderMaterialState::ShaderStages stages);

    cwRhiPipelineKey makePipelineKey(QRhiRenderPassDescriptor* renderPass,
                                     int sampleCount,
                                     const cwRenderMaterialState& material) const;
    cwRhiPipelineRecord* acquirePipeline(const cwRhiPipelineKey& key,
                                                const cwRenderMaterialState& material,
                                                QRhi* rhi,
                                                const QRhiVertexInputLayout& layout,
                                                const SharedItemData& sharedData);
    QRhiSampler* sharedSampler(QRhi* rhi);
    static cwRHIObject::RenderPass toRenderPass(cwRenderMaterialState::RenderPass pass);
};



#endif // CWRHITEXTUREDITEMS_H
