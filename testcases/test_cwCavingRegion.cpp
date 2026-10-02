//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwGridConvergence.h"
#include "cwStationHandle.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QAbstractItemModel>
#include <QUndoStack>


TEST_CASE("Copying caving region's data should work correctly", "[cwCavingRegion]") {

    cwCavingRegion region;
    region.setName("test region");

    cwCavingRegionData regionData = region.data();
    CHECK(regionData.name.toStdString() == "test region");
    CHECK(regionData.caves.size() == 0);

    regionData.name = "new name";
    regionData.caves.append(cwCaveData {
         "cave 1",
        // {}
    });

    region.setData(regionData);
    CHECK(region.name().toStdString() == "new name");
    REQUIRE(region.caveCount() == 1);

    CHECK(region.cave(0)->name().toStdString() == "cave 1");
}

TEST_CASE("A cave the region no longer lists stops following its coordinate system",
          "[cwCavingRegion][gridConvergence]") {
    // The region's frame drives the cave's convergence readout. Removing the
    // cave has to break that drive: cwCavingRegion leaves the parent set on
    // remove (so undo can restore it), which means without the teardown in
    // disconnectCave() the region would keep recomputing a cave it no longer
    // lists — and, once the cave is re-added elsewhere or the undo is dropped,
    // keep a stale readout alive against the wrong frame.
    //
    // The undo stack owns the removed cave and keeps it alive for the
    // assertions; it only reaches the children present when it is set, so it
    // goes on before the cave is added.
    cwCavingRegion region;
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    // The frame is derived from the first fix, so it is anchored on a cave of
    // its own, and the caves under test sit 100km east of it where the grid
    // carries a real convergence rather than the ~0 at the frame's origin.
    cwFixStation anchorFix;
    anchorFix.setStationName(QStringLiteral("a1"));
    anchorFix.setInputCS(QStringLiteral("EPSG:32613"));
    anchorFix.setEasting(500000.0);
    anchorFix.setNorthing(4430000.0);
    anchorFix.setElevation(1655.0);

    cwCave* anchor = new cwCave();
    anchor->setName(QStringLiteral("Anchor"));
    region.addCave(anchor);
    anchor->fixStations()->appendFixStation(anchorFix);
    REQUIRE(region.geoReference()->hasCoordinateSystem());

    cwFixStation fix = anchorFix;
    fix.setEasting(600000.0);

    cwCave* removed = new cwCave();
    removed->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(removed);
    removed->fixStations()->appendFixStation(fix);

    // The control: identical in every way except that the region keeps listing
    // it, so it shows what the CS change below does to a cave still driven.
    cwCave* listed = new cwCave();
    listed->setName(QStringLiteral("Mammoth"));
    region.addCave(listed);
    listed->fixStations()->appendFixStation(fix);

    REQUIRE(removed->gridConvergence()->state() == cwGridConvergence::Valid);
    REQUIRE(listed->gridConvergence()->state() == cwGridConvergence::Valid);
    const double angle = removed->gridConvergence()->angle();
    // Precondition: the readout really is anchored, otherwise the assertions
    // below would hold no matter what the connection did.
    REQUIRE(qAbs(angle) > 0.1);

    region.removeCave(region.indexOf(removed));
    REQUIRE(removed->parent() == region.rootNode());

    // Clearing the frame leaves no grid to converge to, which is exactly the
    // NoCoordinateSystem state — a documented transition rather than an
    // assumption about how a projection behaves.
    region.geoReference()->clear();

    CHECK(listed->gridConvergence()->state() == cwGridConvergence::NoCoordinateSystem);
    CHECK(removed->gridConvergence()->state() == cwGridConvergence::Valid);
    CHECK(removed->gridConvergence()->angle() == Catch::Approx(angle));
}

