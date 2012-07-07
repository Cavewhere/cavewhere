#include "cwImageResolution.h"

cwImageResolution::cwImageResolution(QObject *parent) :
    cwUnitValue(parent)
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
    double convertedValue = cwUnits::convert(value(), (cwUnits::ImageResolutionUnit)unit(), (cwUnits::ImageResolutionUnit)to);
    return cwImageResolution(convertedValue, to);
}
