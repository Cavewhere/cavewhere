// cwGeometry.tests.cpp
// Catch2 v3 testcases for cwGeometry

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QtNumeric>

#include <array>
#include <cstdint>
#include <limits>

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

TEST_CASE("cwGeometry: Type::Points (non-indexed)", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });
    g.setType(cwGeometry::Type::Points);

    SECTION("type round-trips and toString covers Points") {
        REQUIRE(g.type() == cwGeometry::Type::Points);
        REQUIRE(QString::fromLatin1(cwGeometry::toString(cwGeometry::Type::Points)) ==
                QStringLiteral("Points"));
    }

    SECTION("isEmpty returns false for non-empty point cloud with no indices") {
        QVector<QVector3D> points = {
            { 1.0f, 2.0f, 3.0f },
            { 4.0f, 5.0f, 6.0f },
            { 7.0f, 8.0f, 9.0f }
        };
        REQUIRE(g.set(cwGeometry::Semantic::Position, points));
        REQUIRE(g.indices().isEmpty());
        // Non-indexed primitives draw in vertex order — no indices needed.
        REQUIRE_FALSE(g.isEmpty());
        REQUIRE(g.vertexCount() == points.size());
    }

    SECTION("set<QVector3D> + values<QVector3D> round-trip") {
        QVector<QVector3D> in = {
            { 0.0f, 0.0f, 0.0f },
            { -1.5f, 2.5f, 3.5f },
            { 1000.0f, -1000.0f, 0.5f }
        };
        REQUIRE(g.set(cwGeometry::Semantic::Position, in));
        QVector<QVector3D> out = g.values<QVector3D>(cwGeometry::Semantic::Position);
        REQUIRE(out.size() == in.size());
        for (int i = 0; i < in.size(); ++i) {
            requireEqual(out[i], in[i]);
        }
    }

    SECTION("isEmpty stays true for an empty Points geometry") {
        REQUIRE(g.isEmpty());
    }

    SECTION("Triangles still requires indices to be non-empty") {
        cwGeometry tri({
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
        });
        tri.setType(cwGeometry::Type::Triangles);
        tri.resizeVertices(3);
        // Triangles without indices: still considered empty.
        REQUIRE(tri.isEmpty());
        tri.appendTriangle(0, 1, 2);
        REQUIRE_FALSE(tri.isEmpty());
    }
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

// ============================================================
// R1: integer / half / byte attribute support
// ============================================================

TEST_CASE("cwGeometry: componentByteSize covers every AttributeFormat", "[cwGeometry]") {
    using F = cwGeometry::AttributeFormat;
    REQUIRE(cwGeometry::componentByteSize(F::Float) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::Vec2) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::Vec3) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::Vec4) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::UInt) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::UInt2) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::UInt3) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::UInt4) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::SInt) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::SInt4) == 4);
    REQUIRE(cwGeometry::componentByteSize(F::Half) == 2);
    REQUIRE(cwGeometry::componentByteSize(F::Half4) == 2);
    REQUIRE(cwGeometry::componentByteSize(F::UNormByte) == 1);
    REQUIRE(cwGeometry::componentByteSize(F::UNormByte2) == 1);
    REQUIRE(cwGeometry::componentByteSize(F::UNormByte4) == 1);
}

TEST_CASE("cwGeometry: toUNormByte clamps and quantises", "[cwGeometry]") {
    REQUIRE(cwGeometry::toUNormByte(0.0f) == 0);
    REQUIRE(cwGeometry::toUNormByte(1.0f) == 255);
    REQUIRE(cwGeometry::toUNormByte(-0.5f) == 0);
    REQUIRE(cwGeometry::toUNormByte(2.0f) == 255);
    REQUIRE(cwGeometry::toUNormByte(0.5f) == 128); // round half to even (lround → away from zero), 127.5 → 128
}

TEST_CASE("cwGeometry: quint32 round-trip and Position+Classification layout", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::Classification, cwGeometry::AttributeFormat::UInt }
    });

    REQUIRE(g.attributes().size() == 2);
    const cwGeometry::VertexAttribute* pos = g.attribute(cwGeometry::Semantic::Position);
    const cwGeometry::VertexAttribute* cls = g.attribute(cwGeometry::Semantic::Classification);
    REQUIRE(pos != nullptr);
    REQUIRE(cls != nullptr);
    REQUIRE(pos->byteOffset == 0);
    REQUIRE(cls->byteOffset == 12);
    REQUIRE(g.vertexStride() == 16);

    QVector<quint32> classes = { 0u, 2u, 5u, 255u, std::numeric_limits<quint32>::max() };
    REQUIRE(g.set(cwGeometry::Semantic::Classification, classes));
    REQUIRE(g.vertexCount() == classes.size());

    // Position values default to zero — set them too so round-trip is exercised.
    QVector<QVector3D> positions = {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 2.0f, 3.0f },
        { 4.0f, 5.0f, 6.0f },
        { 7.0f, 8.0f, 9.0f },
        { -1.0f, -2.0f, -3.0f }
    };
    REQUIRE(g.set(cwGeometry::Semantic::Position, positions));

    QVector<quint32> outClasses = g.values<quint32>(cwGeometry::Semantic::Classification);
    REQUIRE(outClasses == classes);

    QVector<QVector3D> outPositions = g.values<QVector3D>(cwGeometry::Semantic::Position);
    REQUIRE(outPositions.size() == positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        requireEqual(outPositions[i], positions[i]);
    }

    REQUIRE(g.value<quint32>(cwGeometry::Semantic::Classification, 3) == 255u);
}

