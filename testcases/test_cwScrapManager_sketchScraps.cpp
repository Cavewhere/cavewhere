//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QEvent>
#include <QSignalSpy>
#include <QVector3D>

//Our includes
#include "asyncfuture.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwClinoReading.h"
#include "cwCompassReading.h"
#include "cwDistanceReading.h"
#include "cwPenStroke.h"
#include "cwRegionTreeModel.h"
#include "cwScrap.h"
#include "cwScrapManager.h"
#include "cwShot.h"
#include "cwSketch.h"
#include "cwSketchManager.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"
#include "cwTripLinePlotTask.h"

namespace {

cwStation namedStation(const QString& name)
{
    cwStation s;
    s.setName(name);
    s.setLeft(cwDistanceReading("0"));
    s.setRight(cwDistanceReading("0"));
    s.setUp(cwDistanceReading("0"));
    s.setDown(cwDistanceReading("0"));
    return s;
}

cwShot straightShot(double distance)
{
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading("0"));
    shot.setBackCompass(cwCompassReading("180"));
    shot.setClino(cwClinoReading("0"));
    shot.setBackClino(cwClinoReading("0"));
    return shot;
}

// Region + cave + trip with a four-station chain along +Y at 10 m spacing.
// Stations "a1"..."a4" land at y = 0, 10, 20, 30.
struct Fixture {
    cwCavingRegion region;
    cwRegionTreeModel tree;
    cwScrapManager scrapManager;
    cwSketchManager sketchManager;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;
    cwSketch* sketch = nullptr;

    Fixture()
    {
        auto* chunk = new cwSurveyChunk();
        trip = new cwTrip();
        trip->setName("t1");
        trip->addChunk(chunk);
        for(int i = 0; i < 3; ++i) {
            chunk->appendShot(namedStation(QString("a%1").arg(i + 1)),
                              namedStation(QString("a%1").arg(i + 2)),
                              straightShot(10.0));
        }

        cave = new cwCave();
        cave->setName("c1");
        cave->addTrip(trip);
        region.addCave(cave);

        // Populate the global (cave-level) lookup so morph pairs can
        // resolve. Trip-local and global match here — there's no loop
        // closure acting on a single trip.
        cwStationPositionLookup global;
        for(int i = 0; i < 4; ++i) {
            global.setPosition(QString("a%1").arg(i + 1),
                               QVector3D(0.0, i * 10.0, 0.0));
        }
        cave->setStationPositionLookup(global);

        tree.setCavingRegion(&region);
        scrapManager.setRegionTreeModel(&tree);
        sketchManager.setRegionTreeModel(&tree);
        scrapManager.setSketchManager(&sketchManager);

        sketch = trip->notesSketch()->addSketch(cwSketch::Plan);
    }

    // Runs the event loop until the sketch manager publishes fresh
    // line-plot output for the fixture's trip, or we time out. Returns
    // true on success.
    bool waitForLinePlot(int timeoutMs = 5000)
    {
        QSignalSpy spy(&sketchManager, &cwSketchManager::linePlotUpdated);
        if(!sketchManager.latestLinePlot(trip).isEmpty()) {
            return true;
        }
        return spy.wait(timeoutMs);
    }

    int sketchDerivedScrapCount() const
    {
        int count = 0;
        for(QObject* child : sketch->children()) {
            if(qobject_cast<cwScrap*>(child)) {
                ++count;
            }
        }
        return count;
    }

    cwScrap* firstSketchScrap() const
    {
        for(QObject* child : sketch->children()) {
            if(auto* scrap = qobject_cast<cwScrap*>(child)) {
                return scrap;
            }
        }
        return nullptr;
    }

    // Draws a small closed square around (cx, cy) in trip-local meters.
    void drawClosedSquareAt(cwPenStroke::Kind kind, double cx, double cy, double halfSize = 2.0)
    {
        const int row = sketch->beginStroke(kind, 0.01);
        sketch->appendPoint(row, QPointF(cx - halfSize, cy - halfSize), 1.0, 0);
        sketch->appendPoint(row, QPointF(cx + halfSize, cy - halfSize), 1.0, 0);
        sketch->appendPoint(row, QPointF(cx + halfSize, cy + halfSize), 1.0, 0);
        sketch->appendPoint(row, QPointF(cx - halfSize, cy + halfSize), 1.0, 0);
        sketch->appendPoint(row, QPointF(cx - halfSize, cy - halfSize), 1.0, 0);
        sketch->endStroke();
    }

    // Draws an open polyline from a list of points.
    void drawPolyline(cwPenStroke::Kind kind, const QVector<QPointF>& points)
    {
        const int row = sketch->beginStroke(kind, 0.01);
        for (const auto& p : points) {
            sketch->appendPoint(row, p, 1.0, 0);
        }
        sketch->endStroke();
    }
};

} // namespace

TEST_CASE("Sketch-derived scrap anchors to trip-local stations inside its polygon",
          "[cwScrapManager][sketchScraps]")
{
    Fixture f;
    REQUIRE(f.waitForLinePlot());

    // Square around (0, 5) — half-size 7 m, so y-range is -2..12 which
    // contains a1 (y=0) and a2 (y=10). a3 at y=20 and a4 at y=30 are
    // outside the polygon and must not be anchored.
    f.drawClosedSquareAt(cwPenStroke::ScrapOutline, 0.0, 5.0, 7.0);

    REQUIRE(f.sketchDerivedScrapCount() == 1);
    auto* scrap = f.firstSketchScrap();
    REQUIRE(scrap != nullptr);
    CHECK(scrap->parentKind() == cwScrap::SketchParent);

    const auto stations = scrap->stations();
    REQUIRE(!stations.isEmpty());

    // Every station position is in the bounding-box-normalized [0,1]
    // frame — the same frame the outline lives in, so morph pairs are
    // consistent.
    for(const auto& noteStation : stations) {
        const QPointF p = noteStation.positionOnNote();
        CHECK(p.x() >= -1.0e-9);
        CHECK(p.x() <= 1.0 + 1.0e-9);
    }

    // a1 and a2 fall inside the polygon; a4 is outside.
    QStringList names;
    for(const auto& noteStation : stations) {
        names.append(noteStation.name());
    }
    CHECK(names.contains(QString("a1")));
    CHECK(names.contains(QString("a2")));
    CHECK_FALSE(names.contains(QString("a4")));
}

