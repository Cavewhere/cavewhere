#ifndef CWRHIGRIDPLANE_H
#define CWRHIGRIDPLANE_H

//Our includes
#include "cwRHIObject.h"
#include "cwRenderGridPlane.h"

//Qt includes
class QRhi;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiShaderResourceBindings;
#include <QMatrix4x4>


class cwRHIGridPlane : public cwRHIObject {
public:
    cwRHIGridPlane();
    virtual ~cwRHIGridPlane();

    virtual void initialize(const ResourceUpdateData &data) override;
    virtual void synchronize(const SynchronizeData& data) override;
    virtual void updateResources(const ResourceUpdateData&) override;
    virtual void render(const RenderData& data) override;

private:
    //Shader layout
    struct UniformData {
        float mvpMatrix[16];
        float modelMatrix[16];
    };

    QRhi* m_rhi = nullptr;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_uniformBuffer = nullptr;
    QRhiGraphicsPipeline* m_pipeline = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    cwTracked<QMatrix4x4> m_modelMatrix;
    QMatrix4x4 m_mvpMatrix;

    bool m_resourcesInitialized = false;

    void initializeResources(const ResourceUpdateData& data);
    void updateUniforms(QRhiResourceUpdateBatch* resourceUpdates);
};

#endif // CWRHIGRIDPLANE_H
