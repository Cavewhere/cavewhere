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
#include "cwTripCalibration.h"
#include "cwReadingStates.h"
#include "cwTeam.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwProjectedProfileScrapViewMatrix.h"

//std includes
#include <memory>
#include <iostream>
#include <optional>

//Qt includes
#include <QUuid>

//catch includes
#include <catch2/catch_test_macros.hpp>

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
            QFileInfo info(root->project()->filename());
            auto lastModifiedTime = info.lastModified();

            QThread::msleep(10);

            root->project()->saveAs(root->project()->filename());
            REQUIRE(errorModel->size() == 1);
            expectErrorMessage = QString("Can't overwrite %1 because file is newer that the current version of CaveWhere. To solve this, save it somewhere else").arg(root->project()->filename());
            CHECK(errorModel->at(0) == cwError(expectErrorMessage, cwError::Fatal));

            //Make sure the file hasen't be modified
            CHECK(lastModifiedTime == info.lastModified());
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

//This also test v3->v6
TEST_CASE("Save and load should work correctly for Projected Profile v3->v5", "[ProtoSaveLoad]") {

    auto fileCheck = [](QString filename, auto scrapCheckFunc) {
        auto root = std::make_unique<cwRootData>();

        auto isResourceFile = [](QString filename){
            return filename.indexOf("://") == 0;
        };

        if(isResourceFile(filename)) {
            fileToProject(root->project(), filename);
        } else {
            root->project()->loadFile(filename);
            root->project()->waitLoadToFinish();
        }

        auto errorModel = root->project()->errorModel();
        REQUIRE(errorModel->size() == 0);

        //Test that it loades correctly, 6 stations, has noteStations, has polygon
        //and in running profile mode with running profile, with auto
        REQUIRE(root->project()->cavingRegion()->caveCount() == 1);
        auto cave = root->project()->cavingRegion()->cave(0);
        CHECK(cave->name().toStdString() == "My Cave");

        REQUIRE(cave->tripCount() == 1);
        auto trip = cave->trip(0);
        CHECK(trip->name().toStdString() == "Best Trip");

        auto calibration = trip->calibrations();
        CHECK(calibration->declination() == 1.0);
        CHECK(calibration->distanceUnit() == cwUnits::Meters);
        CHECK(calibration->tapeCalibration() == 0.03);

        CHECK(calibration->hasFrontSights() == true);
        CHECK(calibration->frontCompassCalibration() == 0.01);
        CHECK(calibration->frontClinoCalibration() == 0.02);
        CHECK(calibration->hasCorrectedCompassFrontsight() == false);
        CHECK(calibration->hasCorrectedClinoFrontsight() == false);

        CHECK(calibration->hasBackSights() == true);
        CHECK(calibration->backCompassCalibration() == -0.01);
        CHECK(calibration->backClinoCalibration() == -0.02);
        CHECK(calibration->hasCorrectedCompassBacksight() == true);
        CHECK(calibration->hasCorrectedClinoBacksight() == true);

        REQUIRE(trip->chunkCount() == 1);
        auto chunk = trip->chunk(0);

        struct Station {
            QString stationName;
            QString left;
            QString right;
            QString up;
            QString down;
        };

        QList<Station> stations {
            {"a1", "1", "2", "3", "4"},
            {"a2", {},{},{},{}},
            {"a3", {},{},{},{}},
            {"a4", {},{},{},{}},
            {"a5", {},{},{},{}},
            {"a6", {},{},{},{}}
        };

        REQUIRE(chunk->stationCount() == stations.size());

        auto checkStation = [=](int stationIndex) {
            auto chunkStation = chunk->station(stationIndex);
            auto station = stations.at(stationIndex);
            CHECK(chunkStation.name().toStdString() == station.stationName.toStdString());

            auto checkLRUD = [](auto stateFunc, auto valueFunc, const QString& checkValue) {
                if(!checkValue.isEmpty()) {
                    CHECK(stateFunc() == cwDistanceReading::State::Valid);
                    CHECK(valueFunc().value() == checkValue);
                } else {
                    CHECK(stateFunc() == cwDistanceReading::State::Empty);
                }
            };

            checkLRUD([chunkStation]() { return chunkStation.left().state();},
                      [chunkStation]() { return chunkStation.left(); },
                      station.left
                      );
            checkLRUD([chunkStation]() { return chunkStation.right().state();},
                      [chunkStation]() { return chunkStation.right(); },
                      station.right
                      );
            checkLRUD([chunkStation]() { return chunkStation.up().state();},
                      [chunkStation]() { return chunkStation.up(); },
                      station.up
                      );
            checkLRUD([chunkStation]() { return chunkStation.down().state();},
                      [chunkStation]() { return chunkStation.down(); },
                      station.down
                      );
        };

        struct Clino {

            enum Type {
                Up,
                Down,
                Value
            };

            Type type;
            QString value;
        };

        struct Shot {
            QString distance;
            QString compass;
            QString backCompass;
            Clino clino;
            Clino backClino;
        };

        QList<Shot> shots {
            { "10", {}, {}, {Clino::Down, {}}, {Clino::Up, {}}},
            { "7.3", "50", "50.1", {Clino::Value, "-75"}, {Clino::Value, "-74"}},
            { "4", "220", "219", {Clino::Value, "10"}, {Clino::Value, "11"}},
            { "6", "46", "46.2", {Clino::Value, "-85"}, {Clino::Value, "-84.5"}},
            { "21.5", "35", "34", {Clino::Value, "-78"}, {Clino::Value, "-77"}}
        };

        REQUIRE(chunk->shotCount() == shots.size());

        auto checkShot = [=](int shotIndex) {
            auto chunkShot = chunk->shot(shotIndex);
            auto shot = shots.at(shotIndex);
            CHECK(chunkShot.distance().value().toStdString() == shot.distance.toStdString());
            if(!shot.compass.isEmpty()) {
                CHECK(chunkShot.compass().state() == cwCompassReading::State::Valid);
                CHECK(shot.compass.toStdString() == chunkShot.compass().value().toStdString());
            } else {
                CHECK(chunkShot.compass().state() == cwCompassReading::State::Empty);
            }

            if(!shot.backCompass.isEmpty()) {
                CHECK(chunkShot.backCompass().state() == cwCompassReading::State::Valid);
                CHECK(shot.backCompass.toStdString() == chunkShot.backCompass().value().toStdString());
            } else {
                CHECK(chunkShot.backCompass().state() == cwCompassReading::State::Empty);
            }

            auto checkClino = [](Clino clino, auto clinoStateFunc, auto clinoValueFunc) {
                switch(clino.type) {
                case Clino::Up:
                    CHECK(clinoStateFunc() == cwClinoReading::State::Up);
                    break;
                case Clino::Down:
                    CHECK(clinoStateFunc() == cwClinoReading::State::Down);
                    break;
                case Clino::Value:
                    CHECK(clinoStateFunc() == cwClinoReading::State::Valid);
                    CHECK(clinoValueFunc().value().toStdString() == clino.value.toStdString());
                    break;
                }
            };

            INFO("Front Clino");
            checkClino(shot.clino,
                       [chunkShot]() { return chunkShot.clino().state(); },
            [chunkShot]() { return chunkShot.clino(); }
            );

            INFO("Back clino");
            checkClino(shot.backClino,
                       [chunkShot]() { return chunkShot.backClino().state(); },
            [chunkShot]() { return chunkShot.backClino(); }
            );

        };

        for(int i = 0; i < chunk->shotCount(); i++) {
            INFO("Shot i:" << i);
            checkShot(i);
        }

        REQUIRE(trip->team()->teamMembers().size() == 1);
        CHECK(trip->team()->teamMembers().at(0).name().toStdString() == "Sauce Man");
        CHECK(trip->team()->teamMembers().at(0).jobs() == QStringList({"Super", "Duper"}));

        REQUIRE(trip->notes()->notes().size() == 1);
        auto note = trip->notes()->notes().at(0);

        REQUIRE(note->scraps().size() == 1);
        auto scrap = note->scraps().at(0);
        scrapCheckFunc(scrap);

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

        root->project()->save();
        root->futureManagerModel()->waitForFinished();

        CHECK(root->project()->canSaveDirectly() == true);

        return root->project()->filename();
    };

    auto originalScrapCheck = [](cwScrap* scrap) {
        CHECK(scrap->type() == cwScrap::RunningProfile);
        CHECK(dynamic_cast<cwRunningProfileScrapViewMatrix*>(scrap->viewMatrix()) != nullptr);
        CHECK(scrap->stations().size() == 6);
        CHECK(scrap->polygon().size() > 0);

        scrap->setType(cwScrap::ProjectedProfile);
        CHECK(scrap->type() == cwScrap::ProjectedProfile);
        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()) != nullptr);

        cwProjectedProfileScrapViewMatrix* profileView = static_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
        profileView->setAzimuth(225);
        profileView->setDirection(cwProjectedProfileScrapViewMatrix::RightToLeft);

        CHECK(profileView->azimuth() == 225);
        CHECK(profileView->direction() == cwProjectedProfileScrapViewMatrix::RightToLeft);
    };

    auto profileCheck = [](cwScrap* scrap) {
        CHECK(scrap->type() == cwScrap::ProjectedProfile);
        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix()) != nullptr);
        CHECK(scrap->stations().size() == 6);
        CHECK(scrap->polygon().size() > 0);

        cwProjectedProfileScrapViewMatrix* profileView = static_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
        CHECK(profileView->azimuth() == 225);
        CHECK(profileView->direction() == cwProjectedProfileScrapViewMatrix::RightToLeft);

    };

    auto newFilename = fileCheck("://datasets/test_ProtoBufferSaveLoad/ProjectProfile-test-v3.cw", originalScrapCheck);

    fileCheck(newFilename, profileCheck);
}


