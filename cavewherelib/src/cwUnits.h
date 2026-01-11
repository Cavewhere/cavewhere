/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUNITCOVERTER_H
#define CWUNITCOVERTER_H


//Qt includes
#include <QMetaType>
#include <QObject>
#include <QQmlEngine>
#include <array>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwUnits : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Units)

public:
    // !!NOTICE!! Changing the enum effects SAVE / LOAD and the cwUnit Code !!NOTICE!!
    enum LengthUnit {
        Inches = 0,       //!< Inches
        Feet = 1,       //!< Feet
        Yards = 2,       //!< Yards
        Meters = 3,        //!< Meters
        Millimeters = 4,       //!< Millimeters
        Centimeters = 5,       //!< Centimeters
        Kilometers = 6,       //!< Kilometers
        LengthUnitless = 7,  //!< Invalid units or unit less
        Miles = 8
    };

    // !!NOTICE!! Changing the enum effects SAVE / LOAD and the cwUnit Code !!NOTICE!!
    enum ImageResolutionUnit {
        DotsPerInch = 0,
        DotsPerCentimeter = 1,
        DotsPerMeter = 2
    };

    Q_ENUM(LengthUnit)
    Q_ENUM(ImageResolutionUnit)

    static constexpr double PointsPerInch = 72.0;
    static constexpr double SvgCssDpi = 96.0;

    static constexpr double convert(double value,
                                    cwUnits::LengthUnit from,
                                    cwUnits::LengthUnit to);
    static QStringList lengthUnitNames();
    static QString unitName(cwUnits::LengthUnit unit);
    static cwUnits::LengthUnit toLengthUnit(QString unitString);

    static constexpr double convert(double value,
                                    cwUnits::ImageResolutionUnit from,
                                    cwUnits::ImageResolutionUnit to);
    static QStringList imageResolutionUnitNames();
    static QString unitName(cwUnits::ImageResolutionUnit unit);
    static cwUnits::ImageResolutionUnit toImageResolutionUnit(QString unitString);

private:
    inline static constexpr std::array<double, Miles + 1> LengthUnitsToMeters = {
        0.0254, //Inches
        0.3048, //Feet
        0.9144, //Yard
        1.0, //Meter
        0.001, //millimeter
        0.01, //cm
        1000.0, //km
        0.0, //Unitless
        1609.340 //Miles
    };

    inline static constexpr std::array<double, DotsPerMeter + 1> ResolutionUnitToDotPerMeters = {
        39.3700787, //Dots per inch
        100.0, //Dots per centimeter
        1.0, //Dots per meter
    };

    static constexpr double convert(double value, double fromFactor, double toFactor);

};

/**
 * @brief cwUnits::convert
 *
 * Converts the value to a different unit using the fromFactor and toFactor
 *
 * @param value
 * @param fromFactor - The factor that puts the a common unit
 * @param toFactor - The factor that puts the value into a common unit
 */
inline constexpr double cwUnits::convert(double value, double fromFactor, double toFactor)
{
    return value * fromFactor / toFactor;
}

inline constexpr double cwUnits::convert(double value,
                                         cwUnits::LengthUnit from,
                                         cwUnits::LengthUnit to)
{
    if (from == LengthUnitless || to == LengthUnitless) {
        return value;
    }
    if (to < 0 || to > Miles) {
        return value;
    }
    if (from < 0 || from > Miles) {
        return value;
    }
    return convert(value, LengthUnitsToMeters[from], LengthUnitsToMeters[to]);
}

inline constexpr double cwUnits::convert(double value,
                                         cwUnits::ImageResolutionUnit from,
                                         cwUnits::ImageResolutionUnit to)
{
    if (to < 0 || to > DotsPerMeter) {
        return value;
    }
    if (from < 0 || from > DotsPerMeter) {
        return value;
    }
    return convert(value, ResolutionUnitToDotPerMeters[from], ResolutionUnitToDotPerMeters[to]);
}


Q_DECLARE_METATYPE(cwUnits*)

#endif // CWUNITCOVERTER_H
