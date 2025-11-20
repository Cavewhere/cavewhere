#ifndef CWRHITEXTUREDITEMS_H
#define CWRHITEXTUREDITEMS_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderTexturedItems.h"
#include "cwRhiScene.h"
#include <QMatrix4x4>

class cwRhiTexturedItems : public cwRHIObject
{
public:
    cwRhiTexturedItems();
    ~cwRhiTexturedItems();

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    void render(const RenderData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

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
        cwRhiScene::PipelineRecord* pipelineRecord = nullptr;

        int numberOfIndices = 0;

        cwGeometry geometry;
        QImage image;
        QByteArray uniformBlock;
        cwRenderMaterialState material;
        QMatrix4x4 modelMatrix;

        bool resourcesInitialized = false;
        bool geometryNeedsUpdate = false;
        bool textureNeedsUpdate = false;
        bool uniformNeedsUpdate = false;
        bool pipelineNeedsUpdate = true;
        bool modelMatrixNeedsUpdate = true;

        double memoryUsageMb = 0.0;

        bool visible = true;

        cwRhiTexturedItems* owner = nullptr;

        void initializeResources(const ResourceUpdateData &data, const SharedItemData &sharedData);
        void ensurePipeline(const ResourceUpdateData& data, const SharedItemData &sharedData, const QRhiVertexInputLayout& layout);
        void updateGeometryBuffers(const ResourceUpdateData& data);
        void updateTextureResource(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void updateUniformBuffer(const ResourceUpdateData& data);
        void createShaderResourceBindings(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void releasePipeline();
        QByteArray buildPerDrawUniformPayload() const;
    };

    QHash<uint32_t, Item*> m_items;
    bool m_resourcesInitialized = false;
    bool m_visible = true;
    QPointer<cwRenderTexturedItems> m_renderItems;
    SharedItemData m_sharedData;
    QRhiVertexInputLayout m_inputLayout;
    cwRhiScene* m_scene = nullptr;

    static QRhiShaderResourceBinding::StageFlags toRhiStages(cwRenderMaterialState::ShaderStages stages);
    static QRhiGraphicsPipeline::CullMode toRhiCullMode(cwRenderMaterialState::CullMode mode);
    static QRhiGraphicsPipeline::FrontFace toRhiFrontFace(cwRenderMaterialState::FrontFace face);
    static QRhiGraphicsPipeline::TargetBlend toBlendState(const cwRenderMaterialState& material);
    static quint8 toStageMask(cwRenderMaterialState::ShaderStages stages);

    QString effectiveFragmentShader(const cwRenderMaterialState& material) const;
    QVector<QRhiGraphicsPipeline::TargetBlend> buildTargetBlends(const cwRenderMaterialState& material,
                                                                 cwRHIObject::RenderPass pass,
                                                                 int attachmentCount) const;

    cwRhiPipelineKey makePipelineKey(QRhiRenderPassDescriptor* renderPass,
                                     int sampleCount,
                                     int attachmentCount,
                                     const QString& fragmentShader,
                                     const cwRenderMaterialState& material) const;
    cwRhiScene::PipelineRecord* acquirePipeline(const cwRhiPipelineKey& key,
                                                cwRHIObject::RenderPass pass,
                                                int attachmentCount,
                                                const cwRenderMaterialState& material,
                                                QRhi* rhi,
                                                const QRhiVertexInputLayout& layout,
                                                const SharedItemData& sharedData);
    void releasePipeline(cwRhiScene::PipelineRecord* record);
    QRhiSampler* sharedSampler(QRhi* rhi);
    static cwRHIObject::RenderPass toRenderPass(cwRenderMaterialState::RenderPass pass);
};



#endif // CWRHITEXTUREDITEMS_H
