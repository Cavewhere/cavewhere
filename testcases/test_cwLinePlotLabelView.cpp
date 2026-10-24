//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwLinePlotLabelView.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotManager.h"
#include "cwStationPositionLookup.h"
#include "cwLabel3dGroup.h"
#include "cwLabel3dItem.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwRenderTextBillboards.h"

//Qt includes
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QMetaObject>
#include <QMatrix4x4>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStringList>
#include <QTextLayout>
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

namespace {
constexpr int kLabelViewportWidth = 800;
constexpr int kLabelViewportHeight = 600;

// Meters per pixel for the ortho camera: the fixture's stations sit 10 m apart,
// which puts them 200 px apart on screen, far enough that no two collide in the
// declutter grid.
constexpr double kLabelZoomScale = 0.05;

// A distinct view per call (cwCamera::setViewMatrix ignores an unchanged
// matrix), centered on @a center and looking down -z.
QMatrix4x4 viewLookingAt(const QVector3D& center, float nudge)
{
    QMatrix4x4 view;
    view.translate(-center.x() + nudge, -center.y(), -center.z());
    return view;
}

// Centroid of every label the view holds, so the camera frames the fixture
// wherever the solver put its stations.
QVector3D labelCentroid(cwKeywordItemModel& keywordModel)
{
    QVector3D sum;
    int count = 0;
    for (int i = 0; i < keywordModel.rowCount(); ++i) {
        cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(i));
        if (group == nullptr) {
            continue;
        }
        for (const cwLabel3dItem& label : group->labels()) {
            sum += label.position();
            ++count;
        }
    }
    return count > 0 ? sum / float(count) : QVector3D();
}

QStringList placedTexts(const cwRenderTextBillboards* layer)
{
    QStringList texts;
    for (const cwRenderTextBillboards::RenderSlot& slot : layer->buildRenderSlots()) {
        texts.append(slot.layout ? slot.layout->text() : QString());
    }
    texts.sort();
    return texts;
}

cwLabel3dGroup* groupWithStation(cwKeywordItemModel& keywordModel, const QString& stationName)
{
    for (int i = 0; i < keywordModel.rowCount(); ++i) {
        cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(i));
        if (group == nullptr) {
            continue;
        }
        for (const cwLabel3dItem& label : group->labels()) {
            if (label.text() == stationName) {
                return group;
            }
        }
    }
    return nullptr;
}
}

TEST_CASE("cwLabel3dView draws placed station labels through the shared text layer",
          "[cwLinePlotLabelView][cwLabel3dView]")
{
    TwoTripFixture f = makeTwoTripFixture();

    // Outlive the view, as in production (see the keyword test above).
    cwKeywordItemModel keywordModel;
    cwScene scene;
    cwCamera camera;
    camera.setViewport(QRect(0, 0, kLabelViewportWidth, kLabelViewportHeight));
    camera.setZoomScale(kLabelZoomScale);
    camera.setProjection(camera.orthoProjectionDefault());

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(kLabelViewportWidth, kLabelViewportHeight);

    const QString rootQml = QStringLiteral(
        "import QtQuick\n"
        "import cavewherelib\n"
        "Item {\n"
        "  width: %1; height: %2\n"
        "  LinePlotLabelView { objectName: \"labelView\"; anchors.fill: parent }\n"
        "}\n").arg(kLabelViewportWidth).arg(kLabelViewportHeight);

    QQmlComponent component(&engine);
    component.setData(rootQml.toUtf8(), QUrl());
    std::unique_ptr<QObject> root(component.create());
    REQUIRE(root != nullptr);
    auto* rootItem = qobject_cast<QQuickItem*>(root.get());
    REQUIRE(rootItem != nullptr);
    rootItem->setParentItem(window.contentItem());

    auto labelView = root->findChild<cwLinePlotLabelView*>("labelView");
    REQUIRE(labelView != nullptr);
    REQUIRE(labelView->window() == &window);

    labelView->setScene(&scene);
    labelView->setCamera(&camera);
    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(f.region.get());

    // Frame the stations: one full declutter pass over every group, the
    // per-frame path.
    const QVector3D center = labelCentroid(keywordModel);
    camera.setViewMatrix(viewLookingAt(center, 0.0f));

    // Labels are text billboards now, not QQuickItems under the view.
    CHECK(labelView->childItems().isEmpty());

    cwRenderTextBillboards* layer = scene.textBillboardLayer();
    REQUIRE(layer != nullptr);

    SECTION("every station is placed once; the shared junction station only once")
    {
        CHECK(layer->billboardCount() == 3);
        CHECK(placedTexts(layer) == QStringList({QStringLiteral("a1"),
                                                 QStringLiteral("a2"),
                                                 QStringLiteral("a3")}));
        CHECK(placedTexts(layer).count(QStringLiteral("a2")) == 1);
    }

    SECTION("hiding a trip by keyword visibility removes its text billboards")
    {
        cwLabel3dGroup* tripAGroup = groupWithStation(keywordModel, QStringLiteral("a1"));
        REQUIRE(tripAGroup != nullptr);

        const int before = layer->billboardCount();
        const bool invoked = QMetaObject::invokeMethod(tripAGroup, "setVisible",
                                                       Q_ARG(bool, false));
        REQUIRE(invoked);

        CHECK(layer->billboardCount() < before);
        CHECK_FALSE(placedTexts(layer).contains(QStringLiteral("a1")));

        // On the next pass trip B still labels its own stations, including the
        // junction it shares with the hidden trip.
        camera.setViewMatrix(viewLookingAt(center, 1.0f));
        CHECK(placedTexts(layer) == QStringList({QStringLiteral("a2"),
                                                 QStringLiteral("a3")}));
    }
}
