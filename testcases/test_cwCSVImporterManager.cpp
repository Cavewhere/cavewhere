//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "cwCSVImporterManager.h"
#include "cwCSVImporterTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwCSVLineModel.h"

constexpr double delta = 0.001;
using Catch::Matchers::WithinAbs;

class Checker {
public:

    static void check(const cwShot& testShot, const cwShot& shot) {
        CHECK_THAT(testShot.distance(), WithinAbs(shot.distance(), delta));
        CHECK_THAT(testShot.compass(), WithinAbs(shot.compass(), delta));
        CHECK_THAT(testShot.backCompass(), WithinAbs(shot.backCompass(), delta));
        CHECK_THAT(testShot.clino(), WithinAbs(shot.clino(), delta));
        CHECK_THAT(testShot.backClino(), WithinAbs(shot.backClino(), delta));

        CHECK(testShot.distanceState() == shot.distanceState());
        CHECK(testShot.compassState() == shot.compassState());
        CHECK(testShot.backCompassState() == shot.backCompassState());
        CHECK(testShot.clinoState() == shot.clinoState());
        CHECK(testShot.backClinoState() == shot.backClinoState());
    }

    static void check(const cwStation& testStation, const cwStation& station) {
        CHECK(testStation.name().toStdString() == station.name().toStdString());
        CHECK_THAT(testStation.left(), WithinAbs(station.left(), delta));
        CHECK_THAT(testStation.right(), WithinAbs(station.right(), delta));
        CHECK_THAT(testStation.up(), WithinAbs(station.up(), delta));
        CHECK_THAT(testStation.down(), WithinAbs(station.down(), delta));

        CHECK(testStation.leftInputState() == station.leftInputState());
        CHECK(testStation.rightInputState() == station.rightInputState());
        CHECK(testStation.upInputState() == station.upInputState());
        CHECK(testStation.downInputState() == station.downInputState());
    }

    static void check(const cwSurveyChunk* testChunk, const cwSurveyChunk* chunk) {
        REQUIRE(testChunk->stationCount() == chunk->stationCount());
        for(int i = 0; i < testChunk->stationCount(); i++) {
            INFO("Station:" << i);
            cwStation testStation = testChunk->station(i);
            cwStation station = chunk->station(i);
            check(testStation, station);
        }

        REQUIRE(testChunk->shotCount() == chunk->shotCount());
        for(int i = 0; i < testChunk->shotCount(); i++) {
            INFO("Shot:" << i);
            cwShot testShot = testChunk->shot(i);
            cwShot shot = chunk->shot(i);
            check(testShot, shot);
        }
    }

    static void check(const cwTrip* testTrip, const cwTrip* trip) {
        REQUIRE(testTrip->chunks().size() == trip->chunks().size());
        CHECK(testTrip->calibrations()->distanceUnit() == trip->calibrations()->distanceUnit());
        for(int i = 0; i < testTrip->chunkCount(); i++) {
            INFO("Chunk:" << i);
            cwSurveyChunk* testChunk = testTrip->chunk(i);
            cwSurveyChunk* chunk = trip->chunk(i);
            check(testChunk, chunk);
        }
    }

    static void check(const cwCave* testCave, const cwCave* cave) {
        REQUIRE(testCave->tripCount() == cave->tripCount());
        for(int i = 0; i < testCave->tripCount(); i++) {
            INFO("Cave:" << i);
            cwTrip* testTrip = testCave->trip(i);
            cwTrip* trip = cave->trip(i);
            check(testTrip, trip);
        }
    }
};

TEST_CASE("cwCSVImporterManager should initilize correctly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    CHECK(manager.filename().toStdString() == "");
    CHECK(manager.seperator().toStdString() == ",");
    CHECK(manager.skipHeaderLines() == 1);
    CHECK(manager.availableColumnsModel() != nullptr);
    CHECK(manager.columnsModel() != nullptr);
    REQUIRE(manager.errorModel() != nullptr);
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
    CHECK(manager.lineModel() != nullptr);
    CHECK(manager.distanceUnit() == cwUnits::Meters);
    CHECK(manager.useFromStationForLRUD() == true);
    CHECK(manager.newTripOnEmptyLines() == false);
}

