#ifndef CWGLTFLOADER_H
#define CWGLTFLOADER_H

// Qt
#include <QByteArray>
#include <QMatrix4x4>
#include <QPointer>
#include <QQuaternion>
#include <QVector>
#include <QVector4D>

// Qt RHI
#include <rhi/qrhi.h>

// Our includes
#include "cwGeometry.h"

// tinygltf (forward declare to avoid heavy includes in headers)
namespace tinygltf {
class Model;
class Node;
class Accessor;
class Primitive;
}

// Namespace wrapper so this doesn’t bleed into global
namespace cw::gltf {

// ---------- CPU-side data ----------

struct AttributeView {
    const uint8_t* data = nullptr;
    qsizetype byteLength = 0;
    qsizetype byteStride = 0;
    int componentCount = 0;
    int componentSize = 0;
    bool normalized = false;
};

struct PrimitiveCPU {
    QByteArray vertexInterleaved;
    QByteArray indexData;
    int vertexCount = 0;
    int indexCount = 0;
    QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt16;

    bool hasNormal = false;
    bool hasTangent = false;
    bool hasTexCoord0 = false;

    QVector3D vertex(size_t index) const
    {
        Q_ASSERT(index < vertexCount);

        const int stride = vertexStrideBytes();
        const char* const raw = vertexInterleaved.constData();

        const char* const vptr = raw + index * stride;
        const QVector3D* const vertex = reinterpret_cast<const QVector3D*>(vptr);

        // position is always the first 3 floats in the interleaved layout
        return *vertex;
    }
    void setVertex(QVector3D vertex, size_t index)
    {
        Q_ASSERT(index < vertexCount);

        int stride = vertexStrideBytes();
        char* const raw = vertexInterleaved.data();

        char* const vptr = raw + index * stride;
        QVector3D* floats = reinterpret_cast<QVector3D*>(vptr);

        *floats = vertex;
    }

    int vertexStrideBytes() const
    {
        int floatComponentCount = 3; // position: vec3 (always present)
        if (hasNormal) {
            floatComponentCount += 3; // normal: vec3
        }
        if (hasTangent) {
            floatComponentCount += 4; // tangent: vec4
        }
        if (hasTexCoord0) {
            floatComponentCount += 2; // uv0: vec2
        }
        return static_cast<int>(floatComponentCount * sizeof(float));
    }

};

struct TextureCPU {
    QByteArray pixels; // RGBA8
    int width = 0;
    int height = 0;
    bool isSRGB = false;
};

struct MaterialCPU {
    int baseColorTextureIndex = -1;
    QVector4D baseColorFactor = QVector4D(1, 1, 1, 1);
    int metallicRoughnessTextureIndex = -1;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    int normalTextureIndex = -1;
};

struct MeshCPU {
    QVector<PrimitiveCPU> primitives;
    MaterialCPU material; // keep simple; promote to per-primitive if you prefer
    QMatrix4x4 modelMatrix; // world transform (scene graph–accumulated)

    cwGeometry toGeometry() const;

};

struct SceneCPU {
    QVector<MeshCPU> meshes;
    QVector<TextureCPU> textures;

    // const bool operator==(const SceneCPU& other) {
    //     return meshes == other.meshes;
    // }

    // const bool operator!=(const SceneCPU& other) {
    //     return !operator==(other);
    // }

    void dump() const; // declaration
};

// // ---------- RHI-side data ----------

// struct PrimitiveRHI {
//     QRhiBuffer* vertexBuffer;
//     QRhiBuffer* indexBuffer;
//     int indexCount = 0;
//     int vertexStride = 0;
//     QRhiCommandBuffer::IndexFormat indexFormat = QRhiCommandBuffer::IndexUInt16;
// };

// struct TextureRHI {
//     QRhiTexture* texture;
//     QRhiSampler* sampler;
// };

// struct MaterialRHI {
//     int baseColor = -1;          // index into texturesRhi
//     int metallicRoughness = -1;
//     int normalMap = -1;
//     QVector4D baseColorFactor = QVector4D(1, 1, 1, 1);
//     float metallicFactor = 1.0f;
//     float roughnessFactor = 1.0f;
// };

// struct MeshRHI {
//     QVector<PrimitiveRHI> primitives;
//     MaterialRHI material;
//     QMatrix4x4 modelMatrix;
// };

// struct SceneRHI {
//     QVector<MeshRHI> meshes;
//     QVector<TextureRHI> textures;

// };

// // ---------- Result (multiple values via struct) ----------

// struct GLTFToRHIResult {
//     SceneCPU sceneCPU;
//     SceneRHI sceneRHI;
//     bool success = false;
// };

// ---------- Loader API ----------

class Loader
{
public:
    static SceneCPU loadGltf(const QString& filePath);

private:
    // No instances
    Loader() = delete;
};

} // namespace cw::gltf

#endif // CWGLTFLOADER_H
