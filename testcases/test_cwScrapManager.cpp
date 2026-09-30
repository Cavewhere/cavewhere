//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "LoadProjectHelper.h"
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "TestHelper.h"
#include "asyncfuture.h"
#include "cwTaskManagerModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwLinePlotManager.h"
#include "cwJobSettings.h"
// #include "cwScrapsEntity.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordItem.h"
#include "cwRenderTexturedItemVisibility.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwImageUtils.h"
#include "cwCavingRegion.h"
#include "cwScrap.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwStationPositionLookup.h"
#include "cwExternalCenterlineManager.h"
#include "ExternalCenterlineTestHelpers.h"

//Qt includes
#include <QFile>
#include <QImageReader>
#include <QElapsedTimer>
#include <QThread>
#include <QCoreApplication>
#include <QEvent>
#include <QThreadPool>
#include "cwSignalSpy.h"

//Std includes
#include <random>

//Async includes
#include "asyncfuture.h"

static void requireAutomaticUpdatesEnabled()
{
    cwJobSettings::initialize();
    auto* settings = cwJobSettings::instance();
    REQUIRE(settings->automaticUpdate());
}

TEST_CASE("cwScrapManager should make the file size grow when re-calculaing scraps", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    QFile file;
    qint64 firstSize;

    {
        auto rootData = std::make_unique<cwRootData>();
        auto project = rootData->project();
        fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));

        file.setFileName(project->filename());
        REQUIRE(file.open(QFile::ReadOnly));
        firstSize = file.size();

        auto scrapManager = rootData->scrapManager();

        int numberOfRuns = 5;
        QList<qint64> runTime;
        QElapsedTimer timer;
        for(int i = 0; i < numberOfRuns; i++) {
            timer.restart();
            scrapManager->updateAllScraps();
            scrapManager->waitForFinish();
            runTime.append(timer.nsecsElapsed());
        }

        double runFraction = 1 / static_cast<double>(runTime.size());
        double averageTime = std::accumulate(runTime.begin(), runTime.end(), 0.0,
                                             [runFraction](double average, int runTime)
        {
            return average + runFraction * runTime;
        });

        std::default_random_engine generator;
        std::uniform_real_distribution<double> distribution(0.0,averageTime * 1.5);

        for(int i = 0; i < 20; i++) {
            double waitTime = distribution(generator);

            QTimer timer;
            timer.setInterval(waitTime * 1e-6);
            timer.start();

            scrapManager->updateAllScraps();

            QEventLoop eventLoop;
            QObject::connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
            eventLoop.exec();
        }

        scrapManager->waitForFinish();
        QThreadPool::globalInstance()->waitForDone();
        INFO("Filename:" << project->filename());
    }

    auto nextSize = file.size();
    double ratio = nextSize / static_cast<double>(firstSize);
    CHECK(ratio <= Catch::Approx(1.01));
}

TEST_CASE("cwScrapManager auto update should work propertly", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));

    rootData->futureManagerModel()->waitForFinished();

    cwSignalSpy addRowSpy(rootData->futureManagerModel(), &cwFutureManagerModel::rowsInserted);

    auto scrapManager = rootData->scrapManager();
    CHECK(scrapManager->automaticUpdate() == true);

    scrapManager->setAutomaticUpdate(false);
    CHECK(scrapManager->automaticUpdate() == false);

    //Change the station position
    auto cave = rootData->region()->cave(0);
    auto trip = cave->trip(0);
    auto chunk = trip->chunk(0);
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.0");

    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() > 0);
    auto note = notes.first();
    REQUIRE(note->scraps().size() > 0);
    auto scraps = note->scraps();
    auto scrap = scraps.first();

    rootData->futureManagerModel()->waitForFinished();
    CHECK(scrapManager->dirtyScraps().contains(scrap));

    QEventLoop loop;

    QTimer::singleShot(1, qApp, [&](){
        rootData->futureManagerModel()->waitForFinished();

        auto pendingScraps = scrapManager->dirtyScraps();
        CHECK(pendingScraps.contains(scrap));

        scrapManager->setAutomaticUpdate(true);

        rootData->futureManagerModel()->waitForFinished();
        scrapManager->waitForFinish();

        CHECK_FALSE(scrapManager->dirtyScraps().contains(scrap));

        loop.quit();
    });

    loop.exec();
}


