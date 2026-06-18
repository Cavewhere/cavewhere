/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwSketchCanvas.h"
#include "cwDecoratedStrokePathSource.h"
#include "cwScrapManager.h"
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

// Qt
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// Writes a single-"wall"-brush palette with the given name into <root>/<subdir>.
// (Duplicated from test_cwSketchResolvedPalette.cpp pending the shared testlib
// palette-writer helper of commit 1.1.)
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

TEST_CASE("cwSketchCanvas::currentBrushName defaults to wall and notifies on change",
          "[cwSketchCanvas]")
{
    cwSketchCanvas canvas;

    CHECK(canvas.currentBrushName() == QStringLiteral("wall"));

    QSignalSpy spy(&canvas, &cwSketchCanvas::currentBrushNameChanged);

    canvas.setCurrentBrushName(QStringLiteral("feature"));
    CHECK(canvas.currentBrushName() == QStringLiteral("feature"));
    CHECK(spy.count() == 1);

    // Setting the same value is a no-op (no spurious notification).
    canvas.setCurrentBrushName(QStringLiteral("feature"));
    CHECK(spy.count() == 1);
}

TEST_CASE("cwSketchCanvas re-skins its path model when the sketch re-resolves its palette",
          "[cwSketchCanvas]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid regionId = writeWallPalette(temp.path(), QStringLiteral("rg"), QStringLiteral("Region"));

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());
    REQUIRE(manager->paletteById(regionId) != nullptr);

    SketchInRegion f;

    cwSketchCanvas canvas;
    canvas.setSketch(&f.sketch);

    // The region palette differs from the shipped default, so re-pointing the
    // region re-resolves the sketch's snapshot, which the canvas pushes into the
    // path model (one pathsChanged emission).
    QSignalSpy spy(canvas.pathModel(), &cwDecoratedStrokePathSource::pathsChanged);

    f.region.setDefaultPaletteId(regionId);

    CHECK(f.sketch.resolvedPalette()->id() == regionId);
    CHECK(spy.count() == 1);
}
