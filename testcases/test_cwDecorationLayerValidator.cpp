/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwDecorationLayerValidator.h"
#include "cwDecorationLayer.h"
#include "cwPlacementRuleData.h"
#include "cwPlacementRuleRegistry.h"
#include "cwSymbologyErrorCodes.h"
#include "cwError.h"

// Qt
#include <QColor>
#include <QList>
#include <QSet>
#include <QString>

namespace {

cwPlacementRuleData rule(const QString &name)
{
    cwPlacementRuleData data;
    data.name = name;
    return data;
}

cwError::ErrorType severityOf(const QList<cwDecorationLayerError> &errors, SymbologyErrorCode code)
{
    for (const cwDecorationLayerError &e : errors) {
        if (e.error.errorTypeId() == static_cast<int>(code)) {
            return e.error.type();
        }
    }
    return cwError::NoError;
}

bool hasCode(const QList<cwDecorationLayerError> &errors, SymbologyErrorCode code)
{
    return severityOf(errors, code) != cwError::NoError;
}

// Flat rule index of the first error carrying `code`; -2 (distinct from the
// layer-level -1) when no such error is present.
int ruleIndexOf(const QList<cwDecorationLayerError> &errors, SymbologyErrorCode code)
{
    for (const cwDecorationLayerError &e : errors) {
        if (e.error.errorTypeId() == static_cast<int>(code)) {
            return e.ruleIndex;
        }
    }
    return -2;
}

QList<cwDecorationLayerError> validate(const cwDecorationLayer &layer, const QSet<QString> &glyphs = {})
{
    return cwDecorationLayerValidator::validate(layer, cwPlacementRuleRegistry::instance(), glyphs);
}

} // namespace

TEST_CASE("An empty rule stack is a valid no-ink layer", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    CHECK(validate(layer).isEmpty());
}

TEST_CASE("A well-formed stamp stack reports no problems", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("tick");
    layer.rules = {rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Align to tangent")),
                   rule(QStringLiteral("Rigid stamp"))};
    CHECK(validate(layer, {QStringLiteral("tick")}).isEmpty());
}

TEST_CASE("A well-formed trace stack reports no problems", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Trace"))};
    CHECK(validate(layer).isEmpty());
}

TEST_CASE("An offset polyline stack reports no problems", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Trace"))};
    CHECK(validate(layer).isEmpty());
}

TEST_CASE("A trace stack carrying a fill reports no problems", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Trace"))};
    layer.fillColorLight = QColor(QStringLiteral("#1e90ff"));
    layer.fillColorDark = QColor(QStringLiteral("#1e90ff"));
    CHECK(validate(layer).isEmpty());
}

TEST_CASE("Two terminal rules is fatal", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Rigid stamp")),
                   rule(QStringLiteral("Trace"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::TwoTerminals));
    CHECK(severityOf(errors, SymbologyErrorCode::TwoTerminals) == cwError::Fatal);
}

TEST_CASE("Two Offset stroke rules is fatal", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Trace"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::TwoTransformStrokes));
    CHECK(severityOf(errors, SymbologyErrorCode::TwoTransformStrokes) == cwError::Fatal);
}

TEST_CASE("Rules without a terminal warn", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Uniform spacing"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::NoTerminalForRules));
    CHECK(severityOf(errors, SymbologyErrorCode::NoTerminalForRules) == cwError::Warning);
}

TEST_CASE("A stamp terminal with no Generate rule warns", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("tick");
    layer.rules = {rule(QStringLiteral("Rigid stamp"))};
    const QList<cwDecorationLayerError> errors = validate(layer, {QStringLiteral("tick")});
    REQUIRE(hasCode(errors, SymbologyErrorCode::StampsWithoutGenerate));
    CHECK(severityOf(errors, SymbologyErrorCode::StampsWithoutGenerate) == cwError::Warning);
}

TEST_CASE("A stamp terminal with no glyph name warns", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Rigid stamp"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::StampsWithoutGlyph));
    CHECK_FALSE(hasCode(errors, SymbologyErrorCode::MissingGlyph));
}

TEST_CASE("A stamp terminal naming a glyph absent from the palette warns",
          "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("missing");
    layer.rules = {rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Rigid stamp"))};
    const QList<cwDecorationLayerError> errors = validate(layer, {QStringLiteral("tick")});
    REQUIRE(hasCode(errors, SymbologyErrorCode::MissingGlyph));
    CHECK(severityOf(errors, SymbologyErrorCode::MissingGlyph) == cwError::Warning);
    CHECK_FALSE(hasCode(errors, SymbologyErrorCode::StampsWithoutGlyph));
}

TEST_CASE("Placement rules under a trace terminal are dead and warn",
          "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Trace"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::DeadRulesUnderTrace));
    CHECK(severityOf(errors, SymbologyErrorCode::DeadRulesUnderTrace) == cwError::Warning);
}

TEST_CASE("An unknown rule name warns", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("No Such Rule")),
                   rule(QStringLiteral("Trace"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::UnknownRule));
    CHECK(severityOf(errors, SymbologyErrorCode::UnknownRule) == cwError::Warning);
    CHECK(ruleIndexOf(errors, SymbologyErrorCode::UnknownRule) == 0);
}

TEST_CASE("A rule out of pipeline order warns at its flat index",
          "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("tick");
    // Terminal first, then its placement/orientation rules: each later rule runs
    // before the higher-stage terminal already above it.
    layer.rules = {rule(QStringLiteral("Rigid stamp")),
                   rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Align to tangent"))};
    const QList<cwDecorationLayerError> errors = validate(layer, {QStringLiteral("tick")});
    REQUIRE(hasCode(errors, SymbologyErrorCode::RulesOutOfOrder));
    CHECK(severityOf(errors, SymbologyErrorCode::RulesOutOfOrder) == cwError::Warning);
    // The first out-of-order rule is "Uniform spacing" at flat index 1.
    CHECK(ruleIndexOf(errors, SymbologyErrorCode::RulesOutOfOrder) == 1);
}

TEST_CASE("A well-ordered stamp stack reports no out-of-order problem",
          "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("tick");
    layer.rules = {rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Align to tangent")),
                   rule(QStringLiteral("Rigid stamp"))};
    CHECK_FALSE(hasCode(validate(layer, {QStringLiteral("tick")}),
                        SymbologyErrorCode::RulesOutOfOrder));
}

TEST_CASE("An extra terminal rule is fatal at its flat index",
          "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Rigid stamp")),
                   rule(QStringLiteral("Trace"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::TwoTerminals));
    // Attributed to the second (dead) terminal, not the layer.
    CHECK(ruleIndexOf(errors, SymbologyErrorCode::TwoTerminals) == 1);
}

TEST_CASE("An extra stroke-transform rule is fatal at its flat index",
          "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Trace"))};
    const QList<cwDecorationLayerError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::TwoTransformStrokes));
    CHECK(ruleIndexOf(errors, SymbologyErrorCode::TwoTransformStrokes) == 1);
}
