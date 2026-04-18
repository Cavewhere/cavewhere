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

//Qt includes
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <memory>

// Regression: issue #434 — removing an empty cave crashed with a
// use-after-free in cwLinePlotLabelView::updateCaveStations.
// cwLinePlotLabelView::removeCaves() calls deleteLater() on the cave's
// cwLabel3dGroup but never removes the entry from CaveLabelGroups, so the
// list gets out of sync with the region. When a later
// stationPositionPositionChanged fires on a surviving cave whose new index
// now points at a stale (deleted) group, setLabels() is called on a freed
// object.
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

    // Force the deferred delete so the stale CaveLabelGroups entry points
    // at freed memory before the next line-plot update dereferences it.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    dataCave->setStationPositionLookup(cwStationPositionLookup());
    plotManager->waitToFinish();
}
