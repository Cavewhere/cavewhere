#include "cwImageResolution.h"

cwImageResolution::cwImageResolution(QObject *parent) :
    cwUnitValue(0.0, cwUnits::DotsPerMeter, parent)
{
}

cwImageResolution::cwImageResolution(double value, cwUnits::ImageResolutionUnit unit, QObject *parent) :
    cwUnitValue(value, unit, parent)
{
}

cwImageResolution::cwImageResolution(const cwImageResolution &other) :
    cwUnitValue(other)
{

}

/**
  This convert the length to a new length and returns a new
  cwLength object with that value
  */
cwImageResolution cwImageResolution::convertTo(cwUnits::ImageResolutionUnit to) const {
    cwImageResolution resolution = *this;
    resolution.convertToUnit(to);
    return resolution;
}

/**
 * @brief cwLength::convertToUnit
 * @param newUnit - The new unit this object is going to convert to
 */
void cwImageResolution::convertToUnit(int newUnit) {
    double newValue = cwUnits::convert(value(), (cwUnits::ImageResolutionUnit)unit(), (cwUnits::ImageResolutionUnit)newUnit);
    setValue(newValue);
}
