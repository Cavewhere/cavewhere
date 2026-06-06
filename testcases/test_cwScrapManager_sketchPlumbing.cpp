//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwPenStroke.h"
#include "cwRegionTreeModel.h"
#include "cwScrapManager.h"
#include "cwSketch.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"

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
