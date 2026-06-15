/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Our
#include "cwPlacementRuleParams.h"
#include "cwPlacementRuleParamsCodec.h"
#include "cwPlacementRuleData.h"
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"
#include "cwStrokePath.h"
#include "cwDecorationLayer.h"
#include "cwStampPosition.h"
#include "cwLineBrush.h"
#include "cwSymbologyPaletteIO.h"

// Qt
#include <QByteArray>
#include <QPointF>
#include <QString>
#include <QVariant>
#include <QVector>

// Std
#include <limits>

using Catch::Approx;

// Commit B: placement-rule params are decoded at load into a typed,
// proto-free struct carried by cwPlacementRuleData::parameters (a QVariant).
// These tests cover the codec behavior matrix, cwPlacementRuleData equality
// over QVariant, the rule honoring its params, and the IO round-trip — including
// the opaque-passthrough guarantee that protects unknown/future rules.

namespace {

const QString kUniformSpacing = cwUniformSpacingRuleName();

// A one-layer brush carrying `layer` — the IO round-trip fixture.
cwLineBrush brushWithLayer(const QString &name, const cwDecorationLayer &layer)
{
    cwLineBrush brush;
    brush.name = name;
    brush.displayName = name;
    brush.category = QStringLiteral("test");
    brush.decorations.append(layer);
    return brush;
}

int uniformTickCount(const QVariant &params)
{
    const QVector<QPointF> points = {QPointF(0.0, 0.0), QPointF(10.0, 0.0)};
    const cwStrokePath strokePath(points);

    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("g");
    // worldPerPaperMm = 1 -> spacingMm maps 1:1 to world metres.
    cwPlacementContext context{strokePath, layer, 1.0};
    context.ruleParameters = params;

    QVector<cwStampPosition> positions;
    cwPlacementRuleRegistry::instance().rule(kUniformSpacing)->apply(positions, context);
    return positions.size();
}

} // namespace

TEST_CASE("Codec round-trips a known param struct", "[cwPlacementRuleParams]")
{
    const QByteArray bytes =
        cwPlacementRuleParamsCodec::encode(kUniformSpacing,
                                           QVariant::fromValue(cwUniformSpacingParams{3.0}));
    REQUIRE_FALSE(bytes.isEmpty());

    const QVariant decoded = cwPlacementRuleParamsCodec::decode(kUniformSpacing, bytes);
    REQUIRE(decoded.canConvert<cwUniformSpacingParams>());
    CHECK(decoded.value<cwUniformSpacingParams>() == cwUniformSpacingParams{3.0});
}

TEST_CASE("Codec round-trips Offset stroke params", "[cwPlacementRuleParams]")
{
    // Signed, non-zero so the wire payload isn't empty (proto3 omits zero scalars).
    const QByteArray bytes =
        cwPlacementRuleParamsCodec::encode(cwOffsetStrokeRuleName(),
                                           QVariant::fromValue(cwOffsetStrokeParams{-1.5}));
    REQUIRE_FALSE(bytes.isEmpty());

    const QVariant decoded = cwPlacementRuleParamsCodec::decode(cwOffsetStrokeRuleName(), bytes);
    REQUIRE(decoded.canConvert<cwOffsetStrokeParams>());
    CHECK(decoded.value<cwOffsetStrokeParams>() == cwOffsetStrokeParams{-1.5});
}

TEST_CASE("Codec maps empty bytes to a null QVariant and back", "[cwPlacementRuleParams]")
{
    const QVariant decoded = cwPlacementRuleParamsCodec::decode(kUniformSpacing, QByteArray());
    CHECK_FALSE(decoded.isValid());

    CHECK(cwPlacementRuleParamsCodec::encode(kUniformSpacing, QVariant()).isEmpty());
}

TEST_CASE("Codec preserves unknown-rule bytes verbatim", "[cwPlacementRuleParams]")
{
    const QByteArray opaque = QByteArrayLiteral("\x01\x02\xff future-rule blob");

    const QVariant decoded =
        cwPlacementRuleParamsCodec::decode(QStringLiteral("Future rule"), opaque);
    REQUIRE(decoded.typeId() == QMetaType::QByteArray);
    CHECK(decoded.toByteArray() == opaque);

    // Passthrough on the way back out, regardless of the rule name.
    CHECK(cwPlacementRuleParamsCodec::encode(QStringLiteral("Future rule"), decoded) == opaque);
}

