#include "cwTriangulateLiDARTask.h"
#include "cwConcurrent.h"
#include "cwGeometryBounds.h"
#include "cwGltfBaseColorTexture.h"
#include "cwGltfLoader.h"
#include "cwTriangulateStation.h"
#include "cwTriangulateTask.h"
#include "cwRenderTexturedItems.h"
#include <QHash>
#include <QSet>
#include <cmath>

using namespace Monad;


QList<QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item> > > > cwTriangulateLiDARTask::triangulate(const QList<cwTriangulateLiDARInData> &liDARs)
{
    const auto triangulateOne = [](const cwTriangulateLiDARInData& data) {
        if(data.stationLookup().positions().size() == 0) {
            return Monad::Result<QVector<cwRenderTexturedItems::Item> >("Station Lookup not set");
        }

        cw::gltf::LoadOptions options = {
            cwRenderTexturedItems::geometryLayout()
};
        auto gltf = cw::gltf::Loader::loadGltf(data.gltfFilename(), options);

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
        const cwGltfBaseColorTexture baseColorTexture(data.dataRootPath(),
                                                     data.gltfFilename());

        //Morph the vertexes
        for(auto& mesh : gltf.meshes) {
            for(auto& geometry : mesh.geometries) {
                morphPositions(geometry);

                //Measured on this worker thread, so the render thread never
                //walks the vertices again at the sync barrier
                const auto localBounds = cw::geometry::positionBounds(geometry);

                //Add the render item
                auto& item = renderItems.emplaceBack(std::move(geometry));
                item.localBounds = localBounds;
                baseColorTexture.setOn(item, gltf, mesh.material);
            }
        }

        return Monad::Result<QVector<cwRenderTexturedItems::Item> >(renderItems);
    };

    QList<QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item> > > > futures;
    futures.reserve(liDARs.size());
    for(const cwTriangulateLiDARInData& liDAR : liDARs) {
        futures.append(cwConcurrent::run([data = liDAR, triangulateOne]() {
            return triangulateOne(data);
        }));
    }
    return futures;
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
