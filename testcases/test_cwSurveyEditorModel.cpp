//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwSurveyEditorModel.h"
#include "cwSurveyChunk.h"

//Qt includes
#include <QSignalSpy>

//Test includes
#include "SpyChecker.h"
#include "TestHelper.h"


TEST_CASE("cwSurveyEditorModel new chunk should work correctly", "[cwSurveyEditorModel]") {
    cwTrip trip;
    trip.addNewChunk();

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    REQUIRE(model.rowCount() == 3);

    auto i0 = model.index(0);
    CHECK(i0.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(i0.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());

    CHECK(i0.data(cwSurveyEditorModel::StationVisibleRole).toBool() == false);
    CHECK(i0.data(cwSurveyEditorModel::ShotVisibleRole).toBool() == false);
    CHECK(i0.data(cwSurveyEditorModel::TitleVisibleRole).toBool() == true);

    auto i1 = model.index(1);
    CHECK(!i1.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::StationNameRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::StationRightRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::StationUpRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::StationDownRole).toString().toStdString() == "");

    CHECK(!i1.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotDistanceRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::ShotCompassRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::ShotBackCompassRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::ShotClinoRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::ShotBackClinoRole).toString().toStdString() == "");
    CHECK(i1.data(cwSurveyEditorModel::ShotCalibrationRole).toString().toStdString() == "");

    CHECK(i1.data(cwSurveyEditorModel::StationVisibleRole).toBool() == true);
    CHECK(i1.data(cwSurveyEditorModel::ShotVisibleRole).toBool() == true);
    CHECK(i1.data(cwSurveyEditorModel::TitleVisibleRole).toBool() == false);

    auto i2 = model.index(2);
    CHECK(!i2.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationNameRole).toString().toStdString() == "");
    CHECK(i2.data(cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "");
    CHECK(i2.data(cwSurveyEditorModel::StationRightRole).toString().toStdString() == "");
    CHECK(i2.data(cwSurveyEditorModel::StationUpRole).toString().toStdString() == "");
    CHECK(i2.data(cwSurveyEditorModel::StationDownRole).toString().toStdString() == "");

    CHECK(i2.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::ShotCalibrationRole).isNull());

    CHECK(i2.data(cwSurveyEditorModel::StationVisibleRole).toBool() == true);
    CHECK(i2.data(cwSurveyEditorModel::ShotVisibleRole).toBool() == false);
    CHECK(i2.data(cwSurveyEditorModel::TitleVisibleRole).toBool() == false);
}

