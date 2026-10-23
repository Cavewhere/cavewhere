#include "cwTriangulateLiDARTask.h"
#include "cwConcurrent.h"
#include "cwGeometryBounds.h"
#include "cwGltfBaseColorTexture.h"
#include "cwGltfLoader.h"
#include "cwTriangulateStation.h"
#include "cwTriangulateTask.h"
#include "cwRenderTexturedItems.h"
#include "cwMemoryBudget.h"
#include "cwRenderBudgets.h"
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <cmath>

using namespace Monad;

namespace {

//What one note's node hangs off itself: the glTF load, the file checksum, the
//morph and the textures
constexpr int kNoteSteps = 4;

//How often the morph loop reports; every vertex would lock the tree far more
//often than a bar can show
constexpr int kMorphProgressStride = 256;

//The pipeline's share of the memory ledger
constexpr qint64 kLiDARDecodeBudgetBytes = 512 * cw::budgets::kBytesPerMegabyte;

//The binary buffers decode about one to one and the textures add the rest
constexpr qint64 kDecodedBytesPerGltfByte = 2;

//A note's decode is its weight
qint64 lidarWeight(const cwTriangulateLiDARInData& data)
{
    return QFileInfo(data.gltfFilename()).size() * kDecodedBytesPerGltfByte;
}

//How many geometries the meshes hold between them, which is what the morph and
//the texture loops below each run once for
qsizetype geometryCount(const QVector<cw::gltf::MeshCPU>& meshes)
{
    qsizetype count = 0;
    for(const auto& mesh : std::as_const(meshes)) {
        count += qsizetype(mesh.geometries.size());
    }
    return count;
}

}

AsyncFuture::Limited<cwTriangulateLiDARTask::Result> cwTriangulateLiDARTask::triangulate(QObject* context,
                                                                                         const QList<cwTriangulateLiDARInData> &liDARs,
                                                                                         const cwProgressNodePtr& progressRoot)
{
    const auto triangulateOne = [progressRoot](const cwTriangulateLiDARInData& data) {
        //The scope names the scan, and hands the note's node back on every
        //return below
        cwProgressScope noteScope(progressRoot, QFileInfo(data.gltfFilename()).fileName());
        noteScope.expectChildren(kNoteSteps);

        if(data.stationLookup().positions().size() == 0) {
            return Result("Station Lookup not set");
        }

        cw::gltf::LoadOptions options = {
            cwRenderTexturedItems::geometryLayout()
        };
        auto gltf = cw::gltf::Loader::loadGltf(data.gltfFilename(), options, noteScope);

        auto visibleStations = cwTriangulateTask::buildStationsWithInterpolatedShots(data);

        auto morphPositions = [&](cwGeometry& geometry, cwProgressScope& meshScope) {
            const auto positionAttribute = geometry.attribute(cwGeometry::Semantic::Position);
            for(size_t index = 0; index < geometry.vertexCount(); index++) {
                if(index % kMorphProgressStride == 0) {
                    meshScope.report(qsizetype(index));
                }

                QVector3D vertex = geometry.value<QVector3D>(positionAttribute, index);
                QVector3D newVertex = cwTriangulateTask::morphPoint(visibleStations,
                                                                    data.modelMatrix(),
                                                                    QMatrix4x4(),
                                                                    vertex);

                geometry.set(positionAttribute, index, newVertex);
            }
        };

        QVector<cwRenderTexturedItems::Item> renderItems = reserveRenderItems(gltf.meshes);
        const cwGltfBaseColorTexture baseColorTexture(data.dataRootPath(),
                                                      data.gltfFilename(),
                                                      noteScope);

        //Both loops run once per geometry, so both know their count. They fill
        //two slots of the note at once, since a geometry is morphed and then
        //textured before the next one starts.
        const int geometries = int(geometryCount(gltf.meshes));
        cwProgressScope morphing(noteScope, QStringLiteral("Morphing"));
        morphing.expectChildren(geometries);
        cwProgressScope textures(noteScope, QStringLiteral("Textures"));
        textures.expectChildren(geometries);

        int geometryIndex = 0;

        //Morph the vertexes
        for(auto& mesh : gltf.meshes) {
            for(auto& geometry : mesh.geometries) {
                {
                    cwProgressScope meshScope(morphing,
                                              QStringLiteral("Mesh %1").arg(geometryIndex + 1));
                    meshScope.setTotal(qsizetype(geometry.vertexCount()));
                    morphPositions(geometry, meshScope);
                }

                //Measured on this worker thread, so the render thread never
                //walks the vertices again at the sync barrier
                const auto localBounds = cw::geometry::positionBounds(geometry);

                //Add the render item
                auto& item = renderItems.emplaceBack(std::move(geometry));
                item.localBounds = localBounds;
                // The mesh draws with the scrap shaders, which read a per-draw block.
                item.material = cwRenderTexturedItems::scrapMaterial();
                item.uniformBlock = cwRenderTexturedItems::emptyItemBlock();

                const cwProgressScope textureScope(textures,
                                                   QStringLiteral("Texture %1").arg(geometryIndex + 1));
                baseColorTexture.setOn(item, gltf, mesh.material, textureScope);

                geometryIndex++;
            }
        }

        return Result(renderItems);
    };

    const auto launch = [triangulateOne](const cwTriangulateLiDARInData& data) {
        return cwConcurrent::run(triangulateOne, data);
    };

    return cwMemoryBudget::limitedOnPool(context, liDARs, launch, lidarWeight, kLiDARDecodeBudgetBytes);
}

QVector<cwRenderTexturedItems::Item> cwTriangulateLiDARTask::reserveRenderItems(const QVector<cw::gltf::MeshCPU> &meshes)
{
    QVector<cwRenderTexturedItems::Item> items;
    items.reserve(geometryCount(meshes));
    return items;
}