TEST_CASE("cwCSVImportManager should parse using default values", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.setFilename("://datasets/test_cwCSVImporterManager/defaultColumns.txt");
    manager.waitToFinish();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    chunk1->appendShot(cwStation("1"), cwStation("2"), cwShot("30.4", "20", QString(), "-82", QString()));
    chunk1->appendShot(cwStation("2"), cwStation("3"), cwShot("20.2", "21.1", QString(), "4", QString()));
    chunk1->appendShot(cwStation("3"), cwStation("4"), cwShot("1", "2", QString(), "3", QString()));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();
    chunk2->appendShot(cwStation("2"), cwStation("5"), cwShot("4", "5", QString(), "6", QString()));

    cwTrip* trip = new cwTrip();
    trip->addChunk(chunk1);
    trip->addChunk(chunk2);

    cwCave testCave;
    testCave.addTrip(trip);

    REQUIRE(manager.caves().size() == 1);

    Checker::check(&testCave, manager.caves().first());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);

    CHECK(manager.lineModel()->rowCount() == 5);
    CHECK(manager.lineModel()->columnCount() == 5);

    //Check that the preview lines works correctly
    manager.setPreviewLines(2);
    manager.waitToFinish();

    CHECK(manager.lineModel()->rowCount() == 2); //1 row plus the header row
    CHECK(manager.lineModel()->columnCount() == 5);

    CHECK(manager.previewText().toStdString() == "From,To,Length,Compass,Clino\n1,2,30.4,20,-82\n");
}

TEST_CASE("cwCSVImportManager previewText should works correctly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.setSkipHeaderLines(0);
    manager.setFilename("://datasets/test_cwCSVImporterManager/long.csv");
    manager.waitToFinish();

    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);

    CHECK(manager.lineModel()->rowCount() == 21);
    CHECK(manager.lineModel()->columnCount() == 5);
    CHECK(manager.previewText().split("\n").size() == 21);

    manager.setPreviewLines(cwCSVImporterManager::NoLines);
    manager.waitToFinish();
    CHECK(manager.lineModel()->rowCount() == 1);
    CHECK(manager.lineModel()->columnCount() == 5);
    CHECK(manager.previewText().split("\n").size() == 1);

    manager.setPreviewLines(cwCSVImporterManager::AllLines);
    manager.waitToFinish();
    CHECK(manager.lineModel()->rowCount() == 101);
    CHECK(manager.lineModel()->columnCount() == 5);
    CHECK(manager.previewText().split("\n").size() == 101);

    manager.setPreviewLines(50);
    manager.waitToFinish();
    CHECK(manager.lineModel()->rowCount() == 51);
    CHECK(manager.lineModel()->columnCount() == 5);
    CHECK(manager.previewText().split("\n").size() == 51);

    manager.waitToFinish();

    REQUIRE(manager.caves().size() == 1);
    cwCave* cave = manager.caves().first();
    REQUIRE(cave->trips().size() == 1);
    cwTrip* trip = cave->trips().first();
    REQUIRE(trip->chunks().size() == 1);
    cwSurveyChunk* chunk = trip->chunks().first();

    for(int i = 0; i < chunk->stationCount(); i++) {
        auto station = chunk->station(i);
        CHECK(station.name().toStdString() == QString("%1").arg(i + 1).toStdString());
        CHECK(station.leftInputState() == cwDistanceStates::Empty);
        CHECK(station.rightInputState() == cwDistanceStates::Empty);
        CHECK(station.upInputState() == cwDistanceStates::Empty);
        CHECK(station.downInputState() == cwDistanceStates::Empty);
    }

    for(const auto& shot : chunk->shots()) {
        CHECK(shot.distance() == 100.0);
        CHECK(shot.clino() == 23.0);
        CHECK(shot.compass() == 101.0);
        CHECK(shot.backCompassState() == cwCompassStates::Empty);
        CHECK(shot.backClinoState() == cwClinoStates::Empty);
    }
}