TEST_CASE("cwGeometry: qint32 round-trip", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::SInt }
    });

    QVector<qint32> values = {
        0, -1, 1, std::numeric_limits<qint32>::min(), std::numeric_limits<qint32>::max()
    };
    REQUIRE(g.set(cwGeometry::Semantic::Custom, values));
    REQUIRE(g.values<qint32>(cwGeometry::Semantic::Custom) == values);
}

TEST_CASE("cwGeometry: qfloat16 round-trip", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::Half }
    });

    QVector<qfloat16> values = {
        qfloat16(0.0f), qfloat16(1.0f), qfloat16(-0.5f), qfloat16(123.5f)
    };
    REQUIRE(g.set(cwGeometry::Semantic::Custom, values));
    QVector<qfloat16> out = g.values<qfloat16>(cwGeometry::Semantic::Custom);
    REQUIRE(out.size() == values.size());
    for (int i = 0; i < values.size(); ++i) {
        REQUIRE(float(out[i]) == float(values[i]));
    }
}

TEST_CASE("cwGeometry: quint8 (UNormByte) round-trip", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::UNormByte }
    });

    QVector<quint8> values = { 0, 1, 127, 128, 255 };
    REQUIRE(g.set(cwGeometry::Semantic::Custom, values));
    REQUIRE(g.values<quint8>(cwGeometry::Semantic::Custom) == values);
}

TEST_CASE("cwGeometry: std::array variants round-trip", "[cwGeometry]") {
    SECTION("UInt2 / UInt3 / UInt4") {
        cwGeometry g({
            { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::UInt3 }
        });

        QVector<std::array<quint32, 3>> values = {
            { 1u, 2u, 3u },
            { 100u, 200u, 300u },
            { 0u, 0u, std::numeric_limits<quint32>::max() }
        };
        REQUIRE(g.set(cwGeometry::Semantic::Custom, values));
        QVector<std::array<quint32, 3>> out = g.values<std::array<quint32, 3>>(cwGeometry::Semantic::Custom);
        REQUIRE(out == values);
    }

    SECTION("SInt4") {
        cwGeometry g({
            { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::SInt4 }
        });

        QVector<std::array<qint32, 4>> values = {
            { -1, 0, 1, 2 },
            { std::numeric_limits<qint32>::min(), 0, 0, std::numeric_limits<qint32>::max() }
        };
        REQUIRE(g.set(cwGeometry::Semantic::Custom, values));
        REQUIRE(g.values<std::array<qint32, 4>>(cwGeometry::Semantic::Custom) == values);
    }

    SECTION("Half2") {
        cwGeometry g({
            { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::Half2 }
        });

        QVector<std::array<qfloat16, 2>> values = {
            { qfloat16(0.0f), qfloat16(1.0f) },
            { qfloat16(-1.5f), qfloat16(2.5f) }
        };
        REQUIRE(g.set(cwGeometry::Semantic::Custom, values));
        QVector<std::array<qfloat16, 2>> out = g.values<std::array<qfloat16, 2>>(cwGeometry::Semantic::Custom);
        REQUIRE(out.size() == values.size());
        for (int i = 0; i < values.size(); ++i) {
            REQUIRE(float(out[i][0]) == float(values[i][0]));
            REQUIRE(float(out[i][1]) == float(values[i][1]));
        }
    }

    SECTION("UNormByte4") {
        cwGeometry g({
            { cwGeometry::Semantic::Color0, cwGeometry::AttributeFormat::UNormByte4 }
        });

        QVector<std::array<quint8, 4>> values = {
            { 0, 0, 0, 255 },
            { 255, 128, 64, 0 },
            { 1, 2, 3, 4 }
        };
        REQUIRE(g.set(cwGeometry::Semantic::Color0, values));
        REQUIRE(g.values<std::array<quint8, 4>>(cwGeometry::Semantic::Color0) == values);
    }
}

