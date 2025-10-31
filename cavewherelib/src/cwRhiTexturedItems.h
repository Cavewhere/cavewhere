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
    struct SharedScrapData {
        QRhiSampler* m_sampler = nullptr;
        QRhiTexture* m_loadingTexture = nullptr;
    };

    struct PipelineKey {
        QRhiRenderPassDescriptor* renderPass = nullptr;
        quint8 sampleCount = 1;
        quint64 vertexProgramId = 0;
        quint64 fragmentProgramId = 0;
        quint16 stride = 0;
        quint8 attributeMask = 0;  // bit3 = uv0
        quint8 topology = QRhiGraphicsPipeline::Triangles;
        quint8 cullMode = QRhiGraphicsPipeline::Back;
        quint8 frontFace = QRhiGraphicsPipeline::CCW;
        quint8 flags = 0;          // bit0 depthTest, bit1 depthWrite, bit2 premulAlpha
        bool operator==(const PipelineKey& o) const = default;
    };

    // inline size_t qHash(const PipelineKey& key, size_t seed = 0) {
    //     size_t hash = qHash(quintptr(key.renderPass), seed);
    //     // hash ^= qHash(key.sampleCount)
    //     //      ^ qHash(key.vertexProgramId)
    //     //      ^ qHash(key.fragmentProgramId);
    //     // hash ^= qHash(key.stride)
    //     //      ^ qHash(key.attributeMask);
    //     // hash ^= qHash(key.topology)
    //     //      ^ qHash(key.cullMode)
    //     //      ^ qHash(key.frontFace)
    //     //      ^ qHash(key.flags);
    //     return hash;
    // }

    struct PipelinePack {
        QRhiGraphicsPipeline* pipeline = nullptr;
        QRhiShaderResourceBindings* canonicalLayout = nullptr; // null resources, layout only
    };

    // class PipelineCache {
    // public:
    //     PipelinePack findOrCreate(const PipelineKey& key,
    //                               std::function<PipelinePack(const PipelineKey&)> factory) {
    //         auto it = m_cache.find(key);
    //         if (it != m_cache.end()) {
    //             return it.value();
    //         }
    //         PipelinePack pack = factory(key);
    //         if (pack.pipeline) {
    //             m_cache.insert(key, pack);
    //         }
    //         return pack;
    //     }
    // private: // member data last
    //     QHash<PipelineKey, PipelinePack> m_cache;
    // };

    struct Item {
        Item();
        ~Item();

        // struct ScrapUniform {
        //     QVector2D vTexCoordsScale;
        // };

        QRhiBuffer* vertexBuffer = nullptr;
        QRhiBuffer* indexBuffer = nullptr;
        // QRhiBuffer* texCoordBuffer = nullptr;
        // QRhiBuffer* uniformBuffer = nullptr;
        QRhiTexture* texture = nullptr;
        QRhiSampler* sampler = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;

        int numberOfIndices = 0;

        cwGeometry geometry;
        QImage image;

        bool resourcesInitialized = false;
        bool geometryNeedsUpdate = false;
        bool textureNeedsUpdate = false;
        bool pipelineNeedsUpdate = false;

        double memoryUsaged = 0.0;

        bool visible = true;

        void initializeResources(const ResourceUpdateData &data, const SharedScrapData &sharedData);
        void createShadeResourceBindings(const ResourceUpdateData& data, const SharedScrapData &sharedData);
    };


    QHash<uint32_t, Item*> m_items;
    int m_maxScrapId = 0;
    bool m_resourcesInitialized = false;

    bool m_visible = true;

    QPointer<cwRenderTexturedItems> m_renderScraps;

    QRhiShaderResourceBindings* m_globalSrb = nullptr;

    SharedScrapData m_sharedData;

    QRhiGraphicsPipeline* m_pipeline = nullptr;

    void initializePipeline(const ResourceUpdateData& data);
};



#endif // CWRHITEXTUREDITEMS_H
