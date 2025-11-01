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
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

private:
    struct SharedItemData {
        QRhiSampler* sampler = nullptr;
        QRhiTexture* loadingTexture = nullptr;
    };

    struct PipelineKey {
        QRhiRenderPassDescriptor* renderPass = nullptr;
        int sampleCount = 1;
        QString vertexShader;
        QString fragmentShader;
        quint8 cullMode = 0;
        quint8 frontFace = 0;
        quint8 blendMode = 0;
        quint8 depthTest = 0;
        quint8 depthWrite = 0;
        quint8 globalBinding = 0;
        quint8 perDrawBinding = 0;
        quint8 textureBinding = 0;
        quint8 globalStages = 0;
        quint8 perDrawStages = 0;
        quint8 textureStages = 0;
        quint8 hasPerDraw = 0;
        bool operator==(const PipelineKey& other) const = default;
    };

    friend uint qHash(const PipelineKey& key, uint seed) noexcept;

    struct PipelineRecord {
        PipelineKey key;
        QRhiGraphicsPipeline* pipeline = nullptr;
        QRhiShaderResourceBindings* layout = nullptr;
        int refCount = 0;
    };

    struct Item {
        Item();
        ~Item();

        QRhiBuffer* vertexBuffer = nullptr;
        QRhiBuffer* indexBuffer = nullptr;
        QRhiBuffer* uniformBuffer = nullptr;
        QRhiTexture* texture = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
        PipelineRecord* pipelineRecord = nullptr;

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

        cwRhiTexturedItems* owner = nullptr;

        void initializeResources(const ResourceUpdateData &data, const SharedItemData &sharedData);
        void ensurePipeline(const ResourceUpdateData& data, const SharedItemData &sharedData, const QRhiVertexInputLayout& layout);
        void updateGeometryBuffers(const ResourceUpdateData& data);
        void updateTextureResource(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void updateUniformBuffer(const ResourceUpdateData& data);
        void createShaderResourceBindings(const ResourceUpdateData& data, const SharedItemData &sharedData);
        void releasePipeline();
    };

    QHash<uint32_t, Item*> m_items;
    bool m_resourcesInitialized = false;
    bool m_visible = true;
    QPointer<cwRenderTexturedItems> m_renderItems;
    SharedItemData m_sharedData;
    QRhiVertexInputLayout m_inputLayout;
    QHash<PipelineKey, PipelineRecord*> m_pipelineCache;

    static QRhiShaderResourceBinding::StageFlags toRhiStages(cwRenderMaterialState::ShaderStages stages);
    static QRhiGraphicsPipeline::CullMode toRhiCullMode(cwRenderMaterialState::CullMode mode);
    static QRhiGraphicsPipeline::FrontFace toRhiFrontFace(cwRenderMaterialState::FrontFace face);
    static QRhiGraphicsPipeline::TargetBlend toBlendState(const cwRenderMaterialState& material);
    static quint8 toStageMask(cwRenderMaterialState::ShaderStages stages);

    PipelineKey makePipelineKey(QRhiRenderPassDescriptor* renderPass,
                                int sampleCount,
                                const cwRenderMaterialState& material) const;
    PipelineRecord* acquirePipeline(const PipelineKey& key,
                                    const cwRenderMaterialState& material,
                                    QRhi* rhi,
                                    const QRhiVertexInputLayout& layout,
                                    const SharedItemData& sharedData);
    void releasePipeline(PipelineRecord* record);
    static cwRHIObject::RenderPass toRenderPass(cwRenderMaterialState::RenderPass pass);
};



#endif // CWRHITEXTUREDITEMS_H