TEST_CASE("cwCSVImportManager should parse with custom columns", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.columnsModel()->clear();
    manager.columnsModel()->append(
    {
                    {"Length", cwCSVImporterTask::Length},
                    {"Compass Backsight", cwCSVImporterTask::CompassBackSight},
                    {"Clino Backsight", cwCSVImporterTask::ClinoBackSight},
                    {"From", cwCSVImporterTask::FromStation},
                    {"To", cwCSVImporterTask::ToStation},
                    {"Compass", cwCSVImporterTask::CompassFrontSight},
                    {"Clino", cwCSVImporterTask::ClinoFrontSight},
                    {"Up", cwCSVImporterTask::Up},
                    {"Down", cwCSVImporterTask::Down},
                    {"Left", cwCSVImporterTask::Left},
                    {"Right", cwCSVImporterTask::Right}
                }
                );
    manager.setFilename("://datasets/test_cwCSVImporterManager/allCustomColumns.txt");
    manager.waitToFinish();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();

    cwStation station1("1");
    station1.setUp("1");
    station1.setDown("2");
    station1.setLeft("3");
    station1.setRight("4");

    cwStation station2("2");
    station2.setUp("11");
    station2.setDown("22");
    station2.setLeft("33");
    station2.setRight("44");

    cwStation station3("3");
    station3.setUp("12");
    station3.setDown("23");
    station3.setLeft("34");
    station3.setRight("45");

    cwStation station4("4");

    cwStation station2to5("2");
    station2to5.setUp("13");
    station2to5.setDown("24");
    station2to5.setLeft("35");
    station2to5.setRight("46");

    cwStation station5("5");

    chunk1->appendShot(station1, station2, cwShot("30.4", "20", "6", "-82", "-2"));
    chunk1->appendShot(station2, station3, cwShot("20.2", "21.1", "7", "4", "-4"));
    chunk1->appendShot(station3, station4, cwShot("1", "2", "8", "3", "-5"));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();

    chunk2->appendShot(station2to5, station5, cwShot("4", "5", "9", "6", "-6"));

    cwTrip* trip = new cwTrip();
    trip->addChunk(chunk1);
    trip->addChunk(chunk2);

    cwCave testCave;
    testCave.addTrip(trip);

    REQUIRE(manager.caves().size() == 1);

    Checker::check(&testCave, manager.caves().first());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
}

TEST_CASE("cwCSVImageManager should skipHeaderLines correctly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.setFilename("://datasets/test_cwCSVImporterManager/skipHeaderLines.txt");
    manager.setSkipHeaderLines(3);
    manager.waitToFinish();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    chunk1->appendShot(cwStation("1"), cwStation("2"), cwShot("30.4", "20", QString(), "-82", QString()));
    chunk1->appendShot(cwStation("2"), cwStation("3"), cwShot("20.2", "21.1", QString(), "4", QString()));
    chunk1->appendShot(cwStation("3"), cwStation("4"), cwShot("1", "2", QString(), "3", QString()));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();
    chunk2->appendShot(cwStation("2"), cwStation("5"), cwShot("4", "5", QString(), "6", QString()));

    cwTrip* trip = new cwTrip();
    trip->addChunk(chunk1);
    trip->addChunk(chunk2);

    cwCave testCave;
    testCave.addTrip(trip);

    REQUIRE(manager.caves().size() == 1);

    Checker::check(&testCave, manager.caves().first());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
}

TEST_CASE("cwCSVImageManager should change seperator correctly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.setFilename("://datasets/test_cwCSVImporterManager/seperator.txt");
    manager.setSeperator("|#");
    manager.waitToFinish();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    chunk1->appendShot(cwStation("1"), cwStation("2"), cwShot("30.4", "20", QString(), "-82", QString()));
    chunk1->appendShot(cwStation("2"), cwStation("3"), cwShot("20.2", "21.1", QString(), "4", QString()));
    chunk1->appendShot(cwStation("3"), cwStation("4"), cwShot("1", "2", QString(), "3", QString()));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();
    chunk2->appendShot(cwStation("2"), cwStation("5"), cwShot("4", "5", QString(), "6", QString()));

    cwTrip* trip = new cwTrip();
    trip->addChunk(chunk1);
    trip->addChunk(chunk2);

    cwCave testCave;
    testCave.addTrip(trip);

    REQUIRE(manager.caves().size() == 1);

    Checker::check(&testCave, manager.caves().first());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
}

