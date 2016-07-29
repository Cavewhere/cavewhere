/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUNITVALUEINPUT_H
#define CWUNITVALUEINPUT_H

//Qt includes
#include <QValidator>
#include <QRegularExpression>

//Our includes
#include "cwUnits.h"

class cwValueUnit  {
public:
    cwValueUnit() :
        Value(0.0),
        Unit(-1)
    {
    }

    cwValueUnit(double value, int unit) :
        Value(value),
        Unit(unit)
    {

    }

    double Value;
    int Unit;
};

template <class UnitType, class Validator>
class cwUnitValueInput
{
public:
    cwUnitValueInput();
    cwUnitValueInput(QString input);
    cwUnitValueInput(double input);

    void setValue(double input);
    void setValue(QString input);
    QString value() const;
    double value(UnitType defaultUnit, UnitType toUnit) const;
    double value(UnitType defaultUnit) const;

    QValidator::State validate() const;
    static QValidator::State validate(QString str);

    bool isValid() const;

    bool operator ==(const cwUnitValueInput<UnitType, Validator>& other) const;
    bool operator !=(const cwUnitValueInput<UnitType, Validator>& other) const;

protected:


    QValidator::State State;
    QString Input; //The user input for the distance
    QList<cwValueUnit> ValueUnits; //The parse value of the distance

    static const QRegularExpression FieldRegex;

    QList<cwValueUnit> parseInputIntoValueUnits();

};

template <class UnitType, class Validator>
const QRegularExpression cwUnitValueInput<UnitType, Validator>::FieldRegex("\\s*([-+]?[0-9]*\\.?[0-9]+)\\s*([a-zA-Z%°]+)\\s*|^\\s*([-+]?[0-9]*\\.?[0-9]+)\\s*$|.+");

template <class UnitType, class Validator>
cwUnitValueInput<UnitType, Validator>::cwUnitValueInput() :
    State(QValidator::Intermediate)
{

}

template <class UnitType, class Validator>
cwUnitValueInput<UnitType, Validator>::cwUnitValueInput(QString input) :
    State(QValidator::Intermediate),
    Input(input),
    ValueUnits(parseInputIntoValueUnits())
{

}

template <class UnitType, class Validator>
cwUnitValueInput<UnitType, Validator>::cwUnitValueInput(double input) :
    State(QValidator::Intermediate)
{
    setValue(input);
}

template <class UnitType, class Validator>
void cwUnitValueInput<UnitType, Validator>::setValue(double input)
{
    setValue(QString::number(input));
}

template <class UnitType, class Validator>
void cwUnitValueInput<UnitType, Validator>::setValue(QString input)
{
    if(Input != input) {
        Input = input;
        ValueUnits = parseInputIntoValueUnits();
    }
}

/**
 * @brief cwUnitValueInput<UnitType, Validator>::value
 * @return The input string that the user inputed into this object
 */
template <class UnitType, class Validator>
QString cwUnitValueInput<UnitType, Validator>::value() const
{
    return Input;
}

/**
 * @brief cwUnitValueInput<UnitType, Validator>::value
 * @param unit
 * @return Adds all the value units found in the value and returns the result is unit
 */
template <class UnitType, class Validator>
double cwUnitValueInput<UnitType, Validator>::value(UnitType defaultUnit, UnitType toUnit) const
{
    double calcValue = 0.0;
    foreach(cwValueUnit valueUnit, ValueUnits) {
        UnitType currentUnit = valueUnit.Unit == -1 ? defaultUnit : static_cast<UnitType>(valueUnit.Unit);
        calcValue += cwUnits::convert(valueUnit.Value, currentUnit, toUnit);
    }
    return calcValue;
}

/**
 * @brief cwUnitValueInput::value
 * @param defaultUnit
 * @return The same as calling value(defaultUnit, defaultUnit)
 */
template <class UnitType, class Validator>
double cwUnitValueInput<UnitType, Validator>::value(UnitType defaultUnit) const
{
    return value(defaultUnit, defaultUnit);
}

/**
 * @brief cwUnitValueInput<UnitType, Validator>::validate
 * @return Returns the state of object. It can either be QValidator::Acceptable, QValidator::Intermediate
 * or QValidator::Invalid. QValidator means that the input is almost correct, but isn't quit there
 * yet.
 */
template <class UnitType, class Validator>
QValidator::State cwUnitValueInput<UnitType, Validator>::validate() const
{
    return State;

}

