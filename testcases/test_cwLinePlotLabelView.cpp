//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwLinePlotLabelView.h"
#include "SurveyTreeTestHelper.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotManager.h"
#include "cwExternalCenterlineManager.h"
#include "cwStationPositionLookup.h"
#include "cwLabel3dGroup.h"
#include "cwLabel3dItem.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"

//Test helpers
#include "ExternalCenterlineTestHelpers.h"

//Qt includes
#include <QCoreApplication>
#include <QHash>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QMetaObject>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <memory>

namespace {
// cwLinePlotLabelView::updateGroup creates labels from a QQmlComponent, so the
// view must live inside a QML context.
struct LabelViewHost
{
    QQmlEngine engine;
    std::unique_ptr<QObject> root;
    cwLinePlotLabelView* labelView = nullptr;
};

std::unique_ptr<LabelViewHost> makeLabelViewHost()
{
    auto host = std::make_unique<LabelViewHost>();
    QQmlComponent component(&host->engine);
    component.setData(QByteArrayLiteral(
                          "import QtQuick\n"
                          "import cavewherelib\n"
                          "Item {\n"
                          "  width: 200; height: 200\n"
                          "  LinePlotLabelView { objectName: \"labelView\" }\n"
                          "}\n"),
                      QUrl());
    host->root.reset(component.create());
    REQUIRE(host->root != nullptr);
    host->labelView = host->root->findChild<cwLinePlotLabelView*>(QStringLiteral("labelView"));
    REQUIRE(host->labelView != nullptr);
    return host;
}

cwLabel3dGroup* groupForKeywordItem(cwKeywordItem* item)
{
    return qobject_cast<cwLabel3dGroup*>(item->object());
}

QStringList sortedLabelTexts(cwKeywordItemModel& keywordModel)
{
    QStringList texts;
    for (int i = 0; i < keywordModel.rowCount(); ++i) {
        const cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(i));
        REQUIRE(group != nullptr);
        for (const cwLabel3dItem& label : group->labels()) {
            texts.append(label.text());
        }
    }
    texts.sort();
    return texts;
}
}

// Regression: issue #434 — removing an empty cave used to crash with a
// use-after-free when a positional list of per-cave groups fell out of sync
// with the region. The view is now keyed per trip (a QHash, no positional
// indexing), so the original desync can't happen; this still guards against a
// crash when a station-position change fires on a cave after a sibling removal.
TEST_CASE("Removing an empty cave before a cave with data does not crash cwLinePlotLabelView",
          "[cwLinePlotLabelView]")
{
    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    // The empty cave must be at a lower index than the cave with data:
    // after removal, indexOf(dataCave) drops from 1 to 0, which in the
    // buggy code resolves to the freed group.
    cwCavingRegion region;

    cwCave* emptyCave = new cwCave();
    emptyCave->setName("Cave 2");
    region.addCave(emptyCave);

    cwCave* dataCave = new cwCave();
    dataCave->setName("Cave 1");
    region.addCave(dataCave);

    cwTrip* trip = new cwTrip();
    dataCave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwShot shot;
    shot.setDistance(cwDistanceReading("10.0"));
    shot.setCompass(cwCompassReading("0.0"));
    shot.setClino(cwClinoReading("0.0"));
    chunk->appendShot(cwStation("a1"), cwStation("a2"), shot);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    labelView->setRegion(&region);

    region.removeCave(0);

    // Force the deferred delete so any freed group would be dereferenced by the
    // next station-position update if the view kept a stale reference.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    SurveyTreeTestHelper::setScopedPositions(dataCave, cwStationPositionLookup());
    plotManager->waitToFinish();
}

