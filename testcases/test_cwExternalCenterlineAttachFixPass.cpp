/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCavingRegion.h"
#include "cwExternalCenterlineManager.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyExportManager.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QSignalSpy>
#include <QTemporaryDir>

// Std
#include <memory>

namespace {

constexpr int kBooks = 2;
constexpr int kSurveysPerBook = 25;
constexpr int kShotsPerSurvey = 3;
constexpr int kFixEveryShot = 1;

constexpr int kAttachFixWaitMs = 300000;

//! Two systems every generated #FIX coordinate lies inside.
const QString kFirstChoice = QStringLiteral("EPSG:3857");
const QString kSecondChoice = QStringLiteral("EPSG:3395");

using AttachFuture = QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>;

cwSurveyNode* regionRoot(SavedProjectFixture* fixture)
{
    return fixture->project->cavingRegion()->rootNode();
}

std::unique_ptr<SavedProjectFixture> makeProject(const QString& projectFileBase)
{
    auto fixture = makeSavedProject(projectFileBase, QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    drainPipelines(fixture.get());
    return fixture;
}

//! Attaches writeWallsProject(\a books, \a surveysPerBook) with a fix on every
//! survey and waits for the scan, the frame and the solve to settle.
cwSurveyNode* attachFixedWalls(SavedProjectFixture* fixture, const QTemporaryDir& dir,
                               int books, int surveysPerBook)
{
    const QString entry = writeWallsProject(tempSubdir(dir, QStringLiteral("walls")), books,
                                            surveysPerBook, kShotsPerSurvey, kFixEveryShot);
    auto future = managerOf(fixture)->attachSource(regionRoot(fixture), entry);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachFixWaitMs));
    INFO("attach: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    cwSurveyNode* root = future.result().value().root.data();
    REQUIRE(root != nullptr);
    drainPipelines(fixture);
    return root;
}

//! The node under \a root, \a root included, whose own rows list \a station.
cwSurveyNode* fixHolder(cwSurveyNode* root, const QString& station)
{
    QList<cwSurveyNode*> nodes{root};
    nodes.append(root->allNodes());
    for (cwSurveyNode* node : nodes) {
        if (attachedFixStations(node).contains(station)) {
            return node;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Attaching a Walls project with a fix in every survey settles the frame once",
          "[Attach][AttachFixPass]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    auto fixture = makeProject(QStringLiteral("fix-pass-frame"));
    cwRootData* rootData = fixture->rootData.get();
    cwCavingRegion* region = fixture->project->cavingRegion();

    cwSurveyExportManager exportManager;
    exportManager.setCavingRegion(region);
    REQUIRE(exportManager.canExport());

    QSignalSpy frameSpy(region->geoReference(), &cwGeoReference::localProjectionChanged);
    QSignalSpy solveSpy(rootData->linePlotManager(), &cwLinePlotManager::cavernOutputChanged);
    QSignalSpy exportSpy(&exportManager, &cwSurveyExportManager::canExportChanged);

    cwSurveyNode* root = attachFixedWalls(fixture.get(), sourceDir, kBooks, kSurveysPerBook);
    REQUIRE(root->childNodeCount() == kBooks);
    REQUIRE(subtreeListsAttachedFix(root, generatedWallsProject::stationName(0)));

    // The frame moves once for the whole attach, not once per fix-bearing
    // survey the scan apply hands its fixes to.
    CHECK(frameSpy.count() == 1);
    CHECK(region->geoReference()->state() == cwGeoReference::LocalGrid);

    // A regression guard: the region solves once for the whole attach.
    CHECK(solveSpy.count() == 1);

    // A regression guard: the export gate closes once for the whole attach.
    CHECK(exportSpy.count() == 1);
    CHECK_FALSE(exportManager.canExport());
}

TEST_CASE("Choosing a system twice in one turn lists a derived trip's fixes once",
          "[Attach][AttachFixPass]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    auto fixture = makeProject(QStringLiteral("fix-pass-choice"));
    cwSurveyNode* root = attachFixedWalls(fixture.get(), sourceDir, kBooks, kSurveysPerBook);
    REQUIRE(root->childNodeCount() == kBooks);
    const QString station = generatedWallsProject::stationName(0);
    cwSurveyNode* holder = fixHolder(root, station);
    REQUIRE(holder != nullptr);

    QSignalSpy fixesSpy(holder, &cwSurveyNode::attachedFixesChanged);
    chooseFixSystem(root, kFirstChoice);
    chooseFixSystem(root, kSecondChoice);
    drainPipelines(fixture.get());

    // The rows follow the owner's last choice, refreshed once for the turn.
    CHECK(fixesSpy.count() == 1);
    CHECK(attachedFixFor(holder, station).readInSystem == kSecondChoice);
}
