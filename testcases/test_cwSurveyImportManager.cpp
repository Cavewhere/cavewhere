//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "LoadProjectHelper.h"

//Our includes
#include "cwSurveyImportManager.h"
#include "cwCavingRegion.h"
#include "cwErrorListModel.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwError.h"
#include "TestHelper.h"

//Qt includes
#include <QSignalSpy>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>

TEST_CASE("cwSurveyImportManager should add compass errors to errorModel", "[cwSurveyImportManager]") {

    auto manager = std::make_unique<cwSurveyImportManager>();
    auto region = std::make_unique<cwCavingRegion>();
    auto errorModel = std::make_unique<cwErrorListModel>();
    auto undoStack = std::make_unique<QUndoStack>();

    manager->setCavingRegion(region.get());
    manager->setErrorModel(errorModel.get());
    manager->setUndoStack(undoStack.get());

    CHECK(errorModel->size() == 0);

    QString datasetFile = copyToTempFolder(testcasesDatasetPath("compass/badFile.dat"));
    manager->importCompassDataFile({QUrl::fromLocalFile(datasetFile)});
    manager->waitForCompassToFinish();

    CHECK(errorModel->size() == 1);
}

namespace {
    struct ImportTestRig {
        std::unique_ptr<cwSurveyImportManager> manager = std::make_unique<cwSurveyImportManager>();
        std::unique_ptr<cwCavingRegion> region = std::make_unique<cwCavingRegion>();
        std::unique_ptr<cwErrorListModel> errorModel = std::make_unique<cwErrorListModel>();
        std::unique_ptr<QUndoStack> undoStack = std::make_unique<QUndoStack>();

        ImportTestRig() {
            manager->setCavingRegion(region.get());
            manager->setErrorModel(errorModel.get());
            manager->setUndoStack(undoStack.get());
        }
    };
}

TEST_CASE("importSurvexToTrip adds chunks into an empty trip", "[cwSurveyImportManager][importSurvexToTrip]") {
    ImportTestRig rig;
    cwTrip trip;

    QSignalSpy chunksInsertedSpy(&trip, &cwTrip::chunksInserted);

    const QUrl url = QUrl::fromLocalFile(testcasesDatasetPath("survex/dakeng.svx"));
    rig.manager->importSurvexToTrip(url, &trip);

    REQUIRE(chunksInsertedSpy.wait(10000));

    CHECK(trip.chunkCount() > 0);
    CHECK(rig.errorModel->size() == 0);

    // dakeng.svx ends with `*calibrate tape 1`. The survex importer
    // negates calibration values (see cwSurvexImporter.cpp:824).
    CHECK(trip.calibrations()->tapeCalibration() == -1.0);
}

TEST_CASE("importSurvexToTrip copies trip metadata (date, team, calibration)", "[cwSurveyImportManager][importSurvexToTrip]") {
    ImportTestRig rig;
    cwTrip trip;

    // Synthesize a .svx with date, team, and calibration metadata.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("metadata.svx"));
    {
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        // Note: each *calibrate line in survex resets the trip's
        // calibration to a fresh object with just that one field, so
        // chained *calibrate lines lose earlier values. We assert on
        // the LAST one only. See cwSurvexImporter::parseCalibrate.
        f.write(
            "*begin metadata\n"
            "*date 2024.01.15\n"
            "*team \"Alice Surveyor\"\n"
            "*team \"Bob Notes\"\n"
            "*calibrate declination 3.5\n"
            "1 2 10.0 90 0\n"
            "2 3 5.0 180 -5\n"
            "*end metadata\n");
    }

    QSignalSpy chunksInsertedSpy(&trip, &cwTrip::chunksInserted);
    rig.manager->importSurvexToTrip(QUrl::fromLocalFile(path), &trip);
    REQUIRE(chunksInsertedSpy.wait(10000));

    CHECK(trip.chunkCount() > 0);
    CHECK(trip.date().date() == QDate(2024, 1, 15));
    CHECK(trip.team()->teamMembers().size() == 2);
    // Survex importer negates calibration values.
    CHECK(trip.calibrations()->declination() == -3.5);
}

TEST_CASE("importSurvexToTrip refuses non-empty trip and posts an error", "[cwSurveyImportManager][importSurvexToTrip]") {
    ImportTestRig rig;
    cwTrip trip;

    auto* preexisting = new cwSurveyChunk();
    trip.addChunk(preexisting);
    REQUIRE(trip.chunkCount() == 1);

    const int errorsBefore = rig.errorModel->size();

    const QUrl url = QUrl::fromLocalFile(testcasesDatasetPath("survex/dakeng.svx"));
    rig.manager->importSurvexToTrip(url, &trip);

    // Rejection is synchronous — no need to spin the event loop.
    CHECK(rig.errorModel->size() == errorsBefore + 1);
    CHECK(rig.errorModel->at(errorsBefore).type() == cwError::Fatal);
    CHECK(trip.chunkCount() == 1);
}

TEST_CASE("importSurvexToTrip flattens chunks across multiple top-level blocks", "[cwSurveyImportManager][importSurvexToTrip]") {
    ImportTestRig rig;
    cwTrip trip;

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("multiblock.svx"));
    {
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(
            "*begin one\n"
            "1 2 10.0 90 0\n"
            "*end one\n"
            "*begin two\n"
            "3 4 12.0 180 -3\n"
            "4 5 7.5 200 1\n"
            "*end two\n");
    }

    QSignalSpy chunksInsertedSpy(&trip, &cwTrip::chunksInserted);
    rig.manager->importSurvexToTrip(QUrl::fromLocalFile(path), &trip);
    REQUIRE(chunksInsertedSpy.wait(10000));

    // Both blocks contribute chunks. Drain remaining queued events
    // just in case the second insertion is still pending.
    QCoreApplication::processEvents();

    CHECK(trip.chunkCount() >= 2);
}