namespace {
// Builds a region with one cave and two trips that share the tie-in station a2
// (trip A: a1->a2, trip B: a2->a3), solves the line plot so the cave has solved
// station positions, and returns the region (caller owns it via the unique_ptr).
struct TwoTripFixture {
    std::unique_ptr<cwCavingRegion> region;
    std::unique_ptr<cwLinePlotManager> plotManager;
    cwTrip* tripA = nullptr;
    cwTrip* tripB = nullptr;
    cwCave* cave = nullptr;
};

TwoTripFixture makeTwoTripFixture() {
    TwoTripFixture f;
    f.region = std::make_unique<cwCavingRegion>();

    f.cave = new cwCave();
    f.cave->setName(QStringLiteral("Cave 1"));
    f.region->addCave(f.cave);

    auto makeShot = [](const QString& dist, const QString& compass, const QString& clino) {
        cwShot s;
        s.setDistance(cwDistanceReading(dist));
        s.setCompass(cwCompassReading(compass));
        s.setClino(cwClinoReading(clino));
        return s;
    };

    f.tripA = new cwTrip();
    f.tripA->setName(QStringLiteral("Trip A"));
    f.tripA->calibrations()->setAutoDeclination(false);
    cwSurveyChunk* chunkA = new cwSurveyChunk();
    f.tripA->addChunk(chunkA);
    chunkA->appendShot(cwStation("a1"), cwStation("a2"), makeShot("10.0", "0.0", "0.0"));
    f.cave->addTrip(f.tripA);

    f.tripB = new cwTrip();
    f.tripB->setName(QStringLiteral("Trip B"));
    f.tripB->calibrations()->setAutoDeclination(false);
    cwSurveyChunk* chunkB = new cwSurveyChunk();
    f.tripB->addChunk(chunkB);
    chunkB->appendShot(cwStation("a2"), cwStation("a3"), makeShot("10.0", "90.0", "0.0"));
    f.cave->addTrip(f.tripB);

    f.plotManager = std::make_unique<cwLinePlotManager>();
    f.plotManager->setRegion(f.region.get());
    f.plotManager->waitToFinish();

    return f;
}
}

// 3b: the label view registers one keyword item per trip, carrying the trip's
// Type=Line Plot keyword (shared with the line plot geometry), with the trip's
// label group as the setVisible() target.
TEST_CASE("cwLinePlotLabelView registers per-trip station label keyword visibility",
          "[cwLinePlotLabelView][keyword]")
{
    TwoTripFixture f = makeTwoTripFixture();

    // Declared before the QML root so the model outlives the view (as in
    // production, where both belong to cwRootData): the view's destructor
    // removes its items from the model, which must still be alive.
    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(f.region.get());

    SECTION("one keyword item per trip, each carrying Type=Line Plot over a label group") {
        REQUIRE(keywordModel.rowCount() == 2);

        for (int i = 0; i < keywordModel.rowCount(); ++i) {
            cwKeywordItem* item = keywordModel.item(i);
            cwLabel3dGroup* group = groupForKeywordItem(item);
            REQUIRE(group != nullptr);
            CHECK_FALSE(group->labels().isEmpty());

            const auto keywords = item->keywordModel()->keywords();
            CHECK(keywords.contains(cwKeyword(cwKeywordModel::TypeKey,
                                              QStringLiteral("Line Plot"))));
        }
    }

    SECTION("a shared junction station gets a label in each owning trip's group") {
        int a2Count = 0;
        for (int i = 0; i < keywordModel.rowCount(); ++i) {
            cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(i));
            REQUIRE(group != nullptr);
            for (const cwLabel3dItem& label : group->labels()) {
                if (label.text() == QStringLiteral("a2")) { a2Count++; }
            }
        }
        // The OR-visibility semantics rely on a2 appearing once per owning trip;
        // the view's shared declutter KD-tree rejects the co-located duplicate.
        CHECK(a2Count == 2);
    }

    SECTION("hiding via the keyword target's generic setVisible slot hides the group") {
        cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(0));
        REQUIRE(group != nullptr);
        CHECK(group->isVisible());

        // Mirror cwKeywordVisibility::invokeVisible's generic dispatch onto a
        // plain QObject target (the group is neither QQuickItem nor cwRenderObject).
        const bool invoked = QMetaObject::invokeMethod(group, "setVisible",
                                                       Q_ARG(bool, false));
        CHECK(invoked);
        CHECK_FALSE(group->isVisible());
        // The labels survive — only their visibility gate flips.
        CHECK_FALSE(group->labels().isEmpty());
    }

    SECTION("removing a trip drops its label keyword item") {
        REQUIRE(keywordModel.rowCount() == 2);

        f.cave->removeTrip(1); // trip B

        CHECK(keywordModel.rowCount() == 1);
    }
}

