//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwSurveyNetwork.h"
#include "cwTrip.h"

//Test helpers
#include "cwSignalSpy.h"

TEST_CASE("cwCave setData should reset trips", "[cwCave]") {
    cwCave cave;
    QUndoStack undoStack;

    SECTION("With undo") {
        cave.setUndoStack(&undoStack);
    }

    auto oldTrip1 = new cwTrip(&cave);
    oldTrip1->setName("Old Trip 1");
    QPointer<cwTrip> oldTrip1Ptr(oldTrip1);
    auto oldTrip2 = new cwTrip(&cave);
    oldTrip2->setName("Old Trip 2");
    QPointer<cwTrip> oldTrip2Ptr(oldTrip2);

    cave.addTrip(oldTrip1);
    cave.addTrip(oldTrip2);
    REQUIRE(cave.tripCount() == 2);

    cwTripData tripData;
    tripData.name = "New Trip";

    cwCaveData newData;
    newData.name = "New Cave";
    newData.trips.append(tripData);

    cave.setData(newData);

    CHECK(cave.name().toStdString() == "New Cave");
    REQUIRE(cave.tripCount() == 1);
    CHECK(cave.trip(0)->name().toStdString() == "New Trip");
    CHECK(cave.trip(0) != oldTrip1);
    CHECK(cave.trip(0) != oldTrip2);
    CHECK(cave.indexOf(oldTrip1) == -1);
    CHECK(cave.indexOf(oldTrip2) == -1);

    if(cave.undoStack() != nullptr) {
        REQUIRE(!oldTrip1Ptr.isNull());
        REQUIRE(!oldTrip2Ptr.isNull());
        CHECK(oldTrip1Ptr->parent() == &cave);
        CHECK(oldTrip2Ptr->parent() == &cave);
    } else {
        CHECK(oldTrip1Ptr.isNull());
        CHECK(oldTrip2Ptr.isNull());
    }
}

TEST_CASE("cwCave clearTrips should remove all trips", "[cwCave]") {
    cwCave cave;
    QUndoStack undoStack;

    SECTION("With undo") {
        cave.setUndoStack(&undoStack);
    }

    auto trip1 = new cwTrip(&cave);
    trip1->setName("Trip 1");
    QPointer<cwTrip> trip1Ptr(trip1);
    auto trip2 = new cwTrip(&cave);
    trip2->setName("Trip 2");
    QPointer<cwTrip> trip2Ptr(trip2);

    cave.addTrip(trip1);
    cave.addTrip(trip2);
    REQUIRE(cave.tripCount() == 2);

    cave.clearTrips();

    CHECK(cave.tripCount() == 0);
    CHECK(cave.indexOf(trip1) == -1);
    CHECK(cave.indexOf(trip2) == -1);

    if(cave.undoStack() != nullptr) {
        REQUIRE(!trip1Ptr.isNull());
        REQUIRE(!trip2Ptr.isNull());
        CHECK(trip1Ptr->parent() == &cave);
        CHECK(trip2Ptr->parent() == &cave);
    } else {
        CHECK(trip1Ptr.isNull());
        CHECK(trip2Ptr.isNull());
    }
}

// The equality guard on setNetwork is what keeps the line-plot solve from
// re-notifying on every run (the worker rebuilds the network each time).
// Anything listening to scopedNetworkChanged that also re-runs the solve — the
// fix-station error-role refresh does — turns a missing guard into an endless
// re-solve loop, so pin both halves here.
TEST_CASE("cwCavingRegion::setNetwork notifies only on a genuine change", "[cwCavingRegion]") {
    cwCavingRegion region;
    cwSignalSpy networkSpy(&region, &cwCavingRegion::networkChanged);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("A1"), QStringLiteral("A2"));

    region.setNetwork(network);
    CHECK(networkSpy.count() == 1);

    // Re-setting an equal network must stay silent.
    region.setNetwork(network);
    CHECK(networkSpy.count() == 1);

    cwSurveyNetwork equalCopy;
    equalCopy.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    region.setNetwork(equalCopy);
    CHECK(networkSpy.count() == 1);

    // A real change still notifies — the guard must not swallow it.
    cwSurveyNetwork grown;
    grown.addShot(QStringLiteral("A1"), QStringLiteral("A2"));
    grown.addShot(QStringLiteral("A2"), QStringLiteral("A3"));
    region.setNetwork(grown);
    CHECK(networkSpy.count() == 2);
    CHECK(region.network() == grown);
}

// A node's scoped view is the region's lookup cut down to its own subtree and
// named the way the node names its stations: a sibling's stations are absent,
// and a nested node's stations keep that node's label.
TEST_CASE("cwSurveyNode::scopedPositions holds exactly its subtree", "[cwCavingRegion][cwCave]") {
    cwCavingRegion region;
    auto* caveA = new cwCave();
    caveA->setName(QStringLiteral("CaveA"));
    auto* caveB = new cwCave();
    caveB->setName(QStringLiteral("CaveB"));
    region.addCaves({caveA, caveB});
    auto* section = qobject_cast<cwCave*>(region.addNode(caveA, cwSurveyNode::Kind::Folder));
    REQUIRE(section != nullptr);
    section->setName(QStringLiteral("Upper"));

    const QString caveAPrefix = caveA->scopePrefix();
    const QString sectionPrefix = section->scopePrefix();
    REQUIRE(!caveAPrefix.isEmpty());
    REQUIRE(sectionPrefix.startsWith(caveAPrefix));
    REQUIRE(sectionPrefix.size() > caveAPrefix.size());

    const QVector3D a1(1.0f, 0.0f, 0.0f);
    const QVector3D u1(2.0f, 0.0f, 0.0f);
    const QVector3D b1(3.0f, 0.0f, 0.0f);

    cwStationPositionLookup lookup;
    lookup.setPosition(caveAPrefix + QStringLiteral("a1"), a1);
    lookup.setPosition(sectionPrefix + QStringLiteral("u1"), u1);
    lookup.setPosition(caveB->scopePrefix() + QStringLiteral("b1"), b1);

    cwSignalSpy positionsSpy(caveA, &cwSurveyNode::scopedPositionsChanged);
    region.setStationPositions(lookup);
    CHECK(positionsSpy.count() == 1);

    const QString sectionLabel = sectionPrefix.sliced(caveAPrefix.size());
    const cwStationPositionLookup caveView = caveA->scopedPositions();
    CHECK(caveView.positions().size() == 2);
    CHECK(caveView.position(QStringLiteral("a1")) == a1);
    CHECK(caveView.position(sectionLabel + QStringLiteral("u1")) == u1);
    CHECK_FALSE(caveView.hasPosition(QStringLiteral("b1")));

    const cwStationPositionLookup sectionView = section->scopedPositions();
    CHECK(sectionView.positions().size() == 1);
    CHECK(sectionView.position(QStringLiteral("u1")) == u1);

    CHECK(caveB->scopedPositions().positions().size() == 1);
    CHECK(caveB->scopedPositions().position(QStringLiteral("b1")) == b1);
    CHECK(region.rootNode()->scopedPositions() == lookup);

    // A new solve replaces what the view answers, not just what it caches.
    cwStationPositionLookup moved;
    const QVector3D a1Moved(5.0f, 0.0f, 0.0f);
    moved.setPosition(caveAPrefix + QStringLiteral("a1"), a1Moved);
    region.setStationPositions(moved);
    CHECK(positionsSpy.count() == 2);
    CHECK(caveA->scopedPositions().position(QStringLiteral("a1")) == a1Moved);
    CHECK(section->scopedPositions().isEmpty());
}
