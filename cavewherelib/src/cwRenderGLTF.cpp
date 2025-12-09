//Our includes
#include "cwRenderGLTF.h"
#include "cwRhiTexturedItems.h"
#include "cwConcurrent.h"
#include "cwTriangulateLiDARTask.h"

//Async includes
#include <asyncfuture.h>

using namespace cw::gltf;

cwRenderGLTF::cwRenderGLTF(QObject *parent)
    : cwRenderTexturedItems{parent},
    m_loadRestarter(this)
{
    m_modelMatrixUpdated = m_modelMatrixProperty.addNotifier([this]() {
        auto matrix = m_modelMatrixProperty.value();
        m_modelMatrix.setValue(matrix);
        for (auto id : std::as_const(m_items)) {
            cwRenderTexturedItems::setModelMatrix(id, matrix);
        }
        update();
    });

    m_modelMatrix.setValue(m_modelMatrixProperty.value());

    m_loadRestarter.onFutureChanged([this]() {
        m_futureManagerToken.addJob(cwFuture(m_loadRestarter.future(), QStringLiteral("Loading glTF")));
    });

}

void cwRenderGLTF::setGLTFFilePath(const QString &filePath)
{
    if(m_gltfFilePath != filePath) {
        for(auto id : std::as_const(m_items)) {
            removeItem(id);
        }
        m_items.clear();

        m_status = Status::Loading;

        auto run = [this, filePath]() {
            auto renderObject = this;
            auto modelMatrix = m_modelMatrix.value();

            auto future = cwConcurrent::run([filePath, renderObject, modelMatrix]()->Monad::Result<Load> {
                auto data = cw::gltf::Loader::loadGltf(filePath);

                Load load;
                load.items = cwTriangulateLiDARTask::reserveRenderItems(data.meshes);

                auto toImage = [&](uint64_t textureIndex) {
                    //Probably should use caching so we don't have duplicate
                    return data.textures.at(textureIndex).toImage();
                };

                //Morph the vertexes
                for(auto& mesh : data.meshes) {
                    for(auto& geometry : mesh.geometries) {

                        //Add the render item
                        load.items.emplaceBack(std::move(geometry),
                                               toImage(mesh.material.baseColorTextureIndex));
                    }
                }

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
    return new cwRhiTexturedItems();
}

QFuture<void> cwRenderGLTF::handleLoadFuture(QFuture<Monad::Result<Load> > loadFuture)
{
    return AsyncFuture::observe(loadFuture)
    .context(this, [this, loadFuture]() {

        auto load = loadFuture.result().value();

        cwRenderMaterialState material;
        material.perDrawUniformBinding = 2;
        material.perDrawUniformStages = cwRenderMaterialState::ShaderStage::Vertex
                                        | cwRenderMaterialState::ShaderStage::Fragment;
        material.vertexShader = QStringLiteral(":/shaders/unlit.vert.qsb");
        material.fragmentShader = QStringLiteral(":/shaders/scrap.frag.qsb");

        for(auto& item : load.items) {
            item.modelMatrix = m_modelMatrix.value();
            item.material = material;
            const auto id = addItem(std::move(item));
            m_items.append(id);
        }

        m_status = Status::Ready;
    }).future();

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

/**
Returns the full bounding box of the GLTF object
 */
QBox3D cwRenderGLTF::boundingBox() const
{
    QBox3D boundingBox;
    for (const auto id : m_items) {
        cwGeometryItersecter::Key key{const_cast<cwRenderGLTF*>(this), id};
        auto box = geometryItersecter()->boundingBox(key);
        boundingBox.unite(box);
    }
    return boundingBox;
}
