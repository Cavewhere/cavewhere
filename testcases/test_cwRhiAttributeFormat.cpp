// Catch2 v3 testcases for cwRhiAttributeFormat.h

#include <catch2/catch_test_macros.hpp>

#include "cwRhiAttributeFormat.h"

TEST_CASE("toRhiFormat maps every cwGeometry::AttributeFormat to its Qt RHI counterpart", "[cwRhiAttributeFormat]") {
    using F = cwGeometry::AttributeFormat;
    REQUIRE(toRhiFormat(F::Float)      == QRhiVertexInputAttribute::Float);
    REQUIRE(toRhiFormat(F::Vec2)       == QRhiVertexInputAttribute::Float2);
    REQUIRE(toRhiFormat(F::Vec3)       == QRhiVertexInputAttribute::Float3);
    REQUIRE(toRhiFormat(F::Vec4)       == QRhiVertexInputAttribute::Float4);
    REQUIRE(toRhiFormat(F::UInt)       == QRhiVertexInputAttribute::UInt);
    REQUIRE(toRhiFormat(F::UInt2)      == QRhiVertexInputAttribute::UInt2);
    REQUIRE(toRhiFormat(F::UInt3)      == QRhiVertexInputAttribute::UInt3);
    REQUIRE(toRhiFormat(F::UInt4)      == QRhiVertexInputAttribute::UInt4);
    REQUIRE(toRhiFormat(F::SInt)       == QRhiVertexInputAttribute::SInt);
    REQUIRE(toRhiFormat(F::SInt2)      == QRhiVertexInputAttribute::SInt2);
    REQUIRE(toRhiFormat(F::SInt3)      == QRhiVertexInputAttribute::SInt3);
    REQUIRE(toRhiFormat(F::SInt4)      == QRhiVertexInputAttribute::SInt4);
    REQUIRE(toRhiFormat(F::Half)       == QRhiVertexInputAttribute::Half);
    REQUIRE(toRhiFormat(F::Half2)      == QRhiVertexInputAttribute::Half2);
    REQUIRE(toRhiFormat(F::Half3)      == QRhiVertexInputAttribute::Half3);
    REQUIRE(toRhiFormat(F::Half4)      == QRhiVertexInputAttribute::Half4);
    REQUIRE(toRhiFormat(F::UNormByte)  == QRhiVertexInputAttribute::UNormByte);
    REQUIRE(toRhiFormat(F::UNormByte2) == QRhiVertexInputAttribute::UNormByte2);
    REQUIRE(toRhiFormat(F::UNormByte4) == QRhiVertexInputAttribute::UNormByte4);
}

TEST_CASE("buildRhiInputLayout: Interleaved geometry produces a single binding", "[cwRhiAttributeFormat]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    });

    QRhiVertexInputLayout layout = buildRhiInputLayout(g);

    // One binding (the interleaved buffer), stride = 12 + 8 = 20.
    auto bIt = layout.cbeginBindings();
    REQUIRE(bIt != layout.cendBindings());
    REQUIRE(bIt->stride() == 20);
    ++bIt;
    REQUIRE(bIt == layout.cendBindings());

    // Two attributes — both reference binding 0, with byte offsets 0 and 12.
    auto aIt = layout.cbeginAttributes();
    REQUIRE(aIt != layout.cendAttributes());
    REQUIRE(aIt->binding() == 0);
    REQUIRE(aIt->location() == 0);
    REQUIRE(aIt->format() == QRhiVertexInputAttribute::Float3);
    REQUIRE(aIt->offset() == 0);
    ++aIt;
    REQUIRE(aIt->binding() == 0);
    REQUIRE(aIt->location() == 1);
    REQUIRE(aIt->format() == QRhiVertexInputAttribute::Float2);
    REQUIRE(aIt->offset() == 12);
    ++aIt;
    REQUIRE(aIt == layout.cendAttributes());
}

TEST_CASE("buildRhiInputLayout: Separated geometry produces one binding per attribute", "[cwRhiAttributeFormat]") {
    cwGeometry g({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::Classification, cwGeometry::AttributeFormat::UInt }
    }, cwGeometry::LayoutMode::Separated);

    QRhiVertexInputLayout layout = buildRhiInputLayout(g);

    // Two bindings, strides 12 (Vec3) and 4 (UInt).
    auto bIt = layout.cbeginBindings();
    REQUIRE(bIt != layout.cendBindings());
    REQUIRE(bIt->stride() == 12);
    ++bIt;
    REQUIRE(bIt != layout.cendBindings());
    REQUIRE(bIt->stride() == 4);
    ++bIt;
    REQUIRE(bIt == layout.cendBindings());

    // Two attributes — each references its own binding, byte offset 0.
    auto aIt = layout.cbeginAttributes();
    REQUIRE(aIt->binding() == 0);
    REQUIRE(aIt->location() == 0);
    REQUIRE(aIt->format() == QRhiVertexInputAttribute::Float3);
    REQUIRE(aIt->offset() == 0);
    ++aIt;
    REQUIRE(aIt->binding() == 1);
    REQUIRE(aIt->location() == 1);
    REQUIRE(aIt->format() == QRhiVertexInputAttribute::UInt);
    REQUIRE(aIt->offset() == 0);
}
