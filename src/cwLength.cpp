/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLength.h"

//Qt includes
#include <QDebug>

cwLength::cwLength(QObject *parent) :
    cwUnitValue(parent)
{
}

cwLength::cwLength(double value, cwUnits::LengthUnit unit, QObject* parent) :
    cwUnitValue(value, unit, parent)
{

}

/**
  Copy constructor
  */
cwLength::cwLength(const cwLength& other) :
    cwUnitValue(other)
{

}

/**
  This convert the length to a new length and returns a new
  cwLength object with that value
  */
cwLength cwLength::convertTo(cwUnits::LengthUnit to) const {
    cwLength length = *this;
    length.convertToUnit(to);
    return length;
}

/**
 * @brief cwLength::convertToUnit
 * @param newUnit - The new unit this object is going to convert to
 */
void cwLength::convertToUnit(int newUnit) {
    double newValue = cwUnits::convert(value(), (cwUnits::LengthUnit)unit(), (cwUnits::LengthUnit)newUnit);
    setValue(newValue);
}
