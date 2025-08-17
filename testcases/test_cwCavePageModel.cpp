//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>


//Our includes
#include "cwCavePageModel.h"
#include "cwRootData.h"
#include "cwProject.h"
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwSurveyChunk.h"
#include "TestHelper.h"

//Qt includes
#include <QThreadPool>
#include <QSignalSpy>


TEST_CASE("cwCavePageModel Test with Static Data", "[cwCavePageModel]")
{
    // Set up the root data and project
    auto root = std::make_unique<cwRootData>();
    fileToProject(root->project(), ":/datasets/test_cwCavePageModel/cwCavePageModel.cw");
    auto project = root->project();

    // Wait for plot manager and tasks to finish
    auto plotManager = root->linePlotManager();
    plotManager->waitToFinish();

    root->taskManagerModel()->waitForTasks();
    root->futureManagerModel()->waitForFinished();

    // Access the cave
    REQUIRE(project->cavingRegion()->caveCount() >= 1);
    cwCave* cave = project->cavingRegion()->cave(0);
    REQUIRE(cave != nullptr);

    // Create the model and set the cave
    cwCavePageModel model;
    model.setCave(cave);

    // Static data to verify against
    struct TripTestData {
        QString name;
        QString date;
        QString stationNames;
        double distance;
        int errorCount;
    };

    std::vector<TripTestData> expectedDataUnordered = {
        {"Trip 1", "2024-11-12", "A 1-3", 12.0, 3},
        {"Trip 2", "2024-11-11", "A 3-4", 90.0, 0},
        {"Trip 3", "2024-11-10", "B 2-4", 14.0, 0},
        {"Trip 4", "2024-11-09", "C 2-3", 21.0, 4}
    };

    std::vector<TripTestData> expectedData = [&]() {
        std::vector<TripTestData> testData;
        testData.reserve(expectedDataUnordered.size());

        for(int i = 0; i < model.rowCount(); i++) {
            const QModelIndex index = model.index(i, 0);
            const QString tripName = model.data(index, cwCavePageModel::TripNameRole).toString();
            auto iter = std::find_if(expectedDataUnordered.begin(), expectedDataUnordered.end(),
                                     [tripName](const TripTestData& data) {
                                         return data.name == tripName;
                                     });
            REQUIRE(iter != expectedDataUnordered.end());
            testData.push_back(*iter);
        }

        REQUIRE(testData.size() == expectedDataUnordered.size());

        return testData;
    }();

    auto checkModel = [&]() {
        //Wait for the thread pool to settle
        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();
        model.waitForFinished();

        // Verify row count
        REQUIRE(model.rowCount() == expectedData.size());

        // Test each trip's data
        for (int row = 0; row < expectedData.size(); ++row) {
            INFO("Row:" << row);

            const auto& tripData = expectedData[row];
            QModelIndex index = model.index(row, 0);
            CHECK(index.isValid());

            // Check TripNameRole
            QVariant nameVariant = model.data(index, cwCavePageModel::TripNameRole);
            CHECK(nameVariant.toString().toStdString() == tripData.name.toStdString());

            // Check TripDateRole
            QVariant dateVariant = model.data(index, cwCavePageModel::TripDateRole);
            CHECK(dateVariant.toDate().toString("yyyy-MM-dd").toStdString() == tripData.date.toStdString());

            // Check UsedStationsRole
            QVariant usedStationsVariant = model.data(index, cwCavePageModel::UsedStationsRole);
            CHECK(usedStationsVariant.toString().toStdString() == tripData.stationNames.toStdString());

            // Check TripDistanceRole
            QVariant tripDistanceVariant = model.data(index, cwCavePageModel::TripDistanceRole);
            CHECK(tripDistanceVariant.toReal() == Catch::Approx(tripData.distance).margin(0.01));

            // Check ErrorCountRole
            QVariant errorCountVariant = model.data(index, cwCavePageModel::ErrorCountRole);
            CHECK(errorCountVariant.toInt() == tripData.errorCount);
        }
    };

    // qDebug().noquote() << "Cave error model:" << cave->errorModel()->toStringList().join('\n');
    checkModel();

    // Make sure updates work correctly, use signal spy to check for dataChanges
    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto testCave = project->cavingRegion()->cave(0);

    QSignalSpy dataChangedSpy(&model, &cwCavePageModel::dataChanged);

    auto roleName = [](int role) {
        const QMetaObject metaObj = cwCavePageModel::staticMetaObject;
        int enumIndex = metaObj.indexOfEnumerator("Roles");
        QMetaEnum metaEnum = metaObj.enumerator(enumIndex);
        return metaEnum.valueToKey(role);
    };

    auto checkRoles = [roleName](const QVector<int>& actualRoles, const QVector<int>& expectedRoles) {
        for (int i = 0; i < actualRoles.size(); ++i) {
            INFO("Actual role: " << roleName(actualRoles[i])
                 << " (" << actualRoles[i] << "), "
                 << "Expected role: " << roleName(expectedRoles[i])
                 << " (" << expectedRoles[i] << ")");
            REQUIRE(actualRoles[i] == expectedRoles[i]);
        }
    };

    auto checkDataChangedSpy = [&](int expectedRow, QVector<int> expectedRoles) {
        INFO("Spy roles:" << expectedRoles << expectedRow);
        REQUIRE(dataChangedSpy.count() > 0);
        auto arguments = dataChangedSpy.takeFirst();
        REQUIRE(arguments.size() == 3);
        REQUIRE(arguments[0].value<QModelIndex>() == model.index(expectedRow, 0));
        checkRoles(arguments[2].value<QVector<int>>(), expectedRoles);
        // dataChangedSpy.clear();
    };

    SECTION("Update underlying data") {
        REQUIRE(testCave->tripCount() == 4);

        auto trips = testCave->trips();
        auto tripIter = std::find_if(trips.begin(), trips.end(),
                                 [](const cwTrip* trip) {
                                     return trip->name() == QStringLiteral("Trip 2");
                                 });

        auto iter = std::find_if(expectedData.begin(), expectedData.end(),
                                     [](const auto& tripData) {
                                         return tripData.name == QStringLiteral("Trip 2");
                                     });


        auto trip2 = *tripIter; //testCave->trip(1);

        auto index = std::distance(expectedData.begin(), iter);

        CHECK(trip2->name().toStdString() == "Trip 2");
        CHECK(expectedData[index].name.toStdString() == "Trip 2");

        //Update name
        trip2->setName("New trip 2");
        expectedData[index].name = "New trip 2";
        checkModel();
        checkDataChangedSpy(index, {cwCavePageModel::TripNameRole});

        //Update date
        trip2->setDate(QDateTime(QDate(2025, 1, 2), QTime()));
        expectedData[index].date = "2025-01-02";
        checkModel();
        checkDataChangedSpy(index, {cwCavePageModel::TripDateRole});

        dataChangedSpy.clear();

        //Update shot data and length
        REQUIRE(trip2->chunkCount() == 1);
        auto chunk = trip2->chunk(0);
        chunk->setData(cwSurveyChunk::StationNameRole, 1, "A5");
        expectedData[index].stationNames = "A 5-5";
        checkModel();
        checkDataChangedSpy(index, {cwCavePageModel::TripDistanceRole}); //This one shouldn't really be emitted
        checkDataChangedSpy(index, {cwCavePageModel::UsedStationsRole});
        CHECK(dataChangedSpy.size() == 0);

        //Update the length
        chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, 80);
        expectedData[index].distance = 80;
        checkModel();
        checkDataChangedSpy(index, {cwCavePageModel::TripDistanceRole});
        CHECK(dataChangedSpy.size() == 0);

        //Change error count
        chunk->setData(cwSurveyChunk::StationLeftRole, 0, "");
        expectedData[index].errorCount = 1;
        checkModel();
        checkDataChangedSpy(index, {cwCavePageModel::TripDistanceRole}); //This one shouldn't really be emitted
        checkDataChangedSpy(index, {cwCavePageModel::ErrorCountRole});
    }

    // Test case for adding a new trip
    SECTION("Add trip") {
        REQUIRE(testCave->tripCount() == 4);

        // Set up signal spy for rowsInserted
        QSignalSpy rowsInsertedSpy(&model, &cwCavePageModel::rowsInserted);

        // Create and configure the new trip
        cwTrip* newTrip = new cwTrip();
        newTrip->setName("New Trip");
        newTrip->setDate(QDateTime(QDate(2025, 1, 2), QTime()));
        newTrip->addNewChunk();
        auto chunk = newTrip->chunk(0);
        chunk->setData(cwSurveyChunk::StationNameRole, 0, "A1");
        chunk->setData(cwSurveyChunk::StationNameRole, 1, "E1");
        chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, 40);
        chunk->setData(cwSurveyChunk::ShotCompassRole, 0, 15);
        chunk->setData(cwSurveyChunk::ShotClinoRole, 0, -23);

        // Add the new trip to the cave
        testCave->addTrip(newTrip);

        // Update expected data
        expectedData.push_back({"New Trip", "2025-01-02", "A 1", 40, 10});

        // Check the rowsInserted signal
        REQUIRE(rowsInsertedSpy.count() == 1);  // Ensure the signal was emitted once
        auto args = rowsInsertedSpy.takeFirst(); // Get the first (and only) emission
        REQUIRE(args.size() == 3); // Ensure three arguments in signal

        int first = args[1].toInt();
        int last = args[2].toInt();
        REQUIRE(first == testCave->tripCount() - 1);
        REQUIRE(last == testCave->tripCount() - 1);

        checkModel();
    }

    // Test case for removing an existing trip
    SECTION("Remove trip") {
        REQUIRE(testCave->tripCount() == 4);

        // Set up signal spy for rowsRemoved
        QSignalSpy rowsRemovedSpy(&model, &cwCavePageModel::rowsRemoved);

        // Remove the last trip in the list
        testCave->removeTrip(3);

        // Update expected data
        expectedData.erase(expectedData.begin() + 3);

        // Check the rowsRemoved signal
        REQUIRE(rowsRemovedSpy.count() == 1);  // Ensure the signal was emitted once
        auto args = rowsRemovedSpy.takeFirst(); // Get the first (and only) emission
        REQUIRE(args.size() == 3); // Ensure three arguments in signal

        int first = args[1].toInt();
        int last = args[2].toInt();
        REQUIRE(first == 3);
        REQUIRE(last == 3);

        checkModel();
    }
}
