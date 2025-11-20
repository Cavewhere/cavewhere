/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwImageResolution.h"
#include "cwLength.h"

cwImageResolution::cwImageResolution(QObject *parent) :
    cwUnitValue(0.0, cwUnits::DotsPerMeter, parent)
{
}

cwImageResolution::cwImageResolution(double value, cwUnits::ImageResolutionUnit unit, QObject *parent) :
    cwUnitValue(value, unit, parent)
{
}

/**
  This convert the length to a new length and returns a new
  cwLength object with that value
  */
cwImageResolution::Data cwImageResolution::convertTo(cwUnits::ImageResolutionUnit to) const {
    return convertToHelper(to);
}

/**
 * @brief cwImageResolution::setResolution
 * @param length - The distance of the number of pixels
 * @param numberOfPixels
 *
 * This doesn't change the image resolution units, this simply just updates image's DPI
 */
void cwImageResolution::setResolution(cwLength *length, double numberOfPixels)
{
    cwLength::Data lengthMeter = length->convertTo(cwUnits::Meters);
    double dotsPerMeter = (double)numberOfPixels / lengthMeter.value;
    double value = cwUnits::convert(dotsPerMeter, cwUnits::DotsPerMeter, (cwUnits::ImageResolutionUnit)unit());
    setValue(value);
}

/**
 * @brief cwLength::convertToUnit
 * @param newUnit - The new unit this object is going to convert to
 */
void cwImageResolution::convertToUnit(int newUnit) {
    double newValue = cwUnits::convert(value(), (cwUnits::ImageResolutionUnit)unit(), (cwUnits::ImageResolutionUnit)newUnit);
    setValue(newValue);
}
