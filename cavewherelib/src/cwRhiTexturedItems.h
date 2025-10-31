#ifndef CWRHITEXTUREDITEMS_H
#define CWRHITEXTUREDITEMS_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderTexturedItems.h"

class cwRhiTexturedItems : public cwRHIObject
{
public:
    cwRhiTexturedItems();
    ~cwRhiTexturedItems();

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    void render(const RenderData& data) override;

private:
    struct SharedItemData {
        QRhiSampler* sampler = nullptr;
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
        QRhiGraphicsPipeline* pipeline = nullptr;
        QRhiShaderResourceBindings* canonicalLayout = nullptr;

        QRhiRenderPassDescriptor* renderPass = nullptr;
        int sampleCount = 1;

        int numberOfIndices = 0;

        cwGeometry geometry;
        QImage image;
        QByteArray uniformBlock;
        cwRenderMaterialState material;

        bool resourcesInitialized = false;
        bool geometryNeedsUpdate = false;
        bool textureNeedsUpdate = false;
        bool uniformNeedsUpdate = false;
        bool pipelineNeedsUpdate = true;

        double memoryUsageMb = 0.0;

        bool visible = true;

        void initializeResources(const ResourceUpdateData &data, const SharedItemData &sharedData);
        void ensurePipeline(const ResourceUpdateData& data, const SharedItemData &sharedData, const QRhiVertexInputLayout& layout);
        void updateGeometryBuffers(const ResourceUpdateData& data);
        void updateTextureResource(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void updateUniformBuffer(const ResourceUpdateData& data);
        void createShaderResourceBindings(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void destroyPipeline();
    };

    QHash<uint32_t, Item*> m_items;
    bool m_resourcesInitialized = false;
    bool m_visible = true;
    QPointer<cwRenderTexturedItems> m_renderItems;
    SharedItemData m_sharedData;
    QRhiVertexInputLayout m_inputLayout;

    static QRhiShaderResourceBinding::StageFlags toRhiStages(cwRenderMaterialState::ShaderStages stages);
    static QRhiGraphicsPipeline::CullMode toRhiCullMode(cwRenderMaterialState::CullMode mode);
    static QRhiGraphicsPipeline::FrontFace toRhiFrontFace(cwRenderMaterialState::FrontFace face);
    static QRhiGraphicsPipeline::TargetBlend toBlendState(const cwRenderMaterialState& material);
};



#endif // CWRHITEXTUREDITEMS_H