TEST_CASE("cwCavingRegion setData should reset caves", "[cwCavingRegion]") {
    cwCavingRegion region;
    QUndoStack undoStack;

    SECTION("With undo") {
        region.setUndoStack(&undoStack);
    }

    auto oldCave1 = new cwCave(&region);
    oldCave1->setName("Old Cave 1");
    QPointer<cwCave> oldCave1Ptr(oldCave1);

    auto oldCave2 = new cwCave(&region);
    oldCave2->setName("Old Cave 2");
    QPointer<cwCave> oldCave2Ptr(oldCave2);

    region.addCaves({oldCave1, oldCave2});
    REQUIRE(region.caveCount() == 2);

    cwCavingRegionData newData;
    newData.name = "New Region";
    newData.caves.append(cwCaveData {
        "New Cave",
        {},
        cwStationPositionLookup()
    });

    region.setData(newData);

    CHECK(region.name().toStdString() == "New Region");
    REQUIRE(region.caveCount() == 1);
    CHECK(region.cave(0)->name().toStdString() == "New Cave");
    CHECK(region.cave(0) != oldCave1);
    CHECK(region.cave(0) != oldCave2);
    CHECK(region.indexOf(oldCave1) == -1);
    CHECK(region.indexOf(oldCave2) == -1);

    if(region.undoStack() != nullptr) {
        REQUIRE(!oldCave1Ptr.isNull());
        REQUIRE(!oldCave2Ptr.isNull());
        CHECK(oldCave1Ptr->parent() == region.rootNode());
        CHECK(oldCave2Ptr->parent() == region.rootNode());
    } else {
        //No undostack, caves should have been deleted
        CHECK(oldCave1Ptr.isNull());
        CHECK(oldCave2Ptr.isNull());
    }
}

TEST_CASE("A node under a cave is reachable through allNodes and hidden from caves()",
          "[cwCavingRegion]") {
    // The region lists the root's direct children only. A node deeper in the
    // tree still belongs to the region — it resolves a handle and answers
    // parentRegion() — but it is not a cave the Data page lists.
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwCave* section = new cwCave();
    section->setName(QStringLiteral("Upper Level"));
    cave->addNode(section);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Topo 1"));
    section->addTrip(trip);

    REQUIRE(region.caveCount() == 1);
    CHECK(region.caves() == QList<cwCave*>{cave});
    CHECK_FALSE(region.caves().contains(section));
    CHECK(region.rootNode()->allNodes().contains(section));
    CHECK(section->parentRegion() == &region);

    CHECK(region.caveFor(cwStationHandle(cwStationHandle::NativeCave,
                                         section->id(),
                                         QStringLiteral("a1"))) == section);
    CHECK(region.caveFor(cwStationHandle(cwStationHandle::Trip,
                                         trip->id(),
                                         QStringLiteral("a1"))) == section);
}

TEST_CASE("Cave insert, remove and undo drive the region's row signals in pairs",
          "[cwCavingRegion]") {
    // The rows are the root's children now, so every row signal is relayed. A
    // relay that emitted a begin without its end, or that carried nodesDeleted
    // through undo, shows up here.
    cwCavingRegion region;
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cwSignalSpy aboutToInsert(&region, &QAbstractItemModel::rowsAboutToBeInserted);
    cwSignalSpy inserted(&region, &QAbstractItemModel::rowsInserted);
    cwSignalSpy aboutToRemove(&region, &QAbstractItemModel::rowsAboutToBeRemoved);
    cwSignalSpy removed(&region, &QAbstractItemModel::rowsRemoved);
    cwSignalSpy beginInsert(&region, &cwCavingRegion::beginInsertCaves);
    cwSignalSpy insertedCaves(&region, &cwCavingRegion::insertedCaves);
    cwSignalSpy beginRemove(&region, &cwCavingRegion::beginRemoveCaves);
    cwSignalSpy removedCaves(&region, &cwCavingRegion::removedCaves);
    cwSignalSpy countChanged(&region, &cwCavingRegion::caveCountChanged);
    cwSignalSpy scopeLabels(&region, &cwCavingRegion::scopeLabelsChanged);
    cwSignalSpy ownersDeleted(&region, &cwCavingRegion::ownersDeleted);

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Topo 1"));
    cave->addTrip(trip);
    const QUuid caveId = cave->id();
    const QUuid tripId = trip->id();

    CHECK(aboutToInsert.count() == 1);
    CHECK(inserted.count() == 1);
    CHECK(beginInsert.count() == 1);
    CHECK(insertedCaves.count() == 1);
    CHECK(countChanged.count() == 1);
    CHECK(aboutToRemove.count() == 0);
    CHECK(removed.count() == 0);
    CHECK(ownersDeleted.count() == 0);
    //The cave insert and the trip insert each moved a label.
    CHECK(scopeLabels.count() == 2);

    region.removeCave(0);

    CHECK(aboutToRemove.count() == 1);
    CHECK(removed.count() == 1);
    CHECK(beginRemove.count() == 1);
    CHECK(removedCaves.count() == 1);
    CHECK(countChanged.count() == 2);
    REQUIRE(ownersDeleted.count() == 1);
    const auto ids = ownersDeleted.at(0).at(0).value<QList<QUuid>>();
    CHECK(ids == QList<QUuid>({caveId, tripId}));

    undoStack.undo();

    CHECK(aboutToInsert.count() == 2);
    CHECK(inserted.count() == 2);
    CHECK(beginInsert.count() == 2);
    CHECK(insertedCaves.count() == 2);
    REQUIRE(region.caveCount() == 1);
    //Undo re-inserts the same object, never a copy.
    CHECK(region.cave(0) == cave);
    //Undo takes nothing away, so it says nothing about deleted owners.
    CHECK(ownersDeleted.count() == 1);

    undoStack.redo();

    CHECK(aboutToRemove.count() == 2);
    CHECK(removed.count() == 2);
    CHECK(region.caveCount() == 0);
    CHECK(ownersDeleted.count() == 1);
}

