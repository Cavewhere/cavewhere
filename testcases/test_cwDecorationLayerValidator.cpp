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

cwError::ErrorType severityOf(const QList<cwError> &errors, SymbologyErrorCode code)
{
    for (const cwError &error : errors) {
        if (error.errorTypeId() == static_cast<int>(code)) {
            return error.type();
        }
    }
    return cwError::NoError;
}

bool hasCode(const QList<cwError> &errors, SymbologyErrorCode code)
{
    return severityOf(errors, code) != cwError::NoError;
}

QList<cwError> validate(const cwDecorationLayer &layer, const QSet<QString> &glyphs = {})
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

TEST_CASE("A well-formed polyline stack reports no problems", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Trace polyline"))};
    CHECK(validate(layer).isEmpty());
}

TEST_CASE("An offset polyline stack reports no problems", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Trace polyline"))};
    CHECK(validate(layer).isEmpty());
}

TEST_CASE("Two terminal rules is fatal", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Rigid stamp")),
                   rule(QStringLiteral("Trace polyline"))};
    const QList<cwError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::TwoTerminals));
    CHECK(severityOf(errors, SymbologyErrorCode::TwoTerminals) == cwError::Fatal);
}

TEST_CASE("Two Offset stroke rules is fatal", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Offset stroke")),
                   rule(QStringLiteral("Trace polyline"))};
    const QList<cwError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::TwoTransformStrokes));
    CHECK(severityOf(errors, SymbologyErrorCode::TwoTransformStrokes) == cwError::Fatal);
}

TEST_CASE("Rules without a terminal warn", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Uniform spacing"))};
    const QList<cwError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::NoTerminalForRules));
    CHECK(severityOf(errors, SymbologyErrorCode::NoTerminalForRules) == cwError::Warning);
}

TEST_CASE("A stamp terminal with no Generate rule warns", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.glyphName = QStringLiteral("tick");
    layer.rules = {rule(QStringLiteral("Rigid stamp"))};
    const QList<cwError> errors = validate(layer, {QStringLiteral("tick")});
    REQUIRE(hasCode(errors, SymbologyErrorCode::StampsWithoutGenerate));
    CHECK(severityOf(errors, SymbologyErrorCode::StampsWithoutGenerate) == cwError::Warning);
}

TEST_CASE("A stamp terminal with no glyph name warns", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Rigid stamp"))};
    const QList<cwError> errors = validate(layer);
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
    const QList<cwError> errors = validate(layer, {QStringLiteral("tick")});
    REQUIRE(hasCode(errors, SymbologyErrorCode::MissingGlyph));
    CHECK(severityOf(errors, SymbologyErrorCode::MissingGlyph) == cwError::Warning);
    CHECK_FALSE(hasCode(errors, SymbologyErrorCode::StampsWithoutGlyph));
}

TEST_CASE("Placement rules under a polyline terminal are dead and warn",
          "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("Uniform spacing")),
                   rule(QStringLiteral("Trace polyline"))};
    const QList<cwError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::DeadRulesUnderPolylines));
    CHECK(severityOf(errors, SymbologyErrorCode::DeadRulesUnderPolylines) == cwError::Warning);
}

TEST_CASE("An unknown rule name warns", "[cwDecorationLayerValidator]")
{
    cwDecorationLayer layer;
    layer.rules = {rule(QStringLiteral("No Such Rule")),
                   rule(QStringLiteral("Trace polyline"))};
    const QList<cwError> errors = validate(layer);
    REQUIRE(hasCode(errors, SymbologyErrorCode::UnknownRule));
    CHECK(severityOf(errors, SymbologyErrorCode::UnknownRule) == cwError::Warning);
}
