//Catch includes
#include <catch2/catch_test_macros.hpp>

#include "cwCavingRegion.h"
#include "cwCave.h"


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
