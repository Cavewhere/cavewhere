//Catch includes
#include "catch.hpp"

//Our includes
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "TestHelper.h"

TEST_CASE("cwProject isModified should work correctly", "[cwProject]") {
    cwProject project;
    CHECK(project.isModified() == false);

    cwCave* cave1 = new cwCave();
    cave1->setName("cave1");
    project.cavingRegion()->addCave(cave1);

    CHECK(project.isModified() == true);

    QString testFile = "test_cwProject.cw";
    QFile::remove(testFile);

    project.saveAs("test_cwProject.cw");
    project.waitSaveToFinish();

    CHECK(project.isModified() == false);

    project.newProject();
    CHECK(project.isModified() == false);

    project.loadFile("test_cwProject.cw");
    project.waitLoadToFinish();
    CHECK(project.isModified() == false);

    project.cavingRegion()->cave(0)->addTrip();
    CHECK(project.isModified() == true);

    project.save();
    project.waitSaveToFinish();
    CHECK(project.isModified() == false);

    SECTION("Load file") {
        //If this fails, this is probably because of a version change
        fileToProject(&project, "://datasets/network.cw");
        project.waitLoadToFinish();
        CHECK(project.isModified() == false);

        REQUIRE(project.cavingRegion()->caveCount() == 1);
        REQUIRE(project.cavingRegion()->cave(0)->tripCount() == 1);

        cwTrip* firstTrip = project.cavingRegion()->cave(0)->trip(0);
        firstTrip->setName("Sauce!");
        CHECK(project.isModified() == true);
    }
}
