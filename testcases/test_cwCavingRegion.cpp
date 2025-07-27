//Catch includes
#include <catch2/catch_test_macros.hpp>

#include "cwCavingRegion.h"

TEST_CASE("Copying caving region's data should work correctly", "[cwCavingRegion]") {

    cwCavingRegion region;
    region.setName("test region");

    cwCavingRegion::Data regionData = region.data();
    CHECK(regionData.name.toStdString() == "test region");
    CHECK(regionData.caves.size() == 0);

    regionData.name = "new name";
    regionData.caves.append(cwCave::Data {
         "cave 1",
        // {}
    });

    region.setData(regionData);
    CHECK(region.name().toStdString() == "new name");
    REQUIRE(region.caveCount() == 1);

    CHECK(region.cave(0)->name().toStdString() == "cave 1");
}
