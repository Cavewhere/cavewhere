#include "cwTriangulateLiDARTask.h"
#include "cwConcurrent.h"
#include "cwGltfLoader.h"
#include "cwTriangulateStation.h"
#include "cwTriangulateTask.h"
#include "cwRenderTexturedItems.h"
#include <QHash>
#include <QSet>
#include <cmath>

using namespace Monad;


QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item> > > cwTriangulateLiDARTask::triangulate(const QList<cwTriangulateLiDARInData> &liDARs)
{

    // qDebug() << "I get here!";

    return cwConcurrent::mapped(liDARs, [](const cwTriangulateLiDARInData& data) {
        if(data.stationLookup().positions().size() == 0) {
            return Monad::Result<QVector<cwRenderTexturedItems::Item> >("Station Lookup not set");
        }

        auto gltf = cw::gltf::Loader::loadGltf(data.gltfFilename());

        auto visibleStations = cwTriangulateTask::buildStationsWithInterpolatedShots(data);

        auto morphPositions = [&](cwGeometry& geometry) {
            const auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
            for(size_t index = 0; index < geometry.vertexCount(); index++) {
                QVector3D vertex = geometry.value<QVector3D>(positionAttribute, index);
                QVector3D newVertex = cwTriangulateTask::morphPoint(visibleStations,
                                                                    data.modelMatrix(),
                                                                    // worldMatrix,
                                                                    QMatrix4x4(),
                                                                    vertex);

                // qDebug() << "New:" << newVertex << "old:" << vertex;
                geometry.set(positionAttribute, index, newVertex);
            }
        };


        QVector<cwRenderTexturedItems::Item> renderItems = reserveRenderItems(gltf.meshes);

        auto toImage = [&](uint64_t textureIndex) {
            //Probably should use caching so we don't have duplicate
            return gltf.textures.at(textureIndex).toImage();
        };

        //Morph the vertexes
        for(auto& mesh : gltf.meshes) {
            for(auto& geometry : mesh.geometries) {
                morphPositions(geometry);

                //Add the render item
                cwRenderTexturedItems::Item item;
                item.geometry = std::move(geometry);
                item.texture = toImage(mesh.material.baseColorTextureIndex);
                item.material.perDrawUniformBinding = 2;
                item.material.perDrawUniformStages = cwRenderMaterialState::ShaderStage::Vertex
                                                     | cwRenderMaterialState::ShaderStage::Fragment;
                renderItems.append(std::move(item));
            }
        }

        return Monad::Result<QVector<cwRenderTexturedItems::Item> >(renderItems);

    });
}

QVector<cwRenderTexturedItems::Item> cwTriangulateLiDARTask::reserveRenderItems(const QVector<cw::gltf::MeshCPU> &meshes)
{
    size_t size = 0;
    for(const auto& mesh : std::as_const(meshes)) {
        size += mesh.geometries.size();
    }

    QVector<cwRenderTexturedItems::Item> items;
    items.reserve(size);
    return items;
}
