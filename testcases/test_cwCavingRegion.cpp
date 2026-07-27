//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwGridConvergence.h"

//Qt includes
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
    // A fix station with no input CS of its own falls back to the region's
    // globalCoordinateSystem, so the region drives the cave's convergence
    // readout. Removing the cave has to break that drive: cwCavingRegion leaves
    // the parent set on remove (so undo can restore it), which means without the
    // teardown in disconnectCave() the region would keep recomputing a cave it
    // no longer lists — and, once the cave is re-added elsewhere or the undo is
    // dropped, keep a stale readout alive against the wrong CS.
    //
    // The undo stack owns the removed cave and keeps it alive for the
    // assertions; it only reaches the children present when it is set, so it
    // goes on before the cave is added.
    cwCavingRegion region;
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    // Central meridian -105°, so a station 100km east of it carries a real
    // convergence rather than the ~0 the meridian itself would give.
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    // No input CS of its own, which is what makes these caves read the region's.
    cwFixStation fix;
    fix.setStationName(QStringLiteral("a1"));
    fix.setEasting(600000.0);
    fix.setNorthing(4430000.0);
    fix.setElevation(1655.0);

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
    REQUIRE(removed->parent() == &region);

    // Taking the CS away leaves a fix station with no input CS and no fallback,
    // which is exactly the NoCoordinateSystem state — a documented transition
    // rather than an assumption about how a projection behaves.
    region.geoReference()->setGlobalCoordinateSystem(QString());

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
        CHECK(oldCave1Ptr->parent() == &region);
        CHECK(oldCave2Ptr->parent() == &region);
    } else {
        //No undostack, caves should have been deleted
        CHECK(oldCave1Ptr.isNull());
        CHECK(oldCave2Ptr.isNull());
    }
}
