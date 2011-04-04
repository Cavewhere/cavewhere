#include "cwUnitCoverter.h"

double cwUnitConverter::UnitsToMeters[cwUnitConverter::NumberOfUnits] = {1.0, //Meters
                                                                         0.3048 //Feet
                                                                        };

/**
  Converts value to the 'to' LengthUnit

  If the valueUnit and to unit are equal, this returns the value.

  */
double cwUnitConverter::convert(double value, cwUnitConverter::LengthUnit valueUnit, cwUnitConverter::LengthUnit to) {
    if(valueUnit == to) {
        return value;
    }

    if(valueUnit != Meters) {
        value *= UnitsToMeters[valueUnit];
    }

    value /= UnitsToMeters[to];
    return value;
}