TEST_CASE("cwCSVImageManager should set distance units correctly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.setFilename("://datasets/test_cwCSVImporterManager/defaultColumns.txt");
    manager.setDistanceUnit(cwUnits::Feet);
    manager.waitToFinish();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    chunk1->appendShot(cwStation("1"), cwStation("2"), cwShot("30.4", "20", QString(), "-82", QString()));
    chunk1->appendShot(cwStation("2"), cwStation("3"), cwShot("20.2", "21.1", QString(), "4", QString()));
    chunk1->appendShot(cwStation("3"), cwStation("4"), cwShot("1", "2", QString(), "3", QString()));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();
    chunk2->appendShot(cwStation("2"), cwStation("5"), cwShot("4", "5", QString(), "6", QString()));

    cwTrip* trip = new cwTrip();
    trip->calibrations()->setDistanceUnit(cwUnits::Feet);
    trip->addChunk(chunk1);
    trip->addChunk(chunk2);

    cwCave testCave;
    testCave.addTrip(trip);

    REQUIRE(manager.caves().size() == 1);

    Checker::check(&testCave, manager.caves().first());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
}

TEST_CASE("cwCSVImageManager should process last shot LRUD data correctly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.columnsModel()->clear();
    manager.columnsModel()->append(
    {
                    {"Length", cwCSVImporterTask::Length},
                    {"Compass Backsight", cwCSVImporterTask::CompassBackSight},
                    {"Clino Backsight", cwCSVImporterTask::ClinoBackSight},
                    {"From", cwCSVImporterTask::FromStation},
                    {"To", cwCSVImporterTask::ToStation},
                    {"Compass", cwCSVImporterTask::CompassFrontSight},
                    {"Clino", cwCSVImporterTask::ClinoFrontSight},
                    {"Up", cwCSVImporterTask::Up},
                    {"Down", cwCSVImporterTask::Down},
                    {"Left", cwCSVImporterTask::Left},
                    {"Right", cwCSVImporterTask::Right}
                }
                );
    manager.setFilename("://datasets/test_cwCSVImporterManager/lastLRUD.txt");
    manager.waitToFinish();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();

    cwStation station1("1");
    station1.setUp("1");
    station1.setDown("2");
    station1.setLeft("3");
    station1.setRight("4");

    cwStation station2("2");
    station2.setUp("11");
    station2.setDown("22");
    station2.setLeft("33");
    station2.setRight("44");

    cwStation station3("3");
    station3.setUp("12");
    station3.setDown("23");
    station3.setLeft("34");
    station3.setRight("45");

    cwStation station4("4");
    station4.setUp("14");
    station4.setDown("25");
    station4.setRight("47");

    cwStation station2to5("2");
    station2to5.setUp("13");
    station2to5.setDown("24");
    station2to5.setLeft("35");
    station2to5.setRight("46");

    cwStation station5("5");
    station5.setDown("26");
    station5.setRight("48");


    chunk1->appendShot(station1, station2, cwShot("30.4", "20", "6", "-82", "-2"));
    chunk1->appendShot(station2, station3, cwShot("20.2", "21.1", "7", "4", "-4"));
    chunk1->appendShot(station3, station4, cwShot("1", "2", "8", "3", "-5"));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();

    chunk2->appendShot(station2to5, station5, cwShot("4", "5", "9", "6", "-6"));

    cwTrip* trip = new cwTrip();
    trip->addChunk(chunk1);
    trip->addChunk(chunk2);

    cwCave testCave;
    testCave.addTrip(trip);

    REQUIRE(manager.caves().size() == 1);

    Checker::check(&testCave, manager.caves().first());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
}

