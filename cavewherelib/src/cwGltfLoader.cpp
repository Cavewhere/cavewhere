#include "cwGltfLoader.h"
#include "cwGeometry.h"

// tinygltf
// stb_image is compiled out: every image goes through storeEncodedImageData()
// below, which keeps the encoded bytes instead of decoding them, so a build with
// stb linked in could only hide an accidental eager decode.
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

// Qt
#include <QtGlobal>
#include <QBuffer>
#include <QColorSpace>
#include <QFileInfo>
#include <QImageReader>

// Std
#include <algorithm>

// Local helpers kept in the same namespace
namespace cw::gltf {

namespace {
    //What "Loading glTF" hangs off itself: the parse, the mesh walk and the
    //texture decode
    constexpr int kLoadSteps = 3;

    //How many meshes the texture loop below has anything to do for
    qsizetype texturedMeshCount(const QVector<MeshCPU>& meshes)
    {
        return std::count_if(meshes.cbegin(), meshes.cend(), [](const MeshCPU& mesh) {
            return mesh.material.baseColorTextureIndex >= 0;
        });
    }
}

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

// ---------- CPU builders ----------

// Build a cwGeometry from a glTF primitive
static cwGeometry buildGeometryFromPrimitive(const tinygltf::Model& model,
                                             const tinygltf::Primitive& prim,
                                             const QVector<cwGeometry::AttributeDesc>& requestedLayout)
{
    // POSITION (required)
    const auto posIt = prim.attributes.find("POSITION");
    Q_ASSERT(posIt != prim.attributes.end());
    const tinygltf::Accessor& posAcc = model.accessors[posIt->second];
    Q_ASSERT(posAcc.type == TINYGLTF_TYPE_VEC3);

    // Optional attributes
    const bool hasNormal   = prim.attributes.find("NORMAL")      != prim.attributes.end();
    const bool hasTangent  = prim.attributes.find("TANGENT")     != prim.attributes.end();
    const bool hasTex0     = prim.attributes.find("TEXCOORD_0")  != prim.attributes.end();

    // Views
    AttributeView posView = attributeView(model, posAcc);

    AttributeView norView{};
    if (hasNormal) {
        const tinygltf::Accessor& acc = model.accessors.at(prim.attributes.at("NORMAL"));
        norView = attributeView(model, acc);
    }

    AttributeView tanView{};
    if (hasTangent) {
        const tinygltf::Accessor& acc = model.accessors.at(prim.attributes.at("TANGENT"));
        tanView = attributeView(model, acc);
    }

    AttributeView uv0View{};
    if (hasTex0) {
        const tinygltf::Accessor& acc = model.accessors.at(prim.attributes.at("TEXCOORD_0"));
        uv0View = attributeView(model, acc);
    }

    const int vertexCount = posAcc.count;

    // Helper to read a vector attribute into a QVector<T> with normalization support
    auto readVec = [](const AttributeView& src, int count, int comps, bool normalized) -> QVector<float> {
        QVector<float> out;
        out.resize(count * comps);
        for (int i = 0; i < count; i++) {
            const uint8_t* srcPtr = src.data + i * src.byteStride;
            for (int c = 0; c < comps; c++) {
                float f = 0.0f;
                switch (src.componentSize) {
                case 1: {
                    const uint8_t* v = reinterpret_cast<const uint8_t*>(srcPtr) + c;
                    f = normalized ? (static_cast<float>(*v) / 255.0f) : static_cast<float>(*v);
                } break;
                case 2: {
                    const uint16_t* v = reinterpret_cast<const uint16_t*>(srcPtr);
                    f = normalized ? (static_cast<float>(v[c]) / 65535.0f) : static_cast<float>(v[c]);
                } break;
                case 4: {
                    const float* v = reinterpret_cast<const float*>(srcPtr);
                    f = v[c];
                } break;
                default: {
                    Q_ASSERT(false);
                } break;
                }
                out[i * comps + c] = f;
            }
        }
        return out;
    };

    auto fillAttribute = [&](cwGeometry& geometry,
                             cwGeometry::Semantic semantic,
                             cwGeometry::AttributeFormat format,
                             const AttributeView* view) {
        const int comps = cwGeometry::VertexAttribute{semantic, format, 0}.componentCount();
        const bool hasView = view && view->data && view->componentCount == comps;

        switch (format) {
        case cwGeometry::AttributeFormat::Float: {
            const QVector<float> flat = hasView ? readVec(*view, vertexCount, comps, view->normalized)
                                                 : QVector<float>(vertexCount * comps, 0.0f);
            auto attribute = geometry.attribute(semantic);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, flat[i]);
            }
        } break;
        case cwGeometry::AttributeFormat::Vec2: {
            const QVector<float> flat = hasView ? readVec(*view, vertexCount, comps, view->normalized)
                                                 : QVector<float>(vertexCount * comps, 0.0f);
            auto attribute = geometry.attribute(semantic);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, QVector2D(flat[i * 2 + 0], flat[i * 2 + 1]));
            }
        } break;
        case cwGeometry::AttributeFormat::Vec3: {
            const QVector<float> flat = hasView ? readVec(*view, vertexCount, comps, view->normalized)
                                                 : QVector<float>(vertexCount * comps, 0.0f);
            auto attribute = geometry.attribute(semantic);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, QVector3D(flat[i * 3 + 0], flat[i * 3 + 1], flat[i * 3 + 2]));
            }
        } break;
        case cwGeometry::AttributeFormat::Vec4: {
            const QVector<float> flat = hasView ? readVec(*view, vertexCount, comps, view->normalized)
                                                 : QVector<float>(vertexCount * comps, 0.0f);
            auto attribute = geometry.attribute(semantic);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, QVector4D(flat[i * 4 + 0], flat[i * 4 + 1], flat[i * 4 + 2], flat[i * 4 + 3]));
            }
        } break;
        }
    };

    cwGeometry geometry;
    if (requestedLayout.isEmpty()) {
        // Build geometry with a declarative layout from available attributes
        QVector<cwGeometry::AttributeDesc> geometryDescription {
            { cwGeometry::Semantic::Position,  cwGeometry::AttributeFormat::Vec3 }
        };

        if (hasNormal) {
            geometryDescription.append({cwGeometry::Semantic::Normal, cwGeometry::AttributeFormat::Vec3});
        }

        if (hasTangent) {
            geometryDescription.append({cwGeometry::Semantic::Tangent, cwGeometry::AttributeFormat::Vec4});
        }

        if (hasTex0) {
            geometryDescription.append({cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2});
        }

        geometry = cwGeometry(geometryDescription);
        geometry.resizeVertices(vertexCount);

        // Positions (vec3). glTF requires float here, but we still use the generic path.
        {
            QVector<float> flat = readVec(posView, vertexCount, 3, posView.normalized);
            auto attribute = geometry.attribute(cwGeometry::Semantic::Position);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, QVector3D(flat[i*3+0], flat[i*3+1], flat[i*3+2]));
            }
        }

        if (hasNormal) {
            QVector<float> flat = readVec(norView, vertexCount, 3, norView.normalized);
            auto attribute = geometry.attribute(cwGeometry::Semantic::Normal);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, QVector3D(flat[i * 3 + 0], flat[i * 3 + 1], flat[i * 3 + 2]));
            }
        }

        if (hasTangent) {
            QVector<float> flat = readVec(tanView, vertexCount, 4, tanView.normalized);
            auto attribute = geometry.attribute(cwGeometry::Semantic::Tangent);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, QVector4D(flat[i * 4 + 0], flat[i * 4 + 1], flat[i * 4 + 2], flat[i * 4 + 3]));
            }
        }

        if (hasTex0) {
            QVector<float> flat = readVec(uv0View, vertexCount, 2, uv0View.normalized);
            auto attribute = geometry.attribute(cwGeometry::Semantic::TexCoord0);
            for (int i = 0; i < vertexCount; i++) {
                geometry.set(attribute, i, QVector2D(flat[i * 2 + 0], flat[i * 2 + 1]));
            }
        }
    } else {
        geometry = cwGeometry(requestedLayout);
        geometry.resizeVertices(vertexCount);

        for (const auto& attr : requestedLayout) {
            const AttributeView* view = nullptr;
            AttributeView viewStorage{};

            switch (attr.semantic) {
            case cwGeometry::Semantic::Position:
                view = &posView;
                break;
            case cwGeometry::Semantic::Normal:
                if (hasNormal) {
                    viewStorage = norView;
                    view = &viewStorage;
                }
                break;
            case cwGeometry::Semantic::Tangent:
                if (hasTangent) {
                    viewStorage = tanView;
                    view = &viewStorage;
                }
                break;
            case cwGeometry::Semantic::TexCoord0:
                if (hasTex0) {
                    viewStorage = uv0View;
                    view = &viewStorage;
                }
                break;
            case cwGeometry::Semantic::TexCoord1:
            case cwGeometry::Semantic::Color0:
            case cwGeometry::Semantic::Bitangent:
            case cwGeometry::Semantic::Custom:
                view = nullptr;
                break;
            }

            fillAttribute(geometry, attr.semantic, attr.format, view);
        }
    }

    // Primitive type
    switch (prim.mode) {
    case TINYGLTF_MODE_TRIANGLES: geometry.setType(cwGeometry::Type::Triangles); break;
    // case TINYGLTF_MODE_LINES:     geometry.setType(cwGeometry::Type::Lines);     break;
    default:                      geometry.setType(cwGeometry::Type::None);      break;
    }

    // Indices
    QVector<uint32_t> indices;
    if (prim.indices >= 0) {
        const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
        AttributeView idxView = attributeView(model, idxAcc);
        indices.resize(idxAcc.count);

        const uint8_t* base = idxView.data;
        switch (idxAcc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            for (int i = 0; i < idxAcc.count; i++) {
                indices[i] = static_cast<uint32_t>(*(base + i));
            }
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const uint16_t* src = reinterpret_cast<const uint16_t*>(base);
            for (int i = 0; i < idxAcc.count; i++) {
                indices[i] = static_cast<uint32_t>(src[i]);
            }
        } break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            const uint32_t* src = reinterpret_cast<const uint32_t*>(base);
            for (int i = 0; i < idxAcc.count; i++) {
                indices[i] = static_cast<uint32_t>(src[i]);
            }
        } break;
        default: {
            Q_ASSERT(false);
        } break;
        }
    } else {
        // 0..N-1
        indices.resize(vertexCount);
        for (int i = 0; i < vertexCount; i++) {
            indices[i] = static_cast<uint32_t>(i);
        }
    }
    geometry.setIndices(std::move(indices));

    return geometry;
}

