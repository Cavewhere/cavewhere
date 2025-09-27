#include "cwGltfLoader.h"

// tinygltf
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

// Qt
#include <QtGlobal>
#include <QFileInfo>

// Local helpers kept in the same namespace
namespace cw::gltf {

// ---------- Helpers: attribute/index plumbing ----------

static AttributeView attributeView(const tinygltf::Model& model,
                                   const tinygltf::Accessor& accessor)
{
    AttributeView view;

    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];

    const size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const int componentCount = tinygltf::GetNumComponentsInType(accessor.type);

    const size_t stride = bufferView.byteStride == 0
                              ? componentSize * componentCount
                              : bufferView.byteStride;

    const size_t start = bufferView.byteOffset + accessor.byteOffset;
    const size_t length = accessor.count * stride;

    view.data = buffer.data.data() + start;
    view.byteLength = static_cast<qsizetype>(length);
    view.byteStride = static_cast<qsizetype>(stride);
    view.componentCount = componentCount;
    view.componentSize = static_cast<int>(componentSize);
    view.normalized = accessor.normalized;

    return view;
}

static QRhiCommandBuffer::IndexFormat toRhiIndexFormat(int gltfComponentType)
{
    if (gltfComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        return QRhiCommandBuffer::IndexUInt16;
    }
    if (gltfComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        return QRhiCommandBuffer::IndexUInt32;
    }
    // UNSIGNED_BYTE not supported by QRhi: convert to 16-bit when encountered.
    return QRhiCommandBuffer::IndexUInt16;
}

// ---------- CPU builders ----------

