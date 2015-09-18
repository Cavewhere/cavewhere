/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "catch.hpp"

//Cavewhere inculdes
#include "cwUnitValueList.h"
#include "cwUnits.h"
#include "cwQMLRegister.h"

//Qt includes
#include <QJsonDocument>
#include <QVariant>
#include <QDebug>

class Value {
public:
    Value(QString testValue,
          QString jsonList,
          cwUnits::UnitType type,
          QString toString,
          cwUnits::UnitType defaultUnitType,
          int defaultUnit,
          int toUnit,
          double resultValue) :
        TestValue(testValue),
        JsonList(jsonList),
        Type(type),
        ToString(toString),
        DefaultUnitType(defaultUnitType),
        DefaultUnit(defaultUnit),
        ToUnit(toUnit),
        ResultValue(resultValue)

    {}

    QString TestValue;
    QString JsonList;
    cwUnits::UnitType Type;
    QString ToString;
    cwUnits::UnitType DefaultUnitType;
    int DefaultUnit;
    int ToUnit;
    double ResultValue;
};


TEST_CASE("Valid String to cwUnitValueList", "[unitValueList]") {

    cwQMLRegister::registerQML();

    QList<Value> validValues;
                            //TestValue                         //JsonList                                  //Type                            //toString                //Default UnitType                  //Default Unit          //To Unit              //Converted Units
    validValues.append(Value("1.23",                            "[1.23]",                                   cwUnits::InvalidUnitType,          "1.23",                  cwUnits::LengthUnitType,            cwUnits::Feet,         cwUnits::Meters,        0.374904));
    validValues.append(Value("-5.23",                           "[-5.23]",                                  cwUnits::InvalidUnitType,          "-5.23",                 cwUnits::AngleUnitType,             cwUnits::PercentGrade, cwUnits::Degrees,       -2.9938415818));
    validValues.append(Value("1.23 ft",                         "[1.23, \"ft\"]",                           cwUnits::LengthUnitType,           "1.23ft",                cwUnits::LengthUnitType,            cwUnits::Meters,       cwUnits::Meters,        0.374904));
    validValues.append(Value("0.23ft",                          "[0.23, \"ft\"]",                           cwUnits::LengthUnitType,           "0.23ft",                cwUnits::LengthUnitType,            cwUnits::Meters,       cwUnits::Meters,        0.070104));
    validValues.append(Value("10.23ft 3.2in",                   "[10.23, \"ft\", \"3.2\", \"in\"]",         cwUnits::LengthUnitType,            "10.23ft 3.2in",        cwUnits::LengthUnitType,            cwUnits::Meters,       cwUnits::Meters,        3.19938));
    validValues.append(Value("5deg 10mins 3     seconds",       "[5, \"deg\", 10, \"min\", 3, \"sec\"]",    cwUnits::AngleUnitType,            "5deg 10min 3sec",       cwUnits::AngleUnitType,             cwUnits::Degrees,      cwUnits::Degrees,       5.1675));
    validValues.append(Value("6deg 10minutes, 3 seconds",       "[6, \"deg\", 10, \"min\", 3, \"sec\"]",    cwUnits::AngleUnitType,            "6deg 10min 3sec",       cwUnits::AngleUnitType,             cwUnits::Degrees,      cwUnits::Degrees,       6.1675));
    validValues.append(Value("7deg    10minutes   , 3 seconds", "[7, \"deg\", 10, \"min\", 3, \"sec\"]",    cwUnits::AngleUnitType,            "7deg 10min 3sec",       cwUnits::AngleUnitType,             cwUnits::Degrees,      cwUnits::Degrees,       7.1675));
    validValues.append(Value("7deg    10minutes   ,3 seconds",  "[7, \"deg\", 10, \"min\", 3, \"sec\"]",    cwUnits::AngleUnitType,            "7deg 10min 3sec",       cwUnits::AngleUnitType,             cwUnits::Degrees,      cwUnits::Degrees,       7.1675));
    validValues.append(Value("300 dots per inch",               "[300, \"Dots per inch\"]",                 cwUnits::ImageResolutionUnitType,  "300Dots per inch",      cwUnits::ImageResolutionUnitType,   cwUnits::DotsPerInch,  cwUnits::DotsPerInch,   300.0));

    foreach(Value value, validValues) {
        cwUnitValueList list = cwUnitValueList::fromString(value.TestValue);
        CHECK(list.isValid() == true);

        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(value.JsonList.toLocal8Bit(), &error);
        QVariantList jsonList = document.toVariant().toList();

        CHECK(error.error == QJsonParseError::NoError);

        CHECK(list.toList() == jsonList);

//        qDebug() << "List:" << list.toList();
//        qDebug() << "JsonList:" << jsonList;
//        qDebug() << "Compare:" << (list.toList() == jsonList);
//        qDebug() << "ToString:" << list.toString() << value.ToString << (list.toString() == value.ToString);

        CHECK(list == cwUnitValueList(value.TestValue));
        CHECK(list.type() == value.Type);
        CHECK(list.toString() == value.ToString);

        //QVariant convertion check,  checks to see if we can convert the QVariantList
        QVariant var = QVariant::fromValue(list);
        CHECK(var.toList() == jsonList);
        CHECK(var.toString() == value.ToString);

        //Convert variant sting to cwUnitValueListTest
        QVariant varStr(value.TestValue);
        cwUnitValueList varList = varStr.value<cwUnitValueList>();
        CHECK(varList.isValid() == true);
        CHECK(varList == list);

        try {
            //Convert to signle value
            double convertedValue = 0.0;
            switch(value.DefaultUnitType) {
            case cwUnits::LengthUnitType:
                convertedValue = list.value((cwUnits::LengthUnit)value.DefaultUnit, (cwUnits::LengthUnit)value.ToUnit);
                break;
            case cwUnits::AngleUnitType:
                convertedValue = list.value((cwUnits::AngleUnit)value.DefaultUnit, (cwUnits::AngleUnit)value.ToUnit);
                break;
            case cwUnits::ImageResolutionUnitType:
                convertedValue = list.value((cwUnits::ImageResolutionUnit)value.DefaultUnit, (cwUnits::ImageResolutionUnit)value.ToUnit);
                break;
            default:
                //Test shouldn't get here
                REQUIRE(false);
            }

            CHECK(convertedValue == Approx(value.ResultValue));

        } catch (QString e) {
            qDebug() << "Exception Error: " << e;
        }

    }
}

TEST_CASE("Invalid string to cwUnitValueList", "[unitValueList]") {

    QStringList invalidStrings;
    invalidStrings.append("");
    invalidStrings.append("1ft2");
    invalidStrings.append("2ft3in");
    invalidStrings.append("6aoeu");
    invalidStrings.append("ft");
    invalidStrings.append("no a unit");
    invalidStrings.append("NoUnit");
    invalidStrings.append(".");
    invalidStrings.append("-.");
    invalidStrings.append("12 43 2");
    invalidStrings.append("12 ft 43 deg");
    invalidStrings.append("12 ft 43| in");
    invalidStrings.append("12 ft $43 in");

    foreach(QString str, invalidStrings) {
        cwUnitValueList list = cwUnitValueList::fromString(str);

//        qDebug() << "Str:" << str;
//        qDebug() << "isValid:" << list.isValid();
//        qDebug() << "type:" << list.type() << cwUnits::InvalidUnitType;
//        qDebug() << "============================";

        CHECK(list.isValid() == false);
        CHECK(list.type() == cwUnits::InvalidUnitType);
    }
}


