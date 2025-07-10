//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwTrip.h"
#include "cwSaveLoad.h"
#include "cwRootData.h"

//Qt includes
#include <QStandardPaths>

//Test includes
#include "TestHelper.h"

QDir testDir() {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir dir(desktopPath);
    dir.mkdir("trip");
    dir.cd("trip");
    return dir;
}

TEST_CASE("cwSaveLoad should save and load cwTrip - empty", "[cwSaveLoad]") {
    auto dir = testDir();

    cwTrip emptyTrip;

    cwSaveLoad save;
    save.saveTrip(dir, &emptyTrip);

    save.waitForFinished();
}

TEST_CASE("cwSaveLoad should save and load cwTrip - complex", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

    root->project()->loadFile(filename);
    root->project()->waitLoadToFinish();

    REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    auto cave = root->project()->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() >= 1);
    auto trip = cave->trip(0);

    cwSaveLoad save;
    auto dir = testDir();

    save.saveTrip(dir, trip);
    save.waitForFinished();
}