static PrimitiveCPU buildPrimitiveCPU(const tinygltf::Model& model,
                                      const tinygltf::Primitive& prim)
{
    const auto posIt = prim.attributes.find("POSITION");
    Q_ASSERT(posIt != prim.attributes.end());

    const tinygltf::Accessor& posAcc = model.accessors[posIt->second];
    AttributeView posView = attributeView(model, posAcc);
    Q_ASSERT(posAcc.type == TINYGLTF_TYPE_VEC3);

    AttributeView norView;
    bool hasNormal = false;
    if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) {
        const tinygltf::Accessor& acc = model.accessors[it->second];
        norView = attributeView(model, acc);
        hasNormal = true;
    }

    AttributeView tanView;
    bool hasTangent = false;
    if (auto it = prim.attributes.find("TANGENT"); it != prim.attributes.end()) {
        const tinygltf::Accessor& acc = model.accessors[it->second];
        tanView = attributeView(model, acc); // vec4
        hasTangent = true;
    }

    AttributeView uv0View;
    bool hasTexCoord0 = false;
    if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) {
        const tinygltf::Accessor& acc = model.accessors[it->second];
        uv0View = attributeView(model, acc); // vec2
        hasTexCoord0 = true;
    }

    const int vertexCount = posAcc.count;

    struct Layout {
        int positionOffset = 0;
        int normalOffset = -1;
        int tangentOffset = -1;
        int texCoord0Offset = -1;
        int stride = 0;
    } layout;

    layout.positionOffset = 0;
    layout.stride = sizeof(float) * 3;
    if (hasNormal) {
        layout.normalOffset = layout.stride;
        layout.stride += sizeof(float) * 3;
    }
    if (hasTangent) {
        layout.tangentOffset = layout.stride;
        layout.stride += sizeof(float) * 4;
    }
    if (hasTexCoord0) {
        layout.texCoord0Offset = layout.stride;
        layout.stride += sizeof(float) * 2;
    }

    QByteArray interleaved;
    interleaved.resize(layout.stride * vertexCount);

    auto writeVec = [&](uint8_t* dst, const AttributeView& src, int vertexIndex, int requiredComps, bool isNormalized) {
        const uint8_t* srcPtr = src.data + vertexIndex * src.byteStride;

        for (int c = 0; c < requiredComps; c++) {
            float f = 0.0f;

            switch (src.componentSize) {
            case 1: {
                const uint8_t* v = reinterpret_cast<const uint8_t*>(srcPtr) + c;
                if (isNormalized) {
                    f = static_cast<float>(*v) / 255.0f;
                } else {
                    f = static_cast<float>(*v);
                }
            } break;
            case 2: {
                const uint16_t* v = reinterpret_cast<const uint16_t*>(srcPtr);
                if (isNormalized) {
                    f = static_cast<float>(v[c]) / 65535.0f;
                } else {
                    f = static_cast<float>(v[c]);
                }
            } break;
            case 4: {
                const float* v = reinterpret_cast<const float*>(srcPtr);
                f = v[c];
            } break;
            default: {
                // Extend for other component sizes if needed.
            } break;
            }

            reinterpret_cast<float*>(dst)[c] = f;
        }
    };

    for (int i = 0; i < vertexCount; i++) {
        uint8_t* base = reinterpret_cast<uint8_t*>(interleaved.data()) + i * layout.stride;

        // POSITION (float3)
        std::memcpy(base + layout.positionOffset,
                    posView.data + i * posView.byteStride,
                    sizeof(float) * 3);

        if (hasNormal) {
            writeVec(base + layout.normalOffset, norView, i, 3, norView.normalized);
        }
        if (hasTangent) {
            writeVec(base + layout.tangentOffset, tanView, i, 4, tanView.normalized);
        }
        if (hasTexCoord0) {
            writeVec(base + layout.texCoord0Offset, uv0View, i, 2, uv0View.normalized);
        }
    }

    PrimitiveCPU out;
    out.vertexInterleaved = interleaved;
    out.vertexCount = vertexCount;
    out.hasNormal = hasNormal;
    out.hasTangent = hasTangent;
    out.hasTexCoord0 = hasTexCoord0;

    if (prim.indices >= 0) {
        const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
        AttributeView idxView = attributeView(model, idxAcc);

        // Expand 8-bit indices to 16-bit (QRhi has no 8-bit index format)
        if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            out.indexFormat = QRhiCommandBuffer::IndexUInt16;
            out.indexCount = idxAcc.count;
            out.indexData.resize(sizeof(uint16_t) * idxAcc.count);
            const uint8_t* src = reinterpret_cast<const uint8_t*>(idxView.data);
            uint16_t* dst = reinterpret_cast<uint16_t*>(out.indexData.data());
            for (int i = 0; i < idxAcc.count; i++) {
                dst[i] = static_cast<uint16_t>(src[i]);
            }
        } else {
            out.indexFormat = toRhiIndexFormat(idxAcc.componentType);
            out.indexCount = idxAcc.count;
            out.indexData = QByteArray(reinterpret_cast<const char*>(idxView.data), idxView.byteLength);
        }
    } else {
        // Generate 0..N-1 indices
        out.indexFormat = QRhiCommandBuffer::IndexUInt32;
        out.indexCount = vertexCount;
        out.indexData.resize(sizeof(uint32_t) * vertexCount);
        auto* dst = reinterpret_cast<uint32_t*>(out.indexData.data());
        for (int i = 0; i < vertexCount; i++) {
            dst[i] = static_cast<uint32_t>(i);
        }
    }

    return out;
}

static TextureCPU loadTextureCPU(const tinygltf::Model& model,
                                 int textureIndex,
                                 bool isSRGB)
{
    TextureCPU tex;
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size())) {
        return tex;
    }

    const auto& texture = model.textures[textureIndex];
    const int imageIndex = texture.source;
    if (imageIndex < 0) {
        return tex;
    }

    const auto& img = model.images[imageIndex];

    tex.width = img.width;
    tex.height = img.height;
    tex.isSRGB = isSRGB;

    // Ensure RGBA8
    if (img.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        // tinygltf ensures 4 components by default for most loaders; if not, convert here
        const size_t expected = static_cast<size_t>(img.width) * static_cast<size_t>(img.height) * 4;
        if (img.image.size() == expected) {
            tex.pixels = QByteArray(reinterpret_cast<const char*>(img.image.data()),
                                    static_cast<int>(img.image.size()));
        } else {
            // Fallback conversion could be placed here.
        }
    }

    return tex;
}