TEST_CASE("cwCSVImageManager should set UseFromStationForLRUD correctly", "[cwCSVImporterManager]") {
    //Test that there's an extra, empty station that fills in LRUD
    cwCSVImporterManager manager;
    manager.columnsModel()->clear();
    manager.columnsModel()->append(
    {
                    {"Length", cwCSVImporterTask::Length},
                    {"Compass Backsight", cwCSVImporterTask::CompassBackSight},
                    {"Clino Backsight", cwCSVImporterTask::ClinoBackSight},
                    {"From", cwCSVImporterTask::FromStation},
                    {"To", cwCSVImporterTask::ToStation},
                    {"Compass", cwCSVImporterTask::CompassFrontSight},
                    {"Clino", cwCSVImporterTask::ClinoFrontSight},
                    {"Up", cwCSVImporterTask::Up},
                    {"Down", cwCSVImporterTask::Down},
                    {"Left", cwCSVImporterTask::Left},
                    {"Right", cwCSVImporterTask::Right}
                }
                );
    manager.setFilename("://datasets/test_cwCSVImporterManager/lastToLRUD.txt");
    manager.setUseFromStationForLRUD(false);
    manager.waitToFinish();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();

    cwStation station1("1");
    station1.setUp("14");
    station1.setDown("25");
    station1.setRight("47");

    cwStation station2("2");
    station2.setUp("1");
    station2.setDown("2");
    station2.setLeft("3");
    station2.setRight("4");

    cwStation station3("3");
    station3.setUp("11");
    station3.setDown("22");
    station3.setLeft("33");
    station3.setRight("44");

    cwStation station4("4");
    station4.setUp("12");
    station4.setDown("23");
    station4.setLeft("34");
    station4.setRight("45");

    cwStation station2to5("2");
    station2to5.setDown("26");
    station2to5.setRight("48");

    cwStation station5("5");
    station5.setUp("13");
    station5.setDown("24");
    station5.setLeft("35");
    station5.setRight("46");

    chunk1->appendShot(station1, station2, cwShot("30.4", "20", "6", "-82", "-2"));
    chunk1->appendShot(station2, station3, cwShot("20.2", "21.1", "7", "4", "-4"));
    chunk1->appendShot(station3, station4, cwShot("1", "2", "8", "3", "-5"));

    cwSurveyChunk* chunk2 = new cwSurveyChunk();

    chunk2->appendShot(station2to5, station5, cwShot("4", "5", "9", "6", "-6"));

    cwTrip* trip = new cwTrip();
    trip->addChunk(chunk1);
    trip->addChunk(chunk2);

    cwCave testCave;
    testCave.addTrip(trip);

    REQUIRE(manager.caves().size() == 1);

    Checker::check(&testCave, manager.caves().first());
    CHECK(manager.errorModel()->fatalCount() == 0);
    CHECK(manager.errorModel()->warningCount() == 0);
}

TEST_CASE("cwCSVImporterManager should handle new trip on empty lines correctly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;
    manager.setFilename("://datasets/test_cwCSVImporterManager/newTrips.txt");
    manager.waitToFinish();

    SECTION("Default action") {
        CHECK(manager.newTripOnEmptyLines() == false);

        cwSurveyChunk* chunk1 = new cwSurveyChunk();
        chunk1->appendShot(cwStation("1"), cwStation("2"), cwShot("30.4", "20", QString(), "-82", QString()));
        chunk1->appendShot(cwStation("2"), cwStation("3"), cwShot("20.2", "21.1", QString(), "4", QString()));
        chunk1->appendShot(cwStation("3"), cwStation("4"), cwShot("1", "2", QString(), "3", QString()));

        cwSurveyChunk* chunk2 = new cwSurveyChunk();
        chunk2->appendShot(cwStation("2"), cwStation("5"), cwShot("4", "5", QString(), "6", QString()));

        cwTrip* trip = new cwTrip();
        trip->addChunk(chunk1);
        trip->addChunk(chunk2);

        cwCave testCave;
        testCave.addTrip(trip);

        REQUIRE(manager.caves().size() == 1);
        Checker::check(&testCave, manager.caves().first());
    }

    SECTION("New trips on empty lines") {
        manager.setNewTripOnEmptyLines(true);
        manager.waitToFinish();

        CHECK(manager.newTripOnEmptyLines() == true);

        cwSurveyChunk* chunk1 = new cwSurveyChunk();
        chunk1->appendShot(cwStation("1"), cwStation("2"), cwShot("30.4", "20", QString(), "-82", QString()));

        cwSurveyChunk* chunk2 = new cwSurveyChunk();
        chunk2->appendShot(cwStation("2"), cwStation("3"), cwShot("20.2", "21.1", QString(), "4", QString()));

        cwSurveyChunk* chunk3 = new cwSurveyChunk();
        chunk3->appendShot(cwStation("3"), cwStation("4"), cwShot("1", "2", QString(), "3", QString()));

        cwSurveyChunk* chunk4 = new cwSurveyChunk();
        chunk4->appendShot(cwStation("2"), cwStation("5"), cwShot("4", "5", QString(), "6", QString()));

        cwTrip* trip1 = new cwTrip();
        trip1->addChunk(chunk1);

        cwTrip* trip2 = new cwTrip();
        trip2->addChunk(chunk2);

        cwTrip* trip3 = new cwTrip();
        trip3->addChunk(chunk3);
        trip3->addChunk(chunk4);

        cwCave testCave;
        testCave.addTrip(trip1);
        testCave.addTrip(trip2);
        testCave.addTrip(trip3);

        REQUIRE(manager.caves().size() == 1);
        Checker::check(&testCave, manager.caves().first());
    }
}

