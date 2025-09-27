//Our includes
#include "cwRenderGLTF.h"
#include "cwRHIGltf.h"

using namespace cw::gltf;

cwRenderGLTF::cwRenderGLTF(QObject *parent)
    : cwRenderObject{parent}
{
    m_modelMatrixProperty.setBinding([this]() {
        QMatrix4x4 matrix;

        //TODO: make this user define
        //Default rotation for up
        matrix.rotate(90.0, 1.0, 0.0, 0.0);

        matrix.translate(m_translation);
        auto rotation = m_rotation.value();
        matrix.rotate(rotation.w(), rotation.x(), rotation.y(), rotation.z());
        return matrix;
    });

    m_modelMatrixUpdated = m_modelMatrixProperty.addNotifier([this]() {
        auto matrix = m_modelMatrixProperty.value();
        m_modelMatrix.setValue(matrix);
        for(const auto& key : std::as_const(m_matrixObjects)) {
            geometryItersecter()->setModelMatrix(key, matrix);
        }
        update();
    });

    m_modelMatrix.setValue(m_modelMatrixProperty.value());

}

void cwRenderGLTF::setGLTFFilePath(const QString &filePath)
{
    qDebug() << "Setting gltf path:" << filePath;
    m_data = cw::gltf::Loader::loadGltf(filePath);

    updateGeometryIntersector(m_data);

    m_dataChanged = true;
    update();
}

void cwRenderGLTF::setGLTFUrl(const QUrl &url)
{

}

void cwRenderGLTF::setRotation(float x, float y, float z, float angle)
{
    m_rotation = QVector4D(x, y, z, angle);
}

void cwRenderGLTF::setTranslation(float x, float y, float z)
{
    m_translation = QVector3D(x, y, z);
}

cwRHIObject *cwRenderGLTF::createRHIObject()
{
    return new cwRHIGltf();
}

void cwRenderGLTF::updateGeometryIntersector(const cw::gltf::SceneCPU &data)
{
    // Helper: compute byte stride of an interleaved vertex (pos [+ normal] [+ tangent] [+ uv])
    auto vertexStrideBytes = [](const PrimitiveCPU& primitive) -> int {
        int floatComponentCount = 3; // position: vec3 (always present)
        if (primitive.hasNormal) {
            floatComponentCount += 3; // normal: vec3
        }
        if (primitive.hasTangent) {
            floatComponentCount += 4; // tangent: vec4
        }
        if (primitive.hasTexCoord0) {
            floatComponentCount += 2; // uv0: vec2
        }
        return static_cast<int>(floatComponentCount * sizeof(float));
    };

    // Lambda: extract and transform positions from all primitives in a mesh
    auto extractPoints = [vertexStrideBytes](const MeshCPU& mesh) -> QVector<QVector3D> {
        // Pre-reserve to avoid reallocations
        int totalVertexCount = 0;
        for (const PrimitiveCPU& primitive : mesh.primitives) {
            totalVertexCount += primitive.vertexCount;
        }

        QVector<QVector3D> points;
        points.reserve(totalVertexCount);

        for (const PrimitiveCPU& primitive : mesh.primitives) {
            const int stride = vertexStrideBytes(primitive);
            const char* const raw = primitive.vertexInterleaved.constData();

            for (int vertexIndex = 0; vertexIndex < primitive.vertexCount; ++vertexIndex) {
                const char* const vptr = raw + vertexIndex * stride;
                const float* const floats = reinterpret_cast<const float*>(vptr);

                // position is always the first 3 floats in the interleaved layout
                QVector3D position(floats[0], floats[1], floats[2]);

                // apply world transform (modelMatrix is already world from scene graph)
                const QVector4D transformed = mesh.modelMatrix * QVector4D(position, 1.0f);
                points.append(transformed.toVector3D());
            }
        }

        return points;
    };

    // Lambda: extract (and rebase) indices from all primitives in a mesh
    auto extractIndexes = [](const MeshCPU& mesh) -> QVector<uint> {
        // Pre-reserve to avoid reallocations
        int totalIndexCount = 0;
        for (const PrimitiveCPU& primitive : mesh.primitives) {
            totalIndexCount += primitive.indexCount;
        }

        QVector<uint> indexes;
        indexes.reserve(totalIndexCount);

        uint baseVertex = 0; // increases after each primitive to rebase indices

        for (const PrimitiveCPU& primitive : mesh.primitives) {
            const bool isUint32 = (primitive.indexFormat == QRhiCommandBuffer::IndexUInt32);

            if (isUint32) {
                const uint32_t* const data32 =
                    reinterpret_cast<const uint32_t*>(primitive.indexData.constData());
                const int count = primitive.indexCount;

                for (int i = 0; i < count; ++i) {
                    indexes.append(static_cast<uint>(data32[i]) + baseVertex);
                }
            } else {
                // default/other → treat as 16-bit
                const uint16_t* const data16 =
                    reinterpret_cast<const uint16_t*>(primitive.indexData.constData());
                const int count = primitive.indexCount;

                for (int i = 0; i < count; ++i) {
                    indexes.append(static_cast<uint>(data16[i]) + baseVertex);
                }
            }

            baseVertex += static_cast<uint>(primitive.vertexCount);
        }

        return indexes;
    };

    auto intersector = geometryItersecter();
    intersector->clear(this);

    m_matrixObjects.clear();
    m_matrixObjects.reserve(data.meshes.size());

    uint64_t meshId = 0;
    for (const MeshCPU& mesh : data.meshes) {
        cwGeometryItersecter::Key key(this, meshId);
        cwGeometryItersecter::Object object(
            key,
            extractPoints(mesh),
            extractIndexes(mesh),
            cwGeometryItersecter::Triangles,
            m_modelMatrix.value(),
            /* cullbackfaces */ true
            );
        m_matrixObjects.append(key);
        intersector->addObject(object);
        ++meshId;
    }
}

void cwRenderGLTF::updateModelMatrix()
{
    m_modelMatrix.setValue(m_modelMatrixProperty.value());

}