static MaterialCPU loadMaterialCPU(const tinygltf::Model& model, int materialIndex)
{
    MaterialCPU m;
    if (materialIndex < 0 || materialIndex >= static_cast<int>(model.materials.size())) {
        return m;
    }

    const auto& mat = model.materials[materialIndex];

    if (!mat.pbrMetallicRoughness.baseColorFactor.empty()) {
        m.baseColorFactor = QVector4D(
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[0]),
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[1]),
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[2]),
            static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3]));
    }

    m.baseColorTextureIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
    m.metallicRoughnessTextureIndex = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
    m.metallicFactor = static_cast<float>(mat.pbrMetallicRoughness.metallicFactor);
    m.roughnessFactor = static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor);
    m.normalTextureIndex = mat.normalTexture.index;

    return m;
}

static QMatrix4x4 localMatrix(const tinygltf::Node& node)
{
    QMatrix4x4 m;

    if (node.matrix.size() == 16) {
        // glTF is column-major; Qt’s QMatrix4x4 constructor expects column-major float*
        m = QMatrix4x4(reinterpret_cast<const float*>(node.matrix.data()));
    } else {
        QVector3D translation;
        QVector3D scale(1, 1, 1);
        QQuaternion rotation;

        if (node.translation.size() == 3) {
            translation = QVector3D(node.translation[0], node.translation[1], node.translation[2]);
        }
        if (node.scale.size() == 3) {
            scale = QVector3D(node.scale[0], node.scale[1], node.scale[2]);
        }
        if (node.rotation.size() == 4) {
            rotation = QQuaternion(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        }

        m.translate(translation);
        m.rotate(rotation);
        m.scale(scale);
    }


    // //I'm not sure why 45.0 degrees works it should be -90 or 90
    // QQuaternion rotateToZUp = QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), 45.0);
    // // QMatrix

    // m.rotate(rotateToZUp);
    // m.rotate(45.0, QVector3D(1.0, 0.0, 0.0));
    // qDebug() << "Model matrix after:" << m;
    // qDebug() << "Inverted:" << m.inverted();

    return m;
}

static void gatherMeshesRecursive(const tinygltf::Model& model,
                                  int nodeIndex,
                                  const QMatrix4x4& parent,
                                  QVector<MeshCPU>& outMeshes)
{
    const auto& node = model.nodes[nodeIndex];
    QMatrix4x4 world = parent * localMatrix(node);

    if (node.mesh >= 0) {
        const auto& gltfMesh = model.meshes[node.mesh];

        // Emit one MeshCPU aggregating all its primitives
        MeshCPU m;
        m.modelMatrix = world;

        for (const auto& prim : gltfMesh.primitives) {
            m.primitives.push_back(buildPrimitiveCPU(model, prim));
            // Simple “dominant” material; switch to per-primitive if needed
            m.material = loadMaterialCPU(model, prim.material);
        }

        outMeshes.push_back(std::move(m));
    }

    for (int child : node.children) {
        gatherMeshesRecursive(model, child, world, outMeshes);
    }
}

// ---------- RHI upload helpers ----------

// static TextureRHI createTextureRhi(QRhi* rhi, QRhiResourceUpdateBatch* rub, const TextureCPU& cpu)
// {
//     TextureRHI t;
//     if (cpu.width <= 0 || cpu.height <= 0 || cpu.pixels.isEmpty()) {
//         return t;
//     }

//     // Prefer SRGB8A8 format if your backend/Qt exposes it; QRhi::RGBA8 is widely supported.
//     QRhiTexture::Format format = QRhiTexture::RGBA8;

//     t.texture = rhi->newTexture(format, QSize(cpu.width, cpu.height), 1,
//                                 QRhiTexture::UsedAsTransferSource);
//                                     // | QRhiTexture::UsedWithTransferDestination
//                                     // | QRhiTexture::UsedWithSampled);
//     t.texture->create();

//     t.sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
//                                 QRhiSampler::Repeat, QRhiSampler::Repeat);
//     t.sampler->create();

//     QRhiTextureSubresourceUploadDescription subDesc(cpu.pixels);
//     QRhiTextureUploadEntry entry(0, 0, subDesc);
//     QRhiTextureUploadDescription desc({ entry });

//     rub->uploadTexture(t.texture, desc);

//     return t;
// }

// static PrimitiveRHI createPrimitiveRhi(QRhi* rhi, QRhiResourceUpdateBatch* rub, const PrimitiveCPU& cpu)
// {
//     PrimitiveRHI p;

//     const int vertexStride = cpu.vertexCount > 0
//                                  ? cpu.vertexInterleaved.size() / cpu.vertexCount
//                                  : 0;

//     p.vertexStride = vertexStride;
//     p.indexCount = cpu.indexCount;
//     p.indexFormat = cpu.indexFormat;

//     p.vertexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, cpu.vertexInterleaved.size());
//     p.vertexBuffer->create();

//     p.indexBuffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, cpu.indexData.size());
//     p.indexBuffer->create();

//     rub->uploadStaticBuffer(p.vertexBuffer, 0, cpu.vertexInterleaved.size(), cpu.vertexInterleaved.constData());
//     rub->uploadStaticBuffer(p.indexBuffer, 0, cpu.indexData.size(), cpu.indexData.constData());

//     return p;
// }

// ---------- Public API ----------

// GLTFToRHIResult Loader::buildRhiFromFile(QRhi* rhi,
//                                          const QString& filePath,
//                                          QRhiResourceUpdateBatch*& outResourceUpdates)
// {
//     GLTFToRHIResult result;

//     // if (rhi == nullptr) {
//     //     return result;
//     // }

//     tinygltf::TinyGLTF loader;
//     tinygltf::Model model;
//     std::string error;
//     std::string warning;

//     bool ok = false;
//     if (filePath.endsWith(".glb", Qt::CaseInsensitive)) {
//         ok = loader.LoadBinaryFromFile(&model, &error, &warning, filePath.toStdString());
//     } else {
//         ok = loader.LoadASCIIFromFile(&model, &error, &warning, filePath.toStdString());
//     }
//     if (!ok) {
//         return result;
//     }

//     // CPU: collect meshes via scene roots (supports full node parent->child transforms)
//     QVector<int> roots;
//     int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
//     if (!model.scenes.empty()) {
//         const auto& scene = model.scenes[sceneIndex];
//         roots = QVector<int>(scene.nodes.begin(), scene.nodes.end());
//     }

//     for (int n : std::as_const(roots)) {
//         gatherMeshesRecursive(model, n, QMatrix4x4(), result.sceneCPU.meshes);
//     }

//     // CPU: collect textures actually referenced by materials (here we provision array sized to textures count)
//     result.sceneCPU.textures.resize(static_cast<int>(model.textures.size()));

//     // Load only the textures we actually need (baseColor here; extend as desired)
//     for (const MeshCPU& mesh : std::as_const(result.sceneCPU.meshes)) {
//         if (mesh.material.baseColorTextureIndex >= 0) {
//             const int ti = mesh.material.baseColorTextureIndex;
//             if (result.sceneCPU.textures[ti].width == 0) {
//                 result.sceneCPU.textures[ti] = loadTextureCPU(model, ti, /*isSRGB*/ true);
//             }
//         }
//     }

//     // result.sceneCPU.dump();

//     // // RHI: create resources + uploads
//     // outResourceUpdates = rhi->nextResourceUpdateBatch();

//     // result.sceneRHI.textures.resize(result.sceneCPU.textures.size());
//     // for (int i = 0; i < result.sceneCPU.textures.size(); i++) {
//     //     const TextureCPU& t = result.sceneCPU.textures[i];
//     //     if (t.width > 0) {
//     //         result.sceneRHI.textures[i] = createTextureRhi(rhi, outResourceUpdates, t);
//     //     }
//     // }

//     // result.sceneRHI.meshes.reserve(result.sceneCPU.meshes.size());
//     // for (const MeshCPU& m : result.sceneCPU.meshes) {
//     //     MeshRHI mr;
//     //     mr.modelMatrix = m.modelMatrix;

//     //     for (const PrimitiveCPU& pcpu : m.primitives) {
//     //         mr.primitives.push_back(createPrimitiveRhi(rhi, outResourceUpdates, pcpu));
//     //     }

//     //     MaterialRHI matRhi;
//     //     matRhi.baseColor = m.material.baseColorTextureIndex;
//     //     matRhi.baseColorFactor = m.material.baseColorFactor;
//     //     matRhi.metallicFactor = m.material.metallicFactor;
//     //     matRhi.roughnessFactor = m.material.roughnessFactor;
//     //     // Extend: mr.material.metallicRoughness, mr.material.normalMap …
//     //     mr.material = matRhi;

//     //     result.sceneRHI.meshes.push_back(std::move(mr));
//     // }

//     result.success = true;
//     return result;
// }

SceneCPU Loader::loadGltf(const QString &filePath)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string error;
    std::string warning;

    SceneCPU scene;

    bool ok = false;
    if (filePath.endsWith(".glb", Qt::CaseInsensitive)) {
        ok = loader.LoadBinaryFromFile(&model, &error, &warning, filePath.toStdString());
    } else {
        ok = loader.LoadASCIIFromFile(&model, &error, &warning, filePath.toStdString());
    }
    if (!ok) {
        return scene;
    }

    // CPU: collect meshes via scene roots (supports full node parent->child transforms)
    QVector<int> roots;
    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (!model.scenes.empty()) {
        const auto& scene = model.scenes[sceneIndex];
        roots = QVector<int>(scene.nodes.begin(), scene.nodes.end());
    }

    for (int n : std::as_const(roots)) {
        gatherMeshesRecursive(model, n, QMatrix4x4(), scene.meshes);
    }

    // CPU: collect textures actually referenced by materials (here we provision array sized to textures count)
    scene.textures.resize(static_cast<int>(model.textures.size()));

    // Load only the textures we actually need (baseColor here; extend as desired)
    for (const MeshCPU& mesh : std::as_const(scene.meshes)) {
        if (mesh.material.baseColorTextureIndex >= 0) {
            const int ti = mesh.material.baseColorTextureIndex;
            if (scene.textures[ti].width == 0) {
                scene.textures[ti] = loadTextureCPU(model, ti, /*isSRGB*/ true);
            }
        }
    }

    return scene;
}