TEST_CASE("cwScrapManager shouldn't update scraps that are invalid", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/ignoreInvalidScrap.cw"));
    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(project->cavingRegion()->cave(0)->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    auto note = notes.at(0);
    auto scrap = new cwScrap();

    rootData->futureManagerModel()->waitForFinished();

    note->addScrap(scrap);
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    rootData->futureManagerModel()->waitForFinished();

    scrap->addPoint(QPointF(0.0, 0.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    scrap->addPoint(QPointF(0.0, 1.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    scrap->addPoint(QPointF(1.0, 1.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    scrap->addPoint(QPointF(1.0, 0.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);

    cwNoteStation noteStation;
    noteStation.setPositionOnNote(QPointF(0.5, 0.5));
    noteStation.setName("a1");

    scrap->addStation(noteStation);
    CHECK(rootData->futureManagerModel()->rowCount() == 1);
    rootData->futureManagerModel()->waitForFinished();

    scrap->removeStation(0);
    CHECK(rootData->futureManagerModel()->rowCount() == 1);
    rootData->futureManagerModel()->waitForFinished();
}

TEST_CASE("cwScrapManager should update on viewMatrix change", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
    if(rootData->futureManagerModel()->rowCount() >= 0) {
        rootData->futureManagerModel()->waitForFinished();
    }

    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(project->cavingRegion()->cave(0)->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    auto note = notes.at(0);
    REQUIRE(note->scraps().size() == 1);
    auto scrap = note->scraps().at(0);
    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager != nullptr);

    CHECK(scrap->type() == cwScrap::ProjectedProfile);
    auto profile = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
    REQUIRE(profile);
    CHECK(profile->azimuth() == 135.0);

    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);
    profile->setAzimuth(136.0);


    CHECK(rootData->futureManagerModel()->rowCount() == 1);
    CHECK(scrapManager->dirtyScraps().contains(scrap));

    rootData->futureManagerModel()->waitForFinished();
    scrapManager->waitForFinish();

    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);

    SECTION("Switch to running profile") {
        scrap->setType(cwScrap::RunningProfile);
        CHECK(dynamic_cast<cwRunningProfileScrapViewMatrix*>(scrap->viewMatrix()));

        CHECK(rootData->futureManagerModel()->rowCount() == 1);
        CHECK(scrapManager->dirtyScraps().contains(scrap));

        rootData->futureManagerModel()->waitForFinished();
        scrapManager->waitForFinish();

        CHECK(rootData->futureManagerModel()->rowCount() == 0);
        CHECK(scrapManager->dirtyScraps().contains(scrap) == false);
    }
}

TEST_CASE("cwScrapManager should defer updates while editing", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager);

    auto cave = project->cavingRegion()->cave(0);
    auto trip = cave->trip(0);
    auto note = trip->notes()->notes().first();
    auto scrap = note->scraps().first();
    REQUIRE(scrap);

    rootData->futureManagerModel()->waitForFinished();
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);

    cwSignalSpy rowsInsertedSpy(rootData->futureManagerModel(), &cwFutureManagerModel::rowsInserted);

    scrap->beginEditing();
    QPointF currentPos = scrap->stationData(cwScrap::StationPosition, 0).toPointF();
    for(int i = 0; i < 20; ++i) {
        auto offset = 0.01 * (i + 1);
        scrap->setStationData(cwScrap::StationPosition, 0, currentPos + QPointF(offset, 0.0));
    }

    CHECK(rowsInsertedSpy.count() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap));

    scrap->endEditing();
    CHECK(rowsInsertedSpy.count() == 1);
    rootData->futureManagerModel()->waitForFinished();
    scrapManager->waitForFinish();


    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);
}

TEST_CASE("cwScrapManager scrap render items should remain visible after loading a bundle (.cw) file", "[cwScrapManager][regression]") {
    // Regression test for bug #329: "Save As makes carpeting go away"
    //
    // Root cause: cwProject::filenameChanged fired after bundle load (which
    // completes after scraps are already inserted and registered with the
    // keyword item model).  cwRootData's filenameChanged handler was calling
    // m_keywordItemModel->clear(), which triggered cwKeywordVisibility to call
    // setVisible(false) on every cwRenderTexturedItemVisibility.  The guard in
    // addKeywordItemForScrap() then prevented re-registration, leaving all
    // carpeted scraps permanently invisible in the renderer.
    requireAutomaticUpdatesEnabled();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto* keywordModel = rootData->keywordItemModel();
    REQUIRE(keywordModel != nullptr);

    // Every keyword item whose object is a cwRenderTexturedItemVisibility must
    // report visible == true.  Before the fix the filenameChanged handler
    // cleared the model mid-load, driving all visibility objects to false.
    int visibilityItemCount = 0;
    for (int i = 0; i < keywordModel->rowCount(); ++i) {
        auto* kwItem = keywordModel->item(i);
        REQUIRE(kwItem != nullptr);
        auto* vis = qobject_cast<cwRenderTexturedItemVisibility*>(kwItem->object());
        if (vis) {
            ++visibilityItemCount;
            INFO("cwRenderTexturedItemVisibility at keyword model row " << i << " is not visible");
            CHECK(vis->isVisible());
        }
    }

    // Sanity-check: the dataset has scraps, so we must have found at least one
    // visibility item.
    CHECK(visibilityItemCount > 0);
}

TEST_CASE("Scrap keyword items are cleaned up across a project reload", "[cwScrapManager][regression]") {
    // cwRootData's filenameChanged handler deliberately does NOT clear the
    // keyword item model on reload, relying on scrap keyword items being torn
    // down when their scraps are destroyed.  This guards that contract: reloading
    // a project into the same cwRootData must not accumulate stale scrap keyword
    // items (each reload destroys the previous region's scraps).
    requireAutomaticUpdatesEnabled();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto* keywordModel = rootData->keywordItemModel();
    REQUIRE(keywordModel != nullptr);

    const QString dataset =
        testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw");

    auto scrapVisibilityCount = [&]() {
        int count = 0;
        for (int i = 0; i < keywordModel->rowCount(); ++i) {
            auto* kwItem = keywordModel->item(i);
            REQUIRE(kwItem != nullptr);
            if (qobject_cast<cwRenderTexturedItemVisibility*>(kwItem->object())) {
                ++count;
            }
        }
        return count;
    };

    auto loadAndSettle = [&]() {
        fileToProject(project, dataset);
        rootData->futureManagerModel()->waitForFinished();
        rootData->scrapManager()->waitForFinish();
        // Drain deferred deletions so removed keyword items leave the model.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    };

    loadAndSettle();
    const int afterFirstLoad = scrapVisibilityCount();
    const int rowsAfterFirstLoad = keywordModel->rowCount();
    REQUIRE(afterFirstLoad > 0);

    // Each reload must return the keyword item model to its baseline, never
    // accumulating leftovers from the previous region. scrapVisibilityCount()
    // guards the scrap items specifically; rowCount() guards every per-entity
    // registry (scrap, line plot, ...) against a reload leak.
    for (int reload = 0; reload < 3; ++reload) {
        loadAndSettle();
        INFO("reload iteration " << reload);
        CHECK(scrapVisibilityCount() == afterFirstLoad);
        CHECK(keywordModel->rowCount() == rowsAfterFirstLoad);
    }
}

// Helper: load a v6 .cw file and return the first note
static cwNote* loadV6FirstNote(cwRootData* rootData, const QString& resourcePath) {
    requireAutomaticUpdatesEnabled();
    auto project = rootData->project();
    fileToProject(project, resourcePath);
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(cave->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    return notes.at(0);
}

TEST_CASE("V6 conversion with landscape EXIF coords applies rotation", "[cwScrapManager]") {
    // backgroundRotation-0.08.cw is proto v1 with EXIF Rotate 90 CW.
    // Raw image is 4032x3024 (landscape). Scrap coords are in landscape space.
    // After conversion: originalSize should be post-rotation (3024x4032)
    // and coords should be rotated: (x,y) -> (y, 1-x).
    auto rootData = std::make_unique<cwRootData>();
    auto note = loadV6FirstNote(rootData.get(),
        testcasesDatasetPath("test_cwScrapManager/backgroundRotation-0.08.cw"));
    REQUIRE(note->scraps().size() >= 1);

    // originalSize should be post-rotation
    const QSize storedSize = note->image().originalSize();
    CHECK(storedSize.width() == 3024);
    CHECK(storedSize.height() == 4032);

    // DPI should be set from image
    CHECK(note->image().originalDotsPerMeter() > 0);

    // imageWithAutoTransform should match stored originalSize
    const QString absPath = rootData->project()->absolutePath(note, note->image().path());
    REQUIRE(QFileInfo::exists(absPath));
    QFile file(absPath);
    REQUIRE(file.open(QIODevice::ReadOnly));
    QByteArray data = file.readAll();
    QImage autoImage = cwImageUtils::imageWithAutoTransform(
        data, QFileInfo(absPath).suffix().toLatin1());
    CHECK(autoImage.size() == storedSize);

    // Station coords should be rotated from landscape (~0.660, 0.620)
    // to portrait (~0.620, 0.340)
    auto scrap = note->scrap(0);
    REQUIRE(scrap->stations().size() >= 1);
    const QPointF station0 = scrap->stations().at(0).positionOnNote();
    CHECK(station0.x() == Catch::Approx(0.620).margin(0.01));
    CHECK(station0.y() == Catch::Approx(0.340).margin(0.02));

    // Triangulate and verify it completes
    auto results = rootData->scrapManager()->triangulateScraps({scrap});
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(!results.isEmpty());
    REQUIRE(AsyncFuture::waitForFinished(results.first().data));
    REQUIRE(results.first().data.resultCount() == 1);
    CHECK(!results.first().data.result().isNull());
}

TEST_CASE("V6 conversion with v5 coords in v6 file applies reNormalization", "[cwScrapManager]") {
    // backgroundRotation-2025.2 to 2025.2-101.cw is proto v6 with v5 landscape coords.
    // V6 always applies reNormalization (not rotation) because real v6 users
    // would have redrawn scraps on the auto-rotated display.
    auto rootData = std::make_unique<cwRootData>();
    auto note = loadV6FirstNote(rootData.get(),
        testcasesDatasetPath("test_cwScrapManager/backgroundRotation-2025.2 to 2025.2-101.cw"));
    REQUIRE(note->scraps().size() >= 1);

    // originalSize should be post-rotation
    CHECK(note->image().originalSize().width() == 3024);
    CHECK(note->image().originalSize().height() == 4032);

    // Station coords are re-normalized from landscape to portrait dims.
    // Original v5 station0: (0.6485, 0.5875)
    // reNorm: x' = x * W/H = 0.6485 * 1.333 ≈ 0.865
    //         y' = y * H/W + (1 - H/W) = 0.5875 * 0.75 + 0.25 ≈ 0.691
    auto scrap = note->scrap(0);
    REQUIRE(scrap->stations().size() >= 1);
    const QPointF station0 = scrap->stations().at(0).positionOnNote();
    CHECK(station0.x() == Catch::Approx(0.865).margin(0.01));
    CHECK(station0.y() == Catch::Approx(0.691).margin(0.02));
}

TEST_CASE("V6 conversion with correct EXIF coords does not modify", "[cwScrapManager]") {
    // backgroundRotation-2025.2-101.cw is proto v6 with post-rotation originalSize (3024x4032).
    // Coords are already correct — no transformation should be applied.
    auto rootData = std::make_unique<cwRootData>();
    auto note = loadV6FirstNote(rootData.get(),
        testcasesDatasetPath("test_cwScrapManager/backgroundRotation-2025.2-101.cw"));
    REQUIRE(note->scraps().size() >= 1);

    // originalSize should remain post-rotation (already correct)
    CHECK(note->image().originalSize().width() == 3024);
    CHECK(note->image().originalSize().height() == 4032);

    // Station coords should be unchanged (~0.621, 0.339)
    auto scrap = note->scrap(0);
    REQUIRE(scrap->stations().size() >= 1);
    const QPointF station0 = scrap->stations().at(0).positionOnNote();
    CHECK(station0.x() == Catch::Approx(0.621).margin(0.005));
    CHECK(station0.y() == Catch::Approx(0.339).margin(0.005));
}

namespace {

// A page one thousand pixels square at one hundred pixels per meter, so a
// scrap station's note position turns into paper meters.
constexpr int kAttachedNotePixels = 1000;
constexpr int kAttachedNoteDotsPerMeter = 100;

// Where the scrap marks a1, a2, and a3 on the page (note y runs up): a2 due
// north of a1 and a3 due east of a2, the way the survey below runs.
const QPointF kA1OnNote(0.2, 0.3);
const QPointF kA2OnNote(0.2, 0.8);
const QPointF kA3OnNote(0.7, 0.8);

constexpr double kScaleTolerance = 1e-3;
constexpr float kPositionToleranceMeters = 0.01f;

//! One survey block, a1 fixed at the origin, a1-a2 due north, a2-a3 due east.
QByteArray threeStationSurvey(const QByteArray& a1a2Tape, bool withA3)
{
    QByteArray survey(
        "*begin s\n"
        "*fix a1 0 0 0\n"
        "*data normal from to tape compass clino\n");
    survey += "a1 a2 " + a1a2Tape + " 0 0\n";
    if (withA3) {
        survey += "a2 a3 10.0 90 0\n";
    }
    survey += "*end s\n";
    return survey;
}

struct AttachedScrapSetup {
    std::unique_ptr<SavedProjectFixture> fixture;
    QString source;
    cwScrap* scrap = nullptr;
};

//! A saved project whose trip is attached to a three-station source kept
//! outside the project, carrying one note with a scrap anchored at every
//! station.
AttachedScrapSetup makeAttachedScrap(const QString& projectFileBase)
{
    requireAutomaticUpdatesEnabled();
    AttachedScrapSetup setup;
    setup.fixture = makeSavedProject(projectFileBase,
                                     QStringLiteral("ScrapCave"),
                                     QStringLiteral("ScrapTrip"));
    SavedProjectFixture* fixture = setup.fixture.get();

    setup.source = QDir(fixture->tempDir.path()).filePath(QStringLiteral("source/s.svx"));
    REQUIRE(QDir().mkpath(QFileInfo(setup.source).absolutePath()));
    overwriteFile(setup.source, threeStationSurvey("10.0", true));
    attachThroughManager(fixture, fixture->trip, setup.source);
    drainPipelines(fixture);

    // A saved note names its image by path, so the project can write it.
    cwNote* note = new cwNote();
    note->setName(QStringLiteral("page.png"));
    cwImage image;
    image.setPath(QStringLiteral("page.png"));
    image.setOriginalSize(QSize(kAttachedNotePixels, kAttachedNotePixels));
    image.setOriginalDotsPerMeter(kAttachedNoteDotsPerMeter);
    note->setImage(image);
    fixture->trip->notes()->addNotes({note});

    setup.scrap = new cwScrap();
    note->addScrap(setup.scrap);
    const QList<QPair<QString, QPointF>> anchors {
        {QStringLiteral("s.a1"), kA1OnNote},
        {QStringLiteral("s.a2"), kA2OnNote},
        {QStringLiteral("s.a3"), kA3OnNote},
    };
    for (const auto& anchor : anchors) {
        cwNoteStation noteStation;
        noteStation.setName(anchor.first);
        noteStation.setPositionOnNote(anchor.second);
        setup.scrap->addStation(noteStation);
    }
    drainPipelines(fixture);
    fixture->rootData->scrapManager()->waitForFinish();
    return setup;
}

void reloadAndSettle(SavedProjectFixture* fixture)
{
    auto future = managerOf(fixture)->reloadFromSource(fixture->trip);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture);
    fixture->rootData->scrapManager()->waitForFinish();
}

bool nearPosition(const QVector3D& actual, const QVector3D& expected)
{
    return (actual - expected).length() < kPositionToleranceMeters;
}

} // namespace

TEST_CASE("a scrap on an attached trip re-morphs after Reload moves its stations",
          "[cwScrapManager][Attach]")
{
    auto setup = makeAttachedScrap(QStringLiteral("scrap-reload-moves"));
    SavedProjectFixture* fixture = setup.fixture.get();
    cwTrip* trip = fixture->trip;

    REQUIRE(nearPosition(trip->solvedStationPositions().position(QStringLiteral("s.a2")),
                         QVector3D(0.0f, 10.0f, 0.0f)));
    const double scaleBefore = setup.scrap->noteTransformation()->scale();
    REQUIRE(scaleBefore > 0.0);

    // a2 moves ten meters further north and a3 follows it.
    overwriteFile(setup.source, threeStationSurvey("20.0", true));
    reloadAndSettle(fixture);

    const cwStationPositionLookup lookup = trip->solvedStationPositions();
    CHECK(nearPosition(lookup.position(QStringLiteral("s.a2")), QVector3D(0.0f, 20.0f, 0.0f)));
    CHECK(nearPosition(lookup.position(QStringLiteral("s.a3")), QVector3D(10.0f, 20.0f, 0.0f)));

    // The page did not change, so a survey that grew recomputes a smaller
    // paper-to-cave scale: the scrap's anchors followed the reload.
    const double scaleAfter = setup.scrap->noteTransformation()->scale();
    CHECK(scaleAfter > 0.0);
    CHECK(scaleAfter < scaleBefore * (1.0 - kScaleTolerance));
    CHECK(setup.scrap->numberOfStations() == 3);
}

TEST_CASE("a scrap whose station left the source keeps its other anchors and reports the missing one",
          "[cwScrapManager][Attach]")
{
    auto setup = makeAttachedScrap(QStringLiteral("scrap-station-left"));
    SavedProjectFixture* fixture = setup.fixture.get();
    cwTrip* trip = fixture->trip;

    REQUIRE(trip->solvedStationPositions().hasPosition(QStringLiteral("s.a3")));
    const double scaleBefore = setup.scrap->noteTransformation()->scale();
    REQUIRE(scaleBefore > 0.0);

    overwriteFile(setup.source, threeStationSurvey("10.0", false));
    reloadAndSettle(fixture);

    // The scrap still names all three stations; a3 is the one the solve
    // has no position for, and a1-a2 alone carry the note transform.
    REQUIRE(setup.scrap->numberOfStations() == 3);
    CHECK(setup.scrap->station(2).name() == QStringLiteral("s.a3"));
    const cwStationPositionLookup lookup = trip->solvedStationPositions();
    CHECK(lookup.hasPosition(QStringLiteral("s.a1")));
    CHECK(lookup.hasPosition(QStringLiteral("s.a2")));
    CHECK_FALSE(lookup.hasPosition(QStringLiteral("s.a3")));

    // The page draws a1-a2 and a2-a3 to the same scale, so the transform the
    // remaining anchors compute matches the one all three computed.
    CHECK(setup.scrap->noteTransformation()->scale()
          == Catch::Approx(scaleBefore).epsilon(kScaleTolerance));
}
