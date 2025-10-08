//Our includes
#include "cwRenderGLTF.h"
#include "cwRHIGltf.h"
#include "cwConcurrent.h"

//Async includes
#include <asyncfuture.h>

using namespace cw::gltf;

cwRenderGLTF::cwRenderGLTF(QObject *parent)
    : cwRenderObject{parent},
    m_loadRestarter(this)
{
    // m_modelMatrixProperty.setBinding([this]() {
    //     QMatrix4x4 matrix;

    //     //TODO: make this user define

    //     matrix.translate(m_translation);
    //     auto rotation = m_rotation.value();
    //     matrix.rotate(rotation.w(), rotation.x(), rotation.y(), rotation.z());
    //     //Default rotation for up
    //     matrix.rotate(90.0, 1.0, 0.0, 0.0);
    //     return matrix;
    // });

    m_modelMatrixUpdated = m_modelMatrixProperty.addNotifier([this]() {
        auto matrix = m_modelMatrixProperty.value();
        m_modelMatrix.setValue(matrix);
        for(const auto& key : std::as_const(m_matrixObjects)) {
            qDebug() << "Update intersector:" << matrix;
            geometryItersecter()->setModelMatrix(key, matrix);
        }
        update();
    });

    qDebug() << "Set m_modelMatrix:" << m_modelMatrixProperty.value();

    m_modelMatrix.setValue(m_modelMatrixProperty.value());

    m_loadRestarter.onFutureChanged([this]() {
        m_futureManagerToken.addJob(cwFuture(m_loadRestarter.future(), QStringLiteral("Loading glTF")));
    });

}

void cwRenderGLTF::setGLTFFilePath(const QString &filePath)
{
    if(m_gltfFilePath != filePath) {
        qDebug() << "Setting gltf path:" << filePath;
        auto run = [this, filePath]() {
            auto renderObject = this;
            auto modelMatrix = m_modelMatrix.value();

            auto future = cwConcurrent::run([filePath, renderObject, modelMatrix]()->Monad::Result<Load> {
                auto data = cw::gltf::Loader::loadGltf(filePath);
                auto load = toIntersectors(renderObject, data, modelMatrix);
                load.scene = std::move(data);
                return load;
            });

            return handleLoadFuture(future);
        };

        m_loadRestarter.restart(run);
    }
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

QFuture<void> cwRenderGLTF::handleLoadFuture(QFuture<Monad::Result<Load> > loadFuture)
{
    return AsyncFuture::observe(loadFuture)
    .context(this, [this, loadFuture]() {

        auto load = loadFuture.result().value();

        m_data = std::move(load.scene);
        m_matrixObjects = std::move(load.matrixObjects);

        auto intersector = geometryItersecter();
        intersector->clear(this);

        for(const auto& object : std::as_const(load.intersecterObjects)) {
            intersector->addObject(object);
        }

        auto matrix = m_modelMatrixProperty.value();
        for(const auto& key : std::as_const(m_matrixObjects)) {
            geometryItersecter()->setModelMatrix(key, matrix);
        }

        m_dataChanged = true;
        update();
    }).future();

}

cwRenderGLTF::Load cwRenderGLTF::toIntersectors(cwRenderObject* renderObject,
                                                const cw::gltf::SceneCPU &data,
                                                const QMatrix4x4 &modelMatrix)
{
    QList<cwGeometryItersecter::Object> objects;
    QList<cwGeometryItersecter::Key> keys;
    objects.reserve(data.meshes.size());
    keys.reserve(data.meshes.size());

    uint64_t meshId = 0;
    for (const MeshCPU& mesh : data.meshes) {
        cwGeometryItersecter::Key key(renderObject, meshId);

        auto geometry = mesh.toGeometry();
        geometry.transform = geometry.transform * modelMatrix;

        cwGeometryItersecter::Object object(
            key,
            std::move(geometry)
            );
        keys.append(key);
        objects.append(object);
        ++meshId;
    }

    return Load {
        {}, //Empty scene
        keys,
        objects
    };
}

void cwRenderGLTF::updateModelMatrix()
{
    m_modelMatrix.setValue(m_modelMatrixProperty.value());

}

cwFutureManagerToken cwRenderGLTF::futureManagerToken() const
{
    return m_futureManagerToken;
}

void cwRenderGLTF::setFutureManagerToken(const cwFutureManagerToken &newFutureManagerToken)
{
    if (m_futureManagerToken == newFutureManagerToken) {
        return;
    }
    m_futureManagerToken = newFutureManagerToken;
    emit futureManagerTokenChanged();
}

void cwRenderGLTF::setGltf(const QFuture<Monad::Result<cw::gltf::SceneCPU> > &gltfFuture)
{
    auto run = [this, gltfFuture]() {
        return AsyncFuture::observe(gltfFuture)
        .context(this, [gltfFuture, this]() {

            auto renderObject = this;
            auto modelMatrix = m_modelMatrix.value();
            auto gltf = gltfFuture.result().value();

            auto future = cwConcurrent::run([gltf, renderObject, modelMatrix]()->Monad::Result<Load> {
                auto load = toIntersectors(renderObject, gltf, modelMatrix);
                load.scene = std::move(gltf);
                return load;
            });

            return handleLoadFuture(future);
        }).future();
    };

    m_loadRestarter.restart(run);

}