TEST_CASE("Outline with no nearby stations produces no derived scrap",
          "[cwScrapManager][sketchScraps]")
{
    Fixture f;
    REQUIRE(f.waitForLinePlot());

    // Far from every station (stations live on the y-axis between 0
    // and 30).
    f.drawClosedSquareAt(cwPenStroke::ScrapOutline, 500.0, 500.0, 1.0);

    CHECK(f.sketchDerivedScrapCount() == 0);
}

TEST_CASE("Closed Wall stroke around stations produces an anchored scrap",
          "[cwScrapManager][sketchScraps]")
{
    Fixture f;
    REQUIRE(f.waitForLinePlot());

    f.drawClosedSquareAt(cwPenStroke::Wall, 0.0, 15.0, 6.0);

    REQUIRE(f.sketchDerivedScrapCount() == 1);
    auto* scrap = f.firstSketchScrap();
    REQUIRE(scrap != nullptr);
    CHECK(!scrap->stations().isEmpty());
}

TEST_CASE("Sketch-derived scrap rejects stations that lie outside its polygon "
          "but inside its bounding box neighborhood",
          "[cwScrapManager][sketchScraps]")
{
    Fixture f;
    REQUIRE(f.waitForLinePlot());

    // Square around (0, 5) half-size 3 m → polygon covers y = 2..8.
    // No station lives in that range (a1=0, a2=10, a3=20, a4=30), so the
    // outline drops to the empty-station guard.
    f.drawClosedSquareAt(cwPenStroke::ScrapOutline, 0.0, 5.0, 3.0);

    CHECK(f.sketchDerivedScrapCount() == 0);
}

TEST_CASE("Sketch-derived scrap keeps only stations inside the polygon",
          "[cwScrapManager][sketchScraps]")
{
    Fixture f;
    REQUIRE(f.waitForLinePlot());

    // Square around (0, 15) with half-size 6 m → polygon covers y = 9..21.
    // a2 (y=10) and a3 (y=20) are inside; a1 (y=0) and a4 (y=30) are out.
    f.drawClosedSquareAt(cwPenStroke::ScrapOutline, 0.0, 15.0, 6.0);

    REQUIRE(f.sketchDerivedScrapCount() == 1);
    auto* scrap = f.firstSketchScrap();
    REQUIRE(scrap != nullptr);

    QStringList names;
    for(const auto& s : scrap->stations()) {
        names.append(s.name());
    }
    CHECK(names.contains(QString("a2")));
    CHECK(names.contains(QString("a3")));
    CHECK_FALSE(names.contains(QString("a1")));
    CHECK_FALSE(names.contains(QString("a4")));
}

TEST_CASE("Two-stroke chain around stations produces one anchored scrap",
          "[cwScrapManager][sketchScraps]")
{
    Fixture f;
    REQUIRE(f.waitForLinePlot());

    // Two open L-shaped walls whose endpoints meet at opposite corners
    // of a rectangle covering y = -3..13, containing a1 (y=0) and a2
    // (y=10). The nearest-endpoint matcher chains them into one outline.
    f.drawPolyline(cwPenStroke::Wall,
                   {{-3.0, -3.0}, {3.0, -3.0}, {3.0, 13.0}});
    f.drawPolyline(cwPenStroke::Wall,
                   {{3.0, 13.0}, {-3.0, 13.0}, {-3.0, -3.0}});

    // Drain deleteLater for the single-stroke scrap retired when stroke 2
    // changed the membership key.
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    CHECK(f.sketchDerivedScrapCount() == 1);
    auto* scrap = f.firstSketchScrap();
    REQUIRE(scrap != nullptr);

    QStringList names;
    for(const auto& s : scrap->stations()) {
        names.append(s.name());
    }
    CHECK(names.contains(QString("a1")));
    CHECK(names.contains(QString("a2")));
    CHECK_FALSE(names.contains(QString("a3")));
    CHECK_FALSE(names.contains(QString("a4")));
}

TEST_CASE("Chain scrap identity is preserved across unrelated stroke edits",
          "[cwScrapManager][sketchScraps]")
{
    Fixture f;
    REQUIRE(f.waitForLinePlot());

    f.drawPolyline(cwPenStroke::Wall,
                   {{-3.0, -3.0}, {3.0, -3.0}, {3.0, 13.0}});
    f.drawPolyline(cwPenStroke::Wall,
                   {{3.0, 13.0}, {-3.0, 13.0}, {-3.0, -3.0}});

    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    REQUIRE(f.sketchDerivedScrapCount() == 1);
    cwScrap* original = f.firstSketchScrap();
    REQUIRE(original != nullptr);

    // A Feature stroke triggers a rebuild but never contributes to the
    // outline — the chain's memberStrokeIds is unchanged, so the tracked
    // scrap pointer must survive.
    f.drawPolyline(cwPenStroke::Feature,
                   {{100.0, 100.0}, {101.0, 100.0}, {101.0, 101.0}});

    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    REQUIRE(f.sketchDerivedScrapCount() == 1);
    CHECK(f.firstSketchScrap() == original);
}