/**
 * tinygltf's image loader, replacing the stb default. It keeps the encoded PNG
 * or JPEG bytes and reads only the header for the dimensions, so loading a .glb
 * costs no full-resolution RGBA copy. Callers that want pixels ask
 * TextureCPU::toImage() for them.
 */
static bool storeEncodedImageData(tinygltf::Image* image,
                                  const int imageIndex,
                                  std::string* error,
                                  std::string* warning,
                                  int requestedWidth,
                                  int requestedHeight,
                                  const unsigned char* bytes,
                                  int size,
                                  void* userData)
{
    Q_UNUSED(error)
    Q_UNUSED(requestedWidth)
    Q_UNUSED(requestedHeight)
    Q_UNUSED(userData)

    image->as_is = true;
    image->image.assign(bytes, bytes + size);

    QByteArray encoded = QByteArray::fromRawData(reinterpret_cast<const char*>(bytes), size);
    QBuffer buffer(&encoded);
    QImageReader reader(&buffer);
    const QSize headerSize = reader.size();

    if (!headerSize.isValid()) {
        if (warning != nullptr) {
            *warning += "Can't read the header of image[" + std::to_string(imageIndex)
                        + "]; its size stays unknown\n";
        }
        // The bytes are kept anyway: a decode may still succeed where the header
        // read failed.
        return true;
    }

    image->width = headerSize.width();
    image->height = headerSize.height();

    return true;
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
    tex.encodedPixels = QByteArray(reinterpret_cast<const char*>(img.image.data()),
                                   static_cast<qsizetype>(img.image.size()));

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

    return m;
}

