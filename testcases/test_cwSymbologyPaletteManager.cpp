/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyPaletteIO.h"
#include "cwLineBrush.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// Writes a one-brush palette with the given id into <root>/<subdir>.
void writePalette(const QString &root, const QString &subdir, const QUuid &id,
                  const QString &displayName)
{
    cwLineBrush brush;
    brush.name = QStringLiteral("wall");

    cwSymbologyPaletteData palette;
    palette.id = id;
    palette.name = displayName;
    palette.lineBrushes = {brush};

    const QString dir = QDir(root).filePath(subdir);
    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, dir).hasError());
}

} // namespace

TEST_CASE("Manager always provides the seed palette", "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path()); // empty dir: only the seed

    REQUIRE(manager.palettes().size() == 1);
    CHECK(manager.seedPalette() != nullptr);
    CHECK(manager.paletteById(cwSymbologyPaletteSeed::id()) == manager.seedPalette());
    CHECK(manager.loadErrors().isEmpty());
}

TEST_CASE("Manager enumerates installed palettes by Palette.id, not directory name",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid idA = QUuid::createUuid();
    const QUuid idB = QUuid::createUuid();
    // Directory names deliberately differ from the palettes' display names.
    writePalette(temp.path(), QStringLiteral("dir-one"), idA, QStringLiteral("Alpha"));
    writePalette(temp.path(), QStringLiteral("dir-two"), idB, QStringLiteral("Beta"));

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    // seed + two installed.
    CHECK(manager.palettes().size() == 3);

    auto *a = manager.paletteById(idA);
    auto *b = manager.paletteById(idB);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    CHECK(a->name() == QStringLiteral("Alpha"));
    CHECK(b->name() == QStringLiteral("Beta"));
}

TEST_CASE("Manager resolves duplicate palette ids first-scanned-wins",
          "[cwSymbologyPaletteManager]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid sharedId = QUuid::createUuid();
    // entryInfoList scans by name, so "a-first" precedes "b-second".
    writePalette(temp.path(), QStringLiteral("a-first"), sharedId, QStringLiteral("First"));
    writePalette(temp.path(), QStringLiteral("b-second"), sharedId, QStringLiteral("Second"));

    cwSymbologyPaletteManager manager;
    manager.setPaletteDirectory(temp.path());

    // seed + exactly one of the two duplicates.
    CHECK(manager.palettes().size() == 2);
    auto *kept = manager.paletteById(sharedId);
    REQUIRE(kept != nullptr);
    CHECK(kept->name() == QStringLiteral("First"));

    // The dropped duplicate is reported, not silently swallowed.
    CHECK_FALSE(manager.loadErrors().isEmpty());
}
