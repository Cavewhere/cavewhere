/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSymbologyName.h"

// Qt
#include <QString>

TEST_CASE("Symbology names must be path-safe kebab-case", "[cwSymbologyName]")
{
    // Shared by brush and glyph names: lowercase letters, digits and interior
    // hyphens, non-empty. The rule doubles as the on-disk path-safety guard.
    CHECK(cwSymbology::isValidName(QStringLiteral("floor-step-tick")));
    CHECK(cwSymbology::isValidName(QStringLiteral("a")));
    CHECK(cwSymbology::isValidName(QStringLiteral("tick2")));

    CHECK_FALSE(cwSymbology::isValidName(QString()));
    CHECK_FALSE(cwSymbology::isValidName(QStringLiteral("../escape")));
    CHECK_FALSE(cwSymbology::isValidName(QStringLiteral("a/b")));
    CHECK_FALSE(cwSymbology::isValidName(QStringLiteral("a\\b")));
    CHECK_FALSE(cwSymbology::isValidName(QStringLiteral("dot.name")));
    CHECK_FALSE(cwSymbology::isValidName(QStringLiteral("Wall")));   // uppercase
    CHECK_FALSE(cwSymbology::isValidName(QStringLiteral("-lead")));
    CHECK_FALSE(cwSymbology::isValidName(QStringLiteral("trail-")));
}

TEST_CASE("SymbologyName QML facade delegates to the shared rule", "[cwSymbologyName]")
{
    // The singleton the editors call (SymbologyName.isValidName) must agree with
    // the free function the IO layer enforces.
    cwSymbologyName facade;
    CHECK(facade.isValidName(QStringLiteral("floor-step-tick")));
    CHECK_FALSE(facade.isValidName(QStringLiteral("Wall")));
    CHECK_FALSE(facade.isValidName(QString()));
}
