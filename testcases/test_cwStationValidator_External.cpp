/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Cavewhere includes
#include "cwSignalSpy.h"
#include "cwStationValidator.h"

// Qt includes
#include <QRegularExpression>
#include <QString>
#include <QValidator>

namespace {

// Acceptable maps to 2 in QValidator::State; the regex-based
// implementation never returns Intermediate, so a binary helper makes
// each test case read as "this string is accepted" / "this string is
// rejected" rather than "this string returns 2".
bool isAccepted(const cwStationValidator& validator, const QString& s)
{
    QString input = s;
    int pos = 0;
    return validator.validate(input, pos) == QValidator::Acceptable;
}

} // namespace

TEST_CASE("Native context accepts plain alphanumerics and hyphens, "
          "rejects dots and embedded spaces",
          "[Validator][External]")
{
    cwStationValidator validator;
    REQUIRE_FALSE(validator.external());

    // Bread-and-butter native names.
    CHECK(isAccepted(validator, QStringLiteral("A1")));
    CHECK(isAccepted(validator, QStringLiteral("a1")));
    CHECK(isAccepted(validator, QStringLiteral("A_1")));
    CHECK(isAccepted(validator, QStringLiteral("big-passage")));
    CHECK(isAccepted(validator, QStringLiteral("123")));

    // Dotted names are reserved for external-file namespacing; if a user
    // could type "A.1" into a native cave they would break the
    // round-trip through cwSurvexExporterRegion (Survex treats the dot
    // as a namespace separator).
    CHECK_FALSE(isAccepted(validator, QStringLiteral("A.1")));

    // Embedded spaces only matter for Walls-style names.
    CHECK_FALSE(isAccepted(validator, QStringLiteral("a b")));

    // Other punctuation must stay rejected in both modes - the relaxed
    // external regex only adds dot and space.
    CHECK_FALSE(isAccepted(validator, QStringLiteral("A/1")));
    CHECK_FALSE(isAccepted(validator, QStringLiteral("A!1")));
}

TEST_CASE("External context accepts dotted names, hyphen-dot mixes, "
          "and embedded spaces",
          "[Validator][External]")
{
    cwStationValidator validator;
    validator.setExternal(true);
    REQUIRE(validator.external());

    // Dotted namespacing (Survex *begin / Compass file.station).
    CHECK(isAccepted(validator, QStringLiteral("A.1")));
    CHECK(isAccepted(validator, QStringLiteral("entrance.A1")));

    // Both real-world patterns from the test gate.
    CHECK(isAccepted(validator, QStringLiteral("big-passage.east")));

    // Walls accepts an embedded space as part of a name; the external
    // regex matches that quirk so users can type names that exist in
    // their upstream .srv / .wpj files.
    CHECK(isAccepted(validator, QStringLiteral("a b")));

    // Native-style names must continue to pass in external mode -
    // relaxation only adds characters, never removes them.
    CHECK(isAccepted(validator, QStringLiteral("A1")));
    CHECK(isAccepted(validator, QStringLiteral("big-passage")));

    // Punctuation outside the documented set still rejected.
    CHECK_FALSE(isAccepted(validator, QStringLiteral("A/1")));
    CHECK_FALSE(isAccepted(validator, QStringLiteral("A!1")));
}

TEST_CASE("Switching the external Q_PROPERTY changes the validation outcome "
          "for the same input",
          "[Validator][External]")
{
    // Locks the contract that scrap / note station-reference inputs can
    // bind their validator's external property to the owner's
    // attachment state and the underlying regex follows. The QML
    // wiring lands in Phase 2 / 3; this test pins the C++ side.
    cwStationValidator validator;
    cwSignalSpy externalSpy(&validator, &cwStationValidator::externalChanged);

    QString input = QStringLiteral("A.1");
    int pos = 0;

    REQUIRE_FALSE(validator.external());
    CHECK(validator.validate(input, pos) != QValidator::Acceptable);

    validator.setExternal(true);
    CHECK(externalSpy.size() == 1);
    pos = 0;
    CHECK(validator.validate(input, pos) == QValidator::Acceptable);

    // Idempotent set does not re-emit - otherwise QML bindings that fan
    // out (notes panel + survey editor) would thrash on every scrap
    // selection.
    validator.setExternal(true);
    CHECK(externalSpy.size() == 1);

    // Flip back, signal fires, regex returns to strict mode.
    validator.setExternal(false);
    CHECK(externalSpy.size() == 2);
    pos = 0;
    CHECK(validator.validate(input, pos) != QValidator::Acceptable);
}

TEST_CASE("Static regex accessors compile and accept the documented characters",
          "[Validator][External]")
{
    // Pin the static accessors so downstream consumers (sketch input
    // highlighting, scrap station-reference popovers) that hold the
    // regex directly see the same character set as validate() does.
    // The invalid-character regex variants are documented inverses but
    // use a zero-or-more pattern that trivially matches an empty span,
    // so we only assert isValid() on them here and let the
    // validate() tests above lock the behavioural contract.
    const QRegularExpression nativeValid = cwStationValidator::validCharactersRegex();
    const QRegularExpression externalValid = cwStationValidator::externalStationCharactersRegex();
    const QRegularExpression nativeInvalid = cwStationValidator::invalidCharactersRegex();
    const QRegularExpression externalInvalid = cwStationValidator::externalInvalidCharactersRegex();

    REQUIRE(nativeValid.isValid());
    REQUIRE(externalValid.isValid());
    REQUIRE(nativeInvalid.isValid());
    REQUIRE(externalInvalid.isValid());

    CHECK(nativeValid.match(QStringLiteral("A1")).hasMatch());
    CHECK_FALSE(nativeValid.match(QStringLiteral(".")).hasMatch());
    CHECK_FALSE(nativeValid.match(QStringLiteral(" ")).hasMatch());

    // Pin the anchored-pattern contract. Without anchoring,
    // .match("A.1") would return true on the native regex (it matches
    // the "A") and downstream consumers using the static accessor for
    // a whole-string check would see a false positive.
    CHECK_FALSE(nativeValid.match(QStringLiteral("A.1")).hasMatch());
    CHECK_FALSE(nativeValid.match(QStringLiteral("a b")).hasMatch());

    CHECK(externalValid.match(QStringLiteral("A1")).hasMatch());
    CHECK(externalValid.match(QStringLiteral(".")).hasMatch());
    CHECK(externalValid.match(QStringLiteral(" ")).hasMatch());
    CHECK(externalValid.match(QStringLiteral("-")).hasMatch());
    CHECK(externalValid.match(QStringLiteral("_")).hasMatch());
    CHECK(externalValid.match(QStringLiteral("A.1")).hasMatch());
    CHECK(externalValid.match(QStringLiteral("big-passage.east")).hasMatch());

    // External regex must still reject characters outside its class
    // even with anchoring; verify the anchor doesn't accidentally let
    // forward-slash through.
    CHECK_FALSE(externalValid.match(QStringLiteral("A/1")).hasMatch());
}
