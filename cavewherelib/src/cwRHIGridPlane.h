#ifndef CWRHIGRIDPLANE_H
#define CWRHIGRIDPLANE_H

#include "cwGLObject.h"

class cwRHIGridPlane : public cwRHIObject {
public:
    cwRHIGridPlane();
    virtual ~cwRHIGridPlane();

    virtual void initialize(const ResourceUpdateData &data) override;
    virtual void synchronize(const SynchronizeData& data) override;
    virtual void updateResources(const ResourceUpdateData&) override;
    virtual void render(const RenderData& data) override;

private:
    QRhi* m_rhi = nullptr;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_uniformBuffer = nullptr;
    QRhiGraphicsPipeline* m_pipeline = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    QMatrix4x4 m_modelMatrix;
    QMatrix4x4 m_mvpMatrix;

    // QPointer<cwGLGridPlane> m_guiObject; // Pointer to the GUI object

    bool m_resourcesInitialized = false;

    void initializeResources(const ResourceUpdateData& data);
    void updateUniforms(QRhiResourceUpdateBatch* resourceUpdates);
};

#endif // CWRHIGRIDPLANE_H
