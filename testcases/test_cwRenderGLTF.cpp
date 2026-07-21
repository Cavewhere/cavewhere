//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "cwRenderGLTF.h"
#include "cwGltfLoader.h"
#include "cwFutureManagerModel.h"
#include "cwScene.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QFileInfo>
#include <QBox3D>
#include <QVector4D>
#include <QtTest/QSignalSpy>

using Catch::Matchers::WithinAbs;

static QBox3D boundingFromFile(const QString& path)
{
    const auto scene = cw::gltf::Loader::loadGltf(path);
    QBox3D box;
    bool initialized = false;

    for (const auto& mesh : scene.meshes) {
        for (const auto& geometry : mesh.geometries) {
            const auto* posAttr = geometry.attribute(cwGeometry::Semantic::Position);
            if (posAttr == nullptr) {
                continue;
            }

            for (int i = 0; i < geometry.vertexCount(); ++i) {
                const QVector3D v = geometry.value<QVector3D>(posAttr, i);
                const QVector3D world = (mesh.modelMatrix * QVector4D(v, 1.0f)).toVector3D();
                if (!initialized) {
                    box = QBox3D(world, world);
                    initialized = true;
                } else {
                    box.unite(world);
                }
            }
        }
    }

    return box;
}


TEST_CASE("GLTF file should be loaded correctly", "[cwRenderGLTF]") {

    cwFutureManagerModel futureModel;
    cwRenderGLTF* render = new cwRenderGLTF();
    render->setFutureManagerToken(futureModel.token());

    cwScene scene;
    render->setScene(&scene);

    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwSurveyNotesConcatModel/bones.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    render->setGLTFFilePath(gltfPath);
    REQUIRE(render->status() == cwRenderGLTF::Status::Loading);

    // Wait for the async load that cwRenderGLTF enqueues on the future model.
    futureModel.waitForFinished();
    REQUIRE(render->status() == cwRenderGLTF::Status::Ready);

    const QBox3D expected = boundingFromFile(gltfPath);
    REQUIRE(!expected.isNull());

    const QBox3D expectedStatic(QVector3D(-0.430634f, -0.480260f, -0.711276f),
                          QVector3D( 0.430809f,  0.100640f,  0.710623f));

    CHECK_THAT(expectedStatic.minimum().x(), WithinAbs(expected.minimum().x(), 1e-4f));
    CHECK_THAT(expectedStatic.minimum().y(), WithinAbs(expected.minimum().y(), 1e-4f));
    CHECK_THAT(expectedStatic.minimum().z(), WithinAbs(expected.minimum().z(), 1e-4f));

    CHECK_THAT(expectedStatic.maximum().x(), WithinAbs(expected.maximum().x(), 1e-4f));
    CHECK_THAT(expectedStatic.maximum().y(), WithinAbs(expected.maximum().y(), 1e-4f));
    CHECK_THAT(expectedStatic.maximum().z(), WithinAbs(expected.maximum().z(), 1e-4f));

    const QBox3D actual = render->boundingBox();
    REQUIRE(!actual.isNull());

    CHECK_THAT(actual.minimum().x(), WithinAbs(expected.minimum().x(), 1e-4f));
    CHECK_THAT(actual.minimum().y(), WithinAbs(expected.minimum().y(), 1e-4f));
    CHECK_THAT(actual.minimum().z(), WithinAbs(expected.minimum().z(), 1e-4f));

    CHECK_THAT(actual.maximum().x(), WithinAbs(expected.maximum().x(), 1e-4f));
    CHECK_THAT(actual.maximum().y(), WithinAbs(expected.maximum().y(), 1e-4f));
    CHECK_THAT(actual.maximum().z(), WithinAbs(expected.maximum().z(), 1e-4f));


}

// Regression for issue #505: cwRenderGLTF::setGLTFFilePath must record the path
// it loaded. The bug never assigned m_gltfFilePath, so the getter stayed empty,
// gltfFilePathChanged() never fired, and the `m_gltfFilePath != filePath` guard
// re-launched a full async reload every time the same path was set (restarter
// thrash that aggravates the picker "goes on and off" symptom).
TEST_CASE("cwRenderGLTF records the gltf file path it loaded", "[cwRenderGLTF][Issue505]") {
    cwFutureManagerModel futureModel;

    // Declared before the render object so the scene outlives it (the render
    // object detaches from the scene in its destructor).
    cwScene scene;
    cwRenderGLTF render;
    render.setFutureManagerToken(futureModel.token());
    render.setScene(&scene);

    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwSurveyNotesConcatModel/bones.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    REQUIRE(render.gltfFilePath().isEmpty());

    QSignalSpy pathChangedSpy(&render, &cwRenderGLTF::gltfFilePathChanged);

    render.setGLTFFilePath(gltfPath);

    // The path is recorded synchronously by the setter, independent of the load.
    CHECK(render.gltfFilePath() == gltfPath);
    CHECK(pathChangedSpy.count() == 1);

    // Setting the identical path again must be a no-op: no extra reload, no
    // extra change signal. (With the bug, m_gltfFilePath stayed empty so this
    // re-triggered another load.)
    render.setGLTFFilePath(gltfPath);
    CHECK(pathChangedSpy.count() == 1);

    futureModel.waitForFinished();
    CHECK(render.status() == cwRenderGLTF::Status::Ready);
}
