/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwUnitValueList.h"
#include "cwUnits.h"

//Qt includes
#include <QVariant>
#include <QDebug>

class ValueUnit {
public:
    ValueUnit() :
        Value(0.0),
        Unit(-1)
    {

    }

    bool operator!=(const ValueUnit& other) const { return !operator==(other); }
    bool operator==(const ValueUnit& other) const { return Value == other.Value && Unit == other.Unit; }

    double Value;
    int Unit; //See cwUnits for enum of units
};

class cwUnitValueListData : public QSharedData
{
public:
    cwUnitValueListData() : Type(cwUnits::InvalidUnitType)
    { }

    cwUnits::UnitType Type;
    QList<ValueUnit> ValuesAndUnits;
    QString InvalidString;

};

cwUnitValueList::cwUnitValueList() : data(new cwUnitValueListData)
{

}

/**
 * @brief cwUnitValueList::cwUnitValueList
 * @param listUnitAndValues
 *
 * This will convert a string to cwUnitValueList. If the string is invalid (units are wrong, double
 * numbers, strange characters) this will save the invalid text. The invalid text will be return in
 * the toString.
 */
cwUnitValueList::cwUnitValueList(const QString &listUnitAndValues) : data(new cwUnitValueListData)
{   
    QRegExp valueUnitRegex("(?:[-+]?\\d*\\.?\\d+)|(?:[A-Za-z]+)\\b");
    QRegExp whiteSpace("\\s*,?\\s*");

    try {

        //Extract the units and values out of the unit value
        QStringList strList;
        int pos = 0;
        int lastPos = 0;
        while((pos = valueUnitRegex.indexIn(listUnitAndValues, pos)) != -1) {

            //Check skip text
            int foundWhiteSpaceAt = whiteSpace.indexIn(listUnitAndValues, lastPos);
            if(foundWhiteSpaceAt < pos &&
                    foundWhiteSpaceAt != -1 &&
                    (foundWhiteSpaceAt + whiteSpace.matchedLength() != pos ||
                    foundWhiteSpaceAt != lastPos))
            {
                throw QString("Unexpect text %1").arg(whiteSpace.cap(0));
            }

            //Add the match text
            strList.append(valueUnitRegex.cap(0));

            //Move the index
            pos += valueUnitRegex.matchedLength();
            lastPos = pos;
        }

        if(strList.isEmpty()) {
            throw QString("Invalid value");
        }

        //Join words together with spaces (units that have space in them)
        QStringList valuesAndUnitsStringList;
        QString unit;
        bool okay;
        foreach(QString value, strList) {
            value.toDouble(&okay);
            if(okay) {
                valuesAndUnitsStringList.append(value);
                unit.clear();
            } else {
                //This is probably a unit, add space between already existing units, else just add the value
                if(!unit.isEmpty()) {
                    unit = QString(" ") + value;
                    valuesAndUnitsStringList.last() += unit;
                } else {
                    unit = value;
                    valuesAndUnitsStringList.append(unit);
                }
            }
        }

        //Go through the string list and try to create a list of value and units
        QList<ValueUnit> valuesAndUnits;
        cwUnits::UnitType unitType = cwUnits::InvalidUnitType;

        //values and units must alternate
        for(int i = 0; i < valuesAndUnitsStringList.size(); i++) {
            const QString& str = valuesAndUnitsStringList.at(i);
            if(i % 2 == 0) {
                double value = str.toDouble(&okay);

                if(!okay) {
                    //Invalid
                    throw QString("Invalid value");
                }

                ValueUnit newValueUnit;
                newValueUnit.Value = value;
                valuesAndUnits.append(newValueUnit);
            } else {

                int unit;
                cwUnits::UnitType currentUnitType;
                cwUnits::toUnitAndType(str, &unit, &currentUnitType);

                if(i == 1) {
                    //Set the unitType
                    unitType = currentUnitType;
                } else if(currentUnitType != unitType) {
                    //Mixing units
                    throw QString("Mixing units");
                }

                valuesAndUnits.last().Unit = unit;
            }
        }

        data->Type = unitType;
        data->ValuesAndUnits = valuesAndUnits;

    } catch (QString) {
        data->InvalidString = listUnitAndValues;
        return;
    }
}

cwUnitValueList::cwUnitValueList(const cwUnitValueList &rhs) : data(rhs.data)
{

}

/**
 * @brief cwUnitValueList::operator =
 * @param rhs
 * @return A copy of this object
 *
 * This class uses QSharedData
 */
