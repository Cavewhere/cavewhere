/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwUnits.h"
#include "cwDebug.h"

//Qt includes
#include <QStringList>
#include <QDebug>

/**
  Converts value to the 'to' LengthUnit

  If the valueUnit and to unit are equal, this returns the value.

  */
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
    }
    return LengthUnitless;
}

/**
 * @brief cwUnits::convert
 * @param value - The value in sum resolution
 * @param from - The units of the value
 * @param to - The units of the value that will be converted to
 * @return The value with the from units
 */
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

cwUnits::ImageResolutionUnit cwUnits::toImageResolutionUnit(QString unitString)
{
    if(unitString == "Dots per inch") {
        return DotsPerInch;
    } else if(unitString == "Dots per centimeter") {
        return DotsPerCentimeter;
    } else if(unitString == "Dots per meter") {
        return DotsPerMeter;
    } else {
        qDebug() << "This is a BUG! Can't convert " << unitString << " to image resolution, returning dots per inch";
        return DotsPerInch;
    }
}