template <class UnitType, class Validator>
QValidator::State cwUnitValueInput<UnitType, Validator>::validate(QString str)
{
    cwUnitValueInput<UnitType, Validator> obj(str);
    return obj.validate();
}

/**
 * @brief cwUnitValueInput<UnitType, Validator>::isValid
 * @return True if validate() return QValidator::Acceptable else this returns false
 */
template <class UnitType, class Validator>
bool cwUnitValueInput<UnitType, Validator>::isValid() const
{
    switch(validate()) {
    case QValidator::Acceptable:
        return true;
    default:
        return false;
    }
}

/**
 * @brief cwUnitValueInput<UnitType, Validator>::operator ==
 * @param other
 * @return True if other's value is the same is this object's value, otherwise it returns false
 */
template <class UnitType, class Validator>
bool cwUnitValueInput<UnitType, Validator>::operator ==(const cwUnitValueInput<UnitType, Validator> &other) const
{
    return value() == other.value();
}

/**
 * @brief cwUnitValueInput<UnitType, Validator>::operator !=
 * @param other
 * @return Return true if the other's value is not equal to this object's value, otherwise it's returns true
 */
template <class UnitType, class Validator>
bool cwUnitValueInput<UnitType, Validator>::operator !=(const cwUnitValueInput<UnitType, Validator> &other) const
{
    return !operator ==(other);
}

template <class UnitType, class Validator>
QList<cwValueUnit> cwUnitValueInput<UnitType, Validator>::parseInputIntoValueUnits()
{
    Validator distanceValidator;

    QStringList unitNames = cwUnits::lengthUnitNames();
    QStringList fields;

    auto appendToFields = [&fields](QString field) {
        if(!field.isEmpty()) {
            fields.append(field);
        }
    };

    auto valueOnly = [&](QString field, QValidator::State& state) {
        state = static_cast<QValidator::State>(distanceValidator.validate(field));
        return field.toDouble();
    };

    auto unitOnly = [&unitNames](const QString& field, QValidator::State& state)->UnitType {
        //Try to match partual
        QStringList unitNames = cwUnits::unitNames<UnitType>();
        if(unitNames.contains(field)) {
            state = QValidator::Acceptable;
        } else {
            //See if we can match a part of field
            QRegularExpression regex(QString("^%1").arg(field));
            foreach(QString unitName, unitNames) {
                auto match = regex.match(unitName);
                if(match.hasMatch()) {
                    state = QValidator::Intermediate;
                    break;
                }
            }
        }

        try {
            return cwUnits::toUnit<UnitType>(field);
        } catch (QString e) {
            Q_UNUSED(e);
            return static_cast<UnitType>(-1);
        }
    };

    QList<cwValueUnit> valueUnits;

    QValidator::State finalState = QValidator::Acceptable;

    QRegularExpressionMatchIterator fieldIter = FieldRegex.globalMatch(value());

    while(fieldIter.hasNext()) {
        QRegularExpressionMatch match = fieldIter.next();

        QValidator::State valueState = QValidator::Acceptable;
        QValidator::State unitState = QValidator::Acceptable;

        QString valueField;
        QString unitField;

        switch(match.lastCapturedIndex()) {
        case 0:
            //Extra text at the end of field, see if it's a number, other wise it's invalid text
            bool okay;
            match.captured(0).trimmed().toDouble(&okay);
            if(okay && finalState == QValidator::Acceptable) {
                finalState = QValidator::Intermediate;
            } else {
                finalState = QValidator::Invalid;
            }
            break;
        case 2:
            valueField = match.captured(1);
            unitField = match.captured(2);
            break;
        case 3:
            valueField = match.captured(3);
            break;
        }

        cwValueUnit valueUnit;
        valueUnit.Value = valueOnly(valueField, valueState);
        valueUnit.Unit = unitOnly(unitField, unitState);

        finalState = std::min(finalState, std::min(valueState, unitState));

        if(finalState == QValidator::Invalid) {
            break;
        }

        appendToFields(valueField);
        appendToFields(unitField);

        valueUnits.append(valueUnit);
    }


    if(finalState == QValidator::Intermediate &&
            valueUnits.size() == 1)
    {
        bool okay;
        value().trimmed().toDouble(&okay);
        if(okay) {
            //Value is just a double, accept it, nothing more nothing less
            finalState = QValidator::Acceptable;
        }
    }

    if(finalState == QValidator::Acceptable) {
        Input = fields.join(" ");
    } else {
        valueUnits.clear();
    }

    State = finalState;

    return valueUnits;
}

#endif // CWUNITVALUEINPUT_H
