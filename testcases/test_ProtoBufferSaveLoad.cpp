//Test includes
#include "TestHelper.h"

//Our includes
#include "cwRegionLoadTask.h"
#include "cwRegionSaveTask.h"
#include "cwRootData.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwSurveyNetwork.h"
#include "cwErrorListModel.h"
#include "cavewhereVersion.h"
#include "cwSurveyNoteModel.h"
#include "cwTaskManagerModel.h"
#include "cwImageProvider.h"

//std includes
#include <memory>
#include <iostream>

//Qt includes
#include <QUuid>

//catch includes
#include "catch.hpp"

TEST_CASE("Save / Load should work with cwSurveyNetwork", "[ProtoSaveLoad]") {

    auto root = std::make_unique<cwRootData>();

    cwCave* cave = new cwCave();
    cwTrip* trip = new cwTrip();

    cave->addTrip(trip);
    root->region()->addCave(cave);

    cwSurveyChunk* chunk = new cwSurveyChunk;

    cwStation a1("a1");
    cwStation a2("a2");
    cwStation a3("a3");

    cwShot a1_to_a2("100", "0.0", "0.0", "0.0", "0.0");
    cwShot a2_to_a3("50", "0.0", "0.0", "0.0", "0.0");

    chunk->appendShot(a1, a2, a1_to_a2);
    chunk->appendShot(a2, a3, a2_to_a3);

    trip->addChunk(chunk);

    root->linePlotManager()->waitToFinish();

    CHECK(root->project()->isTemporaryProject() == true);

    auto filename = prependTempFolder(QString("test_cwSurveyNetwork-") + QUuid::createUuid().toString().remove(QRegularExpression("{|}|-")) + ".cw");
    root->project()->saveAs(filename);
    root->project()->waitSaveToFinish();

    root->project()->newProject();
    root->project()->waitSaveToFinish();

    CHECK(root->region()->caveCount() == 0);

    root->project()->loadFile(filename);
    root->project()->waitLoadToFinish();
    REQUIRE(root->region()->caveCount() == 1);
    CHECK(root->project()->isTemporaryProject() == false);

    auto loadCave = root->region()->cave(0);
    auto network = loadCave->network();

    auto testStationNeigbors = [=](QString stationName, QStringList
            neighbors) {
        auto neighborsAtStation = network.neighbors(stationName);
        auto foundNeighbors = QSet<QString>(neighborsAtStation.begin(), neighborsAtStation.end());
        auto checkNeigbbors = QSet<QString>(neighbors.begin(), neighbors.end());
        CHECK(foundNeighbors == checkNeigbbors);
    };

    testStationNeigbors("a1", {"A2"});
    testStationNeigbors("a2", {"A1", "A3"});
    testStationNeigbors("a3", {"A2"});
}

TEST_CASE("Loading should report errors correctly", "[ProtoSaveLoad]") {
    auto root = std::make_unique<cwRootData>();

    SECTION("Bad file name that doesn't exist") {
        QString filename = "badFileName.cw";
        REQUIRE(!QFile::exists(filename));
        auto fullPath = QDir::current().absoluteFilePath(filename);

        auto errorModel = root->project()->errorModel();
        CHECK(errorModel->size() == 0);

        root->project()->loadFile(filename);
        root->project()->waitLoadToFinish();

        REQUIRE(errorModel->size() == 1);
        CHECK(errorModel->at(0) == cwError(QString("Couldn't open '%1' because it doesn't exist").arg(filename), cwError::Fatal));

        CHECK(!QFile::exists(filename));
    }

    SECTION("Bad file that does exist") {
        fileToProject(root->project(), "://datasets/test_ProtoBufferSaveLoad/badFile.cw");
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        CHECK(errorModel->at(0) == cwError("Couldn't prepare select Caving Region:'file is not a database' sql:'SELECT protoBuffer FROM ObjectData where id = 1'", cwError::Fatal));

        CHECK(root->project()->isTemporaryProject());
    }

    SECTION("Detect thet protoBuf is corrupt") {
        fileToProject(root->project(), "://datasets/test_ProtoBufferSaveLoad/corrupted.cw");
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        CHECK(errorModel->at(0) == cwError("Couldn't read proto buffer. Corrupted?!", cwError::Fatal));

        CHECK(root->project()->isTemporaryProject());
    }

    SECTION("New version should be detected propertly") {
        CHECK(root->project()->cavingRegion()->caveCount() == 0);

        fileToProject(root->project(), "://datasets/test_ProtoBufferSaveLoad/newerVersion.cw");
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        QString expectErrorMessage = QString("Upgrade CaveWhere to 0.08-70-g20d1d7c to load this file! Current file version is 10000. CaveWhere %1 supports up to file version %2. You are loading a newer CaveWhere file than this version supports. You will loss data if you save")
                .arg(CavewhereVersion)
                .arg(cwRegionIOTask::protoVersion());
        CHECK(errorModel->at(0) == cwError(expectErrorMessage, cwError::Warning));
        errorModel->clear();

        CHECK(root->project()->cavingRegion()->caveCount() == 1);
        CHECK(root->project()->isTemporaryProject() == false);
        CHECK(root->project()->canSaveDirectly() == false);

        SECTION("Check that saveAs fails to the file") {
            root->project()->saveAs(root->project()->filename());
            REQUIRE(errorModel->size() == 1);
            expectErrorMessage = QString("Can't overwrite %1 because file is newer that the current version of CaveWhere. To solve this, save it somewhere else").arg(root->project()->filename());
            CHECK(errorModel->at(0) == cwError(expectErrorMessage, cwError::Fatal));

            CHECK(root->project()->isModified() == false);
        }
    }

#ifndef Q_OS_WIN
    //This testcase is broken on windows because removing the read permission still al
    SECTION("File doesn't have read permissions") {
        CHECK(root->project()->cavingRegion()->caveCount() == 0);

        auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

        QFile file(filename);
        CHECK(file.setPermissions(QFileDevice::WriteOwner | QFileDevice::WriteUser));

        QFileInfo info(filename);
        REQUIRE(!info.isReadable());

        root->project()->loadFile(filename);
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        QString expectErrorMessage = QString("Couldn't open '%1' because you don't have permission to read it").arg(filename);

        CHECK(errorModel->at(0) == cwError(expectErrorMessage, cwError::Fatal));

        CHECK(root->project()->isTemporaryProject());
    }
#endif

    SECTION("File doesn't have write permissions") {

        CHECK(root->project()->cavingRegion()->caveCount() == 0);

        auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

        QFile file(filename);
        CHECK(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::ReadUser));

        QFileInfo info(filename);
        REQUIRE(!info.isWritable());
        REQUIRE(info.isReadable());

        root->project()->loadFile(filename);
        root->project()->waitLoadToFinish();
        auto errorModel = root->project()->errorModel();

        REQUIRE(errorModel->size() == 1);

        QString expectErrorMessage = QString("'%1' is a ReadOnly file. Copying it to a temporary file").arg(filename);

        CHECK(errorModel->at(0) == cwError(expectErrorMessage, cwError::Warning));

        CHECK(root->project()->filename().toStdString() != filename.toStdString());
        CHECK(root->project()->isTemporaryProject());
        CHECK(root->project()->cavingRegion()->caveCount() == 1);
        CHECK(root->project()->canSaveDirectly() == false);

    }

    root->taskManagerModel()->waitForTasks();
    root->futureManagerModel()->waitForFinished();
}
