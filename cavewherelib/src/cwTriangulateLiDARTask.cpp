#include "cwTriangulateLiDARTask.h"
#include "cwConcurrent.h"
#include "cwGltfLoader.h"
#include "cwTriangulateStation.h"
#include "cwTriangulateTask.h"
#include "cwRenderTexturedItems.h"

using namespace Monad;


QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item> > > cwTriangulateLiDARTask::triangulate(const QList<cwTriangulateLiDARInData> &liDARs)
{

    // qDebug() << "I get here!";

    return cwConcurrent::mapped(liDARs, [](const cwTriangulateLiDARInData& data) {

        // qDebug() << "I get here! Mapped!" << data.gltfFilename() << data.stationLookup().positions().size();

        if(data.stationLookup().positions().size() == 0) {
            return Monad::Result<QVector<cwRenderTexturedItems::Item> >("Station Lookup not set");
        }

        auto gltf = cw::gltf::Loader::loadGltf(data.gltfFilename());
        // gltf.dump();

        //Solve for rotation matrix,

            //THIS IS FOR TESTING, remove
            // QMatrix4x4 northMatrix;
            // northMatrix.rotate(-88.34, QVector3D(0.0, 0.0, 1.0));

            // QMatrix4x4 worldMatrix = northMatrix * data.modelMatrix();


            auto visibleStations = cw::transform(data.noteStations(), [&](const cwNoteLiDARStation& station) {
                const QString stationName = station.name();
                return cwTriangulateStation(
                    stationName,
                    station.positionOnNote(),
                    data.stationLookup().position(stationName)
                    );
            });

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
                renderItems.emplaceBack(std::move(geometry),
                                        toImage(mesh.material.baseColorTextureIndex));
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
