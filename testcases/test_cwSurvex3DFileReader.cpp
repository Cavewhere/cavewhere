//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwSurvex3DFileReader.h"
#include "cwCavernTask.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QFileInfo>

TEST_CASE("cwSurvex3DFileReader should return empty lookup for missing file", "[cwSurvex3DFileReader]") {
    cwSurvex3DFileReader reader;
    auto lookup = reader.readStationPositions("/nonexistent/file.3d");
    CHECK(lookup.isEmpty());
}

TEST_CASE("cwSurvex3DFileReader should read station positions from .3d file", "[cwSurvex3DFileReader]") {
    // Run cavern to produce a .3d file from the test dataset
    QString survexDataFile = copyToTempFolder("://datasets/test_cwSurvexport/data.svx");
    REQUIRE(QFile::exists(survexDataFile));

    cwCavernTask cavern;
    cavern.setSurvexFile(survexDataFile);
    cavern.start();
    cavern.waitToFinish();

    REQUIRE(!cavern.output3dFileName().isEmpty());
    REQUIRE(QFileInfo(cavern.output3dFileName()).exists());

    // Read station positions directly from the .3d file
    cwSurvex3DFileReader reader;
    cwStationPositionLookup lookup = reader.readStationPositions(cavern.output3dFileName());

    CHECK(!lookup.isEmpty());

    // The test dataset has 6 stations: 26, 26a-26e
    // Expected positions match the existing cwSurvexportCSVTask test dataset
    auto positions = lookup.positions();
    CHECK(positions.size() == 6);

    // Verify known station positions (station 26 is fixed at origin)
    CHECK(lookup.hasPosition("26"));
    QVector3D pos26 = lookup.position("26");
    CHECK(pos26.x() == Catch::Approx(0.0).margin(0.01));
    CHECK(pos26.y() == Catch::Approx(0.0).margin(0.01));
    CHECK(pos26.z() == Catch::Approx(0.0).margin(0.01));

    // Station 26a should be nearby with non-zero coordinates
    CHECK(lookup.hasPosition("26a"));
    QVector3D pos26a = lookup.position("26a");
    CHECK(pos26a.x() == Catch::Approx(0.93).margin(0.01));
    CHECK(pos26a.y() == Catch::Approx(-5.40).margin(0.01));
    CHECK(pos26a.z() == Catch::Approx(2.32).margin(0.01));
}
