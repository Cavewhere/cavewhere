//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwLinePlotLabelView.h"
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

// Regression: issue #434 — removing an empty cave used to crash with a
// use-after-free when a positional list of per-cave groups fell out of sync
// with the region. The view is now keyed per trip (a QHash, no positional
// indexing), so the original desync can't happen; this still guards against a
// crash when a station-position change fires on a cave after a sibling removal.
TEST_CASE("Removing an empty cave before a cave with data does not crash cwLinePlotLabelView",
          "[cwLinePlotLabelView]")
{
    // cwLinePlotLabelView::updateGroup creates labels from a QQmlComponent,
    // which requires the item to live inside a QML context.
    QQmlEngine engine;
    const char* rootQml =
        "import QtQuick\n"
        "import cavewherelib\n"
        "Item {\n"
        "  width: 200; height: 200\n"
        "  LinePlotLabelView { objectName: \"labelView\" }\n"
        "}\n";

    QQmlComponent component(&engine);
    component.setData(QByteArray(rootQml), QUrl());
    std::unique_ptr<QObject> root(component.create());
    REQUIRE(root != nullptr);

    auto labelView = root->findChild<cwLinePlotLabelView*>("labelView");
    REQUIRE(labelView != nullptr);

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

    dataCave->setStationPositionLookup(cwStationPositionLookup());
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

cwLabel3dGroup* groupForKeywordItem(cwKeywordItem* item) {
    return qobject_cast<cwLabel3dGroup*>(item->object());
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

    QQmlEngine engine;
    const char* rootQml =
        "import QtQuick\n"
        "import cavewherelib\n"
        "Item {\n"
        "  width: 200; height: 200\n"
        "  LinePlotLabelView { objectName: \"labelView\" }\n"
        "}\n";

    QQmlComponent component(&engine);
    component.setData(QByteArray(rootQml), QUrl());
    std::unique_ptr<QObject> root(component.create());
    REQUIRE(root != nullptr);

    auto labelView = root->findChild<cwLinePlotLabelView*>("labelView");
    REQUIRE(labelView != nullptr);

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
// lookup (scope prefix trip_<uuid>.), named with the scope-relative tail that
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

    // The label view builds label data from a QQmlComponent, so it must live in
    // a QML context (as in the fixtures above).
    cwKeywordItemModel keywordModel;

    QQmlEngine engine;
    const char* rootQml =
        "import QtQuick\n"
        "import cavewherelib\n"
        "Item {\n"
        "  width: 200; height: 200\n"
        "  LinePlotLabelView { objectName: \"labelView\" }\n"
        "}\n";

    QQmlComponent component(&engine);
    component.setData(QByteArray(rootQml), QUrl());
    std::unique_ptr<QObject> root(component.create());
    REQUIRE(root != nullptr);

    auto labelView = root->findChild<cwLinePlotLabelView*>("labelView");
    REQUIRE(labelView != nullptr);

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