void SceneCPU::dump() const
{
    qDebug() << "SceneCPU:";
    qDebug() << " Mesh count:" << meshes.size();
    qDebug() << " Texture count:" << textures.size();

    for (int mi = 0; mi < meshes.size(); ++mi) {
        const MeshCPU& mesh = meshes[mi];
        qDebug() << "  Mesh" << mi
                 << "Primitives:" << mesh.primitives.size()
                 << "Matrix:" << mesh.modelMatrix;

        // Material
        const MaterialCPU& mat = mesh.material;
        qDebug() << "    Material:"
                 << " baseColorTex=" << mat.baseColorTextureIndex
                 << " metallicRoughnessTex=" << mat.metallicRoughnessTextureIndex
                 << " normalTex=" << mat.normalTextureIndex
                 << " baseColorFactor=" << mat.baseColorFactor
                 << " metallic=" << mat.metallicFactor
                 << " roughness=" << mat.roughnessFactor;

        // Primitives
        for (int pi = 0; pi < mesh.primitives.size(); ++pi) {
            const PrimitiveCPU& prim = mesh.primitives[pi];
            qDebug() << "    Primitive" << pi
                     << " verts=" << prim.vertexCount
                     << " idx=" << prim.indexCount
                     << " stride=" << (prim.vertexCount > 0
                                           ? prim.vertexInterleaved.size() / prim.vertexCount
                                           : 0)
                     << " hasNormal=" << prim.hasNormal
                     << " hasTangent=" << prim.hasTangent
                     << " hasTexCoord0=" << prim.hasTexCoord0;
        }
    }

    for (int ti = 0; ti < textures.size(); ++ti) {
        const TextureCPU& tex = textures[ti];
        qDebug() << "  Texture" << ti
                 << " size=" << tex.width << "x" << tex.height
                 << " bytes=" << tex.pixels.size()
                 << " isSRGB=" << tex.isSRGB;
    }
}

} // namespace cw::gltf
