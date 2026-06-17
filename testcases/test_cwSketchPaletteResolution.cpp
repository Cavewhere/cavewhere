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
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// Writes a single-brush "wall" palette with the given id into <root>/<subdir>.
// The brush's displayName encodes which palette/version it came from, so both
// cross-palette substitution and same-id reloads are observable.
void writeWallPaletteWithId(const QString &root, const QString &subdir,
                            const QUuid &id, const QString &wallDisplayName)
{
    cwLineBrush wall;
    wall.name = QStringLiteral("wall");
    wall.displayName = wallDisplayName;

    cwSymbologyPaletteData palette;
    palette.id = id;
    palette.name = wallDisplayName;
    palette.lineBrushes = {wall};

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, QDir(root).filePath(subdir)).hasError());
}

// Convenience overload that mints a fresh id and returns it.
QUuid writeWallPalette(const QString &root, const QString &subdir,
                       const QString &wallDisplayName)
{
    const QUuid id = QUuid::createUuid();
    writeWallPaletteWithId(root, subdir, id, wallDisplayName);
    return id;
}

// Writes a palette with only a "feature" brush — used to exercise the
// missing-brush fallback for a "wall" stroke.
QUuid writeFeatureOnlyPalette(const QString &root, const QString &subdir)
{
    cwLineBrush feature;
    feature.name = QStringLiteral("feature");
    feature.displayName = QStringLiteral("Feature");

    cwSymbologyPaletteData palette;
    palette.id = QUuid::createUuid();
    palette.name = QStringLiteral("Feature Only");
    palette.lineBrushes = {feature};

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, QDir(root).filePath(subdir)).hasError());
    return palette.id;
}

// Restores the singleton manager / settings to a clean state so a resolution
// test never leaks installed palettes or an app-wide default into siblings.
struct SingletonGuard {
    ~SingletonGuard()
    {
        cwSketchSettings::instance()->setDefaultPaletteId(QUuid());
        cwSymbologyPaletteManager::instance()->setPaletteDirectory(
            cwSymbologyPaletteManager::defaultPaletteDirectory());
    }
};

// A sketch attached to a region (via cave + trip), since the palette is a
// project-wide (region) choice — the sketch has no per-sketch palette.
struct SketchInRegion {
    cwCavingRegion region;
    cwCave *cave = new cwCave();
    cwTrip *trip = new cwTrip();
    cwSketch sketch;

    SketchInRegion()
    {
        cave->addTrip(trip);
        region.addCave(cave); // region takes ownership of cave (and its trip)
        sketch.setParentTrip(trip);
    }

    QString wallDisplayName() const
    {
        const auto brush = sketch.paletteSnapshot().findBrush(QStringLiteral("wall"));
        return brush.has_value() ? brush->displayName : QString();
    }
};

} // namespace

TEST_CASE("Sketch palette resolver walks region -> settings -> default",
          "[cwSketchPaletteResolution]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid regionPaletteId   = writeWallPalette(temp.path(), QStringLiteral("rg"), QStringLiteral("Wall Region"));
    const QUuid settingsPaletteId = writeWallPalette(temp.path(), QStringLiteral("st"), QStringLiteral("Wall Settings"));

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());
    REQUIRE(manager->paletteById(regionPaletteId) != nullptr);

    SketchInRegion f;

    // All levels null -> shipped default. The default palette ships a "wall"
    // brush, so it resolves (its displayName is whatever the seed authored, not
    // one of ours).
    REQUIRE_FALSE(f.wallDisplayName().isEmpty());
    const QString defaultWallName = f.wallDisplayName();

    // Settings only.
    cwSketchSettings::instance()->setDefaultPaletteId(settingsPaletteId);
    CHECK(f.wallDisplayName() == QStringLiteral("Wall Settings"));

    // Region beats settings.
    f.region.setDefaultPaletteId(regionPaletteId);
    CHECK(f.wallDisplayName() == QStringLiteral("Wall Region"));

    // A region id that doesn't resolve falls through to settings rather than
    // blanking.
    f.region.setDefaultPaletteId(QUuid::createUuid());
    CHECK(f.wallDisplayName() == QStringLiteral("Wall Settings"));

    // Clearing every level returns to the shipped default.
    f.region.setDefaultPaletteId(QUuid());
    cwSketchSettings::instance()->setDefaultPaletteId(QUuid());
    CHECK(f.wallDisplayName() == defaultWallName);
}

