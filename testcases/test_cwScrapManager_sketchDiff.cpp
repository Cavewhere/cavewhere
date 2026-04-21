//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QPointer>
#include <QSignalSpy>
#include <QUuid>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwPenStroke.h"
#include "cwRegionTreeModel.h"
#include "cwScrap.h"
#include "cwScrapManager.h"
#include "cwSketch.h"
#include "cwSketchScrapOutline.h"
#include "cwSketchViewState.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"

namespace {

struct Fixture {
    cwCavingRegion region;
    cwRegionTreeModel tree;
    cwScrapManager manager;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;
    cwSketch* sketch = nullptr;

    Fixture() {
        tree.setCavingRegion(&region);
        manager.setRegionTreeModel(&tree);
        cave = new cwCave();
        region.addCave(cave);
        trip = new cwTrip();
        cave->addTrip(trip);
        sketch = trip->notesSketch()->addSketch(cwSketch::Plan);
    }

    // Drives a full stroke through the public pen API so the test exercises
    // the real strokeEnded path that triggers the diff.
    QUuid drawClosedSquare(cwPenStroke::Kind kind)
    {
        const int row = sketch->beginStroke(kind, 0.01);
        sketch->appendPoint(row, QPointF(0.0, 0.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(1.0, 0.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(1.0, 1.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(0.0, 1.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(0.0, 0.0), 1.0, 0);
        sketch->endStroke();
        return sketch->strokes().at(row).id;
    }

    QUuid drawOpenLine(cwPenStroke::Kind kind)
    {
        const int row = sketch->beginStroke(kind, 0.01);
        sketch->appendPoint(row, QPointF(0.0, 0.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(1.0, 0.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(1.0, 1.0), 1.0, 0);
        sketch->endStroke();
        return sketch->strokes().at(row).id;
    }

    QUuid drawBowtie(cwPenStroke::Kind kind)
    {
        const int row = sketch->beginStroke(kind, 0.01);
        sketch->appendPoint(row, QPointF(0.0, 0.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(1.0, 1.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(1.0, 0.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(0.0, 1.0), 1.0, 0);
        sketch->appendPoint(row, QPointF(0.0, 0.0), 1.0, 0);
        sketch->endStroke();
        return sketch->strokes().at(row).id;
    }

};

int sketchDerivedScrapCount(const cwSketch* sketch)
{
    int count = 0;
    for(QObject* child : sketch->children()) {
        if(qobject_cast<cwScrap*>(child)) {
            ++count;
        }
    }
    return count;
}

cwScrap* firstSketchScrap(const cwSketch* sketch)
{
    for(QObject* child : sketch->children()) {
        if(auto* scrap = qobject_cast<cwScrap*>(child)) {
            return scrap;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Closed Wall stroke produces a sketch-parented scrap",
          "[cwScrapManager][sketchDiff]")
{
    Fixture f;
    QSignalSpy updatedSpy(&f.manager, &cwScrapManager::sketchDerivedScrapsUpdated);

    f.drawClosedSquare(cwPenStroke::Wall);

    REQUIRE(updatedSpy.count() >= 1);
    REQUIRE(sketchDerivedScrapCount(f.sketch) == 1);

    auto* scrap = firstSketchScrap(f.sketch);
    REQUIRE(scrap != nullptr);
    CHECK(scrap->parentKind() == cwScrap::SketchParent);
    CHECK(scrap->parentSketch() == f.sketch);
    CHECK(scrap->points().size() == 4);
}

TEST_CASE("Closed ScrapOutline stroke produces a sketch-parented scrap",
          "[cwScrapManager][sketchDiff]")
{
    Fixture f;
    f.drawClosedSquare(cwPenStroke::ScrapOutline);
    CHECK(sketchDerivedScrapCount(f.sketch) == 1);
}

TEST_CASE("Open Wall stroke self-caps into a scrap",
          "[cwScrapManager][sketchDiff]")
{
    // Nearest-endpoint matching has no distance cutoff: an open wall
    // self-pairs at its two endpoints, the close-back seam becomes an
    // auto-cap edge, and the resulting triangle is a valid outline.
    Fixture f;
    f.drawOpenLine(cwPenStroke::Wall);
    CHECK(sketchDerivedScrapCount(f.sketch) == 1);
}

TEST_CASE("Closed Feature stroke is ignored",
          "[cwScrapManager][sketchDiff]")
{
    Fixture f;
    f.drawClosedSquare(cwPenStroke::Feature);
    CHECK(sketchDerivedScrapCount(f.sketch) == 0);
}

TEST_CASE("Self-intersecting closed stroke salvages into one scrap",
          "[cwScrapManager][sketchDiff]")
{
    // Patch 1: ringSelfIntersects no longer drops the outline; the
    // detector's QPainterPath::simplified salvage emits the larger outer
    // lobe. A bowtie drawn as a Wall stroke now produces exactly one
    // sketch-derived scrap.
    Fixture f;
    f.drawBowtie(cwPenStroke::Wall);
    CHECK(sketchDerivedScrapCount(f.sketch) == 1);
}

TEST_CASE("Unrelated stroke changes preserve existing scrap identity",
          "[cwScrapManager][sketchDiff]")
{
    Fixture f;
    f.drawClosedSquare(cwPenStroke::Wall);
    REQUIRE(sketchDerivedScrapCount(f.sketch) == 1);

    QPointer<cwScrap> firstScrap = firstSketchScrap(f.sketch);
    REQUIRE(firstScrap);

    // An extra stroke rerun should not destroy-and-recreate the existing scrap:
    // identity is by source-stroke UUID, so the QPointer must survive.
    f.drawOpenLine(cwPenStroke::Feature); // unrelated change, triggers re-detect
    REQUIRE_FALSE(firstScrap.isNull());
    CHECK(sketchDerivedScrapCount(f.sketch) == 1);
}

TEST_CASE("Deleting the source stroke removes the derived scrap",
          "[cwScrapManager][sketchDiff]")
{
    Fixture f;
    f.drawClosedSquare(cwPenStroke::Wall);
    REQUIRE(sketchDerivedScrapCount(f.sketch) == 1);

    QPointer<cwScrap> scrap = firstSketchScrap(f.sketch);
    REQUIRE(scrap);

    f.sketch->clearStrokes();

    // deleteLater defers destruction; drain the event queue so we can observe it.
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    CHECK(scrap.isNull());
    CHECK(sketchDerivedScrapCount(f.sketch) == 0);
}

TEST_CASE("Destroying the sketch drops derived scraps and tracking entry",
          "[cwScrapManager][sketchDiff]")
{
    Fixture f;
    f.drawClosedSquare(cwPenStroke::Wall);
    REQUIRE(sketchDerivedScrapCount(f.sketch) == 1);

    QPointer<cwSketch> sketchPtr = f.sketch;
    QPointer<cwScrap>  scrapPtr  = firstSketchScrap(f.sketch);
    REQUIRE(scrapPtr);

    f.trip->notesSketch()->removeNote(0);
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    CHECK(sketchPtr.isNull());
    CHECK(scrapPtr.isNull());
    CHECK(f.manager.trackedSketchCount() == 0);
}

TEST_CASE("Degenerate stroke shows up in sketchRejectedStrokes with reason tag",
          "[cwScrapManager][sketchDiff]")
{
    // Too-few-points strokes don't produce a scrap but must surface in the
    // per-sketch rejection list so the debug overlay can paint them with
    // the reason tag.
    Fixture f;
    f.sketch->viewState()->setDebugOverlayVisible(true);
    const int row = f.sketch->beginStroke(cwPenStroke::Wall, 0.01);
    f.sketch->appendPoint(row, QPointF(0.0, 0.0), 1.0, 0);
    f.sketch->endStroke();
    const QUuid strokeId = f.sketch->strokes().at(row).id;

    CHECK(sketchDerivedScrapCount(f.sketch) == 0);
    const auto rejected = f.manager.sketchRejectedStrokes(f.sketch);
    REQUIRE(rejected.size() == 1);
    CHECK(rejected.first().id == strokeId);
    CHECK(rejected.first().reason == QString::fromLatin1(
        cwSketchScrapRejectReasons::TooFewPoints));
}
