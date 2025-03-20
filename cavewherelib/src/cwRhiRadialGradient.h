#ifndef CWRADIALGRADIENTRHIOBJECT_H
#define CWRADIALGRADIENTRHIOBJECT_H

#include "cwRHIObject.h"

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

private:
    QRhiVertexInputLayout m_inputLayout;
    QRhiGraphicsPipeline* m_pipeline = nullptr;
    QRhiShaderResourceBindings* m_srb = nullptr;
    QRhiBuffer* m_vertexBuffer = nullptr;
    QRhiBuffer* m_uniformBuffer = nullptr;

    struct UniformData {
        QVector4D color1;
        QVector4D color2;
        QVector2D center;
        float radius;
        float radiusOffset;
        //make sure you add padding if you put more parameters in here
    } m_uniformData;
};

#endif // CWRADIALGRADIENTRHIOBJECT_H