TEST_CASE("cwGeometry: mixed Vec3 + UInt + Vec2 round-trip", "[cwGeometry]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::Classification, cwGeometry::AttributeFormat::UInt },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    });

    REQUIRE(g.vertexStride() == 12 + 4 + 8);
    REQUIRE(g.attribute(cwGeometry::Semantic::Position)->byteOffset == 0);
    REQUIRE(g.attribute(cwGeometry::Semantic::Classification)->byteOffset == 12);
    REQUIRE(g.attribute(cwGeometry::Semantic::TexCoord0)->byteOffset == 16);

    QVector<QVector3D> positions = { {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f} };
    QVector<quint32> classes = { 7u, 42u };
    QVector<QVector2D> uvs = { {0.1f, 0.2f}, {0.3f, 0.4f} };

    REQUIRE(g.set(cwGeometry::Semantic::Position, positions));
    REQUIRE(g.set(cwGeometry::Semantic::Classification, classes));
    REQUIRE(g.set(cwGeometry::Semantic::TexCoord0, uvs));

    QVector<QVector3D> outPos = g.values<QVector3D>(cwGeometry::Semantic::Position);
    QVector<quint32> outCls   = g.values<quint32>(cwGeometry::Semantic::Classification);
    QVector<QVector2D> outUv  = g.values<QVector2D>(cwGeometry::Semantic::TexCoord0);

    REQUIRE(outCls == classes);
    REQUIRE(outPos.size() == positions.size());
    REQUIRE(outUv.size() == uvs.size());
    for (int i = 0; i < positions.size(); ++i) {
        requireEqual(outPos[i], positions[i]);
        requireEqual(outUv[i], uvs[i]);
    }
}

TEST_CASE("cwGeometry: alignment-aware buildLayout", "[cwGeometry]") {
    SECTION("UNormByte then Vec3 — Vec3 aligned to 4, stride padded to 16") {
        cwGeometry g({
            { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::UNormByte },
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
        });

        REQUIRE(g.attributes()[0].byteOffset == 0);
        REQUIRE(g.attributes()[1].byteOffset == 4);
        REQUIRE(g.vertexStride() == 16); // 4 + 12
    }

    SECTION("UNormByte then Half2 — Half2 aligned to 2, stride padded to 8") {
        cwGeometry g({
            { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::UNormByte },
            { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Half2 }
        });

        REQUIRE(g.attributes()[0].byteOffset == 0);
        REQUIRE(g.attributes()[1].byteOffset == 2); // padded from 1 → 2 to align to component size
        REQUIRE(g.vertexStride() == 8);              // 2 + 4 = 6 → padded up to 8
    }

    SECTION("Three UNormBytes pack tight at 0/1/2, stride padded to 4") {
        cwGeometry g({
            { cwGeometry::Semantic::Color0, cwGeometry::AttributeFormat::UNormByte },
            { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::UNormByte },
            { cwGeometry::Semantic::TexCoord1, cwGeometry::AttributeFormat::UNormByte }
        });

        REQUIRE(g.attributes()[0].byteOffset == 0);
        REQUIRE(g.attributes()[1].byteOffset == 1);
        REQUIRE(g.attributes()[2].byteOffset == 2);
        REQUIRE(g.vertexStride() == 4);
    }

    SECTION("Half2 then Float — Float aligned to 4, stride padded to 8") {
        cwGeometry g({
            { cwGeometry::Semantic::Custom, cwGeometry::AttributeFormat::Half2 },
            { cwGeometry::Semantic::Tangent, cwGeometry::AttributeFormat::Float }
        });

        REQUIRE(g.attributes()[0].byteOffset == 0);
        REQUIRE(g.attributes()[1].byteOffset == 4);
        REQUIRE(g.vertexStride() == 8);
    }
}

TEST_CASE("cwGeometry: toString covers Classification + new formats", "[cwGeometry]") {
    REQUIRE(QString::fromLatin1(cwGeometry::toString(cwGeometry::Semantic::Classification)) ==
            QStringLiteral("Classification"));
    REQUIRE(QString::fromLatin1(cwGeometry::toString(cwGeometry::AttributeFormat::UInt)) ==
            QStringLiteral("UInt"));
    REQUIRE(QString::fromLatin1(cwGeometry::toString(cwGeometry::AttributeFormat::SInt4)) ==
            QStringLiteral("SInt4"));
    REQUIRE(QString::fromLatin1(cwGeometry::toString(cwGeometry::AttributeFormat::Half2)) ==
            QStringLiteral("Half2"));
    REQUIRE(QString::fromLatin1(cwGeometry::toString(cwGeometry::AttributeFormat::UNormByte4)) ==
            QStringLiteral("UNormByte4"));
}
