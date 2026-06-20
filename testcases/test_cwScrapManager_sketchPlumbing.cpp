//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLineBrush.h"
#include "cwPenStroke.h"
#include "cwRegionTreeModel.h"
#include "cwScrapManager.h"
#include "cwSketch.h"
#include "cwSketchSettings.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteIO.h"
#include "cwSymbologyPaletteManager.h"
#include "cwTrip.h"

#include <QDir>
#include <QTemporaryDir>
#include <QUuid>

namespace {

struct Fixture {
    cwCavingRegion region;
    cwRegionTreeModel tree;
    cwScrapManager manager;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;

    Fixture() {
        tree.setCavingRegion(&region);
        manager.setRegionTreeModel(&tree);
        cave = new cwCave();
        region.addCave(cave);
        trip = new cwTrip();
        cave->addTrip(trip);
    }
};

// Restores the singleton manager / settings so a palette test never leaks an
// installed directory or an app-wide default into sibling tests.
struct SingletonGuard {
    ~SingletonGuard() {
        cwSketchSettings::instance()->setDefaultPaletteId(QUuid());
        cwSymbologyPaletteManager::instance()->setPaletteDirectory(QString());
    }
};

void writeWallPalette(const QString& root, const QString& subdir,
                      const QUuid& id, const QString& wallDisplayName) {
    cwLineBrush wall;
    wall.name = QStringLiteral("wall");
    wall.displayName = wallDisplayName;

    cwSymbologyPaletteData palette;
    palette.id = id;
    palette.name = wallDisplayName;
    palette.lineBrushes = {wall};

    REQUIRE_FALSE(cwSymbologyPaletteIO::save(palette, QDir(root).filePath(subdir)).hasError());
}

} // namespace

TEST_CASE("cwScrapManager tracks sketches inserted into the region tree",
          "[cwScrapManager][sketchPlumbing]")
{
    Fixture f;

    REQUIRE(f.manager.trackedSketchCount() == 0);

    auto* sketch = f.trip->notesSketch()->addSketch(cwSketch::Plan);
    REQUIRE(sketch != nullptr);

    CHECK(f.manager.isTrackingSketch(sketch));
    CHECK(f.manager.trackedSketchCount() == 1);
}

TEST_CASE("cwScrapManager drops sketch tracking entry on row removal",
          "[cwScrapManager][sketchPlumbing]")
{
    Fixture f;

    auto* sketch = f.trip->notesSketch()->addSketch(cwSketch::Plan);
    REQUIRE(f.manager.isTrackingSketch(sketch));

    f.trip->notesSketch()->removeNote(0);

    CHECK_FALSE(f.manager.isTrackingSketch(sketch));
    CHECK(f.manager.trackedSketchCount() == 0);
}

TEST_CASE("cwScrapManager picks up sketches already present on region attach",
          "[cwScrapManager][sketchPlumbing]")
{
    Fixture f;

    auto* first  = f.trip->notesSketch()->addSketch(cwSketch::Plan);
    auto* second = f.trip->notesSketch()->addSketch(cwSketch::RunningProfile);

    cwScrapManager fresh;
    fresh.setRegionTreeModel(&f.tree);

    CHECK(fresh.isTrackingSketch(first));
    CHECK(fresh.isTrackingSketch(second));
    CHECK(fresh.trackedSketchCount() == 2);
}

TEST_CASE("cwScrapManager wires each sketch exactly once across insert paths",
          "[cwScrapManager][sketchPlumbing]")
{
    // A sketch added before the manager is attached goes through
    // handleRegionReset; one added afterwards goes through inserted(). If
    // either path double-subscribes, a single strokeEnded will emit twice.
    cwCavingRegion region;
    cwRegionTreeModel tree;
    tree.setCavingRegion(&region);

    auto* cave = new cwCave();
    region.addCave(cave);
    auto* trip = new cwTrip();
    cave->addTrip(trip);

    auto* preAttached = trip->notesSketch()->addSketch(cwSketch::Plan);

    cwScrapManager manager;
    manager.setRegionTreeModel(&tree);
    auto* postAttached = trip->notesSketch()->addSketch(cwSketch::Plan);

    QSignalSpy updatedSpy(&manager, &cwScrapManager::sketchDerivedScrapsUpdated);

    preAttached->beginStroke(QStringLiteral("wall"));
    preAttached->endStroke();
    CHECK(updatedSpy.count() == 1);
    updatedSpy.clear();

    postAttached->beginStroke(QStringLiteral("wall"));
    postAttached->endStroke();
    CHECK(updatedSpy.count() == 1);
}

