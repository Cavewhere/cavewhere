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
    // !!NOTICE!! Changing the enum effects SAVE / LOAD and the cwUnit Code !!NOTICE!!
    enum LengthUnit {
        Inches,       //!< Inches
        Feet,       //!< Feet
        Yards,       //!< Yards
        Meters,        //!< Meters
        Millimeters,       //!< Millimeters
        Centimeters,       //!< Centimeters
        Kilometers,       //!< Kilometers
        LengthUnitless,  //!< Invalid units or unit less
        Miles
    };

    // !!NOTICE!! Changing the enum effects SAVE / LOAD and the cwUnit Code !!NOTICE!!
    enum ImageResolutionUnit {
        DotsPerInch,
        DotsPerCentimeter,
        DotsPerMeter
    };

    static double convert(double value,
                          cwUnits::LengthUnit from,
                          cwUnits::LengthUnit to);
    static QStringList lengthUnitNames();
    static QString unitName(cwUnits::LengthUnit unit);
    static cwUnits::LengthUnit toLengthUnit(QString unitString);

    static double convert(double value,
                          cwUnits::ImageResolutionUnit from,
                          cwUnits::ImageResolutionUnit to);
    static QStringList imageResolutionUnitNames();
    static QString unitName(cwUnits::ImageResolutionUnit unit);
    static cwUnits::ImageResolutionUnit toImageResolutionUnit(QString unitString);

private:
    static double LengthUnitsToMeters[Miles + 1];
    static double ResolutionUnitToDotPerMeters[DotsPerMeter + 1];

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
