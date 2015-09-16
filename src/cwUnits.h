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


class cwUnits : public QObject
{
    Q_OBJECT

    Q_ENUMS(LengthUnit ImageResolutionUnit)
public:
    enum UnitType {
        InvalidUnitType,
        LengthUnitType,
        ImageResolutionUnitType,
        AngleUnitType
    };

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

    // !!NOTICE!! Changing the enum effects SAVE / LOAD and the cwUnit Code !!NOTICE!!
    enum AngleUnit {
        Degrees = 0,
        Minutes = 1,
        Seconds = 2,
        Gradians = 3,
        Mils = 4,
        PercentGrade = 5
    };

    static double convert(double value,
                          cwUnits::LengthUnit from,
                          cwUnits::LengthUnit to);
    static QStringList lengthUnitNames();
    static QString unitName(cwUnits::LengthUnit unit);
    static cwUnits::LengthUnit toLengthUnit(QString unitString);
    static bool canConvertLengthUnit(QString unitString);

    static double convert(double value,
                          cwUnits::ImageResolutionUnit from,
                          cwUnits::ImageResolutionUnit to);
    static QStringList imageResolutionUnitNames();
    static QString unitName(cwUnits::ImageResolutionUnit unit);
    static cwUnits::ImageResolutionUnit toImageResolutionUnit(QString unitString);
    static bool canConvertResolutionUnit(QString unitString);

    static double convert(double value,
                          cwUnits::AngleUnit from,
                          cwUnits::AngleUnit to);
    static QStringList angleUnitNames();
    static QString unitName(cwUnits::AngleUnit unit);
    static cwUnits::AngleUnit toAngleUnit(QString unitString);
    static bool canConvertAngleUnit(QString unitString);

    static void toUnitAndType(QString unitString, int *unit, UnitType *type);
    static QString toString(int unit, UnitType type);


private:
    static double LengthUnitsToMeters[Miles + 1];
    static double ResolutionUnitToDotPerMeters[DotsPerMeter + 1];
    static double AngleUnitToDegrees[PercentGrade + 1];

    static double convert(double value, double fromFactor, double toFactor);

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
inline double cwUnits::convert(double value, double fromFactor, double toFactor)
{
    return value * fromFactor / toFactor;
}


Q_DECLARE_METATYPE(cwUnits*)

#endif // CWUNITCOVERTER_H
