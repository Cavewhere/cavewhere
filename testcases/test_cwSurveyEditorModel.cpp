//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwSurveyEditorModel.h"

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
