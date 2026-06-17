/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyPaletteIO.h"

// Qt
#include <QString>

// std
#include <iostream>

// Hidden, manually-run regenerator for the shipped default palette files
// (cavewherelib/palettes/cavewhere-default). The leading '.' tag hides it from
// default runs so a normal test run never writes the source tree. Run it by hand
// after changing cwSymbologyPaletteSeed::create(), then git add the regenerated
// files:
//
//   CW_DEFAULT_PALETTE_DIR=<repo>/cavewherelib/palettes/cavewhere-default \
//     cavewhere-test "[regenerateDefaultPalette]"
//
// The files are produced from create() so nobody hand-edits serialized JSON; the
// drift-guard test below asserts the committed qrc files still equal what create()
// produces.
TEST_CASE("Regenerate the default palette files from the seed generator",
          "[.][regenerateDefaultPalette]")
{
    const QString directory = qEnvironmentVariable("CW_DEFAULT_PALETTE_DIR");
    REQUIRE_FALSE(directory.isEmpty()); // set CW_DEFAULT_PALETTE_DIR to the target directory

    const auto result =
        cwSymbologyPaletteIO::save(cwSymbologyPaletteSeed::create(), directory);
    INFO("save error: " << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    std::cout << "[regenerateDefaultPalette] wrote default palette to: "
              << directory.toStdString() << std::endl;
}

// Drift guard: the palette embedded in cwPalettes.qrc must still match what the
// seed generator produces. If create() changes, this fails until the committed
// files are regenerated with [regenerateDefaultPalette] above. Equality is over
// cwSymbologyPaletteData (id/name/description/author/version/brushes/glyphs) —
// the JSON cavewhereVersion stamp is not a field, so it doesn't affect this.
TEST_CASE("The shipped default palette qrc matches the seed generator",
          "[cwDefaultPalette]")
{
    const auto loaded = cwSymbologyPaletteIO::load(QStringLiteral(":/palettes/cavewhere-default"));
    INFO("load error: " << loaded.errorMessage().toStdString());
    REQUIRE_FALSE(loaded.hasError());
    CHECK(loaded.value().palette == cwSymbologyPaletteSeed::create());
}
