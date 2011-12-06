//Our includes
#include "cwUnits.h"
#include "cwDebug.h"

//Qt includes
#include <QStringList>
#include <QDebug>

double cwUnits::UnitsToMeters[cwUnits::Unitless + 1] = {0.0254, //Inches
                                                        0.3048, //Feet
                                                        0.9144, //Yard
                                                        1.0, //Meter
                                                        0.001, //millimeter
                                                        0.01, //cm
                                                        1000.0, //km
                                                        0.0 //Unitless
                                                       };

/**
  Converts value to the 'to' LengthUnit

  If the valueUnit and to unit are equal, this returns the value.

  */
double cwUnits::convert(double value, cwUnits::LengthUnit from, cwUnits::LengthUnit to) {
    if(from == Unitless || to == Unitless) {
        return value;
    }

    if(to < 0 || to > Unitless ) {
        qDebug() << "Can't convert to unit" << LOCATION;
        return value;
    }

    if(from < 0 || from > Unitless) {
        qDebug() << "Can't convert from unit" << LOCATION;
        return value;
    }

    double fromFactor = UnitsToMeters[from];
    double toFactor = UnitsToMeters[to];

    return value * fromFactor / toFactor;
}

/**
    Get's all the unit names for the cwUnit
  */
QStringList cwUnits::lengthUnitNames()
{
    QStringList units;
    for(int i = in; i <= Unitless; i++) {
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
    case in:
        return "in";
    case ft:
        return "ft";
    case yd:
        return "yd";
    case m:
        return "m";
    case mm:
        return "mm";
    case cm:
        return "cm";
    case km:
        return "km";
    case Unitless:
        return "unitless";
    default:
        return "error";
    }
}

