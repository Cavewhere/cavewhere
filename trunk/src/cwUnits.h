#ifndef CWUNITCOVERTER_H
#define CWUNITCOVERTER_H


//Qt includes
#include <QMetaType>
#include <QObject>


class cwUnits : public QObject
{
    Q_OBJECT

    Q_ENUMS(LengthUnit)
public:
    enum LengthUnit {
        in,       //!< Inches
        ft,       //!< Feet
        yd,       //!< Yards
        m,        //!< Meters
        mm,       //!< Millimeters
        cm,       //!< Centimeters
        km,       //!< Kilometers
        Unitless  //!< Invalid units or unit less
    };


    static double convert(double value,
                          cwUnits::LengthUnit from,
                          cwUnits::LengthUnit to);

    static QStringList lengthUnitNames();
    static QString unitName(cwUnits::LengthUnit unit);
    static cwUnits::LengthUnit toLengthUnit(QString unitString);

    private:
        static double UnitsToMeters[Unitless + 1];

};

Q_DECLARE_METATYPE(cwUnits*)

#endif // CWUNITCOVERTER_H
