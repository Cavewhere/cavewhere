// cwGeometry.tests.cpp
// Catch2 v3 testcases for cwGeometry

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

#include "cwGeometry.h"

using Catch::Matchers::WithinAbs;

static void requireEqual(const QVector2D& a, const QVector2D& b, float eps = 1e-5f) {
    REQUIRE_THAT(a.x(), WithinAbs(b.x(), eps));
    REQUIRE_THAT(a.y(), WithinAbs(b.y(), eps));
}

static void requireEqual(const QVector3D& a, const QVector3D& b, float eps = 1e-5f) {
    REQUIRE_THAT(a.x(), WithinAbs(b.x(), eps));
    REQUIRE_THAT(a.y(), WithinAbs(b.y(), eps));
    REQUIRE_THAT(a.z(), WithinAbs(b.z(), eps));
}

static void requireEqual(const QVector4D& a, const QVector4D& b, float eps = 1e-5f) {
    REQUIRE_THAT(a.x(), WithinAbs(b.x(), eps));
    REQUIRE_THAT(a.y(), WithinAbs(b.y(), eps));
    REQUIRE_THAT(a.z(), WithinAbs(b.z(), eps));
    REQUIRE_THAT(a.w(), WithinAbs(b.w(), eps));
}

TEST_CASE("cwGeometry: layout construction and stride", "[cwGeometry]") {
    SECTION("initializer_list layout builds attributes and stride") {
        cwGeometry g({
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
            { cwGeometry::Semantic::Normal,   cwGeometry::AttributeFormat::Vec3 },
            { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
        });

        REQUIRE(g.attributes().size() == 3);
        REQUIRE(g.vertexStride() == int((3 + 3 + 2) * sizeof(float)));

        const cwGeometry::VertexAttribute* pos = g.attribute(cwGeometry::Semantic::Position);
        REQUIRE(pos != nullptr);
        REQUIRE(pos->byteOffset == 0);

        const cwGeometry::VertexAttribute* nrm = g.attribute(cwGeometry::Semantic::Normal);
        REQUIRE(nrm != nullptr);
        REQUIRE(nrm->byteOffset == int(3 * sizeof(float)));

        const cwGeometry::VertexAttribute* uv0 = g.attribute(cwGeometry::Semantic::TexCoord0);
        REQUIRE(uv0 != nullptr);
        REQUIRE(uv0->byteOffset == int((3 + 3) * sizeof(float)));
    }

    SECTION("QVector layout builds identically") {
        QVector<cwGeometry::AttributeDesc> layout = {
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
            { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
        };
        cwGeometry g(layout);
        REQUIRE(g.attributes().size() == 2);
        REQUIRE(g.vertexStride() == int((3 + 2) * sizeof(float)));
    }
}

TEST_CASE("cwGeometry: resize, counts, and emptiness", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });

    REQUIRE(g.vertexCount() == 0);
    REQUIRE(g.isEmpty());

    g.resizeVertices(4);
    REQUIRE(g.vertexCount() == 4);
    REQUIRE(g.vertexData().size() == 4 * g.vertexStride());

    g.appendTriangle(0, 1, 2);
    REQUIRE_FALSE(g.isEmpty());

    g.clearIndexData();
    REQUIRE(g.isEmpty());

    g.clearVertexData();
    REQUIRE(g.vertexCount() == 0);
    REQUIRE(g.isEmpty());
}

TEST_CASE("cwGeometry: set and get single attribute values", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 },
        { cwGeometry::Semantic::Color0, cwGeometry::AttributeFormat::Vec4 },
        { cwGeometry::Semantic::Tangent, cwGeometry::AttributeFormat::Float }
    });

    const int count = 3;
    g.resizeVertices(count);

    // Per element set
    REQUIRE(g.set(cwGeometry::Semantic::Position, 0, QVector3D(1.0f, 2.0f, 3.0f)));
    REQUIRE(g.set(cwGeometry::Semantic::Position, 1, QVector3D(4.0f, 5.0f, 6.0f)));
    REQUIRE(g.set(cwGeometry::Semantic::Position, 2, QVector3D(7.0f, 8.0f, 9.0f)));

    REQUIRE(g.set(cwGeometry::Semantic::TexCoord0, 0, QVector2D(0.1f, 0.2f)));
    REQUIRE(g.set(cwGeometry::Semantic::TexCoord0, 1, QVector2D(0.3f, 0.4f)));
    REQUIRE(g.set(cwGeometry::Semantic::TexCoord0, 2, QVector2D(0.5f, 0.6f)));

    REQUIRE(g.set(cwGeometry::Semantic::Color0, 0, QVector4D(0.1f, 0.2f, 0.3f, 0.4f)));
    REQUIRE(g.set(cwGeometry::Semantic::Color0, 1, QVector4D(0.5f, 0.6f, 0.7f, 0.8f)));
    REQUIRE(g.set(cwGeometry::Semantic::Color0, 2, QVector4D(0.9f, 1.0f, 1.1f, 1.2f)));

    REQUIRE(g.set(cwGeometry::Semantic::Tangent, 0, 10.0f));
    REQUIRE(g.set(cwGeometry::Semantic::Tangent, 1, 11.0f));
    REQUIRE(g.set(cwGeometry::Semantic::Tangent, 2, 12.0f));

    // Get single value
    requireEqual(g.value<QVector3D>(cwGeometry::Semantic::Position, 0), QVector3D(1.0f, 2.0f, 3.0f));
    requireEqual(g.value<QVector3D>(cwGeometry::Semantic::Position, 1), QVector3D(4.0f, 5.0f, 6.0f));
    requireEqual(g.value<QVector3D>(cwGeometry::Semantic::Position, 2), QVector3D(7.0f, 8.0f, 9.0f));

    requireEqual(g.value<QVector2D>(cwGeometry::Semantic::TexCoord0, 0), QVector2D(0.1f, 0.2f));
    requireEqual(g.value<QVector2D>(cwGeometry::Semantic::TexCoord0, 1), QVector2D(0.3f, 0.4f));
    requireEqual(g.value<QVector2D>(cwGeometry::Semantic::TexCoord0, 2), QVector2D(0.5f, 0.6f));

    requireEqual(g.value<QVector4D>(cwGeometry::Semantic::Color0, 0), QVector4D(0.1f, 0.2f, 0.3f, 0.4f));
    requireEqual(g.value<QVector4D>(cwGeometry::Semantic::Color0, 1), QVector4D(0.5f, 0.6f, 0.7f, 0.8f));
    requireEqual(g.value<QVector4D>(cwGeometry::Semantic::Color0, 2), QVector4D(0.9f, 1.0f, 1.1f, 1.2f));

    REQUIRE_THAT(g.value<float>(cwGeometry::Semantic::Tangent, 0), WithinAbs(10.0f, 1e-6f));
    REQUIRE_THAT(g.value<float>(cwGeometry::Semantic::Tangent, 1), WithinAbs(11.0f, 1e-6f));
    REQUIRE_THAT(g.value<float>(cwGeometry::Semantic::Tangent, 2), WithinAbs(12.0f, 1e-6f));
}

