/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch include
#include "catch.hpp"

#include "cwLengthInput.h"
#include "cwUnits.h"

TEST_CASE("Test cwLengthInput setting and getting", "[cwLengthInput]") {

    class Row {
    public:
        Row(QString input,
            QString value,
            cwUnits::LengthUnit unit,
            double valueDouble,
            QValidator::State state,
            bool valid)
            :
            Input(input),
            Value(value),
            Unit(unit),
            ValueDouble(valueDouble),
            State(state),
            Valid(valid)
        {}

        QString Input;
        QString Value;
        cwUnits::LengthUnit Unit;
        double ValueDouble;
        QValidator::State State;
        bool Valid;

    };

    QList<Row> rows;
    rows << Row("4 m", "4 m", cwUnits::Meters, 4.0, QValidator::Acceptable, true);
    rows << Row("4m", "4 m", cwUnits::Meters, 4.0, QValidator::Acceptable, true);
    rows << Row("4 in", "4 in", cwUnits::Meters, 0.1016, QValidator::Acceptable, true);
    rows << Row("4 ft", "4 ft", cwUnits::Meters, 1.2192, QValidator::Acceptable, true);
    rows << Row("4 yd", "4 yd", cwUnits::Meters, 3.6576, QValidator::Acceptable, true);
    rows << Row("4 km", "4 km", cwUnits::Meters, 4000.0, QValidator::Acceptable, true);
    rows << Row("4 in 6 ft 2 m", "4 in 6 ft 2 m", cwUnits::Meters, 3.9304, QValidator::Acceptable, true);
    rows << Row("4", "4", cwUnits::Meters, 4.0, QValidator::Acceptable, true);
    rows << Row("", "", cwUnits::Meters, 0.0, QValidator::Intermediate, false);
    rows << Row("aeou", "aeou", cwUnits::Meters, 0.0, QValidator::Invalid, false);
    rows << Row("4i", "4i", cwUnits::Meters, 0.0, QValidator::Intermediate, false);
    rows << Row("4 f", "4 f", cwUnits::Meters, 0.0, QValidator::Intermediate, false);
    rows << Row("4 f 3", "4 f 3", cwUnits::Meters, 0.0, QValidator::Invalid, false);
    rows << Row("4 ft 3 ", "4 ft 3 ", cwUnits::Meters, 0.0, QValidator::Intermediate, false);
    rows << Row("4 ft 3i", "4 ft 3i", cwUnits::Meters, 0.0, QValidator::Intermediate, false);
    rows << Row("4.23 ft", "4.23 ft", cwUnits::Inches, 50.76, QValidator::Acceptable, true);
    rows << Row("   3mi 2 ft 2.23m  ", "3 mi 2 ft 2.23 m", cwUnits::Kilometers, 4.8308716, QValidator::Acceptable, true);

    foreach(const Row& row, rows) {
        cwLengthInput length;
        length.setValue(row.Input);

        INFO("Row input: \"" << row.Input.toStdString() << "\"");
        CHECK(length.value().toStdString() == row.Value.toStdString());
        CHECK(length.value(cwUnits::Meters, row.Unit) == Approx(row.ValueDouble));
        CHECK(length.validate() == row.State);
        CHECK(length.isValid() == row.Valid);
    }
}

TEST_CASE("Test Default constructor for cwLengthInput", "[cwLengthInput]") {
    cwLengthInput input;
    CHECK(input.value().toStdString() == "");
    CHECK(input.validate() == QValidator::Intermediate);
    CHECK(input.isValid() == false);
}

TEST_CASE("Test Value constructor for cwLengthInput", "[cwLengthInput]") {
    cwLengthInput input(54.2);
    CHECK(input.value().toStdString() == "54.2");
    CHECK(input.validate() == QValidator::Acceptable);
    CHECK(input.isValid() == true);
    CHECK(input.value(cwUnits::Feet, cwUnits::Meters) == 16.52016);
}
