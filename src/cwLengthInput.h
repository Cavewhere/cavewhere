/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDISTANCE_H
#define CWDISTANCE_H

//Qt includse
//#include <QValidator>
//#include <QRegularExpression>

//Our includes
#include "cwUnits.h"
//#include "cwGlobals.h"
#include "cwUnitValueInput.h"
#include "cwDistanceValidator.h"

typedef cwUnitValueInput<cwUnits::LengthUnit, cwDistanceValidator> cwLengthInput;

//class CAVEWHERE_LIB_EXPORT cwLengthInput : public cwUnitValueInput
//{
//public:
//    cwLengthInput(cwUnits::LengthUnit defaultUnit = cwUnits::Meters);
//    cwLengthInput(QString input, cwUnits::LengthUnit defaultUnit = cwUnits::Meters);
//    cwLengthInput(double input, cwUnits::LengthUnit defaultUnit = cwUnits::Meters);

//    void setDefaultUnit(cwUnits::LengthUnit defaultUnit);
//    cwUnits::LengthUnit defaultUnit() const;

//    void setValue(double input);
//    void setValue(QString input);
//    QString value() const;
//    double value(cwUnits::LengthUnit unit) const;

//    QValidator::State validate() const;
//    static QValidator::State validate(QString distanceStr);

//    bool isValid() const;

//    bool operator ==(const cwLengthInput& other) const;
//    bool operator !=(const cwLengthInput& other) const;

//private:
//    class ValueUnit  {
//    public:
//        ValueUnit() :
//            Value(0.0),
//            Unit(cwUnits::LengthUnitless)
//        {
//        }

//        ValueUnit(double value, cwUnits::LengthUnit unit) :
//            Value(value),
//            Unit(unit)
//        {

//        }

//        double Value;
//        cwUnits::LengthUnit Unit;
//    };


//    QValidator::State State;
//    cwUnits::LengthUnit DefaultUnit;
//    QString Input; //The user input for the distance
//    QList<ValueUnit> ValueUnits; //The parse value of the distance

//    static const QRegularExpression FieldRegex;

//    QList<ValueUnit> parseInputIntoValueUnits();

//};

#endif // CWDISTANCE_H
