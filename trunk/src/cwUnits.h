#ifndef CWUNITCOVERTER_H
#define CWUNITCOVERTER_H


//Qt includes
#include <QMetaType>

class cwUnits
{
public:
    enum LengthUnit {
        Meters,
        DecimalFeet,
        NumberOfUnits
    };


    static double convert(double value,
                          cwUnits::LengthUnit valuesUnit,
                          cwUnits::LengthUnit to);

    private:
        static double UnitsToMeters[NumberOfUnits];

};

Q_DECLARE_METATYPE(cwUnits::LengthUnit)

#endif // CWUNITCOVERTER_H