TEST_CASE("addCaves inserts a batch as one undo step", "[cwCavingRegion]") {
    cwCavingRegion region;
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cwCave* first = new cwCave();
    first->setName(QStringLiteral("Fisher Ridge"));
    cwCave* second = new cwCave();
    second->setName(QStringLiteral("Mammoth"));

    region.addCaves({first, second});

    REQUIRE(region.caveCount() == 2);
    CHECK(undoStack.count() == 1);

    undoStack.undo();
    CHECK(region.caveCount() == 0);

    undoStack.redo();
    REQUIRE(region.caveCount() == 2);
    CHECK(region.cave(0) == first);
    CHECK(region.cave(1) == second);
}

TEST_CASE("A cave added to a region with a coordinate system picks it up on insert",
          "[cwCavingRegion][gridConvergence]") {
    // The grid a cave converges to is the region's frame, and the cave only
    // learns of the region by being inserted.
    cwCavingRegion region;
    region.geoReference()->restore(cwGeoReference::Frozen, QStringLiteral("EPSG:32613"), {}, QString());

    cwFixStation fix;
    fix.setStationName(QStringLiteral("a1"));
    fix.setInputCS(QStringLiteral("EPSG:32613"));
    fix.setEasting(600000.0);
    fix.setNorthing(4430000.0);
    fix.setElevation(1655.0);

    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    cave->fixStations()->appendFixStation(fix);

    REQUIRE(cave->gridConvergence()->state() == cwGridConvergence::NoCoordinateSystem);

    region.addCave(cave);

    CHECK(cave->gridConvergence()->state() == cwGridConvergence::Valid);
}

TEST_CASE("addNode names, places and undoes a node", "[cwCavingRegion]") {
    cwCavingRegion region;
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cwSurveyNode* cave = region.addNode(nullptr, cwSurveyNode::Kind::Cave);

    REQUIRE(cave != nullptr);
    CHECK(cave->name() == QStringLiteral("Cave 1"));
    CHECK(cave->kind() == cwSurveyNode::Kind::Cave);
    REQUIRE(region.caveCount() == 1);
    CHECK(region.cave(0) == cave);
    CHECK(undoStack.count() == 1);

    cwSurveyNode* folder = region.addNode(cave, cwSurveyNode::Kind::Folder);

    REQUIRE(folder != nullptr);
    CHECK(folder->name() == QStringLiteral("Folder 1"));
    CHECK(folder->kind() == cwSurveyNode::Kind::Folder);
    CHECK(folder->parentNode() == cave);
    CHECK(region.rootNode()->allNodes().contains(folder));
    CHECK_FALSE(region.caves().contains(qobject_cast<cwCave*>(folder)));
    CHECK(region.caveCount() == 1);

    undoStack.undo();
    CHECK(cave->childNodeCount() == 0);

    cwCavingRegion otherRegion;
    cwCave* foreign = new cwCave();
    foreign->setName(QStringLiteral("Elsewhere"));
    otherRegion.addCave(foreign);

    CHECK(region.addNode(foreign, cwSurveyNode::Kind::Folder) == nullptr);
    CHECK(foreign->childNodeCount() == 0);

    //QML passes enums as ints, so a kind outside the enum has to be refused
    //rather than inserted under an empty name.
    const int caveCountBefore = region.caveCount();
    CHECK(region.addNode(nullptr, static_cast<cwSurveyNode::Kind>(99)) == nullptr);
    CHECK(region.caveCount() == caveCountBefore);
}
