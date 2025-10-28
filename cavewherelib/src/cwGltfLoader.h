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
    QVector<cwGeometry> geometries;
    MaterialCPU material; // keep simple; promote to per-primitive if you prefer
    QMatrix4x4 modelMatrix; // world transform (scene graph–accumulated)
};

struct SceneCPU {
    QVector<MeshCPU> meshes;
    QVector<TextureCPU> textures;

    void dump() const; // declaration
};


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
