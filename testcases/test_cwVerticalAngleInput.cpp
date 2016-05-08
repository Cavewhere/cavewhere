/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch include
#include "catch.hpp"

#include "cwVerticalAngleInput.h"
#include "cwUnits.h"

TEST_CASE("Test cwVertcialAngleInput setting and getting", "[cwVerticalAngleInput]") {

    class Row {
    public:
        Row(QString input,
            QString value,
            cwUnits::AngleUnit unit,
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
        cwUnits::AngleUnit Unit;
        double ValueDouble;
        QValidator::State State;
        bool Valid;

    };

    QList<Row> rows;
    rows << Row("4 deg", "4 deg", cwUnits::Degrees, 4.0, QValidator::Acceptable, true);
    rows << Row("4deg", "4 deg", cwUnits::Degrees, 4.0, QValidator::Acceptable, true);
    rows << Row("4 sec", "4 sec", cwUnits::Degrees, 0.001111, QValidator::Acceptable, true);
    rows << Row("4 min", "4 min", cwUnits::Degrees, 0.066667, QValidator::Acceptable, true);
    rows << Row("4 grad", "4 grad", cwUnits::Degrees, 3.6, QValidator::Acceptable, true);
    rows << Row("4 mils", "4 mils", cwUnits::Degrees, 0.225, QValidator::Acceptable, true);
    rows << Row("4 %", "4 %", cwUnits::Degrees, 2.2906100426, QValidator::Acceptable, true);
    rows << Row("4 deg 6 min 2 sec", "4 deg 6 min 2 sec", cwUnits::Degrees, 4.100555555555555, QValidator::Acceptable, true);
    rows << Row("4", "4", cwUnits::Degrees, 4.0, QValidator::Acceptable, true);
    rows << Row("", "", cwUnits::Degrees, 0.0, QValidator::Intermediate, false);
    rows << Row("aeou", "aeou", cwUnits::Degrees, 0.0, QValidator::Invalid, false);
    rows << Row("4de", "4de", cwUnits::Degrees, 0.0, QValidator::Intermediate, false);
    rows << Row("4 s", "4 s", cwUnits::Degrees, 0.0, QValidator::Intermediate, false);
    rows << Row("4 m 3", "4 m 3", cwUnits::Degrees, 0.0, QValidator::Invalid, false);
    rows << Row("4 deg 3 ", "4 deg 3 ", cwUnits::Degrees, 0.0, QValidator::Intermediate, false);
    rows << Row("4 deg 3m", "4 deg 3m", cwUnits::Degrees, 0.0, QValidator::Intermediate, false);
    rows << Row("4.23 deg", "4.23 deg", cwUnits::Minutes, 253.8, QValidator::Acceptable, true);
    rows << Row("   3min 2 sec 2.23deg  ", "3 min 2 sec 2.23 deg", cwUnits::Seconds, 8210.0, QValidator::Acceptable, true);

    foreach(const Row& row, rows) {
        cwVerticalAngleInput angle;
        angle.setValue(row.Input);

        INFO("Row input: \"" << row.Input.toStdString() << "\"");
        CHECK(angle.value().toStdString() == row.Value.toStdString());
        CHECK(angle.value((cwUnits::VerticalAngleUnit)cwUnits::Degrees, (cwUnits::VerticalAngleUnit)row.Unit) == Approx(row.ValueDouble));
        CHECK(angle.validate() == row.State);
        CHECK(angle.isValid() == row.Valid);
    }
}
