//Catch includes
#include "catch.hpp"

//Our includes
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "TestHelper.h"
#include "cwImageProvider.h"
#include "cwRootData.h"
#include "cwSurveyNoteModel.h"
#include "cwTaskManagerModel.h"

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
        //If this fails, this is probably because of a version change, or other save changes
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

TEST_CASE("Image data should save and load correctly", "[cwProject]") {

    qRegisterMetaType<QList <cwImage> >("QList<cwImage>");

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    project->saveAs("imageTest-" + QUuid::createUuid().toString().left(5));

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    QString filename = copyToTempFolder("://datasets/test_cwTextureUploadTask/PhakeCave.PNG");
    QImage originalImage(filename);

    trip->notes()->addFromFiles({QUrl("file:/" + filename)}, project);

    rootData->taskManagerModel()->waitForTasks();

    SECTION("Is Modifed shouldn't effect the save and load") {
        //Make sure is modified doesn't modify the underlying file
        CHECK(project->isModified() == true);
    }

    REQUIRE(trip->notes()->notes().size() == 1);

    cwImage image = trip->notes()->notes().first()->image();
    cwImageProvider provider;
    provider.setProjectPath(project->filename());
    QImage sqlImage = provider.image(image.original());

    CHECK(!sqlImage.isNull());
    CHECK(originalImage == sqlImage);
}
