#ifndef CWRADIALGRADIENTRHIOBJECT_H
#define CWRADIALGRADIENTRHIOBJECT_H

#include "cwRHIObject.h"
#include "cwRhiFrameRenderer.h"

//Qt includes
#include <QMatrix4x4>
#include <QVector2D>
#include <QColor>
#include <QScopedPointer>
#include <rhi/qrhi.h>

class cwRhiRadialGradient : public cwRHIObject
{
public:
    cwRhiRadialGradient();
    ~cwRhiRadialGradient() override;

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    void render(const RenderData& data) override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

private:
    static constexpr const char* kVertexShaderPath = ":/shaders/radial_gradient.vert.qsb";
    static constexpr const char* kFragmentShaderPath = ":/shaders/radial_gradient.frag.qsb";

    QRhiVertexInputLayout m_inputLayout;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_uniformBuffer = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;

    cwRhiFrameRenderer* m_frame = nullptr;
    bool m_resourcesInitialized = false;

    struct UniformData {
        QVector4D color1;
        QVector4D color2;
        QVector2D center;
        float radius;
        float radiusOffset;
        //make sure you add padding if you put more parameters in here
    } m_uniformData;

    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;
};

#endif // CWRADIALGRADIENTRHIOBJECT_H
