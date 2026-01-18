//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGltfLoader.h"
#include "cwRenderTexturedItems.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QFileInfo>

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
        REQUIRE(actual.byteOffset == offset);
        offset += actual.byteSize();
    }

    REQUIRE(geometry.vertexStride() == offset);
    REQUIRE(geometry.vertexStride() == expectedStride(layout));
}
}

TEST_CASE("GLTF loader repacks geometry for textured items", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder("://datasets/test_cwGltfLoader/test.glb");
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
            REQUIRE_FALSE(geometry.vertexData().isEmpty());
            REQUIRE(geometry.vertexCount() == kExpectedVertexCount);
            REQUIRE(geometry.vertexStride() > 0);
            REQUIRE(geometry.indices().size() == kExpectedIndexCount);

            requireLayoutMatches(geometry, options.requestedLayout);

            REQUIRE(geometry.attribute(cwGeometry::Semantic::Position) != nullptr);
            REQUIRE(geometry.attribute(cwGeometry::Semantic::TexCoord0) != nullptr);
            REQUIRE(geometry.vertexData().size() == geometry.vertexCount() * geometry.vertexStride());
        }
    }

    REQUIRE(sawGeometry);
}

TEST_CASE("GLTF loader preserves full geometry when no options are provided", "[cwGltfLoader]")
{
    const QString gltfPath = copyToTempFolder("://datasets/test_cwGltfLoader/test.glb");
    REQUIRE_FALSE(gltfPath.isEmpty());
    REQUIRE(QFileInfo::exists(gltfPath));

    const auto scene = cw::gltf::Loader::loadGltf(gltfPath);
    REQUIRE_FALSE(scene.meshes.isEmpty());

    bool sawGeometry = false;
    for (const auto& mesh : scene.meshes) {
        for (const auto& geometry : mesh.geometries) {
            sawGeometry = true;
            REQUIRE_FALSE(geometry.vertexData().isEmpty());
            REQUIRE(geometry.vertexCount() == kExpectedVertexCount);
            REQUIRE(geometry.vertexStride() > 0);
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

            REQUIRE(geometry.vertexStride() == 32);
            REQUIRE(geometry.vertexData().size() == geometry.vertexCount() * geometry.vertexStride());
        }
    }

    REQUIRE(sawGeometry);
}