TEST_CASE("cwSurveyEditorModel should update when survey data changes", "[cwSurveyEditorModel]") {
    cwTrip trip;
    cwSurveyEditorModel model;

    cwSignalSpy modelResetSpy(&model, &QAbstractItemModel::modelReset);
    cwSignalSpy rowsInsertedSpy(&model, &QAbstractItemModel::rowsInserted);
    cwSignalSpy rowsRemovedSpy(&model, &QAbstractItemModel::rowsRemoved);
    cwSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    modelResetSpy.setObjectName("modelResetSpy");
    rowsInsertedSpy.setObjectName("rowsInsertSpy");
    rowsRemovedSpy.setObjectName("rowsRemovedSpy");
    dataChangedSpy.setObjectName("dataChangedSpy");

    SpyChecker spyChecker({
        { &modelResetSpy, 0 },
        { &rowsInsertedSpy, 0 },
        { &rowsRemovedSpy, 0 },
        { &dataChangedSpy, 0 }
    });

    // Set the trip into the model (this will trigger a complete reset).
    model.setTrip(&trip);

    spyChecker[&modelResetSpy]++;
    spyChecker.checkSpies();
    spyChecker.clearSpyCounts();

    // Add a new survey chunk to the trip.
    trip.addNewChunk();
    spyChecker[&rowsInsertedSpy]++;
    spyChecker.checkSpies();
    spyChecker.clearSpyCounts();

    // The new chunk creates 2 stations (plus a tile row at index 0), so rowCount should be 3.
    REQUIRE(model.rowCount() == 3);

    // Get the new chunk (assumed to be the last one).
    int chunkCount = trip.chunkCount();
    cwSurveyChunk* newChunk = trip.chunk(chunkCount - 1);
    REQUIRE(newChunk != nullptr);

    // Verify that the station data is initially empty.
    auto checkStationDataEmpty = [&](const QModelIndex &index) {
        CHECK(model.data(index, cwSurveyEditorModel::StationNameRole).toString().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationLeftRole).toString().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationRightRole).toString().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationUpRole).toString().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationDownRole).toString().isEmpty());
    };

    auto checkShotDataEmpty = [&](const QModelIndex &index) {
        CHECK(model.data(index, cwSurveyEditorModel::ShotDistanceRole).toString().toStdString() == "");
        CHECK(model.data(index, cwSurveyEditorModel::ShotCompassRole).toString().toStdString() == "");
        CHECK(model.data(index, cwSurveyEditorModel::ShotBackCompassRole).toString().toStdString() == "");
        CHECK(model.data(index, cwSurveyEditorModel::ShotClinoRole).toString().toStdString() == "");
        CHECK(model.data(index, cwSurveyEditorModel::ShotBackClinoRole).toString().toStdString() == "");
        CHECK(model.data(index, cwSurveyEditorModel::ShotCalibrationRole).toString().isEmpty());
    };

    //Checks the surveyEditorModel agaist what it's watching
    auto checkModelData = [&]() {
        const auto tripChunks = trip.chunks();
        int row = 0;
        for(int c = 0; c < tripChunks.size(); c++) {
            const auto& chunk = tripChunks.at(c);
            INFO("Row:" << row);
            INFO("Chunk:" << c << " " << chunk);
            //Check skip rows
            const auto stations = chunk->stations();

            //Check the title row is empty
            auto titleIndex = model.index(row);
            checkStationDataEmpty(titleIndex);
            checkShotDataEmpty(titleIndex);
            CHECK(titleIndex.data(cwSurveyEditorModel::TitleVisibleRole).toBool() == true);
            CHECK(model.data(titleIndex, cwSurveyEditorModel::StationVisibleRole).toBool() == false);
            CHECK(model.data(titleIndex, cwSurveyEditorModel::ShotVisibleRole).toBool() == false);

            row++;

            for(int i = 0; i < stations.size(); i++) {
                INFO("i:" << i);
                auto station = chunk->station(i);

                auto index = model.index(row);
                INFO("modelIndex:" << index);
                CHECK(index.data(cwSurveyEditorModel::TitleVisibleRole).toBool() == false);
                CHECK(model.data(index, cwSurveyEditorModel::StationNameRole).toString().toStdString() == station.name().toStdString());
                CHECK(model.data(index, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == station.left().value().toStdString());
                CHECK(model.data(index, cwSurveyEditorModel::StationRightRole).toString().toStdString() == station.right().value().toStdString());
                CHECK(model.data(index, cwSurveyEditorModel::StationUpRole).toString().toStdString() == station.up().value().toStdString());
                CHECK(model.data(index, cwSurveyEditorModel::StationDownRole).toString().toStdString() == station.down().value().toStdString());
                CHECK(model.data(index, cwSurveyEditorModel::StationVisibleRole).toBool() == true);

                if(i < chunk->shotCount()) {
                    auto shot = chunk->shot(i);
                    CHECK(model.data(index, cwSurveyEditorModel::ShotDistanceRole).toString().toStdString() == shot.distance().value().toStdString());
                    CHECK(model.data(index, cwSurveyEditorModel::ShotCompassRole).toString().toStdString() == shot.compass().value().toStdString());
                    CHECK(model.data(index, cwSurveyEditorModel::ShotBackCompassRole).toString().toStdString() == shot.backCompass().value().toStdString());
                    CHECK(model.data(index, cwSurveyEditorModel::ShotClinoRole).toString().toStdString() == shot.clino().value().toStdString());
                    CHECK(model.data(index, cwSurveyEditorModel::ShotBackClinoRole).toString().toStdString() == shot.backClino().value().toStdString());
                    CHECK(model.data(index, cwSurveyEditorModel::ShotVisibleRole).toBool() == true);
                } else {
                    CHECK(model.data(index, cwSurveyEditorModel::ShotVisibleRole).toBool() == false);
                }

                row++;
            }
        }
    };

    QModelIndex idx1 = model.index(1);
    QModelIndex idx2 = model.index(2);

    CHECK(idx1.isValid());
    CHECK(idx2.isValid());

    checkStationDataEmpty(idx1);
    checkShotDataEmpty(idx1);

    checkStationDataEmpty(idx2);

    auto checkData = [&](const QModelIndex& index, cwSurveyEditorModel::Roles role) {
        spyChecker[&dataChangedSpy]++;
        spyChecker.requireSpies();
        REQUIRE(dataChangedSpy.last().size() == 3);
        CHECK(dataChangedSpy.last().at(0).toModelIndex() == index);
        CHECK(dataChangedSpy.last().at(1).toModelIndex() == index);
        CHECK(dataChangedSpy.last().at(2).value<QVector<int>>() == QVector<int>({role}));
    };

    // Now change the data in the new survey chunk.
    // Update station at index 0.
    // Now change the data in the new survey chunk.
    // Update station at index 0.
    newChunk->setData(cwSurveyChunk::StationNameRole, 0, "A1");
    checkData(idx1, cwSurveyEditorModel::StationNameRole);

    newChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.0");
    checkData(idx1, cwSurveyEditorModel::StationLeftRole);

    newChunk->setData(cwSurveyChunk::StationRightRole, 0, "2.0");
    checkData(idx1, cwSurveyEditorModel::StationRightRole);

    newChunk->setData(cwSurveyChunk::StationUpRole, 0, "3.0");
    checkData(idx1, cwSurveyEditorModel::StationUpRole);

    newChunk->setData(cwSurveyChunk::StationDownRole, 0, "4.0");
    checkData(idx1, cwSurveyEditorModel::StationDownRole);

    // Update shot at index 0.
    newChunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.0");
    checkData(idx1, cwSurveyEditorModel::ShotDistanceRole);

    newChunk->setData(cwSurveyChunk::ShotCompassRole, 0, "100.0");
    checkData(idx1, cwSurveyEditorModel::ShotCompassRole);

    newChunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "200.0");
    checkData(idx1, cwSurveyEditorModel::ShotBackCompassRole);

    newChunk->setData(cwSurveyChunk::ShotClinoRole, 0, "30.0");
    checkData(idx1, cwSurveyEditorModel::ShotClinoRole);

    newChunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "40.0");
    checkData(idx1, cwSurveyEditorModel::ShotBackClinoRole);

    // Update station at index 1.
    newChunk->setData(cwSurveyChunk::StationNameRole, 1, "A2");
    checkData(idx2, cwSurveyEditorModel::StationNameRole);

    newChunk->setData(cwSurveyChunk::StationLeftRole, 1, "1.1");
    checkData(idx2, cwSurveyEditorModel::StationLeftRole);

    newChunk->setData(cwSurveyChunk::StationRightRole, 1, "2.1");
    checkData(idx2, cwSurveyEditorModel::StationRightRole);

    newChunk->setData(cwSurveyChunk::StationUpRole, 1, "3.1");
    checkData(idx2, cwSurveyEditorModel::StationUpRole);

    newChunk->setData(cwSurveyChunk::StationDownRole, 1, "4.1");
    checkData(idx2, cwSurveyEditorModel::StationDownRole);

    // Since signals are delivered directly, we can now check that the model reflects the updated data.
    CHECK(model.data(idx1, cwSurveyEditorModel::StationNameRole).toString().toStdString() == "A1");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "1.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationRightRole).toString().toStdString() == "2.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationUpRole).toString().toStdString() == "3.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationDownRole).toString().toStdString() == "4.0");

    CHECK(model.data(idx1, cwSurveyEditorModel::ShotDistanceRole).toString().toStdString() == "10.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::ShotCompassRole).toString().toStdString() == "100.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::ShotBackCompassRole).toString().toStdString() == "200.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::ShotClinoRole).toString().toStdString() == "30.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::ShotBackClinoRole).toString().toStdString() == "40.0");

    CHECK(model.data(idx2, cwSurveyEditorModel::StationNameRole).toString().toStdString() == "A2");
    CHECK(model.data(idx2, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "1.1");
    CHECK(model.data(idx2, cwSurveyEditorModel::StationRightRole).toString().toStdString() == "2.1");
    CHECK(model.data(idx2, cwSurveyEditorModel::StationUpRole).toString().toStdString() == "3.1");
    CHECK(model.data(idx2, cwSurveyEditorModel::StationDownRole).toString().toStdString() == "4.1");

    // Verify that the expected signals were emitted.
    spyChecker.checkSpies();

    checkModelData();

    SECTION("Add new shot to the first chunk") {
        newChunk->appendNewShot();

        spyChecker[&rowsInsertedSpy]++;
        spyChecker.requireSpies();
        REQUIRE(rowsInsertedSpy.last().size() == 3);
        REQUIRE(rowsInsertedSpy.last().at(0) == QModelIndex());
        REQUIRE(rowsInsertedSpy.last().at(1) == 3);
        REQUIRE(rowsInsertedSpy.last().at(2) == 3);

        CHECK(model.rowCount() == 4);
        auto idx3 = model.index(3);
        checkStationDataEmpty(idx3);
        checkShotDataEmpty(idx3);

        //--- Update the stations new shot and station data
        newChunk->setData(cwSurveyChunk::StationNameRole, 2, "A3");
        checkData(idx3, cwSurveyEditorModel::StationNameRole);

        newChunk->setData(cwSurveyChunk::StationLeftRole, 2, "1.02");
        checkData(idx3, cwSurveyEditorModel::StationLeftRole);

        newChunk->setData(cwSurveyChunk::StationRightRole, 2, "2.02");
        checkData(idx3, cwSurveyEditorModel::StationRightRole);

        newChunk->setData(cwSurveyChunk::StationUpRole, 2, "3.02");
        checkData(idx3, cwSurveyEditorModel::StationUpRole);

        newChunk->setData(cwSurveyChunk::StationDownRole, 2, "4.02");
        checkData(idx3, cwSurveyEditorModel::StationDownRole);

        // Update shot at index 1.
        newChunk->setData(cwSurveyChunk::ShotDistanceRole, 1, "10.02");
        checkData(idx2, cwSurveyEditorModel::ShotDistanceRole);

        newChunk->setData(cwSurveyChunk::ShotCompassRole, 1, "100.02");
        checkData(idx2, cwSurveyEditorModel::ShotCompassRole);

        newChunk->setData(cwSurveyChunk::ShotBackCompassRole, 1, "200.02");
        checkData(idx2, cwSurveyEditorModel::ShotBackCompassRole);

        newChunk->setData(cwSurveyChunk::ShotClinoRole, 1, "30.02");
        checkData(idx2, cwSurveyEditorModel::ShotClinoRole);

        newChunk->setData(cwSurveyChunk::ShotBackClinoRole, 1, "40.02");
        checkData(idx2, cwSurveyEditorModel::ShotBackClinoRole);

        CHECK(model.data(idx3, cwSurveyEditorModel::StationNameRole).toString().toStdString() == "A3");
        CHECK(model.data(idx3, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "1.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::StationRightRole).toString().toStdString() == "2.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::StationUpRole).toString().toStdString() == "3.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::StationDownRole).toString().toStdString() == "4.02");

        CHECK(model.data(idx2, cwSurveyEditorModel::ShotDistanceRole).toString().toStdString() == "10.02");
        CHECK(model.data(idx2, cwSurveyEditorModel::ShotCompassRole).toString().toStdString() == "100.02");
        CHECK(model.data(idx2, cwSurveyEditorModel::ShotBackCompassRole).toString().toStdString() == "200.02");
        CHECK(model.data(idx2, cwSurveyEditorModel::ShotClinoRole).toString().toStdString() == "30.02");
        CHECK(model.data(idx2, cwSurveyEditorModel::ShotBackClinoRole).toString().toStdString() == "40.02");

        checkModelData();

        SECTION("Add another chunk") {
            // Remember the current number of rows before adding a new chunk.
            int prevRowCount = model.rowCount();

            // Add another new survey chunk to the trip.
            trip.addNewChunk();
            spyChecker[&rowsInsertedSpy]++;
            spyChecker.requireSpies();

            // The new chunk creates 2 stations plus a tile row, so it adds 3 rows.
            REQUIRE(model.rowCount() == prevRowCount + 3);

            // Get the newly added chunk (should be the last one in the trip).
            int newChunkCount = trip.chunkCount();
            cwSurveyChunk* secondChunk = trip.chunk(newChunkCount - 1);
            REQUIRE(secondChunk != nullptr);

            // Compute the starting index for the new chunk's rows in the model.
            int startIndex = prevRowCount;
            // The new chunk layout:
            //   Row at startIndex: tile row (unused for data updates).
            //   Row at startIndex+1: station (and shot) data for index 0.
            //   Row at startIndex+2: station data for index 1.
            QModelIndex tileIdx = model.index(startIndex);
            QModelIndex stationIdx1 = model.index(startIndex + 1);
            QModelIndex stationIdx2 = model.index(startIndex + 2);

            // Verify that the station data for the new chunk is initially empty.
            checkStationDataEmpty(stationIdx1);
            checkShotDataEmpty(stationIdx1);
            checkStationDataEmpty(stationIdx2);
            // (No shot row exists for station index 1 by default.)

            //--- Update station 0 data in the new chunk.
            secondChunk->setData(cwSurveyChunk::StationNameRole, 0, "B1");
            checkData(stationIdx1, cwSurveyEditorModel::StationNameRole);

            secondChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationLeftRole);

            secondChunk->setData(cwSurveyChunk::StationRightRole, 0, "2.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationRightRole);

            secondChunk->setData(cwSurveyChunk::StationUpRole, 0, "3.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationUpRole);

            secondChunk->setData(cwSurveyChunk::StationDownRole, 0, "4.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationDownRole);

            // Update shot data for station 0 in the new chunk.
            secondChunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "15.0");
            checkData(stationIdx1, cwSurveyEditorModel::ShotDistanceRole);

            secondChunk->setData(cwSurveyChunk::ShotCompassRole, 0, "150.0");
            checkData(stationIdx1, cwSurveyEditorModel::ShotCompassRole);

            secondChunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "250.0");
            checkData(stationIdx1, cwSurveyEditorModel::ShotBackCompassRole);

            secondChunk->setData(cwSurveyChunk::ShotClinoRole, 0, "35.0");
            checkData(stationIdx1, cwSurveyEditorModel::ShotClinoRole);

            secondChunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "45.0");
            checkData(stationIdx1, cwSurveyEditorModel::ShotBackClinoRole);

            //--- Update station 1 data in the new chunk.
            secondChunk->setData(cwSurveyChunk::StationNameRole, 1, "B2");
            checkData(stationIdx2, cwSurveyEditorModel::StationNameRole);

            secondChunk->setData(cwSurveyChunk::StationLeftRole, 1, "1.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationLeftRole);

            secondChunk->setData(cwSurveyChunk::StationRightRole, 1, "2.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationRightRole);

            secondChunk->setData(cwSurveyChunk::StationUpRole, 1, "3.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationUpRole);

            secondChunk->setData(cwSurveyChunk::StationDownRole, 1, "4.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationDownRole);

            //--- Verify that the model reflects the updated data for the second chunk.
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationNameRole).toString().toStdString() == "B1");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "1.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationRightRole).toString().toStdString() == "2.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationUpRole).toString().toStdString() == "3.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationDownRole).toString().toStdString() == "4.5");

            CHECK(model.data(stationIdx1, cwSurveyEditorModel::ShotDistanceRole).toString().toStdString() == "15.0");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::ShotCompassRole).toString().toStdString() == "150.0");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::ShotBackCompassRole).toString().toStdString() == "250.0");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::ShotClinoRole).toString().toStdString() == "35.0");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::ShotBackClinoRole).toString().toStdString() == "45.0");

            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationNameRole).toString().toStdString() == "B2");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "1.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationRightRole).toString().toStdString() == "2.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationUpRole).toString().toStdString() == "3.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationDownRole).toString().toStdString() == "4.6");

            // Verify that the expected signals were emitted.
            spyChecker.checkSpies();

            checkModelData();

            SECTION("Remove chunk") {
                int prevRowCount = model.rowCount();

                // Add a new chunk; this new chunk creates (stationCount() + 1) rows.
                trip.addNewChunk();
                spyChecker[&rowsInsertedSpy]++;
                spyChecker.requireSpies();

                // For the newly added chunk, compute its row count.
                cwSurveyChunk* addedChunk = trip.chunk(trip.chunkCount() - 1);
                int newChunkRowCount = addedChunk->stationCount() + 1;
                REQUIRE(model.rowCount() == prevRowCount + newChunkRowCount);

                SECTION("Remove the first chunk") {
                    int currentRowCount = model.rowCount();
                    int currentChunkCount = trip.chunkCount();
                    cwSurveyChunk* firstChunk = trip.chunk(0);
                    REQUIRE(firstChunk != nullptr);

                    int chunkRowCount = firstChunk->stationCount() + 1;
                    int firstRow = 0; // first chunk always starts at row 0
                    int lastRow = firstRow + chunkRowCount - 1;

                    trip.removeChunk(firstChunk);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == firstRow);
                    CHECK(params.at(2).toInt() == lastRow);

                    REQUIRE(model.rowCount() == currentRowCount - chunkRowCount);
                    REQUIRE(trip.chunkCount() == currentChunkCount - 1);

                    checkModelData();
                }

                SECTION("Remove the middle chunk") {
                    int currentRowCount = model.rowCount();
                    int currentChunkCount = trip.chunkCount();

                    // Determine the middle chunk's index.
                    int midIndex = 1;
                    cwSurveyChunk* middleChunk = trip.chunk(midIndex);
                    REQUIRE(middleChunk != nullptr);

                    int chunkRowCount = middleChunk->stationCount() + 1;

                    // Calculate the starting row for the middle chunk.
                    int firstRow = 0;
                    for (int i = 0; i < midIndex; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        firstRow += chunk->stationCount() + 1;
                    }
                    int lastRow = firstRow + chunkRowCount - 1;

                    trip.removeChunk(middleChunk);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == firstRow);
                    CHECK(params.at(2).toInt() == lastRow);

                    REQUIRE(model.rowCount() == currentRowCount - chunkRowCount);
                    REQUIRE(trip.chunkCount() == currentChunkCount - 1);

                    checkModelData();
                }

                SECTION("Remove the last chunk") {
                    int currentRowCount = model.rowCount();
                    int currentChunkCount = trip.chunkCount();
                    int lastIndex = currentChunkCount - 1;
                    cwSurveyChunk* lastChunk = trip.chunk(lastIndex);
                    REQUIRE(lastChunk != nullptr);

                    int chunkRowCount = lastChunk->stationCount() + 1;

                    // Calculate the starting row for the last chunk.
                    int firstRow = 0;
                    for (int i = 0; i < lastIndex; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        firstRow += chunk->stationCount() + 1;
                    }
                    int lastRow = firstRow + chunkRowCount - 1;

                    trip.removeChunk(lastChunk);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == firstRow);
                    CHECK(params.at(2).toInt() == lastRow);

                    REQUIRE(model.rowCount() == currentRowCount - chunkRowCount);
                    REQUIRE(trip.chunkCount() == currentChunkCount - 1);

                    checkModelData();
                }

                SECTION("Remove all the chunks") {
                    int currentChunkCount = trip.chunkCount();
                    int totalRows = 0;
                    for (int i = 0; i < currentChunkCount; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        totalRows += chunk->stationCount() + 1;
                    }

                    trip.removeChunks(0, currentChunkCount - 1);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == 0);
                    CHECK(params.at(2).toInt() == totalRows - 1);

                    REQUIRE(trip.chunkCount() == 0);
                    REQUIRE(model.rowCount() == 0);

                    checkModelData();
                }
            }
        }

        SECTION("Remove a station for newChunk") {
            // newChunk should have 3 stations and 2 shots
            // newChunk is assumed to be the first chunk in the trip.
            REQUIRE(trip.chunkCount() >= 1);
            REQUIRE(newChunk == trip.chunk(0));
            REQUIRE(newChunk->stationCount() == 3);
            REQUIRE(newChunk->shotCount() == 2);

            // Helper: Compute the global row for a station in newChunk.
            // Since newChunk is the first chunk, its tile row is at index 0, and
            // the station row for stationIndex i is at row (i + 1).
            auto expectedRow = [](int stationIndex) {
                return stationIndex + 1;
            };

            SECTION("Remove the first station - down") {
                int prevModelRow = model.rowCount();
                int prevStationCount = newChunk->stationCount();
                int prevShotCount = newChunk->shotCount();
                int removedRow = expectedRow(0);  // should be row 1

                newChunk->removeStation(0, cwSurveyChunk::Below);
                spyChecker[&rowsRemovedSpy]++;
                spyChecker.requireSpies();

                auto params = rowsRemovedSpy.last();
                REQUIRE(params.size() == 3);
                // Expect the parent index to be invalid (i.e. QModelIndex())
                CHECK(params.at(0).toModelIndex() == QModelIndex());
                // The removed row should be at the expected row.
                CHECK(params.at(1).toInt() == removedRow);
                CHECK(params.at(2).toInt() == removedRow);

                // Verify the new counts.
                CHECK(newChunk->stationCount() == prevStationCount - 1);
                CHECK(newChunk->shotCount() == prevShotCount - 1);
                CHECK(model.rowCount() == prevModelRow - 1);

                checkModelData();
            }

            SECTION("Remove the second Station") {
                // Two scenarios: removal with 'Above' and with 'Below'.
                SECTION("Direction up") {
                    int prevModelRow = model.rowCount();
                    int prevStationCount = newChunk->stationCount();
                    int prevShotCount = newChunk->shotCount();
                    int stationRow = expectedRow(1);  // station index 1 -> row 2
                    int shotRow = expectedRow(0);

                    newChunk->removeStation(1, cwSurveyChunk::Above);
                    spyChecker[&rowsRemovedSpy] += 2;
                    spyChecker.requireSpies();

                    auto stationRemoveSpyResults = rowsRemovedSpy.at(rowsRemovedSpy.size() - 2);
                    auto shotRemoveSpyResults = rowsRemovedSpy.at(rowsRemovedSpy.size() - 1);

                    REQUIRE(stationRemoveSpyResults.size() == 3);
                    CHECK(stationRemoveSpyResults.at(0).toModelIndex() == QModelIndex());
                    CHECK(stationRemoveSpyResults.at(1).toInt() == stationRow);
                    CHECK(stationRemoveSpyResults.at(2).toInt() == stationRow);

                    REQUIRE(shotRemoveSpyResults.size() == 3);
                    CHECK(shotRemoveSpyResults.at(0).toModelIndex() == QModelIndex());
                    CHECK(shotRemoveSpyResults.at(1).toInt() == stationRow);
                    CHECK(shotRemoveSpyResults.at(2).toInt() == stationRow);

                    CHECK(newChunk->stationCount() == prevStationCount - 1);
                    CHECK(newChunk->shotCount() == prevShotCount - 1);
                    CHECK(model.rowCount() == prevModelRow - 1);

                    checkModelData();
                }

                SECTION("Direction down") {
                    int prevModelRow = model.rowCount();
                    int prevStationCount = newChunk->stationCount();
                    int prevShotCount = newChunk->shotCount();
                    int removedRow = expectedRow(1);  // station index 1 -> row 2

                    newChunk->removeStation(1, cwSurveyChunk::Below);
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto params = rowsRemovedSpy.last();
                    REQUIRE(params.size() == 3);
                    CHECK(params.at(0).toModelIndex() == QModelIndex());
                    CHECK(params.at(1).toInt() == removedRow);
                    CHECK(params.at(2).toInt() == removedRow);

                    CHECK(newChunk->stationCount() == prevStationCount - 1);
                    CHECK(newChunk->shotCount() == prevShotCount - 1);
                    CHECK(model.rowCount() == prevModelRow - 1);

                    checkModelData();
                }
            }

            // SECTION("Remove the last Station - up") {
            //     int prevModelRow = model.rowCount();
            //     int prevStationCount = newChunk->stationCount();
            //     int prevShotCount = newChunk->shotCount();
            //     // For the last station, its index is (stationCount()-1). In the baseline, 3-1 = 2.
            //     int removedRow = expectedRow(newChunk->stationCount() - 1);  // should be row 3

            //     newChunk->removeStation(newChunk->stationCount() - 1, cwSurveyChunk::Above);
            //     spyChecker[&rowsRemovedSpy]++;
            //     spyChecker.requireSpies();

            //     auto params = rowsRemovedSpy.last();
            //     REQUIRE(params.size() == 3);
            //     CHECK(params.at(0).toModelIndex() == QModelIndex());
            //     CHECK(params.at(1).toInt() == removedRow);
            //     CHECK(params.at(2).toInt() == removedRow);

            //     CHECK(newChunk->stationCount() == prevStationCount - 1);
            //     CHECK(newChunk->shotCount() == prevShotCount - 1);
            //     CHECK(model.rowCount() == prevModelRow - 1);

            //     checkModelData();
            // }
        }
    }
}
