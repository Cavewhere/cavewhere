#ifndef CWRHIGLTF_H
#define CWRHIGLTF_H

// Qt / RHI
#include <QHash>
#include <QVector>
#include <QMatrix4x4>
#include <QVector4D>
#include <rhi/qrhi.h>

// Our includes
#include "cwRHIObject.h"
#include "cwGltfLoader.h"

class cwRHIGltf : public cwRHIObject
{
public:
    cwRHIGltf();
    ~cwRHIGltf() override;

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    void render(const RenderData& data) override;

private:
    struct PrimitiveGPU {
        QRhiBuffer* vertex;
        QRhiBuffer* index;
        int vertexStride = 0;
        int indexCount = 0;
        QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt32;
        // Attribute mask (which streams are present in the interleaved buffer)
        bool hasNormal = false;
        bool hasTangent = false;
        bool hasTexCoord0 = false;
    };

    struct MeshGPU {
        QVector<PrimitiveGPU> primitives;
        QMatrix4x4 modelMatrix;
        int baseColorTextureIndex = -1;  // into m_textures
        QVector4D baseColorFactor = QVector4D(1,1,1,1);
    };

    struct TextureGPU {
        QRhiTexture* texture;
        QRhiSampler* sampler;
        bool valid() const { return texture && sampler; }
    };

    struct PipelineKey {
        bool hasNormal = false;
        bool hasTangent = false;
        bool hasTexCoord0 = false;
        int stride = 0;

        bool operator==(const PipelineKey& o) const {
            return hasNormal == o.hasNormal
                   && hasTangent == o.hasTangent
                   && hasTexCoord0 == o.hasTexCoord0
                   && stride == o.stride;
        }

        friend size_t qHash(const PipelineKey& k, size_t seed) noexcept {
            // Compact bit-pack: [tangent|normal|uv0|stride...]
            quint64 bits = (k.hasNormal ? 1u : 0u)
                           | ((k.hasTangent ? 1u : 0u) << 1)
                           | ((k.hasTexCoord0 ? 1u : 0u) << 2)
                           | (quint64(k.stride) << 3);
            return qHash(bits, seed); // uses Qt’s quint64 hash
        }
    };

    struct PipelineKeyHash {
        size_t operator()(const PipelineKey& k) const noexcept {
            quint64 bits = (k.hasNormal ? 1u : 0u)
            | ((k.hasTangent ? 1u : 0u) << 1)
                | ((k.hasTexCoord0 ? 1u : 0u) << 2);
            bits |= (quint64(k.stride) << 3);
            return qHash(bits);
        }
    };

    struct PipelinePack {
        QRhiGraphicsPipeline* pipeline;
        // QRhiShaderResourceBindings* shaderResourceBindings; // set 0: 0 scene ubo, 1 model ubo, 2 material ubo, 3 baseColor
    };

    // CPU data snapshot from cwRenderGLTF
    cw::gltf::SceneCPU m_sceneCPU;

    // GPU resources
    QVector<MeshGPU> m_meshes;
    QVector<TextureGPU> m_textures;

    // Common dynamic UBOs
    QRhiBuffer* m_sceneUbo;    // mat4 viewProj
    QRhiBuffer* m_modelUbo;    // mat4 model
    QRhiBuffer* m_materialUbo; // vec4 baseColorFactor (extend as needed)

    // Pipeline cache keyed by vertex layout
    QHash<PipelineKey, PipelinePack> m_pipelines;

    // // Pipeline cache keyed by vertex layout
    // QHash<PipelineKey, PipelinePack, PipelineKeyHash> m_pipelines;

    // Shaders
    QShader m_vertexShader;
    QShader m_fragmentShader;

    // Global shader resources
    // QRhiShaderResourceBindings* m_globalShaderResourceBindings = nullptr;

    // Helpers
    void ensureShaders();
    PipelinePack* ensurePipeline(QRhi* rhi, const PipelineKey& key, QRhiRenderPassDescriptor* rpDesc);
    static QRhiVertexInputLayout makeInputLayout(const PipelineKey& key);
    static QRhiGraphicsPipeline::TargetBlend premultipliedAlphaBlend();

    // Resource build helpers
    void buildTextures(QRhi* rhi, QRhiResourceUpdateBatch* rub);
    void buildMeshes(QRhi* rhi, QRhiResourceUpdateBatch* rub);

private: // member data last
    bool m_initialized = false;
    bool m_resourcesDirty = false;
    // bool m_modelMatrixDirty = false;

    QMatrix4x4 m_modelMatrix;
};

// inline size_t qHash(const cwRHIGltf::PipelineKey& k, size_t seed = 0) noexcept
// {
//     // Compact bit-pack: [tangent|normal|uv0|stride...]
//     quint64 bits = (k.hasNormal ? 1u : 0u)
//                    | ((k.hasTangent ? 1u : 0u) << 1)
//                    | ((k.hasTexCoord0 ? 1u : 0u) << 2)
//                    | (quint64(k.stride) << 3);
//     return qHash(bits, seed); // uses Qt’s quint64 hash
// }

#endif // CWRHIGLTF_H
