//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGltfLoader.h"
#include "cwRenderTexturedItems.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>

namespace {
constexpr int kExpectedVertexCount = 130010;
constexpr int kExpectedIndexCount = 130458;

int expectedStride(const QVector<cwGeometry::AttributeDesc>& layout)
{
    int stride = 0;
    for (const auto& desc : layout) {
        cwGeometry::VertexAttribute attr;
        attr.semantic = desc.semantic;
        attr.format = desc.format;
        stride += attr.byteSize();
    }
    return stride;
}

void requireLayoutMatches(const cwGeometry& geometry,
                          const QVector<cwGeometry::AttributeDesc>& layout)
{
    const auto& attributes = geometry.attributes();
    REQUIRE(attributes.size() == layout.size());

    int offset = 0;
    for (int i = 0; i < layout.size(); ++i) {
        const auto& expected = layout[i];
        const auto& actual = attributes[i];
        REQUIRE(actual.semantic == expected.semantic);
        REQUIRE(actual.format == expected.format);
        REQUIRE(actual.byteOffsetInBuffer == offset);
        offset += actual.byteSize();
    }

    const auto buffers = geometry.vertexBuffers();
    REQUIRE(buffers.size() == 1);
    REQUIRE(buffers[0].stride == offset);
    REQUIRE(buffers[0].stride == expectedStride(layout));
}
}

TEST_CASE("GLTF loader repacks geometry for textured items", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwGltfLoader/test.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    cw::gltf::LoadOptions options;
    options.requestedLayout = cwRenderTexturedItems::geometryLayout();

    const auto scene = cw::gltf::Loader::loadGltf(gltfPath, options);
    REQUIRE_FALSE(scene.meshes.isEmpty());

    bool sawGeometry = false;
    for (const auto& mesh : scene.meshes) {
        for (const auto& geometry : mesh.geometries) {
            sawGeometry = true;
            const auto buffers = geometry.vertexBuffers();
            REQUIRE(buffers.size() == 1);
            REQUIRE_FALSE(buffers[0].data->isEmpty());
            REQUIRE(geometry.vertexCount() == kExpectedVertexCount);
            REQUIRE(buffers[0].stride > 0);
            REQUIRE(geometry.indices().size() == kExpectedIndexCount);

            requireLayoutMatches(geometry, options.requestedLayout);

            REQUIRE(geometry.attribute(cwGeometry::Semantic::Position) != nullptr);
            REQUIRE(geometry.attribute(cwGeometry::Semantic::TexCoord0) != nullptr);
            REQUIRE(buffers[0].data->size() == geometry.vertexCount() * buffers[0].stride);
        }
    }

    REQUIRE(sawGeometry);
}

TEST_CASE("GLTF loader preserves full geometry when no options are provided", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwGltfLoader/test.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    const auto scene = cw::gltf::Loader::loadGltf(gltfPath);
    REQUIRE_FALSE(scene.meshes.isEmpty());

    bool sawGeometry = false;
    for (const auto& mesh : scene.meshes) {
        for (const auto& geometry : mesh.geometries) {
            sawGeometry = true;
            const auto buffers = geometry.vertexBuffers();
            REQUIRE(buffers.size() == 1);
            REQUIRE_FALSE(buffers[0].data->isEmpty());
            REQUIRE(geometry.vertexCount() == kExpectedVertexCount);
            REQUIRE(buffers[0].stride > 0);
            REQUIRE(geometry.indices().size() == kExpectedIndexCount);

            const auto* position = geometry.attribute(cwGeometry::Semantic::Position);
            const auto* texCoord0 = geometry.attribute(cwGeometry::Semantic::TexCoord0);
            const auto* normal = geometry.attribute(cwGeometry::Semantic::Normal);

            REQUIRE(position != nullptr);
            REQUIRE(texCoord0 != nullptr);
            REQUIRE(normal != nullptr);

            const auto& attributes = geometry.attributes();
            REQUIRE(attributes.size() == 3);
            REQUIRE(attributes[0].semantic == cwGeometry::Semantic::Position);
            REQUIRE(attributes[1].semantic == cwGeometry::Semantic::Normal);
            REQUIRE(attributes[2].semantic == cwGeometry::Semantic::TexCoord0);

            REQUIRE(position->format == cwGeometry::AttributeFormat::Vec3);
            REQUIRE(normal->format == cwGeometry::AttributeFormat::Vec3);
            REQUIRE(texCoord0->format == cwGeometry::AttributeFormat::Vec2);

            REQUIRE(buffers[0].stride == 32);
            REQUIRE(buffers[0].data->size() == geometry.vertexCount() * buffers[0].stride);
        }
    }

    REQUIRE(sawGeometry);
}

