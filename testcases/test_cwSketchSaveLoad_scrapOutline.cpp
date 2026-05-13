//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QVector3D>

//Our includes
#include "LoadProjectHelper.h"
#include "asyncfuture.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwClinoReading.h"
#include "cwCompassReading.h"
#include "cwDistanceReading.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwPenStroke.h"
#include "cwProject.h"
#include "cwRegionTreeModel.h"
#include "cwRootData.h"
#include "cwScrap.h"
#include "cwScrapManager.h"
#include "cwShot.h"
#include "cwSketch.h"
#include "cwSketchManager.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

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

// Four-station chain a1..a4 along +Y at 10 m spacing.
void populateTripWithChain(cwTrip* trip)
{
    auto* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    for(int i = 0; i < 3; ++i) {
        chunk->appendShot(namedStation(QString("a%1").arg(i + 1)),
                          namedStation(QString("a%1").arg(i + 2)),
                          straightShot(10.0));
    }
}

void drawClosedSquare(cwSketch* sketch,
                      cwPenStroke::Kind kind,
                      double cx, double cy,
                      double halfSize)
{
    const int row = sketch->beginStroke(kind, 0.01);
    sketch->appendPoint(row, QPointF(cx - halfSize, cy - halfSize), 1.0, 0);
    sketch->appendPoint(row, QPointF(cx + halfSize, cy - halfSize), 1.0, 0);
    sketch->appendPoint(row, QPointF(cx + halfSize, cy + halfSize), 1.0, 0);
    sketch->appendPoint(row, QPointF(cx - halfSize, cy + halfSize), 1.0, 0);
    sketch->appendPoint(row, QPointF(cx - halfSize, cy - halfSize), 1.0, 0);
    sketch->endStroke();
}

QList<cwScrap*> sketchScrapChildren(const cwSketch* sketch)
{
    QList<cwScrap*> result;
    for(QObject* child : sketch->children()) {
        if(auto* scrap = qobject_cast<cwScrap*>(child)) {
            result.append(scrap);
        }
    }
    return result;
}

// Canonical fingerprint of a sketch's derived scraps: one entry per scrap,
// sorted by its points vector so ordering across save/load is stable. Each
// entry carries the normalized outline points and the list of stations
// (sorted by name) with their positionOnNote — the inputs that determine
// morph output deterministically.
struct ScrapFingerprint {
    QVector<QPointF> points;
    QList<QPair<QString, QPointF>> stations;

    bool operator<(const ScrapFingerprint& other) const {
        return std::lexicographical_compare(
            points.cbegin(), points.cend(),
            other.points.cbegin(), other.points.cend(),
            [](const QPointF& a, const QPointF& b) {
                return a.x() != b.x() ? a.x() < b.x() : a.y() < b.y();
            });
    }

    bool operator==(const ScrapFingerprint& other) const {
        return points == other.points && stations == other.stations;
    }
};