static void gatherMeshesRecursive(const tinygltf::Model& model,
                                  int nodeIndex,
                                  const QMatrix4x4& parent,
                                  QVector<MeshCPU>& outMeshes,
                                  const QVector<cwGeometry::AttributeDesc>& requestedLayout)
{
    const auto& node = model.nodes[nodeIndex];
    QMatrix4x4 world = parent * localMatrix(node);

    if (node.mesh >= 0) {
        const auto& gltfMesh = model.meshes[node.mesh];

        // Emit one MeshCPU aggregating all its primitives
        MeshCPU m;
        m.modelMatrix = world;

        for (const auto& prim : gltfMesh.primitives) {
            m.geometries.push_back(buildGeometryFromPrimitive(model, prim, requestedLayout));
            // Simple “dominant” material; switch to per-primitive if needed
            m.material = loadMaterialCPU(model, prim.material);
        }

        outMeshes.push_back(std::move(m));
    }

    for (int child : node.children) {
        gatherMeshesRecursive(model, child, world, outMeshes, requestedLayout);
    }
}

SceneCPU Loader::loadGltf(const QString &filePath)
{
    return loadGltf(filePath, LoadOptions{});
}

SceneCPU Loader::loadGltf(const QString &filePath, const LoadOptions& options,
                          const cwProgressScope& parent)
{
    cwProgressScope loading(parent, QStringLiteral("Loading glTF"));
    loading.expectChildren(kLoadSteps);

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(&storeEncodedImageData, nullptr);

    tinygltf::Model model;
    std::string error;
    std::string warning;

    SceneCPU scene;

    bool ok = false;
    {
        //tinygltf reads the whole file in one call and says nothing on the way,
        //so the parse stays an opaque leaf
        cwProgressScope parsing(loading, QStringLiteral("Parsing"));

        if (filePath.endsWith(".glb", Qt::CaseInsensitive)) {
            ok = loader.LoadBinaryFromFile(&model, &error, &warning, filePath.toStdString());
        } else {
            ok = loader.LoadASCIIFromFile(&model, &error, &warning, filePath.toStdString());
        }
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

    {
        cwProgressScope collecting(loading, QStringLiteral("Collecting meshes"));
        collecting.setTotal(roots.size());

        for (int n : std::as_const(roots)) {
            gatherMeshesRecursive(model, n, QMatrix4x4(), scene.meshes, options.requestedLayout);
            collecting.advance();
        }
    }

    // CPU: collect textures actually referenced by materials (here we provision array sized to textures count)
    scene.textures.resize(static_cast<int>(model.textures.size()));

    {
        cwProgressScope decoding(loading, QStringLiteral("Decoding textures"));
        decoding.setTotal(texturedMeshCount(scene.meshes));

        // Load only the textures we actually need (baseColor here; extend as desired)
        for (const MeshCPU& mesh : std::as_const(scene.meshes)) {
            if (mesh.material.baseColorTextureIndex >= 0) {
                const int ti = mesh.material.baseColorTextureIndex;
                if (scene.textures.at(ti).encodedPixels.isEmpty()) {
                    scene.textures[ti] = loadTextureCPU(model, ti, /*isSRGB*/ true);
                }
                decoding.advance();
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
                 << "Primitives:" << mesh.geometries.size()
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

        // Geometry
        for (int pi = 0; pi < mesh.geometries.size(); ++pi) {
            mesh.geometries[pi].dump();
        }
    }

    for (int ti = 0; ti < textures.size(); ++ti) {
        const TextureCPU& tex = textures[ti];
        qDebug() << "  Texture" << ti
                 << " size=" << tex.width << "x" << tex.height
                 << " encodedBytes=" << tex.encodedPixels.size()
                 << " isSRGB=" << tex.isSRGB;
    }
}


QImage baseColorImage(const SceneCPU& scene, const MaterialCPU& material)
{
    const int index = material.baseColorTextureIndex;
    if (index < 0 || index >= scene.textures.size()) {
        return {};
    }
    return scene.textures.at(index).toImage();
}

QImage TextureCPU::toImage() const
{
    if (encodedPixels.isEmpty()) {
        return {};
    }

    // A fresh decode per call, retaining nothing: a caller that wants the pixels
    // twice holds on to the QImage.
    QImage image = QImage::fromData(encodedPixels);
    if (image.isNull()) {
        return {};
    }

    if (image.format() != QImage::Format_RGBA8888) {
        image = image.convertToFormat(QImage::Format_RGBA8888);
    }

    image.setColorSpace(isSRGB ? QColorSpace::SRgb : QColorSpace::SRgbLinear);

    return image;
}



} // namespace cw::gltf
