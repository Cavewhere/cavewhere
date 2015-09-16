/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwUnits.h"
#include "cwGlobals.h"
#include "cwDebug.h"

//Qt includes
#include <QStringList>
#include <QDebug>

//Std includes
#include <math.h>

double cwUnits::LengthUnitsToMeters[cwUnits::Miles + 1] = {0.0254, //Inches
                                                           0.3048, //Feet
                                                           0.9144, //Yard
                                                           1.0, //Meter
                                                           0.001, //millimeter
                                                           0.01, //cm
                                                           1000.0, //km
                                                           0.0, //Unitless
                                                           1609.340 //Miles
                                                          };


double cwUnits::ResolutionUnitToDotPerMeters[cwUnits::DotsPerMeter + 1] = {
      39.3700787, //Dots per inch
      100.0, //Dots per centimeter
      1.0, //Dots per meter
};

double cwUnits::AngleUnitToDegrees[cwUnits::PercentGrade + 1] = {
    1.0, //Degrees
    1.0/60.0, //Minutes
    1.0/3600.0, //Seconds
    0.9, //Grads
    0.05625, //Mils
    0.0 //Percent, non-linear must be handled in function
};


/**
  Converts value to the 'to' LengthUnit

  If the valueUnit and to unit are equal, this returns the value.

  */
double cwUnits::convert(double value, cwUnits::LengthUnit from, cwUnits::LengthUnit to) {
    if(from == LengthUnitless || to == LengthUnitless) {
        return value;
    }

    if(to < 0 || to > Miles ) {
        qDebug() << "Can't convert to unit" << LOCATION;
        return value;
    }

    if(from < 0 || from > Miles) {
        qDebug() << "Can't convert from unit" << LOCATION;
        return value;
    }

    double fromFactor = LengthUnitsToMeters[from];
    double toFactor = LengthUnitsToMeters[to];
    return convert(value, fromFactor, toFactor);
}

/**
    Get's all the unit names for the cwUnit
  */
QStringList cwUnits::lengthUnitNames()
{
    QStringList units;
    for(int i = Inches; i <= Miles; i++) {
        units.append(unitName((LengthUnit)i));
    }
    return units;
}

/**
  Get's a unit name for a unit
  */
QString cwUnits::unitName(cwUnits::LengthUnit unit)
{
    switch(unit) {
    case Inches:
        return "in";
    case Feet:
        return "ft";
    case Yards:
        return "yd";
    case Meters:
        return "m";
    case Millimeters:
        return "mm";
    case Centimeters:
        return "cm";
    case Kilometers:
        return "km";
    case LengthUnitless:
        return "unitless";
    case Miles:
        return "mi";
    default:
        return "error";
    }
}

/**
  \brief Converts a string to the enumerate length

  If the string doesn't match any of the units, this will return UnitLess
  */
cwUnits::LengthUnit cwUnits::toLengthUnit(QString unitString) {
    unitString = unitString.toLower();

    if(unitString == "ft") {
        return Feet;
    } else if(unitString == "feet") {
        return Feet;
    } else if(unitString == "metric") {
        return Meters;
    } else if(unitString == "meters") {
        return Meters;
    } else if(unitString == "metres") {
        return Meters;
    } else if(unitString == "m") {
        return Meters;
    } else if(unitString == "yards") {
        return Yards;
    } else if(unitString == "yd") {
        return Yards;
    } else if(unitString == "in") {
        return Inches;
    } else if(unitString == "inches") {
        return Inches;
    } else if(unitString == "inch") {
        return Inches;
    } else if(unitString == "mm") {
        return Millimeters;
    } else if(unitString == "cm") {
        return Centimeters;
    } else if(unitString == "km") {
        return Kilometers;
    } else if(unitString == "mi") {
        return Miles;
    } else if(unitString == "unitless") {
        return LengthUnitless;
    }

    throw QString("Invalid length unitString:") + unitString;
}

/**
 * @brief cwUnits::canConvertLengthUnit
 * @param unitString - The unit string, for example "ft" will return true, where as "asentu" return false
 * @return True if toLengthUnit() will work and false if it'll fail
 */
bool cwUnits::canConvertLengthUnit(QString unitString)
{
    try {
        toLengthUnit(unitString);
    } catch (QString error) {
        Q_UNUSED(error);
        return false;
    }
    return true;
}

