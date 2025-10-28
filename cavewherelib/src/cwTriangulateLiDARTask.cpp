#include "cwTriangulateLiDARTask.h"
#include "cwConcurrent.h"
#include "cwGltfLoader.h"
#include "cwTriangulateStation.h"
#include "cwTriangulateTask.h"

using namespace Monad;

QFuture<Result<cw::gltf::SceneCPU>> cwTriangulateLiDARTask::triangulate(const QList<cwTriangulateLiDARInData>& liDARs)
{

    // qDebug() << "I get here!";

    return cwConcurrent::mapped(liDARs, [](const cwTriangulateLiDARInData& data) {

        // qDebug() << "I get here! Mapped!" << data.gltfFilename() << data.stationLookup().positions().size();

        if(data.stationLookup().positions().size() == 0) {
            return Result<cw::gltf::SceneCPU>("Station Lookup not set");
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

        //Morph the vertexes
        for(auto& mesh : gltf.meshes) {
            for(auto& geometry : mesh.geometries) {

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
            }
        }

        return Result<cw::gltf::SceneCPU>(std::move(gltf));

    });
}