TEST_CASE("baseColorImage handles materials without a baseColor texture", "[cwGltfLoader]")
{
    constexpr int kTextureWidth = 4;
    constexpr int kTextureHeight = 2;

    cw::gltf::TextureCPU texture;
    texture.width = kTextureWidth;
    texture.height = kTextureHeight;
    texture.isSRGB = true;
    texture.pixels = QByteArray(kTextureWidth * kTextureHeight * 4, char(0xFF));

    cw::gltf::SceneCPU scene;
    scene.textures.append(texture);

    SECTION("valid index returns the texture's image") {
        cw::gltf::MaterialCPU material;
        material.baseColorTextureIndex = 0;

        const QImage image = cw::gltf::baseColorImage(scene, material);
        REQUIRE_FALSE(image.isNull());
        REQUIRE(image.width() == kTextureWidth);
        REQUIRE(image.height() == kTextureHeight);
    }

    SECTION("default material returns a null image") {
        const cw::gltf::MaterialCPU material;
        REQUIRE(material.baseColorTextureIndex == -1);
        REQUIRE(cw::gltf::baseColorImage(scene, material).isNull());
    }

    SECTION("out of range index returns a null image") {
        cw::gltf::MaterialCPU material;
        material.baseColorTextureIndex = scene.textures.size();
        REQUIRE(cw::gltf::baseColorImage(scene, material).isNull());
    }
}

TEST_CASE("GLTF loader caches scenes and shares texture pixels", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder(testcasesDatasetPath("test_cwGltfLoader/test.glb"));
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    cw::gltf::LoadOptions options;
    options.requestedLayout = cwRenderTexturedItems::geometryLayout();

    const auto firstScene = cw::gltf::Loader::loadGltf(gltfPath, options);
    REQUIRE_FALSE(firstScene.textures.isEmpty());
    const auto& firstTexture = firstScene.textures.at(0);
    REQUIRE_FALSE(firstTexture.pixels.isEmpty());

    SECTION("a second load of the same file shares the first load's pixels") {
        const auto secondScene = cw::gltf::Loader::loadGltf(gltfPath, options);
        REQUIRE_FALSE(secondScene.textures.isEmpty());
        REQUIRE(secondScene.textures.at(0).pixels.constData() == firstTexture.pixels.constData());
    }

    SECTION("a modified file loads fresh pixels") {
        QFile file(gltfPath);
        REQUIRE(file.open(QIODevice::ReadWrite));
        REQUIRE(file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(60),
                                 QFileDevice::FileModificationTime));
        file.close();

        const auto reloadedScene = cw::gltf::Loader::loadGltf(gltfPath, options);
        REQUIRE_FALSE(reloadedScene.textures.isEmpty());
        REQUIRE(reloadedScene.textures.at(0).pixels.constData() != firstTexture.pixels.constData());
    }

    SECTION("toImage borrows the texture's pixel buffer") {
        const QImage image = firstTexture.toImage();
        REQUIRE_FALSE(image.isNull());
        REQUIRE(image.width() == firstTexture.width);
        REQUIRE(image.height() == firstTexture.height);
        REQUIRE(reinterpret_cast<const char*>(image.constBits()) == firstTexture.pixels.constData());
    }
}
