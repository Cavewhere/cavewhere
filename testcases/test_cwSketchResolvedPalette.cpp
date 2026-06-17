/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSketch.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSketchSettings.h"
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteIO.h"
#include "cwLineBrush.h"
#include "cwPaletteSnapshot.h"

// Qt
#include <QDir>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// Writes a single-"wall"-brush palette with the given id into <root>/<subdir>.
QUuid writeWallPalette(const QString &root, const QString &subdir, const QString &name)
{
    cwLineBrush wall;
    wall.name = QStringLiteral("wall");
    wall.displayName = name;

    cwSymbologyPaletteData palette;
    palette.id = QUuid::createUuid();
    palette.name = name;
    palette.lineBrushes = {wall};

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, QDir(root).filePath(subdir)).hasError());
    return palette.id;
}

// Restores the singleton manager / settings so the test doesn't leak installed
// palettes or an app-wide default into sibling tests.
struct SingletonGuard {
    ~SingletonGuard()
    {
        cwSketchSettings::instance()->setDefaultPaletteId(QUuid());
        cwSymbologyPaletteManager::instance()->setPaletteDirectory(
            cwSymbologyPaletteManager::defaultPaletteDirectory());
    }
};

// A sketch attached to a region (cave + trip) — the palette is a region choice.
struct SketchInRegion {
    cwCavingRegion region;
    cwCave *cave = new cwCave();
    cwTrip *trip = new cwTrip();
    cwSketch sketch;

    SketchInRegion()
    {
        cave->addTrip(trip);
        region.addCave(cave);
        sketch.setParentTrip(trip);
    }
};

} // namespace

TEST_CASE("cwSketch::resolvedPalette walks region -> settings -> default",
          "[cwSketchResolvedPalette]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid regionId   = writeWallPalette(temp.path(), QStringLiteral("rg"), QStringLiteral("Region"));
    const QUuid settingsId = writeWallPalette(temp.path(), QStringLiteral("st"), QStringLiteral("Settings"));

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());
    REQUIRE(manager->paletteById(regionId) != nullptr);

    SketchInRegion f;

    // All levels null -> the shipped default object.
    REQUIRE(f.sketch.resolvedPalette() != nullptr);
    CHECK(f.sketch.resolvedPalette() == manager->defaultPalette());

    // Settings only.
    cwSketchSettings::instance()->setDefaultPaletteId(settingsId);
    CHECK(f.sketch.resolvedPalette()->id() == settingsId);

    // Region beats settings.
    f.region.setDefaultPaletteId(regionId);
    CHECK(f.sketch.resolvedPalette()->id() == regionId);

    // A region id that doesn't resolve falls through to settings.
    f.region.setDefaultPaletteId(QUuid::createUuid());
    CHECK(f.sketch.resolvedPalette()->id() == settingsId);

    // Clearing every level returns to the shipped default.
    f.region.setDefaultPaletteId(QUuid());
    cwSketchSettings::instance()->setDefaultPaletteId(QUuid());
    CHECK(f.sketch.resolvedPalette() == manager->defaultPalette());
}

TEST_CASE("cwSketch::resolvedPalette is consistent with paletteSnapshot",
          "[cwSketchResolvedPalette]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid regionId = writeWallPalette(temp.path(), QStringLiteral("rg"), QStringLiteral("Region"));

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());

    SketchInRegion f;
    f.region.setDefaultPaletteId(regionId);

    REQUIRE(f.sketch.resolvedPalette() != nullptr);
    CHECK(f.sketch.resolvedPalette()->id() == regionId);
    // The snapshot the render path consumes is the resolved palette's snapshot.
    CHECK(f.sketch.resolvedPalette()->snapshot() == f.sketch.paletteSnapshot());
}
