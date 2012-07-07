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
    double convertedValue = cwUnits::convert(value(), (cwUnits::LengthUnit)unit(), (cwUnits::LengthUnit)to);
    return cwLength(convertedValue, to);
}