cwUnitValueList &cwUnitValueList::operator=(const cwUnitValueList &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwUnitValueList::~cwUnitValueList()
{

}

/**
 * @brief cwUnitValueList::operator !=
 * @param other
 * @return True if the other is not equal to this object and false if it doesn't
 */
bool cwUnitValueList::operator !=(const cwUnitValueList &other)
{
    return !operator==(other);
}

/**
 * @brief cwUnitValueList::operator ==
 * @param other
 * @return True if other is eqaul to this object and false if it doesn't
 */
bool cwUnitValueList::operator ==(const cwUnitValueList &other)
{
    if(other.data == data) {
        return true;
    }

    if(isValid() == other.isValid()) {
        if(!isValid()) {
            return data->InvalidString == other.data->InvalidString;
        } else {
            if(data->Type != data->Type) {
                return false;
            }

            if(data->ValuesAndUnits.size() != data->ValuesAndUnits.size()) {
                return false;
            }

            for(int i = 0; i < data->ValuesAndUnits.size(); i++) {
                const ValueUnit& value = data->ValuesAndUnits.at(i);
                const ValueUnit& otherValue = data->ValuesAndUnits.at(i);

                if(value != otherValue) {
                    return false;
                }
            }

            return true;
        }
    }
    return false;
}

/**
 * @brief cwUnitValueList::value
 * @param unit
 * @return Returns the value of the list toUnit. If the list doesn't have a unit, the defaultFromUnit
 * is used
 */
double cwUnitValueList::value(cwUnits::LengthUnit defaultFromUnit, cwUnits::LengthUnit toUnit) const
{
    Q_ASSERT(data->Type == cwUnits::InvalidUnitType || data->Type == cwUnits::LengthUnitType);
    return value((int)defaultFromUnit, (int)toUnit, cwUnits::LengthUnitType);
}

/**
 * @brief cwUnitValueList::value
 * @param unit
 * @return Returns the value of the list toUnit. If the list doesn't have a unit, the defaultFromUnit
 * is used
 */
double cwUnitValueList::value(cwUnits::ImageResolutionUnit defaultFromUnit, cwUnits::ImageResolutionUnit toUnit) const
{
    Q_ASSERT(data->Type == cwUnits::InvalidUnitType || data->Type == cwUnits::ImageResolutionUnitType);
    return value((int)defaultFromUnit, (int)toUnit, cwUnits::ImageResolutionUnitType);
}

/**
 * @brief cwUnitValueList::value
 * @param defaultFromUnit
 * @param unit
 * @return Returns the value of the list toUnit. If the list doesn't have a unit, the defaultFromUnit
 * is used
 */
double cwUnitValueList::value(cwUnits::AngleUnit defaultFromUnit, cwUnits::AngleUnit toUnit) const
{
    Q_ASSERT(data->Type == cwUnits::InvalidUnitType || data->Type == cwUnits::AngleUnitType);
    return value((int)defaultFromUnit, (int)toUnit, cwUnits::AngleUnitType);
}

/**
 * @brief cwUnitValueList::type
 * @return Return's the type of unit value list. See cwUnits::UnitType for details
 *
 * This type is useful for this class to covert the value to a different singler unit. For example
 * "5ft 1in" to meters.
 */
cwUnits::UnitType cwUnitValueList::type() const
{
    return data->Type;
}

/**
 * @brief cwUnitValueList::isValid
 * @return True if list is valid and false if it's invalid
 */
bool cwUnitValueList::isValid() const
{
    return data->InvalidString.isEmpty() && !data->ValuesAndUnits.isEmpty();
}

/**
 * @brief cwUnitValueList::toString
 * @return String represntation of the cwUnitValueList
 *
 * If this object isn't valid, this will return the original string that was passed
 * to this object (in constructor or fromString()). If the object is valid, the string
 * will be made up of value and unit pairs. There's no spaces between the value and the unit.
 * For example this could return "10.5ft 3in"
 */
QString cwUnitValueList::toString() const
{
    if(!isValid()) {
        return data->InvalidString;
    }

    QString templateStr("%1%2");
    QString str;
    for(int i = 0; i < data->ValuesAndUnits.size(); i++) {
        const ValueUnit& value = data->ValuesAndUnits.at(i);
        QString unitStr = cwUnits::toString(value.Unit, data->Type);
        str += (i != 0 ? " " : "") + templateStr.arg(value.Value).arg(unitStr);
    }

    return str;
}

/**
 * @brief cwUnitValueList::toList
 * @return Converts this object to QVariantList. The variant list will be made up of
 * [value], or [value, unit] or [value, unit, value, unit, etc...] where value is a
 * double and unit is enum value found in cwUnits (LengthUnit, AngleUnit, or ImageResolutionUnit)
 */
QVariantList cwUnitValueList::toList() const
{
    QVariantList list;
    foreach(ValueUnit value, data->ValuesAndUnits) {
        //Add the value
        list.append(value.Value);

        //Add the unit
        if(data->Type != cwUnits::InvalidUnitType) {
            QString unit = cwUnits::toString(value.Unit, data->Type);
            list.append(unit);
        }
    }

    return list;
}

/**
 * @brief cwUnitValueList::fromString
 * @param string
 * @return
 */
cwUnitValueList cwUnitValueList::fromString(const QString &string)
{
    return cwUnitValueList(string);
}

/**
 * @brief cwUnitValueList::value
 * @param unit
 * @return
 */
double cwUnitValueList::value(int defaultFromUnit, int toUnit, cwUnits::UnitType unitType) const
{
    Q_ASSERT(isValid());

    double toReturn = 0.0;
    foreach(const ValueUnit& valueUnit, data->ValuesAndUnits) {

        int unit = valueUnit.Unit == -1 ? defaultFromUnit : valueUnit.Unit;

        switch(unitType) {
        case cwUnits::LengthUnitType:
            toReturn += cwUnits::convert(valueUnit.Value, (cwUnits::LengthUnit)unit, (cwUnits::LengthUnit)toUnit);
            break;
        case cwUnits::AngleUnitType:
            toReturn += cwUnits::convert(valueUnit.Value, (cwUnits::AngleUnit)unit, (cwUnits::AngleUnit)toUnit);
            break;
        case cwUnits::ImageResolutionUnitType:
            toReturn += cwUnits::convert(valueUnit.Value, (cwUnits::ImageResolutionUnit)unit, (cwUnits::ImageResolutionUnit)toUnit);
            break;
        default:
            throw QString("Can't convert to unit value, type invalid");
        }
    }
    return toReturn;
}



