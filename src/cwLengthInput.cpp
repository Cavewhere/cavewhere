/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLengthInput.h"
#include "cwDistanceValidator.h"
#include "cwUnits.h"

const QRegularExpression cwLengthInput::FieldRegex("\\s*([-+]?[0-9]*\\.?[0-9]+)\\s*([a-zA-Z]+)\\s*|^\\s*([-+]?[0-9]*\\.?[0-9]+)\\s*$|.+");

cwLengthInput::cwLengthInput(cwUnits::LengthUnit defaultUnit) :
    State(QValidator::Intermediate),
    DefaultUnit(defaultUnit)
{

}

cwLengthInput::cwLengthInput(QString input, cwUnits::LengthUnit defaultUnit) :
    State(QValidator::Intermediate),
    DefaultUnit(defaultUnit),
    Input(input),
    ValueUnits(parseInputIntoValueUnits())
{

}

cwLengthInput::cwLengthInput(double input, cwUnits::LengthUnit defaultUnit) :
    State(QValidator::Intermediate),
    DefaultUnit(defaultUnit)
{
    setValue(input);
}

/**
 * @brief cwLengthInput::setDefaultUnit
 * @param defaultUnit
 * Sets the default
 */
void cwLengthInput::setDefaultUnit(cwUnits::LengthUnit defaultUnit)
{
    DefaultUnit = defaultUnit;
}

cwUnits::LengthUnit cwLengthInput::defaultUnit() const
{
    return DefaultUnit;
}

void cwLengthInput::setValue(double input)
{
    setValue(QString::number(input));
}

void cwLengthInput::setValue(QString input)
{
    if(Input != input) {
        Input = input;
        ValueUnits = parseInputIntoValueUnits();
    }
}

/**
 * @brief cwLengthInput::value
 * @return The input string that the user inputed into this object
 */
QString cwLengthInput::value() const
{
    return Input;
}

/**
 * @brief cwLengthInput::value
 * @param unit
 * @return Adds all the value units found in the value and returns the result is unit
 */
double cwLengthInput::value(cwUnits::LengthUnit unit) const
{
    double calcValue = 0.0;
    foreach(ValueUnit valueUnit, ValueUnits) {
        cwUnits::LengthUnit currentUnit = valueUnit.Unit == cwUnits::LengthUnitless ? DefaultUnit : valueUnit.Unit;
        calcValue += cwUnits::convert(valueUnit.Value, currentUnit, unit);
    }
    return calcValue;
}

/**
 * @brief cwLengthInput::validate
 * @return Returns the state of object. It can either be QValidator::Acceptable, QValidator::Intermediate
 * or QValidator::Invalid. QValidator means that the input is almost correct, but isn't quit there
 * yet.
 */
QValidator::State cwLengthInput::validate() const
{
    return State;

}

QValidator::State cwLengthInput::validate(QString distanceStr)
{
    cwLengthInput distance(distanceStr);
    return distance.validate();
}

/**
 * @brief cwLengthInput::isValid
 * @return True if validate() return QValidator::Acceptable else this returns false
 */
bool cwLengthInput::isValid() const
{
    switch(validate()) {
    case QValidator::Acceptable:
        return true;
    default:
        return false;
    }
}

/**
 * @brief cwLengthInput::operator ==
 * @param other
 * @return True if other's value is the same is this object's value, otherwise it returns false
 */
bool cwLengthInput::operator ==(const cwLengthInput &other) const
{
    return value() == other.value();
}

/**
 * @brief cwLengthInput::operator !=
 * @param other
 * @return Return true if the other's value is not equal to this object's value, otherwise it's returns true
 */
bool cwLengthInput::operator !=(const cwLengthInput &other) const
{
    return !operator ==(other);
}

QList<cwLengthInput::ValueUnit> cwLengthInput::parseInputIntoValueUnits()
{
//    QStringList fields = Input.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    cwDistanceValidator distanceValidator;

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

    auto unitOnly = [&unitNames](const QString& field, QValidator::State& state) {
        //Try to match partual
        QStringList unitNames = cwUnits::lengthUnitNames();
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
            return cwUnits::toLengthUnit(field);
        } catch (QString e) {
            Q_UNUSED(e);
            return cwUnits::LengthUnitless;
        }
    };

    QList<ValueUnit> valueUnits;

    QValidator::State finalState = QValidator::Acceptable;

    QRegularExpressionMatchIterator fieldIter = FieldRegex.globalMatch(value());

    while(fieldIter.hasNext()) {
        QRegularExpressionMatch match = fieldIter.next();

        QValidator::State valueState = QValidator::Acceptable;
        QValidator::State unitState = QValidator::Acceptable;

        QString valueField;
        QString unitField;

//        qDebug() << "Last capture index:" << match.lastCapturedIndex();

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

        ValueUnit valueUnit;
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