TEST_CASE("cwScrapManager re-evaluates derived scraps on stroke signals",
          "[cwScrapManager][sketchPlumbing]")
{
    Fixture f;
    auto* sketch = f.trip->notesSketch()->addSketch(cwSketch::Plan);

    QSignalSpy updatedSpy(&f.manager, &cwScrapManager::sketchDerivedScrapsUpdated);

    // Drive a full stroke through the public API so the test exercises the
    // real signal path rather than faking strokeEnded directly.
    sketch->beginStroke(QStringLiteral("wall"));
    sketch->endStroke();
    REQUIRE(updatedSpy.count() == 1);
    CHECK(updatedSpy.takeFirst().at(0).value<cwSketch*>() == sketch);

    sketch->clearStrokes();
    CHECK(updatedSpy.count() >= 1);
}

TEST_CASE("cwScrapManager re-derives scraps when the resolved palette changes (Tier 2)",
          "[cwScrapManager][sketchPlumbing]")
{
    SingletonGuard guard;

    QTemporaryDir temp;
    REQUIRE(temp.isValid());

    const QUuid idA = QUuid::createUuid();
    writeWallPalette(temp.path(), QStringLiteral("a"), idA, QStringLiteral("Wall v1"));

    auto* paletteManager = cwSymbologyPaletteManager::instance();
    paletteManager->setPaletteDirectory(temp.path());

    Fixture f;
    f.region.setDefaultPaletteId(idA);

    auto* sketch = f.trip->notesSketch()->addSketch(cwSketch::Plan);
    REQUIRE(f.manager.isTrackingSketch(sketch));

    cwSymbologyPalette* palette = sketch->resolvedPalette();
    REQUIRE(palette != nullptr);
    REQUIRE(palette->id() == idA);

    // Disk-loaded palettes are read-only; editing one in place requires marking
    // it writable first (palette mutation is writable-guarded).
    palette->setWritable(true);

    QSignalSpy updatedSpy(&f.manager, &cwScrapManager::sketchDerivedScrapsUpdated);

    // An in-memory edit to the active palette re-resolves the snapshot (Tier 1),
    // which is the detector's symbology source — so the derived scraps re-derive
    // without a stroke edit (Tier 2).
    cwLineBrush wall;
    wall.name = QStringLiteral("wall");
    wall.displayName = QStringLiteral("Wall v2");
    palette->setBrush(wall);

    CHECK(updatedSpy.count() == 1);
    CHECK(updatedSpy.takeFirst().at(0).value<cwSketch*>() == sketch);

    // A no-op edit re-resolves to the same snapshot -> no re-derive.
    palette->setBrush(wall);
    CHECK(updatedSpy.count() == 0);
}

TEST_CASE("cwScrapManager stops re-evaluating once a sketch leaves the tree",
          "[cwScrapManager][sketchPlumbing]")
{
    Fixture f;

    auto* kept    = f.trip->notesSketch()->addSketch(cwSketch::Plan);
    auto* removed = f.trip->notesSketch()->addSketch(cwSketch::RunningProfile);
    REQUIRE(f.manager.isTrackingSketch(kept));
    REQUIRE(f.manager.isTrackingSketch(removed));

    f.trip->notesSketch()->removeNote(1);
    REQUIRE_FALSE(f.manager.isTrackingSketch(removed));

    QSignalSpy updatedSpy(&f.manager, &cwScrapManager::sketchDerivedScrapsUpdated);
    kept->beginStroke(QStringLiteral("wall"));
    kept->endStroke();
    CHECK(updatedSpy.count() == 1);
}
