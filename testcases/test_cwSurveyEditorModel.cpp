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

    QModelIndex idx0 = model.index(1);
    QModelIndex idx1 = model.index(2);

    CHECK(idx0.isValid());
    CHECK(idx1.isValid());

    checkStationDataEmpty(idx0);
    checkShotDataEmpty(idx0);

    checkStationDataEmpty(idx1);

    // Now change the data in the new survey chunk.
    // Update station at index 0.
    newChunk->setData(cwSurveyChunk::StationNameRole, 0, "A1");
    spyChecker[&dataChangedSpy]++;
    CHECK(dataChangedSpy.last().at(0).toModelIndex() == idx0);
    CHECK(dataChangedSpy.last().at(1).toModelIndex() == idx0);
    CHECK(dataChangedSpy.last().at(2).value<QVector<int>>() == QVector<int>({cwSurveyEditorModel::StationNameRole}));


    newChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.0");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::StationRightRole, 0, "2.0");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::StationUpRole, 0, "3.0");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::StationDownRole, 0, "4.0");
    spyChecker[&dataChangedSpy]++;

    // Update shot at index 0.
    newChunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.0");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::ShotCompassRole, 0, "100.0");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "200.0");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::ShotClinoRole, 0, "30.0");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "40.0");
    spyChecker[&dataChangedSpy]++;

    // Update station at index 0.
    newChunk->setData(cwSurveyChunk::StationNameRole, 1, "A2");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::StationLeftRole, 1, "1.1");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::StationRightRole, 1, "2.1");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::StationUpRole, 1, "3.1");
    spyChecker[&dataChangedSpy]++;
    newChunk->setData(cwSurveyChunk::StationDownRole, 1, "4.1");
    spyChecker[&dataChangedSpy]++;

    // Since signals are delivered directly, we can now check that the model reflects the updated data.
    CHECK(model.data(idx0, cwSurveyEditorModel::StationNameRole).toString().toStdString() == "A1");
    CHECK(model.data(idx0, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "1.0");
    CHECK(model.data(idx0, cwSurveyEditorModel::StationRightRole).toString().toStdString() == "2.0");
    CHECK(model.data(idx0, cwSurveyEditorModel::StationUpRole).toString().toStdString() == "3.0");
    CHECK(model.data(idx0, cwSurveyEditorModel::StationDownRole).toString().toStdString() == "4.0");

    CHECK(model.data(idx0, cwSurveyEditorModel::ShotDistanceRole).toString().toStdString() == "10.0");
    CHECK(model.data(idx0, cwSurveyEditorModel::ShotCompassRole).toString().toStdString() == "100.0");
    CHECK(model.data(idx0, cwSurveyEditorModel::ShotBackCompassRole).toString().toStdString() == "200.0");
    CHECK(model.data(idx0, cwSurveyEditorModel::ShotClinoRole).toString().toStdString() == "30.0");
    CHECK(model.data(idx0, cwSurveyEditorModel::ShotBackClinoRole).toString().toStdString() == "40.0");

    CHECK(model.data(idx1, cwSurveyEditorModel::StationNameRole).toString().toStdString() == "A2");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationLeftRole).toString().toStdString() == "1.1");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationRightRole).toString().toStdString() == "2.1");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationUpRole).toString().toStdString() == "3.1");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationDownRole).toString().toStdString() == "4.1");

    // Verify that the expected signals were emitted.
    spyChecker.checkSpies();
}