TEST_CASE("Codec preserves malformed bytes for a known rule", "[cwPlacementRuleParams]")
{
    // A blob that is not a valid UniformSpacingParams wire message.
    const QByteArray malformed = QByteArrayLiteral("\xff\xff\xff\xff not-a-proto");

    const QVariant decoded = cwPlacementRuleParamsCodec::decode(kUniformSpacing, malformed);
    REQUIRE(decoded.typeId() == QMetaType::QByteArray);
    CHECK(decoded.toByteArray() == malformed);

    CHECK(cwPlacementRuleParamsCodec::encode(kUniformSpacing, decoded) == malformed);
}

TEST_CASE("cwPlacementRuleData compares equal over QVariant params", "[cwPlacementRuleParams]")
{
    const cwPlacementRuleData a{kUniformSpacing,
                                QVariant::fromValue(cwUniformSpacingParams{3.0})};
    const cwPlacementRuleData b{kUniformSpacing,
                                QVariant::fromValue(cwUniformSpacingParams{3.0})};
    const cwPlacementRuleData c{kUniformSpacing,
                                QVariant::fromValue(cwUniformSpacingParams{4.0})};

    CHECK(a == b);
    CHECK_FALSE(a == c);
}

TEST_CASE("Uniform spacing rule honors its params", "[cwPlacementRuleParams]")
{
    // 3 mm spacing over a 10 m stroke -> s = 0,3,6,9 -> 4 ticks.
    CHECK(uniformTickCount(QVariant::fromValue(cwUniformSpacingParams{3.0})) == 4);

    // Null params fall to the struct's 2 mm default -> s = 0,2,4,6,8,10 -> 6 ticks.
    CHECK(uniformTickCount(QVariant()) == 6);

    // Wrong-typed params also fall to the 2 mm default.
    CHECK(uniformTickCount(QVariant::fromValue(QStringLiteral("nonsense"))) == 6);
}

TEST_CASE("Uniform spacing rejects non-finite spacing from a crafted file",
          "[cwPlacementRuleParams]")
{
    // spacingMm is file-driven: a crafted/corrupt palette could carry +Inf, which
    // slips past the `<= 0` guard and would loop forever (the stride never
    // advances). NaN would also pass `<= 0`. Both must produce zero stamps, not a
    // hang or unbounded allocation.
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    CHECK(uniformTickCount(QVariant::fromValue(cwUniformSpacingParams{inf})) == 0);
    CHECK(uniformTickCount(QVariant::fromValue(cwUniformSpacingParams{nan})) == 0);
}

TEST_CASE("Brush IO round-trips typed rule params", "[cwPlacementRuleParams]")
{
    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("tick");
    layer.rules.append(cwPlacementRuleData{
        kUniformSpacing, QVariant::fromValue(cwUniformSpacingParams{3.0})});

    const cwLineBrush brush = brushWithLayer(QStringLiteral("paramtest"), layer);

    const auto loaded =
        cwSymbologyPaletteIO::brushFromJson(cwSymbologyPaletteIO::brushToJson(brush));
    REQUIRE_FALSE(loaded.hasError());

    const QVector<cwDecorationLayer> layers = loaded.value().decorations;
    REQUIRE(layers.size() == 1);
    REQUIRE(layers.first().rules.size() == 1);

    const cwPlacementRuleData &rule = layers.first().rules.first();
    CHECK(rule.name == kUniformSpacing);
    REQUIRE(rule.parameters.canConvert<cwUniformSpacingParams>());
    CHECK(rule.parameters.value<cwUniformSpacingParams>() == cwUniformSpacingParams{3.0});
}

TEST_CASE("Brush IO preserves an unknown rule's opaque params", "[cwPlacementRuleParams]")
{
    const QByteArray opaque = QByteArrayLiteral("\x0a\x05 hello future");

    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("tick");
    layer.rules.append(cwPlacementRuleData{
        QStringLiteral("Future rule"), QVariant::fromValue(opaque)});

    const cwLineBrush brush = brushWithLayer(QStringLiteral("opaquetest"), layer);

    const auto loaded =
        cwSymbologyPaletteIO::brushFromJson(cwSymbologyPaletteIO::brushToJson(brush));
    REQUIRE_FALSE(loaded.hasError());

    const QVector<cwDecorationLayer> layers = loaded.value().decorations;
    REQUIRE(layers.size() == 1);
    REQUIRE(layers.first().rules.size() == 1);

    const cwPlacementRuleData &rule = layers.first().rules.first();
    CHECK(rule.name == QStringLiteral("Future rule"));
    REQUIRE(rule.parameters.typeId() == QMetaType::QByteArray);
    CHECK(rule.parameters.toByteArray() == opaque);
}
