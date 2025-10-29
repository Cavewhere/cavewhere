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

        void initializeResources(const ResourceUpdateData &data, const SharedScrapData &sharedData);
        void createShadeResourceBindings(const ResourceUpdateData& data, const SharedScrapData &sharedData);
    };


    QHash<uint32_t, Item*> m_items;
    int m_maxScrapId = 0;
    bool m_resourcesInitialized = false;

    bool m_visible = true;

    QPointer<cwRenderTexturedItems> m_renderScraps;

    QRhiShaderResourceBindings* m_globalSrb = nullptr;

    SharedScrapData m_sharedScrapData;

    QRhiGraphicsPipeline* m_pipeline = nullptr;

    void initializePipeline(const ResourceUpdateData& data);
};



#endif // CWRHITEXTUREDITEMS_H