TEST_CASE("cwCSVImageManager should handle errors properly", "[cwCSVImporterManager]") {
    cwCSVImporterManager manager;

    SECTION("Error for invalid file") {
        manager.setFilename("no-file.csv");
        manager.waitToFinish();
        REQUIRE(manager.errorModel()->fatalCount() == 1);
        CHECK(manager.errorModel()->warningCount() == 0);

        CHECK(manager.errorModel()->errors()->first().type() == cwError::Fatal);
        //This might not be a cross-platform error message
        CHECK(manager.errorModel()->errors()->first().message().toStdString() == "Can't open \"no-file.csv\". The system cannot find the file specified.");
    }

    SECTION("Warning with zero filesize") {
        manager.setFilename("://datasets/test_cwCSVImporterManager/emptyFile.txt");
        manager.waitToFinish();
        CHECK(manager.errorModel()->fatalCount() == 0);
        REQUIRE(manager.errorModel()->warningCount() == 1);

        CHECK(manager.errorModel()->errors()->first().type() == cwError::Warning);
        CHECK(manager.errorModel()->errors()->first().message().toStdString() == "No lines read, file empty?");
    }

    SECTION("Warning with columns miss aligned") {
        manager.setFilename("://datasets/test_cwCSVImporterManager/defaultColumnsColumnMissAlignment.txt");
        manager.waitToFinish();
        CHECK(manager.errorModel()->fatalCount() == 0);
        REQUIRE(manager.errorModel()->warningCount() == 2);

        CHECK(manager.errorModel()->errors()->at(0).type() == cwError::Warning);
        CHECK(manager.errorModel()->errors()->at(0).message().toStdString() == "Looking for 5 columns but found 6 on line 2");

        CHECK(manager.errorModel()->errors()->at(1).type() == cwError::Warning);
        CHECK(manager.errorModel()->errors()->at(1).message().toStdString() == "Looking for 5 columns but found 4 on line 4");
    }

    SECTION("Last LRUD shot wasn't found warning") {
        manager.columnsModel()->clear();
        manager.columnsModel()->append(
        {
                        {"Length", cwCSVImporterTask::Length},
                        {"Compass Backsight", cwCSVImporterTask::CompassBackSight},
                        {"Clino Backsight", cwCSVImporterTask::ClinoBackSight},
                        {"From", cwCSVImporterTask::FromStation},
                        {"To", cwCSVImporterTask::ToStation},
                        {"Compass", cwCSVImporterTask::CompassFrontSight},
                        {"Clino", cwCSVImporterTask::ClinoFrontSight},
                        {"Up", cwCSVImporterTask::Up},
                        {"Down", cwCSVImporterTask::Down},
                        {"Left", cwCSVImporterTask::Left},
                        {"Right", cwCSVImporterTask::Right}
                    }
                    );
        manager.setFilename("://datasets/test_cwCSVImporterManager/lastToLRUDError.txt");
        manager.waitToFinish();

        CHECK(manager.errorModel()->fatalCount() == 0);
        REQUIRE(manager.errorModel()->warningCount() >= 1);

        CHECK(manager.errorModel()->errors()->first().type() == cwError::Warning);
        CHECK(manager.errorModel()->errors()->first().message().toStdString() == "Can't set LRUD data for shot \"5\" because shot \"3\" to \"5\" on line 7.");
    }
}
