//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwSurveyEditorModel.h"
#include "cwSurveyChunk.h"
#include "cwSurveyEditorBoxData.h"
#include "cwErrorListModel.h"

//Qt includes
#include <QSignalSpy>

//Test includes
#include "SpyChecker.h"
#include "TestHelper.h"


TEST_CASE("cwSurveyEditorModel new chunk should work correctly", "[cwSurveyEditorModel]") {
    cwTrip trip;
    trip.addNewChunk();

    REQUIRE(trip.chunkCount() > 0);
    auto firstChunk = trip.chunk(0);

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    REQUIRE(model.rowCount() == 4);

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
    CHECK(i0.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i0.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::TitleRow);
    CHECK(i0.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == -1);

    auto i1 = model.index(1); //Station 1
    CHECK(!i1.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(!i1.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i1.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(i1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == 0);

    auto i2 = model.index(2); //Shot
    CHECK(i2.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(!i2.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i2.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::ShotCalibrationRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(i2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == 0);

    auto i3 = model.index(3); //Station
    CHECK(!i3.data(cwSurveyEditorModel::StationNameRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationLeftRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationRightRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationUpRole).isNull());
    CHECK(!i3.data(cwSurveyEditorModel::StationDownRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    CHECK(i3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk() == firstChunk);
    CHECK(i3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(i3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().indexInChunk() == 1);

    CHECK(i3.data(cwSurveyEditorModel::ShotDistanceRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotCompassRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotBackCompassRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotClinoRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotBackClinoRole).isNull());
    CHECK(i3.data(cwSurveyEditorModel::ShotCalibrationRole).isNull());
}

TEST_CASE("cwSurveyEditorModel box data stays correct after appendNewShot", "[cwSurveyEditorModel]") {
    cwTrip trip;
    trip.addNewChunk();
    cwSurveyChunk* chunk = trip.chunk(0);

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    auto setStationData = [&](int index, const QString& name, const QString& left, const QString& right, const QString& up, const QString& down) {
        chunk->setData(cwSurveyChunk::StationNameRole, index, name);
        chunk->setData(cwSurveyChunk::StationLeftRole, index, left);
        chunk->setData(cwSurveyChunk::StationRightRole, index, right);
        chunk->setData(cwSurveyChunk::StationUpRole, index, up);
        chunk->setData(cwSurveyChunk::StationDownRole, index, down);
    };

    auto setShotData = [&](int index, const QString& distance, const QString& compass, const QString& backCompass, const QString& clino, const QString& backClino) {
        chunk->setData(cwSurveyChunk::ShotDistanceRole, index, distance);
        chunk->setData(cwSurveyChunk::ShotCompassRole, index, compass);
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, index, backCompass);
        chunk->setData(cwSurveyChunk::ShotClinoRole, index, clino);
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, index, backClino);
    };

    setStationData(0, "A0", "1.0", "2.0", "3.0", "4.0");
    setStationData(1, "A1", "1.1", "2.1", "3.1", "4.1");
    setShotData(0, "10.0", "100.0", "200.0", "30.0", "40.0");

    chunk->appendNewShot();
    setStationData(2, "A2", "1.2", "2.2", "3.2", "4.2");
    setShotData(1, "20.0", "110.0", "210.0", "31.0", "41.0");

    int previousRowCount = model.rowCount();
    chunk->appendNewShot();

    REQUIRE(model.rowCount() == previousRowCount + 2);

    auto findStationRow = [&](const QString& name) {
        for(int row = 0; row < model.rowCount(); ++row) {
            auto index = model.index(row);
            auto stationData = index.data(cwSurveyEditorModel::StationNameRole);
            if(stationData.isNull()) {
                continue;
            }
            const auto boxData = stationData.value<cwSurveyEditorBoxData>();
            if(boxData.reading().value() == name) {
                return index;
            }
        }
        return QModelIndex();
    };

    auto a0Row = findStationRow("A0");
    auto a1Row = findStationRow("A1");
    auto a2Row = findStationRow("A2");

    REQUIRE(a0Row.isValid());
    REQUIRE(a1Row.isValid());
    REQUIRE(a2Row.isValid());

    CHECK(a0Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 0);
    CHECK(a1Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 1);
    CHECK(a2Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 2);

    auto newShotRow = model.index(model.rowCount() - 2);
    auto newStationRow = model.index(model.rowCount() - 1);

    auto newShotRowIndex = newShotRow.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();
    auto newStationRowIndex = newStationRow.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();

    CHECK(newShotRowIndex.rowType() == cwSurveyEditorRowIndex::ShotRow);
    CHECK(newShotRowIndex.indexInChunk() == 2);
    CHECK(newShotRow.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());

    CHECK(newStationRowIndex.rowType() == cwSurveyEditorRowIndex::StationRow);
    CHECK(newStationRowIndex.indexInChunk() == 3);
    CHECK(newStationRow.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
}

TEST_CASE("cwSurveyEditorModel updates box indices after insertStation", "[cwSurveyEditorModel]") {
    cwTrip trip;
    trip.addNewChunk();
    cwSurveyChunk* chunk = trip.chunk(0);

    cwSurveyEditorModel model;
    model.setTrip(&trip);

    auto setStationData = [&](int index, const QString& name, const QString& left, const QString& right, const QString& up, const QString& down) {
        chunk->setData(cwSurveyChunk::StationNameRole, index, name);
        chunk->setData(cwSurveyChunk::StationLeftRole, index, left);
        chunk->setData(cwSurveyChunk::StationRightRole, index, right);
        chunk->setData(cwSurveyChunk::StationUpRole, index, up);
        chunk->setData(cwSurveyChunk::StationDownRole, index, down);
    };

    auto setShotData = [&](int index, const QString& distance, const QString& compass, const QString& backCompass, const QString& clino, const QString& backClino) {
        chunk->setData(cwSurveyChunk::ShotDistanceRole, index, distance);
        chunk->setData(cwSurveyChunk::ShotCompassRole, index, compass);
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, index, backCompass);
        chunk->setData(cwSurveyChunk::ShotClinoRole, index, clino);
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, index, backClino);
    };

    setStationData(0, "A0", "1.0", "2.0", "3.0", "4.0");
    setStationData(1, "A1", "1.1", "2.1", "3.1", "4.1");
    setShotData(0, "10.0", "100.0", "200.0", "30.0", "40.0");

    chunk->appendNewShot();
    setStationData(2, "A2", "1.2", "2.2", "3.2", "4.2");
    setShotData(1, "20.0", "110.0", "210.0", "31.0", "41.0");

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    dataChangedSpy.clear();

    int previousRowCount = model.rowCount();
    chunk->insertStation(1, cwSurveyChunk::Above);

    REQUIRE(model.rowCount() == previousRowCount + 2);

    auto findStationRow = [&](const QString& name) {
        for(int row = 0; row < model.rowCount(); ++row) {
            auto index = model.index(row);
            auto stationData = index.data(cwSurveyEditorModel::StationNameRole);
            if(stationData.isNull()) {
                continue;
            }
            const auto boxData = stationData.value<cwSurveyEditorBoxData>();
            if(boxData.reading().value() == name) {
                return index;
            }
        }
        return QModelIndex();
    };

    auto findShotRow = [&](const QString& distance) {
        for(int row = 0; row < model.rowCount(); ++row) {
            auto index = model.index(row);
            auto shotData = index.data(cwSurveyEditorModel::ShotDistanceRole);
            if(shotData.isNull()) {
                continue;
            }
            const auto boxData = shotData.value<cwSurveyEditorBoxData>();
            if(boxData.reading().value() == distance) {
                return index;
            }
        }
        return QModelIndex();
    };

    auto a1Row = findStationRow("A1");
    auto a2Row = findStationRow("A2");
    auto shot10Row = findShotRow("10.0");
    auto shot20Row = findShotRow("20.0");

    REQUIRE(a1Row.isValid());
    REQUIRE(a2Row.isValid());
    REQUIRE(shot10Row.isValid());
    REQUIRE(shot20Row.isValid());

    CHECK(a1Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 2);
    CHECK(a2Row.data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 3);
    CHECK(shot10Row.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 0);
    CHECK(shot20Row.data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().rowIndex().indexInChunk() == 2);

    auto dataChangedCovers = [&](int row, cwSurveyEditorModel::Role role) {
        for(const auto& call : dataChangedSpy) {
            const auto topLeft = call.at(0).toModelIndex();
            const auto bottomRight = call.at(1).toModelIndex();
            const auto roles = call.at(2).value<QVector<int>>();
            if(row >= topLeft.row() && row <= bottomRight.row()
                && (roles.contains(role) || roles.isEmpty()))
            {
                return true;
            }
        }
        return false;
    };

    CHECK(dataChangedCovers(a1Row.row(), cwSurveyEditorModel::StationNameRole));
    CHECK(dataChangedCovers(a2Row.row(), cwSurveyEditorModel::StationNameRole));
    CHECK(dataChangedCovers(shot20Row.row(), cwSurveyEditorModel::ShotDistanceRole));
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

    // The new chunk creates 2 stations (plus a tile row at index 0), so rowCount should be 4.
    REQUIRE(model.rowCount() == 4);

    // Get the new chunk (assumed to be the last one).
    int chunkCount = trip.chunkCount();
    cwSurveyChunk* newChunk = trip.chunk(chunkCount - 1);
    REQUIRE(newChunk != nullptr);

    // Verify that the station data is initially empty.
    auto checkStationDataEmpty = [&](const QModelIndex &index) {
        CHECK(model.data(index, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
    };

    auto checkShotDataEmpty = [&](const QModelIndex &index) {
        CHECK(model.data(index, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
        CHECK(model.data(index, cwSurveyEditorModel::ShotCalibrationRole).value<cwSurveyEditorBoxData>().reading().value().isEmpty());
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
            const auto shots = chunk->shots();

            //Check the title row is empty
            auto titleIndex = model.index(row);
            checkStationDataEmpty(titleIndex);
            checkShotDataEmpty(titleIndex);
            CHECK(titleIndex.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::TitleRow);
            // CHECK(titleIndex.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::TitleRow);

            row++;

            REQUIRE(stations.size() == shots.size() + 1);

            for(int i = 0; i < stations.size(); i++) {
                INFO("station i:" << i);
                auto station = chunk->station(i);

                auto index = model.index(row);
                INFO("modelIndex:" << index);
                // CHECK(index.data(cwSurveyEditorModel::TitleVisibleRole).toBool() == false);                
                auto rowIndex = index.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();
                CHECK(rowIndex.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(rowIndex.chunk() == chunk);
                CHECK(rowIndex.indexInChunk() == i);

                const auto stationNameData = model.data(index, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>();
                CHECK(stationNameData.reading().value().toStdString() == station.name().toStdString());
                CHECK(stationNameData.chunkDataRole() == cwSurveyChunk::StationNameRole);
                CHECK(stationNameData.rowIndex() == rowIndex);

                const auto stationLeftData = model.data(index, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>();
                CHECK(stationLeftData.reading().value().toStdString() == station.left().value().toStdString());
                CHECK(stationLeftData.chunkDataRole() == cwSurveyChunk::StationLeftRole);
                CHECK(stationLeftData.rowIndex() == rowIndex);

                const auto stationRightData = model.data(index, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>();
                CHECK(stationRightData.reading().value().toStdString() == station.right().value().toStdString());
                CHECK(stationRightData.chunkDataRole() == cwSurveyChunk::StationRightRole);
                CHECK(stationRightData.rowIndex() == rowIndex);

                const auto stationUpData = model.data(index, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>();
                CHECK(stationUpData.reading().value().toStdString() == station.up().value().toStdString());
                CHECK(stationUpData.chunkDataRole() == cwSurveyChunk::StationUpRole);
                CHECK(stationUpData.rowIndex() == rowIndex);

                const auto stationDownData = model.data(index, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>();
                CHECK(stationDownData.reading().value().toStdString() == station.down().value().toStdString());
                CHECK(stationDownData.chunkDataRole() == cwSurveyChunk::StationDownRole);
                CHECK(stationDownData.rowIndex() == rowIndex);

                row++;

                index = model.index(row);
                INFO("modelIndex:" << index);

                if(i < chunk->shotCount()) {
                    auto shot = chunk->shot(i);

                    auto rowIndex = index.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>();
                    CHECK(rowIndex.rowType() == cwSurveyEditorRowIndex::ShotRow);
                    CHECK(rowIndex.chunk() == chunk);
                    CHECK(rowIndex.indexInChunk() == i);

                    const auto distance = model.data(index, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>();
                    const auto compass = model.data(index, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>();
                    const auto backCompass = model.data(index, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>();
                    const auto clino = model.data(index, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>();
                    const auto backClino = model.data(index, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>();

                    CHECK(distance.reading().value().toStdString() == shot.distance().value().toStdString());
                    CHECK(compass.reading().value().toStdString() == shot.compass().value().toStdString());
                    CHECK(backCompass.reading().value().toStdString() == shot.backCompass().value().toStdString());
                    CHECK(clino.reading().value().toStdString() == shot.clino().value().toStdString());
                    CHECK(backClino.reading().value().toStdString() == shot.backClino().value().toStdString());

                    CHECK(distance.chunkDataRole() == cwSurveyChunk::ShotDistanceRole);
                    CHECK(compass.chunkDataRole() == cwSurveyChunk::ShotCompassRole);
                    CHECK(backCompass.chunkDataRole() == cwSurveyChunk::ShotBackCompassRole);
                    CHECK(clino.chunkDataRole() == cwSurveyChunk::ShotClinoRole);
                    CHECK(backClino.chunkDataRole() == cwSurveyChunk::ShotBackClinoRole);

                    CHECK(distance.rowIndex() == rowIndex);
                    CHECK(compass.rowIndex() == rowIndex);
                    CHECK(backCompass.rowIndex() == rowIndex);
                    CHECK(clino.rowIndex() == rowIndex);
                    CHECK(backClino.rowIndex() == rowIndex);

                    // CHECK(index.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::ShotRow);
                    row++;
                }
            }
        }

        CHECK(row == model.rowCount());
    };

    QModelIndex idx1 = model.index(1);
    QModelIndex idx2 = model.index(2);
    QModelIndex idx3 = model.index(3);

    CHECK(idx1.isValid());
    CHECK(idx2.isValid());
    CHECK(idx3.isValid());

    checkStationDataEmpty(idx1);
    checkShotDataEmpty(idx2);
    checkStationDataEmpty(idx1);

    auto checkData = [&](const QModelIndex& index, cwSurveyEditorModel::Role role, int errorCountChanged = 0) {
        spyChecker[&dataChangedSpy] += errorCountChanged;
        spyChecker[&dataChangedSpy]++;

        spyChecker.requireSpies();
        REQUIRE(dataChangedSpy.last().size() == 3);
        // qDebug() << "dataChange0:" << dataChangedSpy.last().at(0).toModelIndex() << index;
        CHECK(dataChangedSpy.first().at(0).toModelIndex() == index);
        // qDebug() << "dataChange1:" << dataChangedSpy.last().at(1).toModelIndex() << index;
        CHECK(dataChangedSpy.first().at(1).toModelIndex() == index);
        CHECK(dataChangedSpy.first().at(2).value<QVector<int>>() == QVector<int>({role}));

        spyChecker.clearSpyCounts();
    };

    // Now change the data in the new survey chunk.
    // Update station at index 0.
    // Now change the data in the new survey chunk.
    // Update station at index 0.
    newChunk->setData(cwSurveyChunk::StationNameRole, 0, "A1");
    checkData(idx1, cwSurveyEditorModel::StationNameRole, 4); //Added 4 LRUD warning

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
    checkData(idx2, cwSurveyEditorModel::ShotDistanceRole, 1); //Added a fatel error to station

    newChunk->setData(cwSurveyChunk::ShotCompassRole, 0, "100.0");
    checkData(idx2, cwSurveyEditorModel::ShotCompassRole);

    newChunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "200.0");
    checkData(idx2, cwSurveyEditorModel::ShotBackCompassRole, 1); //Adds a warning because more than 2deg off compass

    newChunk->setData(cwSurveyChunk::ShotClinoRole, 0, "30.0");
    checkData(idx2, cwSurveyEditorModel::ShotClinoRole);

    newChunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "40.0"); //Adds a warning because more than 2deg off clino
    checkData(idx2, cwSurveyEditorModel::ShotBackClinoRole, 1);

    // Update station at index 1.
    newChunk->setData(cwSurveyChunk::StationNameRole, 1, "A2");
    checkData(idx3, cwSurveyEditorModel::StationNameRole, 4); //Adds warning for empty LRUDs, removes 1 warning from the station name

    newChunk->setData(cwSurveyChunk::StationLeftRole, 1, "1.1");
    checkData(idx3, cwSurveyEditorModel::StationLeftRole);

    newChunk->setData(cwSurveyChunk::StationRightRole, 1, "2.1");
    checkData(idx3, cwSurveyEditorModel::StationRightRole);

    newChunk->setData(cwSurveyChunk::StationUpRole, 1, "3.1");
    checkData(idx3, cwSurveyEditorModel::StationUpRole);

    newChunk->setData(cwSurveyChunk::StationDownRole, 1, "4.1");
    checkData(idx3, cwSurveyEditorModel::StationDownRole);

    // Since signals are delivered directly, we can now check that the model reflects the updated data.
    CHECK(model.data(idx1, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "A1");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.0");
    CHECK(model.data(idx1, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.0");

    CHECK(model.data(idx2, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "10.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "100.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "200.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "30.0");
    CHECK(model.data(idx2, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "40.0");

    CHECK(model.data(idx3, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "A2");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.1");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.1");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.1");
    CHECK(model.data(idx3, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.1");

    // Verify that the expected signals were emitted.
    spyChecker.checkSpies();

    checkModelData();

    SECTION("Add new shot to the first chunk") {
        newChunk->appendNewShot();

        spyChecker[&rowsInsertedSpy]++;
        spyChecker.requireSpies();
        REQUIRE(rowsInsertedSpy.last().size() == 3);
        REQUIRE(rowsInsertedSpy.last().at(0) == QModelIndex());
        REQUIRE(rowsInsertedSpy.last().at(1).toInt() == 4);
        REQUIRE(rowsInsertedSpy.last().at(2).toInt() == 5);

        CHECK(model.rowCount() == 6); //3 stations + 2 shots + 1 title
        auto idx3 = model.index(4); //New shot
        checkShotDataEmpty(idx3);
        auto idx4 = model.index(5); //New station
        checkStationDataEmpty(idx4);

        CHECK(idx3.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::ShotRow);
        CHECK(idx4.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
        // CHECK(idx3.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::ShotRow);
        // CHECK(idx4.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::StationRow);

        //--- Update the stations new shot and station data
        newChunk->setData(cwSurveyChunk::StationNameRole, 2, "A3"); //Adds warning 4 to the a3's LRUD and adds fatals 5 for all the survey data
        checkData(idx4, cwSurveyEditorModel::StationNameRole, 4 + 5);

        newChunk->setData(cwSurveyChunk::StationLeftRole, 2, "1.02");
        checkData(idx4, cwSurveyEditorModel::StationLeftRole);

        newChunk->setData(cwSurveyChunk::StationRightRole, 2, "2.02");
        checkData(idx4, cwSurveyEditorModel::StationRightRole);

        newChunk->setData(cwSurveyChunk::StationUpRole, 2, "3.02");
        checkData(idx4, cwSurveyEditorModel::StationUpRole);

        newChunk->setData(cwSurveyChunk::StationDownRole, 2, "4.02");
        checkData(idx4, cwSurveyEditorModel::StationDownRole);

        // Update shot at index 1.
        newChunk->setData(cwSurveyChunk::ShotDistanceRole, 1, "10.02");
        checkData(idx3, cwSurveyEditorModel::ShotDistanceRole);

        newChunk->setData(cwSurveyChunk::ShotCompassRole, 1, "100.02"); //Fatal error removed?
        checkData(idx3, cwSurveyEditorModel::ShotCompassRole, 1);

        newChunk->setData(cwSurveyChunk::ShotBackCompassRole, 1, "200.02"); //Warning added 2 deg off
        checkData(idx3, cwSurveyEditorModel::ShotBackCompassRole, 1);

        newChunk->setData(cwSurveyChunk::ShotClinoRole, 1, "30.02"); //Fatal error removed
        checkData(idx3, cwSurveyEditorModel::ShotClinoRole, 1);

        newChunk->setData(cwSurveyChunk::ShotBackClinoRole, 1, "40.02"); //Warning added, 2 deg off
        checkData(idx3, cwSurveyEditorModel::ShotBackClinoRole, 1);


        CHECK(model.data(idx3, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "10.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "100.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "200.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "30.02");
        CHECK(model.data(idx3, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "40.02");

        CHECK(model.data(idx4, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "A3");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.02");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.02");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.02");
        CHECK(model.data(idx4, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.02");

        checkModelData();

        SECTION("Add another chunk") {
            // Remember the current number of rows before adding a new chunk.
            int prevRowCount = model.rowCount();

            // Add another new survey chunk to the trip.
            trip.addNewChunk();
            spyChecker[&rowsInsertedSpy]++;
            spyChecker.requireSpies();

            // The new chunk creates 2 stations plus a tile row, so it adds 4 rows.
            REQUIRE(model.rowCount() == prevRowCount + 4);

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
            QModelIndex titleIdx = model.index(startIndex);
            QModelIndex stationIdx1 = model.index(startIndex + 1);
            QModelIndex shotIdx1 = model.index(startIndex + 2);
            QModelIndex stationIdx2 = model.index(startIndex + 3);

            // Verify that the station data for the new chunk is initially empty.
            checkStationDataEmpty(titleIdx);
            checkShotDataEmpty(titleIdx);
            checkStationDataEmpty(stationIdx1);
            checkShotDataEmpty(shotIdx1);
            checkStationDataEmpty(stationIdx2);
            // (No shot row exists for station index 1 by default.)

            CHECK(titleIdx.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::TitleRow);
            CHECK(stationIdx1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);
            CHECK(shotIdx1.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::ShotRow);
            CHECK(stationIdx2.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().rowType() == cwSurveyEditorRowIndex::StationRow);

            // CHECK(titleIdx.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::TitleRow);
            // CHECK(stationIdx1.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::StationRow);
            // CHECK(shotIdx1.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::ShotRow);
            // CHECK(stationIdx2.data(cwSurveyEditorModel::RowTypeRole).toInt() == cwSurveyEditorModel::StationRow);

            //--- Update station 0 data in the new chunk.
            secondChunk->setData(cwSurveyChunk::StationNameRole, 0, "B1"); //4 warnings for LRUD's
            checkData(stationIdx1, cwSurveyEditorModel::StationNameRole, 4);

            secondChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationLeftRole);

            secondChunk->setData(cwSurveyChunk::StationRightRole, 0, "2.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationRightRole);

            secondChunk->setData(cwSurveyChunk::StationUpRole, 0, "3.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationUpRole);

            secondChunk->setData(cwSurveyChunk::StationDownRole, 0, "4.5");
            checkData(stationIdx1, cwSurveyEditorModel::StationDownRole);

            // Update shot data for station 0 in the new chunk.
            secondChunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "15.0"); //Fatal for station name
            checkData(shotIdx1, cwSurveyEditorModel::ShotDistanceRole, 1);

            secondChunk->setData(cwSurveyChunk::ShotCompassRole, 0, "150.0");
            checkData(shotIdx1, cwSurveyEditorModel::ShotCompassRole);

            secondChunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "250.0"); //Two degrees off warning
            checkData(shotIdx1, cwSurveyEditorModel::ShotBackCompassRole, 1);

            secondChunk->setData(cwSurveyChunk::ShotClinoRole, 0, "35.0");
            checkData(shotIdx1, cwSurveyEditorModel::ShotClinoRole);

            secondChunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "45.0"); //Two degrees off warning
            checkData(shotIdx1, cwSurveyEditorModel::ShotBackClinoRole, 1);

            //--- Update station 1 data in the new chunk.
            secondChunk->setData(cwSurveyChunk::StationNameRole, 1, "B2"); //4 warning for LRUD
            checkData(stationIdx2, cwSurveyEditorModel::StationNameRole, 4);

            secondChunk->setData(cwSurveyChunk::StationLeftRole, 1, "1.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationLeftRole);

            secondChunk->setData(cwSurveyChunk::StationRightRole, 1, "2.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationRightRole);

            secondChunk->setData(cwSurveyChunk::StationUpRole, 1, "3.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationUpRole);

            secondChunk->setData(cwSurveyChunk::StationDownRole, 1, "4.6");
            checkData(stationIdx2, cwSurveyEditorModel::StationDownRole);

            //--- Verify that the model reflects the updated data for the second chunk.
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "B1");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.5");
            CHECK(model.data(stationIdx1, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.5");

            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "15.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "150.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotBackCompassRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "250.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "35.0");
            CHECK(model.data(shotIdx1, cwSurveyEditorModel::ShotBackClinoRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "45.0");

            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "B2");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationLeftRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "1.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationRightRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "2.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationUpRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "3.6");
            CHECK(model.data(stationIdx2, cwSurveyEditorModel::StationDownRole).value<cwSurveyEditorBoxData>().reading().value().toStdString() == "4.6");

            // Verify that the expected signals were emitted.
            spyChecker.checkSpies();

            checkModelData();

            SECTION("offsetBoxIndex should work correctly") {
                trip.addNewChunk();
                auto thirdChunk = trip.chunk(trip.chunkCount() - 1);

                REQUIRE(trip.chunkCount() == 3);

                auto station01 = model.index(3).data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>();
                CHECK(station01.indexInChunk() == 1);
                CHECK(station01.chunk() == newChunk);
                CHECK(station01.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station01.chunkDataRole() == cwSurveyChunk::StationNameRole);

                auto station20 = model.offsetBoxIndex(station01.boxIndex(), 4);
                CHECK(station20.indexInChunk() == 0);
                CHECK(station20.chunk() == thirdChunk);
                CHECK(station20.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station20.chunkDataRole() == cwSurveyChunk::StationNameRole);

                auto station02 = model.offsetBoxIndex(station01.boxIndex(), 1);
                CHECK(station02.indexInChunk() == 2);
                CHECK(station02.chunk() == newChunk);
                CHECK(station02.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station02.chunkDataRole() == cwSurveyChunk::StationNameRole);


                auto station21 = model.offsetBoxIndex(station01.boxIndex(), 5);
                CHECK(station21.indexInChunk() == 1);
                CHECK(station21.chunk() == thirdChunk);
                CHECK(station21.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station21.chunkDataRole() == cwSurveyChunk::StationNameRole);

                auto station10 = model.offsetBoxIndex(station01.boxIndex(), 2);
                CHECK(station10.indexInChunk() == 0);
                CHECK(station10.chunk() == secondChunk);
                CHECK(station10.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station10.chunkDataRole() == cwSurveyChunk::StationNameRole);

                //Out of bounds forwards
                auto stationOutOfBounds = model.offsetBoxIndex(station01.boxIndex(), 6);
                CHECK(stationOutOfBounds.indexInChunk() == -1);
                CHECK(stationOutOfBounds.chunk() == nullptr);

                auto stationOutOfBounds1 = model.offsetBoxIndex(station01.boxIndex(), 99);
                CHECK(stationOutOfBounds1.indexInChunk() == -1);
                CHECK(stationOutOfBounds1.chunk() == nullptr);

                //Negative offset
                auto station00 = model.offsetBoxIndex(station01.boxIndex(), -1);
                CHECK(station00.indexInChunk() == 0);
                CHECK(station00.chunk() == newChunk);
                CHECK(station00.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station00.chunkDataRole() == cwSurveyChunk::StationNameRole);

                auto stationbad = model.offsetBoxIndex(station01.boxIndex(), -2);
                CHECK(stationbad.indexInChunk() == -1);
                CHECK(stationbad.chunk() == nullptr);

                auto station21data = model.index(13).data(cwSurveyEditorModel::StationNameRole).value<cwSurveyEditorBoxData>();
                CHECK(station21data.indexInChunk() == 1);
                CHECK(station21data.chunk() == thirdChunk);
                CHECK(station21data.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station21data.chunkDataRole() == cwSurveyChunk::StationNameRole);

                auto station11 = model.offsetBoxIndex(station21data.boxIndex(), -2);
                CHECK(station11.indexInChunk() == 1);
                CHECK(station11.chunk() == secondChunk);
                CHECK(station11.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station11.chunkDataRole() == cwSurveyChunk::StationNameRole);

                auto station00a = model.offsetBoxIndex(station21data.boxIndex(), -6);
                CHECK(station00a.indexInChunk() == 0);
                CHECK(station00a.chunk() == newChunk);
                CHECK(station00a.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station00a.chunkDataRole() == cwSurveyChunk::StationNameRole);

                auto station10a = model.offsetBoxIndex(station21data.boxIndex(), -3);
                CHECK(station10a.indexInChunk() == 0);
                CHECK(station10a.chunk() == secondChunk);
                CHECK(station10a.rowType() == cwSurveyEditorRowIndex::StationRow);
                CHECK(station10a.chunkDataRole() == cwSurveyChunk::StationNameRole);

                //Make sure 0 offset return the same thing
                auto station01Other = model.offsetBoxIndex(station01.boxIndex(), 0);
                CHECK(station01.boxIndex() == station01Other);

                //Make sure shot offset works correctly
                auto shot00 = model.index(2).data(cwSurveyEditorModel::ShotDistanceRole).value<cwSurveyEditorBoxData>();
                CHECK(shot00.indexInChunk() == 0);
                CHECK(shot00.chunk() == newChunk);
                CHECK(shot00.rowType() == cwSurveyEditorRowIndex::ShotRow);
                CHECK(shot00.chunkDataRole() == cwSurveyChunk::ShotDistanceRole);

                auto shot01 = model.offsetBoxIndex(shot00.boxIndex(), 1);
                CHECK(shot01.indexInChunk() == 1);
                CHECK(shot01.chunk() == newChunk);
                CHECK(shot01.rowType() == cwSurveyEditorRowIndex::ShotRow);
                CHECK(shot01.chunkDataRole() == cwSurveyChunk::ShotDistanceRole);
            }

            SECTION("Remove chunk") {
                int prevRowCount = model.rowCount();

                // Add a new chunk; this new chunk creates (stationCount() + 1) rows.
                trip.addNewChunk();
                spyChecker[&rowsInsertedSpy]++;
                spyChecker.requireSpies();

                // For the newly added chunk, compute its row count.
                cwSurveyChunk* addedChunk = trip.chunk(trip.chunkCount() - 1);
                int newChunkRowCount = addedChunk->stationCount() + addedChunk->shotCount() + 1;
                REQUIRE(model.rowCount() == prevRowCount + newChunkRowCount);

                SECTION("Remove the first chunk") {
                    int currentRowCount = model.rowCount();
                    int currentChunkCount = trip.chunkCount();
                    cwSurveyChunk* firstChunk = trip.chunk(0);
                    REQUIRE(firstChunk != nullptr);

                    int chunkRowCount = firstChunk->stationCount() + firstChunk->shotCount() + 1;
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

                    int chunkRowCount = middleChunk->stationCount() + middleChunk->shotCount() + 1;

                    // Calculate the starting row for the middle chunk.
                    int firstRow = 0;
                    for (int i = 0; i < midIndex; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        firstRow += chunk->stationCount() + chunk->shotCount() + 1;
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

                    int chunkRowCount = lastChunk->stationCount() + lastChunk->shotCount() + 1;

                    // Calculate the starting row for the last chunk.
                    int firstRow = 0;
                    for (int i = 0; i < lastIndex; i++) {
                        cwSurveyChunk* chunk = trip.chunk(i);
                        firstRow += chunk->stationCount() + chunk->shotCount() + 1;
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
                        totalRows += chunk->stationCount() + chunk->shotCount() + 1;
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

            auto findNewChunkBase = [&](const cwSurveyChunk* chunk) {
                for(int i = 0; i < model.rowCount(); i++) {
                    auto index = model.index(i);

                    if(chunk == index.data(cwSurveyEditorModel::RowIndexRole).value<cwSurveyEditorRowIndex>().chunk()) {
                        return index.row();
                    }
                }
                return -1;
            };

            int baseRowIndex = findNewChunkBase(newChunk);

            SECTION("Remove the first station - down") {
                int prevModelRow = model.rowCount();
                int prevStationCount = newChunk->stationCount();
                int prevShotCount = newChunk->shotCount();

                newChunk->removeStation(0, cwSurveyChunk::Below);
                spyChecker[&dataChangedSpy] += 2; //Error model change + shifted indices
                spyChecker[&rowsRemovedSpy]++;
                spyChecker.requireSpies();

                int firstStationRow = baseRowIndex + 1; //+1 for title

                auto params = rowsRemovedSpy.last();
                REQUIRE(params.size() == 3);
                // Expect the parent index to be invalid (i.e. QModelIndex())
                CHECK(params.at(0).toModelIndex() == QModelIndex());
                // The removed row should be at the expected row.
                CHECK(params.at(1).toInt() == firstStationRow);
                CHECK(params.at(2).toInt() == firstStationRow + 1);

                // Verify the new counts.
                CHECK(newChunk->stationCount() == prevStationCount - 1);
                CHECK(newChunk->shotCount() == prevShotCount - 1);
                CHECK(model.rowCount() == prevModelRow - 2);

                checkModelData();
            }

            SECTION("Remove the second Station") {
                // Two scenarios: removal with 'Above' and with 'Below'.
                SECTION("Direction Above") {
                    int prevModelRow = model.rowCount();
                    int prevStationCount = newChunk->stationCount();
                    int prevShotCount = newChunk->shotCount();

                    newChunk->removeStation(1, cwSurveyChunk::Above);
                    spyChecker[&dataChangedSpy] += 2; //Error model change + shifted indices
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto removeSpyResults = rowsRemovedSpy.last();

                    int stationRow = baseRowIndex + 3; //+1 for title +2 for the second station

                    REQUIRE(removeSpyResults.size() == 3);
                    CHECK(removeSpyResults.at(0).toModelIndex() == QModelIndex());
                    CHECK(removeSpyResults.at(1).toInt() == stationRow - 1); //Deleting the shot above the station
                    CHECK(removeSpyResults.at(2).toInt() == stationRow);

                    CHECK(newChunk->stationCount() == prevStationCount - 1);
                    CHECK(newChunk->shotCount() == prevShotCount - 1);
                    CHECK(model.rowCount() == prevModelRow - 2);

                    checkModelData();
                }

                SECTION("Direction down") {
                    int prevModelRow = model.rowCount();
                    int prevStationCount = newChunk->stationCount();
                    int prevShotCount = newChunk->shotCount();

                    newChunk->removeStation(1, cwSurveyChunk::Below);
                    spyChecker[&dataChangedSpy]++; //Shifted indices
                    spyChecker[&rowsRemovedSpy]++;
                    spyChecker.requireSpies();

                    auto removeSpyResults = rowsRemovedSpy.last();

                    int stationRow = baseRowIndex + 3; //+1 for title +2 for the second station

                    REQUIRE(removeSpyResults.size() == 3);
                    CHECK(removeSpyResults.at(0).toModelIndex() == QModelIndex());
                    CHECK(removeSpyResults.at(1).toInt() == stationRow);
                    CHECK(removeSpyResults.at(2).toInt() == stationRow + 1); //Deleting the shot below the station

                    CHECK(newChunk->stationCount() == prevStationCount - 1);
                    CHECK(newChunk->shotCount() == prevShotCount - 1);
                    CHECK(model.rowCount() == prevModelRow - 2);

                    checkModelData();
                }
            }

            SECTION("Remove the last Station - up") {
                int prevModelRow = model.rowCount();
                int prevStationCount = newChunk->stationCount();
                int prevShotCount = newChunk->shotCount();

                int removedRow = baseRowIndex + 5; //+1 for title +4 for the third (and last) station

                newChunk->removeStation(newChunk->stationCount() - 1, cwSurveyChunk::Above);
                // spyChecker[&dataChangedSpy]++; //Error Model change
                spyChecker[&rowsRemovedSpy]++;
                spyChecker.requireSpies();

                auto params = rowsRemovedSpy.last();
                REQUIRE(params.size() == 3);
                CHECK(params.at(0).toModelIndex() == QModelIndex());
                CHECK(params.at(1).toInt() == removedRow - 1); //Deleting the shot above the station
                CHECK(params.at(2).toInt() == removedRow);

                CHECK(newChunk->stationCount() == prevStationCount - 1);
                CHECK(newChunk->shotCount() == prevShotCount - 1);
                CHECK(model.rowCount() == prevModelRow - 2);

                checkModelData();
            }
        }
    }
}