/**
 * @brief cwUnits::convert
 * @param value - The value in sum resolution
 * @param from - The units of the value
 * @param to - The units of the value that will be converted to
 * @return The value with the from units
 */
double cwUnits::convert(double value, cwUnits::ImageResolutionUnit from, cwUnits::ImageResolutionUnit to)
{
    if(to < 0 || to > DotsPerMeter ) {
        qDebug() << "Can't convert to unit" << LOCATION;
        return value;
    }

    if(from < 0 || from > DotsPerMeter) {
        qDebug() << "Can't convert from unit" << LOCATION;
        return value;
    }

    double fromFactor = ResolutionUnitToDotPerMeters[from];
    double toFactor = ResolutionUnitToDotPerMeters[to];
    return convert(value, fromFactor, toFactor);
}

/**
 * @brief cwUnits::imageResolutionUnitNames
 * @return All the names of all the units
 *.ep.e
 */
QStringList cwUnits::imageResolutionUnitNames()
{
    QStringList units;
    for(int i = DotsPerInch; i <= DotsPerMeter; i++) {
        units.append(unitName((ImageResolutionUnit)i));
    }
    return units;

}

/**
  Get's a unit name for a unit
  */
QString cwUnits::unitName(cwUnits::ImageResolutionUnit unit)
{
    switch(unit) {
    case DotsPerInch:
        return "Dots per inch";
    case DotsPerCentimeter:
        return "Dots per centimeter";
    case DotsPerMeter:
        return "Dots per meter";
    default:
        return "Unknown";
    }
}

/**
 * @brief cwUnits::toImageResolutionUnit
 * @param unitString
 * @return The enumurated type for the image resolution from unitString
 */
cwUnits::ImageResolutionUnit cwUnits::toImageResolutionUnit(QString unitString)
{
    unitString = unitString.toLower();
    if(unitString == "dots per inch") {
        return DotsPerInch;
    } else if(unitString == "dots per centimeter") {
        return DotsPerCentimeter;
    } else if(unitString == "dots per meter") {
        return DotsPerMeter;
    }

    throw QString("Invalid image resolution unitString:") + unitString;
}

/**
 * @brief cwUnits::canConvertResolutionUnit
 * @param unitString - The unit string, for example "Dots per inch" will return true, where as "asentu" return false
 * @return True if toImageResolutionUnit() will work and false if it'll fail
 */
bool cwUnits::canConvertResolutionUnit(QString unitString)
{
    try {
        toImageResolutionUnit(unitString);
    } catch (QString error) {
        Q_UNUSED(error);
        return false;
    }
    return true;
}


/**
 * @brief cwUnits::convert
 * @param value
 * @param from
 * @param to
 * @return
 */
double cwUnits::convert(double value, cwUnits::AngleUnit from, cwUnits::AngleUnit to)
{
    if(to < 0 || to > PercentGrade ) {
        qDebug() << "Can't convert to unit" << LOCATION;
        return value;
    }

    if(from < 0 || from > PercentGrade) {
        qDebug() << "Can't convert from unit" << LOCATION;
        return value;
    }

    if(from == PercentGrade || to == PercentGrade) {
        //Handle percent grade calculation


        if(from == to) {
            return value;
        } else if(from != PercentGrade) {
            value = cwUnits::convert(value, from, Degrees); //Recusive call to convert to degrees
            from = Degrees;
        } else if(to != PercentGrade) {
            value = cwUnits::convert(value, to, Degrees); //Recusive call to convert to degrees
            to = Degrees;
        }

        if(from == Degrees) {
            Q_ASSERT(to == PercentGrade);
            value = qMin(90.0, qMax(-90.0, value));
            double result = tan(value * cwGlobals::DegreesToRadians) * 100.0;
            return result;
        }

        //Converting from percent gradiant to degrees
        Q_ASSERT(from == PercentGrade);
        Q_ASSERT(to == Degrees);

        double result = atan(value / 100.0) * cwGlobals::RadiansToDegrees;
        return result;
    }

    double fromFactor = AngleUnitToDegrees[from];
    double toFactor = AngleUnitToDegrees[to];
    return convert(value, fromFactor, toFactor);
}

/**
 * @brief cwUnits::angleUnitNames
 * @return
 */
