#ifndef CWRHIPIPELINETYPES_H
#define CWRHIPIPELINETYPES_H

// Qt includes
#include <rhi/qrhi.h>
#include <QString>

// Identity of a graphics pipeline: everything that, if changed, requires a
// different QRhiGraphicsPipeline. Used as the key of the scene's shared pipeline
// cache and of each object's cwRhiPipelineSet. Lives in its own leaf header so
// both cwRhiScene and cwRhiPipelineSet can share it without an include cycle
// (cwRhiScene.h needs cwRHIObject.h, which needs cwRhiPipelineSet.h).
struct cwRhiPipelineKey {
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
    quint8 topology = static_cast<quint8>(QRhiGraphicsPipeline::Triangles);
    bool operator==(const cwRhiPipelineKey& other) const = default;
};

size_t qHash(const cwRhiPipelineKey& key, size_t seed) noexcept;

// A cached graphics pipeline plus its shared-resource-bindings layout, reference
// counted by cwRhiScene's pipeline cache. cwRhiPipelineSet holds references to
// these; cwRhiScene owns their lifetime (acquirePipeline / releasePipeline).
struct cwRhiPipelineRecord {
    cwRhiPipelineKey key;
    QRhiGraphicsPipeline* pipeline = nullptr;
    QRhiShaderResourceBindings* layout = nullptr;
    quint32 refCount = 0;
};

#endif // CWRHIPIPELINETYPES_H