// An externally-attached trip owns no cwSurveyChunk, so trip->uniqueStations()
// is empty and the view produced no station labels — the line plot drew but its
// stations were unlabeled. The labels must instead come from the solved cave
// lookup (scope prefix "<tripLabel>."), named with the scope-relative tail that
// cwScopeStationListModel's panel shows.
TEST_CASE("cwLinePlotLabelView shows station labels for an externally attached trip",
          "[cwLinePlotLabelView][Attach]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("labels-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(&region);

    // The attached trip is the region's only trip; the view registers a label
    // keyword item only when the trip has labels, so its external stations must
    // produce exactly one row.
    REQUIRE(keywordModel.rowCount() == 1);

    cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(0));
    REQUIRE(group != nullptr);

    QStringList labelTexts;
    for (const cwLabel3dItem& label : group->labels()) {
        labelTexts.append(label.text());
    }

    // survex_simple.svx: *begin Simple with a1, a2, a3, named with the
    // scope-relative tail (matching the trip's station-list panel).
    CHECK(labelTexts.size() == 3);
    CHECK(labelTexts.contains(QStringLiteral("simple.a1")));
    CHECK(labelTexts.contains(QStringLiteral("simple.a2")));
    CHECK(labelTexts.contains(QStringLiteral("simple.a3")));
}

// A trip two levels down hangs from a Section rather than from the region's
// cave list, so the view has to follow node and trip inserts at every depth to
// label it.
TEST_CASE("cwLinePlotLabelView labels the trips of a depth-2 node",
          "[cwLinePlotLabelView]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));

    cwLinePlotManager plotManager;
    plotManager.setRegion(&region);
    plotManager.waitToFinish();

    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(&region);
    REQUIRE(keywordModel.rowCount() == 0);

    cwCave* section = new cwCave();
    section->setName(QStringLiteral("Upper"));
    addNativeTripWithShot(section, QStringLiteral("First"),
                          QStringLiteral("a1"), QStringLiteral("a2"));
    cave->addNode(section);
    plotManager.waitToFinish();
    REQUIRE_FALSE(plotManager.hasSolveError());

    REQUIRE(keywordModel.rowCount() == 1);
    CHECK(sortedLabelTexts(keywordModel) == QStringList({QStringLiteral("a1"), QStringLiteral("a2")}));

    addNativeTripWithShot(section, QStringLiteral("Second"),
                          QStringLiteral("a2"), QStringLiteral("a3"));
    plotManager.waitToFinish();
    REQUIRE_FALSE(plotManager.hasSolveError());
    CHECK(keywordModel.rowCount() == 2);
}

// FX.6: a derived trip's labels come from the station list the external-file
// scan fills in, which lands apart from the solve. Renaming a Compass survey
// leaves every station position as it was, so the re-solve moves nothing; the
// view must still hear that the scan replaced the trip's stations.
TEST_CASE("cwLinePlotLabelView labels a Compass .dat's stations after the scan replaces the trip",
          "[cwLinePlotLabelView][Attach][Compass]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString datPath = QDir(sourceDir.path()).filePath(QStringLiteral("rename.dat"));
    const QByteArray original = fileContents(fixturePath(QStringLiteral("compass_simple.dat")));
    overwriteFile(datPath, original);

    auto fixture = makeSavedProject(QStringLiteral("labels-compass-rename"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();

    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    // Connected before the attach, as in the app, where the 3D view exists
    // before any source is attached.
    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(region);

    cwSurveyNode* sourceRoot = attachSourceThroughManager(fixture.get(), region->rootNode(), datPath);
    drainPipelines(fixture.get());
    REQUIRE(sourceRoot->tripCount() == 1);

    QByteArray renamed = original;
    renamed.replace("SURVEY NAME: A", "SURVEY NAME: B");
    REQUIRE(renamed != original);
    overwriteFile(datPath, renamed);

    auto future = managerOf(fixture.get())->reloadFromSource(sourceRoot);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("reload: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

    REQUIRE(sourceRoot->tripCount() == 1);
    const cwTrip* trip = sourceRoot->trip(0);
    CHECK(trip->name() == QStringLiteral("B"));

    QStringList solved;
    for (const auto& station : trip->solvedStations()) {
        solved.append(station.first);
    }
    solved.sort();
    const QStringList expected({QStringLiteral("A1"), QStringLiteral("A2"), QStringLiteral("A3")});
    REQUIRE(solved == expected);

    // The native trip has no shots, so the .dat trip is the only labeled one.
    CHECK(keywordModel.rowCount() == 1);
    CHECK(sortedLabelTexts(keywordModel) == expected);
}
