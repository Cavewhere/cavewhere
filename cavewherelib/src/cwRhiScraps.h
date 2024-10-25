#ifndef CWRHISCRAPS_H
#define CWRHISCRAPS_H

#include "cwRHIObject.h"
#include "cwTriangulatedData.h"
#include "cwImageTexture.h"
#include "cwFutureManagerToken.h"
#include "cwProject.h"

#include <QHash>
#include <QPointer>
#include <rhi/qrhi.h>

class cwScrap;
class cwRenderScraps;

class cwRhiScraps : public cwRHIObject
{
public:
    cwRhiScraps();
    ~cwRhiScraps();

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    void render(const RenderData& data) override;

private:
    struct RhiScrap {
        RhiScrap();
        ~RhiScrap();

        // struct ScrapUniform {
        //     QVector2D vTexCoordsScale;
        // };

        QRhiBuffer* vertexBuffer = nullptr;
        QRhiBuffer* indexBuffer = nullptr;
        QRhiBuffer* texCoordBuffer = nullptr;
        // QRhiBuffer* uniformBuffer = nullptr;
        QRhiTexture* texture = nullptr;
        QRhiSampler* sampler = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;

        int numberOfIndices = 0;
        // int scrapId = -1;

        cwTriangulatedData geometry;
        bool resourcesInitialized = false;

        void initializeResources(const ResourceUpdateData &data);
        void releaseResources();
    };


    QHash<cwScrap*, RhiScrap*> m_scraps;
    int m_maxScrapId = 0;
    bool m_resourcesInitialized = false;


    cwProject* m_project = nullptr;
    cwFutureManagerToken m_futureManagerToken;
    bool m_visible = true;

    QPointer<cwRenderScraps> m_renderScraps;

    QRhiShaderResourceBindings* m_globalSrb = nullptr;
    QRhiSampler* m_sampler = nullptr;

    QRhiGraphicsPipeline* m_pipeline = nullptr;

    void initializePipeline(const ResourceUpdateData& data);
};

#endif // CWRHISCRAPS_H