TEST_CASE("Switching the region palette substitutes the brush by name",
          "[cwSketchPaletteResolution]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid paletteA = writeWallPalette(temp.path(), QStringLiteral("a"), QStringLiteral("Wall A"));
    const QUuid paletteB = writeWallPalette(temp.path(), QStringLiteral("b"), QStringLiteral("Wall B"));
    const QUuid featureOnly = writeFeatureOnlyPalette(temp.path(), QStringLiteral("c"));

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());

    SketchInRegion f;

    f.region.setDefaultPaletteId(paletteA);
    auto wall = f.sketch.paletteSnapshot().findBrush(QStringLiteral("wall"));
    REQUIRE(wall.has_value());
    CHECK(wall->displayName == QStringLiteral("Wall A"));

    // Same brush name, different palette -> resolves to B's brush.
    f.region.setDefaultPaletteId(paletteB);
    wall = f.sketch.paletteSnapshot().findBrush(QStringLiteral("wall"));
    REQUIRE(wall.has_value());
    CHECK(wall->displayName == QStringLiteral("Wall B"));

    // A palette with no "wall" -> the stroke's brush is missing (no ink).
    f.region.setDefaultPaletteId(featureOnly);
    CHECK_FALSE(f.sketch.paletteSnapshot().findBrush(QStringLiteral("wall")).has_value());
    CHECK(f.sketch.paletteSnapshot().findBrush(QStringLiteral("feature")).has_value());
}

TEST_CASE("paletteSnapshotChanged fires only when the resolved palette changes",
          "[cwSketchPaletteResolution]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid paletteA = writeWallPalette(temp.path(), QStringLiteral("a"), QStringLiteral("Wall A"));

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());

    SketchInRegion f; // resolves to the shipped default
    QSignalSpy spy(&f.sketch, &cwSketch::paletteSnapshotChanged);

    // A region id that doesn't resolve while already on the default -> no change.
    f.region.setDefaultPaletteId(QUuid::createUuid());
    CHECK(spy.count() == 0);

    // A real palette -> the resolved palette changes -> one emission.
    f.region.setDefaultPaletteId(paletteA);
    CHECK(spy.count() == 1);

    // Setting the same id again is a no-op set -> no further emission.
    f.region.setDefaultPaletteId(paletteA);
    CHECK(spy.count() == 1);
}

TEST_CASE("A manager reload refreshes the snapshot when brushes change under the same id",
          "[cwSketchPaletteResolution]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid idA = QUuid::createUuid();
    writeWallPaletteWithId(temp.path(), QStringLiteral("a"), idA, QStringLiteral("Wall v1"));

    auto *manager = cwSymbologyPaletteManager::instance();
    manager->setPaletteDirectory(temp.path());

    SketchInRegion f;
    f.region.setDefaultPaletteId(idA);
    REQUIRE(f.wallDisplayName() == QStringLiteral("Wall v1"));

    QSignalSpy spy(&f.sketch, &cwSketch::paletteSnapshotChanged);

    // Rewrite the palette in place (same id, new brush content) and rescan.
    // The resolved id is unchanged, but the snapshot must still refresh.
    writeWallPaletteWithId(temp.path(), QStringLiteral("a"), idA, QStringLiteral("Wall v2"));
    manager->reload();

    CHECK(f.wallDisplayName() == QStringLiteral("Wall v2"));
    CHECK(spy.count() >= 1);
}