QStringList cwUnits::angleUnitNames()
{
    QStringList units;
    for(int i = Degrees; i <= Mils; i++) {
        units.append(unitName((AngleUnit)i));
    }
    return units;
}

/**
 * @brief cwUnits::unitName
 * @param unit
 * @return
 */
QString cwUnits::unitName(cwUnits::AngleUnit unit)
{
    switch(unit) {
    case Degrees:
        return "deg";
    case Minutes:
        return "min";
    case Seconds:
        return "sec";
    case Gradians:
        return "grad";
    case Mils:
        return "mils";
    case PercentGrade:
        return "%";
    default:
        return "Unknown";
    }
}

/**
 * @brief cwUnits::toAngleUnit
 * @param unitString
 * @return
 */
cwUnits::AngleUnit cwUnits::toAngleUnit(QString unitString)
{
    unitString = unitString.toLower();
    if(unitString == "deg") {
        return Degrees;
    } else if(unitString == "degs") {
        return Degrees;
    } else if(unitString == "°") {
        return Degrees;
    } else if(unitString == "degrees") {
        return Degrees;
    } else if(unitString == "degree") {
        return Degrees;
    } else if(unitString == "min") {
        return Minutes;
    } else if(unitString == "mins") {
        return Minutes;
    } else if(unitString == "'") {
        return Minutes;
    } else if(unitString == "minute") {
        return Minutes;
    } else if(unitString == "minutes") {
        return Minutes;
    } else if(unitString == "sec") {
        return Seconds;
    } else if(unitString == "secs") {
        return Seconds;
    } else if(unitString == "\"") {
        return Seconds;
    } else if(unitString == "second") {
        return Seconds;
    } else if(unitString == "seconds") {
        return Seconds;
    } else if(unitString == "grad") {
        return Gradians;
    } else if(unitString == "gradian") {
        return Gradians;
    } else if(unitString == "gradians") {
        return Gradians;
    } else if(unitString == "mils") {
        return Mils;
    } else if(unitString == "mil") {
        return Mils;
    } else if(unitString == "percent") {
        return PercentGrade;
    } else if(unitString == "%") {
        return PercentGrade;
    }

    throw QString("Invalid angle unitString:") + unitString;
}

/**
 * @brief cwUnits::canConvertResolutionUnit
 * @param unitString - The unit string, for example "degrees" will return true, where as "asentu" return false
 * @return True if toAngleUnit() will work and false if it'll fails
 */
bool cwUnits::canConvertAngleUnit(QString unitString)
{
    try {
        toAngleUnit(unitString);
    } catch (QString error) {
        Q_UNUSED(error);
        return false;
    }
    return true;
}

/**
 * @brief cwUnits::toUnitAndType
 * @param unitString
 * @return param - unit - The unit that that's guess'ed, will be cwUnits::LengthUnit, cwUnits::AngleUnit, cwUnits::ImageResolutionUnit
 * @return param - type - The unit's type, see cwUnits::UnitType
 *
 * This attempts to convert a string to unit and type of unit. If this fails to guess unit, this
 * throws and exception.
 */
void cwUnits::toUnitAndType(QString unitString, int* unit, cwUnits::UnitType* type)
{
    if(cwUnits::canConvertLengthUnit(unitString)) {
        *unit = cwUnits::toLengthUnit(unitString);
        *type = cwUnits::LengthUnitType;
    } else if(cwUnits::canConvertAngleUnit(unitString)) {
        *unit = cwUnits::toAngleUnit(unitString);
        *type = cwUnits::AngleUnitType;
    } else if(cwUnits::canConvertResolutionUnit(unitString)) {
        *unit = cwUnits::toImageResolutionUnit(unitString);
        *type = cwUnits::ImageResolutionUnitType;
    } else {
        *unit = -1;
        *type = cwUnits::InvalidUnitType;
        throw QString("Can't convert unitString");
    }
}

/**
 * @brief cwUnits::toString
 * @param unit - The unit
 * @param type - The type of unit, see cwUnits::UnitType
 * @return The string version of unit
 */
QString cwUnits::toString(int unit, cwUnits::UnitType type)
{
    switch(type) {
    case LengthUnitType:
        return unitName((LengthUnit)unit);
    case AngleUnitType:
        return unitName((AngleUnit)unit);
    case ImageResolutionUnitType:
        return unitName((ImageResolutionUnit)unit);
    default:
        return QString("");
    }
}