QList<ScrapFingerprint> fingerprintSketch(const cwSketch* sketch)
{
    QList<ScrapFingerprint> out;
    for(cwScrap* scrap : sketchScrapChildren(sketch)) {
        ScrapFingerprint fp;
        fp.points = scrap->points();
        for(const cwNoteStation& s : scrap->stations()) {
            fp.stations.append({s.name(), s.positionOnNote()});
        }
        std::sort(fp.stations.begin(), fp.stations.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        out.append(fp);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Spins the event loop until `sketch` has at least `expected` derived scraps,
// or until timeout. Returns true on success.
bool waitForDerivedScraps(cwScrapManager* scrapManager,
                          cwSketch* sketch,
                          int expected,
                          int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while(timer.elapsed() < timeoutMs) {
        if(scrapManager->derivedScrapCount(sketch) >= expected) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return scrapManager->derivedScrapCount(sketch) >= expected;
}

cwSketch* findSketchInRoot(cwRootData* root)
{
    auto* region = root->region();
    for(int c = 0; c < region->caveCount(); ++c) {
        cwCave* cave = region->cave(c);
        for(int t = 0; t < cave->tripCount(); ++t) {
            cwTrip* trip = cave->trip(t);
            for(QObject* n : trip->notesSketch()->notes()) {
                if(auto* sk = qobject_cast<cwSketch*>(n)) {
                    return sk;
                }
            }
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Sketch-derived scraps round trip through a full project save/load",
          "[cwSketchSaveLoad][sketchScraps]")
{
    QTemporaryDir tempDir(QDir::tempPath()
                          + QStringLiteral("/test_cwSketchSaveLoad_scrapOutline-%1-XXXXXX")
                                .arg(QCoreApplication::applicationPid()));
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("scrap-outline.cwproj"));

    QList<ScrapFingerprint> preSave;
    QString savedProjectPath;

    // -------------- Build, populate, save --------------
    {
        auto root = std::make_unique<cwRootData>();
        auto* project = root->project();
        auto* region = project->cavingRegion();

        region->addCave();
        cwCave* cave = region->cave(0);
        cave->setName(QStringLiteral("Cave"));
        cave->addTrip();
        cwTrip* trip = cave->trip(0);
        trip->setName(QStringLiteral("Trip"));
        // Zero declination so trip-local == global and the comparison is
        // robust against any calibration drift across versions.
        trip->calibrations()->setDeclinationManual(0.0);
        populateTripWithChain(trip);

        // Drive the line plot so the cave's global station lookup populates.
        root->linePlotManager()->waitToFinish();
        REQUIRE(!cave->stationPositionLookup().isEmpty());

        // Name the sketch before inserting — a nameless sketch writes to
        // "cwsketch" (no stem), and cwSaveLoad's signal-driven save path
        // does not rename the old file on nameChanged, so a later rename
        // would leave the empty-named file as a stray sibling that
        // reloads as a second stroke-less sketch.
        auto* sketch = new cwSketch();
        sketch->setViewType(cwSketch::Plan);
        sketch->setName(QStringLiteral("ScrapOutlineSketch"));
        trip->notesSketch()->addSketches({ sketch });

        // Wait for the sketch manager's per-trip line plot so station
        // positions are available before drawing strokes.
        QSignalSpy spy(root->sketchManager(), &cwSketchManager::linePlotUpdated);
        if(root->sketchManager()->latestLinePlot(trip).isEmpty()) {
            REQUIRE(spy.wait(5000));
        }

        // Two closed strokes: ScrapOutline around (0,5) covers a1/a2;
        // Wall around (0,20) covers a3/a4. Each must produce one scrap.
        drawClosedSquare(sketch, cwPenStroke::ScrapOutline, 0.0, 5.0, 4.0);
        drawClosedSquare(sketch, cwPenStroke::Wall,         0.0, 20.0, 4.0);

        REQUIRE(waitForDerivedScraps(root->scrapManager(), sketch, 2));

        preSave = fingerprintSketch(sketch);
        REQUIRE(preSave.size() == 2);
        for(const auto& fp : preSave) {
            CHECK(!fp.points.isEmpty());
            CHECK(!fp.stations.isEmpty());
        }

        REQUIRE(project->saveAs(projectPath));
        root->futureManagerModel()->waitForFinished();
        project->waitSaveToFinish();
        // saveAs from a temporary project moves the tree into a containing
        // directory, so query the authoritative path after flush.
        savedProjectPath = project->filename();
        REQUIRE(!savedProjectPath.isEmpty());
    }

    // -------------- Reload into a fresh root --------------
    auto reopened = std::make_unique<cwRootData>();
    addTokenManager(reopened->project());
    reopened->project()->newProject();
    reopened->project()->loadOrConvert(savedProjectPath);
    reopened->project()->waitLoadToFinish();
    reopened->futureManagerModel()->waitForFinished();
    reopened->linePlotManager()->waitToFinish();

    cwSketch* loaded = findSketchInRoot(reopened.get());
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->strokes().size() == 2);

    // Poll the sketch manager's per-trip line plot — handleRegionReset
    // may have fired synchronously during loadOrConvert, producing the
    // first result before we can attach a signal spy.
    {
        cwTrip* reopenedTrip = loaded->parentTrip();
        QElapsedTimer timer;
        timer.start();
        while(reopened->sketchManager()->latestLinePlot(reopenedTrip).isEmpty()
              && timer.elapsed() < 10000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        REQUIRE(!reopened->sketchManager()->latestLinePlot(reopenedTrip).isEmpty());
    }
    REQUIRE(waitForDerivedScraps(reopened->scrapManager(), loaded, 2));

    const QList<ScrapFingerprint> postLoad = fingerprintSketch(loaded);
    REQUIRE(postLoad.size() == preSave.size());
    for(int i = 0; i < preSave.size(); ++i) {
        CHECK(postLoad.at(i) == preSave.at(i));
    }
}