TEST_CASE("cwGeometry: bulk set and bulk get values()", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::Normal, cwGeometry::AttributeFormat::Vec3 }
    });

    QVector<QVector3D> positions = {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 2.0f, 3.0f },
        { -1.0f, -2.0f, -3.0f },
        { 5.0f, 6.0f, 7.0f }
    };

    QVector<QVector3D> normals = {
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f }
    };

    REQUIRE(g.set(cwGeometry::Semantic::Position, positions));
    REQUIRE(g.vertexCount() == positions.size());
    REQUIRE(g.vertexData().size() == positions.size() * g.vertexStride());

    REQUIRE(g.set(cwGeometry::Semantic::Normal, normals));

    // values() round trip
    QVector<QVector3D> outPositions = g.values<QVector3D>(cwGeometry::Semantic::Position);
    QVector<QVector3D> outNormals = g.values<QVector3D>(cwGeometry::Semantic::Normal);

    REQUIRE(outPositions.size() == positions.size());
    REQUIRE(outNormals.size() == normals.size());

    for (int i = 0; i < positions.size(); i++) {
        requireEqual(outPositions[i], positions[i]);
        requireEqual(outNormals[i], normals[i]);
    }

    // const correctness: call through const reference
    const cwGeometry& cg = g;
    QVector<QVector3D> outPositionsConst = cg.values<QVector3D>(cwGeometry::Semantic::Position);
    REQUIRE(outPositionsConst.size() == positions.size());
    for (int i = 0; i < positions.size(); i++) {
        requireEqual(outPositionsConst[i], positions[i]);
    }
}

TEST_CASE("cwGeometry: indices and primitive helpers", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });
    g.resizeVertices(4);

    REQUIRE(g.indices().isEmpty());

    g.appendTriangle(0, 1, 2);
    g.appendTriangle(2, 1, 3);
    REQUIRE(g.indices().size() == 6);
    REQUIRE(g.indices()[0] == 0u);
    REQUIRE(g.indices()[1] == 1u);
    REQUIRE(g.indices()[2] == 2u);
    REQUIRE(g.indices()[3] == 2u);
    REQUIRE(g.indices()[4] == 1u);
    REQUIRE(g.indices()[5] == 3u);

    g.clearIndexData();
    REQUIRE(g.indices().isEmpty());

    g.appendLine(0, 1);
    REQUIRE(g.indices().size() == 2);
    REQUIRE(g.indices()[0] == 0u);
    REQUIRE(g.indices()[1] == 1u);

    g.setType(cwGeometry::Type::Lines);
    REQUIRE(g.type() == cwGeometry::Type::Lines);
}

TEST_CASE("cwGeometry: transform and flags", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });

    QMatrix4x4 m;
    m.translate(1.0f, 2.0f, 3.0f);
    g.setTransform(m);

    REQUIRE(g.transform()(0, 3) == m(0, 3));
    REQUIRE(g.transform()(1, 3) == m(1, 3));
    REQUIRE(g.transform()(2, 3) == m(2, 3));

    REQUIRE(g.cullBackfaces());
    g.setCullBackfaces(false);
    REQUIRE_FALSE(g.cullBackfaces());
}

TEST_CASE("cwGeometry: attribute lookup and format mismatch safety", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });

    // Missing attribute returns nullptr and safe fallbacks
    REQUIRE(g.attribute(cwGeometry::Semantic::TexCoord0) == nullptr);
    QVector<QVector2D> uvs = { {0.0f, 0.0f} };
    REQUIRE_FALSE(g.set(cwGeometry::Semantic::TexCoord0, uvs));

    // Format mismatch: try to set Vec2 data into Vec3 attribute, expect false
    QVector<QVector2D> badPositions = { {1.0f, 2.0f} };
    REQUIRE_FALSE(g.set(cwGeometry::Semantic::Position, badPositions));
}
