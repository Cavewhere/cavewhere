//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGeometryItersecter.h"
#include "cwGeometry.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QPointer>
#include <QTemporaryDir>
#include <QVector3D>
#include <QVector>

//QMath3d
#include <qray3d.h>

#include "LazFixtureHelper.h"

namespace {

// A 3x3 grid in the z = 0 plane, one unit apart, centred on the origin. Small
// enough to build instantly, spread enough that meanSpacingXY is meaningful.
constexpr float kGridSpacing = 1.0f;
constexpr int kGridSide = 3;

QVector<QVector3D> gridPoints()
{
    QVector<QVector3D> points;
    for (int x = 0; x < kGridSide; ++x) {
        for (int y = 0; y < kGridSide; ++y) {
            points.append(QVector3D((x - 1) * kGridSpacing, (y - 1) * kGridSpacing, 0.0f));
        }
    }
    return points;
}

cwRenderPointCloud::GeometryData gridGeometry()
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });
    geometry.setType(cwGeometry::Type::Points);
    geometry.set(cwGeometry::Semantic::Position, gridPoints());

    return {
        .geometry = std::move(geometry),
        .bboxMin = QVector3D(-kGridSpacing, -kGridSpacing, 0.0f),
        .bboxMax = QVector3D(kGridSpacing, kGridSpacing, 0.0f),
        .meanSpacingXY = kGridSpacing,
    };
}

// Straight down -Z through the grid's centre point at the origin.
QRay3D rayThroughOrigin()
{
    return QRay3D(QVector3D(0.0f, 0.0f, 50.0f), QVector3D(0.0f, 0.0f, -1.0f));
}

cwPickQuery pointPickQuery()
{
    cwPickQuery query;
    query.kinds = cwPickQuery::Kind::Points;
    return query;
}

QDir prepareGisLayersDir(const QTemporaryDir& tempDir)
{
    const QString path = QDir(tempDir.path()).filePath(cwLazLayerModel::folderName());
    QDir().mkpath(path);
    return QDir(path);
}

} // namespace

// A Node that outlives its render object is a use-after-free — the intersecter
// keys geometry by raw cwRenderObject* and dereferences it on every pick. These
// pin the purge in cwScene::removeItem() that prevents it; see that function
// for why it lives there. Both cases below delete a registered render object,
// and before the fix ASAN caught each one reading cwRenderObject::isVisible()
// out of freed memory, via isPickable() <- visibleBoundingBox().
//
// Each test REQUIREs the purge *before* it picks, so a regression reports as a
// readable Catch2 failure instead of taking the suite down with an ASAN abort.
// Neither reaches removeItem()'s `removedCount == 0` early return — that needs
// a live RHI to drain the sync queues — so nothing here pins the purge's
// placement above it.
TEST_CASE("cwGeometryItersecter forgets a deleted render object",
          "[cwGeometryItersecter][cwRenderPointCloud]")
{
    cwScene scene;
    auto* intersecter = scene.geometryItersecter();

    auto* cloud = new cwRenderPointCloud();
    cloud->setScene(&scene);
    cloud->setGeometry(gridGeometry());

    // Captured while the cloud is alive; the Key is only ever compared by
    // pointer value, never dereferenced, so it stays a usable probe after the
    // delete below. cwScene::removeItem() plays the same trick with
    // renderObjectId() (issues #491/#512).
    const cwGeometryItersecter::Key key{cloud, 0};

    intersecter->waitForFinish();
    REQUIRE_FALSE(intersecter->boundingBox(key).isNull());
    REQUIRE(intersecter->intersectsDetailed(rayThroughOrigin(), pointPickQuery()).hit());

    // Exactly what cwLazLayersSceneNode::dematerialize() does when a LAZ layer
    // is removed or disabled. Note setScene(nullptr) runs first, which nulls
    // m_scene — so geometryItersecter() returns nullptr from here on, and a
    // purge written in ~cwRenderObject() would silently do nothing.
    cloud->setScene(nullptr);
    delete cloud;

    REQUIRE(intersecter->boundingBox(key).isNull());
    CHECK(intersecter->visibleBoundingBox().isNull());
    CHECK_FALSE(intersecter->intersectsDetailed(rayThroughOrigin(), pointPickQuery()).hit());
}

TEST_CASE("cwGeometryItersecter forgets a removed LAZ layer's point cloud",
          "[cwGeometryItersecter][cwLazLayersSceneNode]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    auto* intersecter = scene.geometryItersecter();

    cwLazLayersSceneNode node;
    node.setScene(&scene);

    cwLazLayerModel model;
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    const QString filePath = gisLayersDir.filePath(QStringLiteral("lifetime-%1.laz")
                                                       .arg(QCoreApplication::applicationPid()));
    REQUIRE(writeSyntheticLazFile(filePath, gridPoints()));
    model.setGisLayersDir(gisLayersDir);
    model.rescan();

    cwLazLayer* layer = model.layerAt(model.count() - 1);
    REQUIRE(layer != nullptr);
    REQUIRE(waitForLazLayerLoaded(layer));

    cwRenderPointCloud* cloud = node.pointCloudForLayer(layer);
    REQUIRE(cloud != nullptr);
    const cwGeometryItersecter::Key key{cloud, 0};

    intersecter->waitForFinish();
    REQUIRE_FALSE(intersecter->boundingBox(key).isNull());

    QPointer<cwRenderPointCloud> guard(cloud);
    model.removeAt(0);
    REQUIRE(guard.isNull());

    REQUIRE(intersecter->boundingBox(key).isNull());
    CHECK(intersecter->visibleBoundingBox().isNull());
    CHECK_FALSE(intersecter->intersectsDetailed(rayThroughOrigin(), pointPickQuery()).hit());
}
