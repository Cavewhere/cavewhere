#ifndef CWUNITCOVERTER_H
#define CWUNITCOVERTER_H

class cwUnitConverter
{
public:
    enum LengthUnit {
        Meters,
        DecimalFeet,
        NumberOfUnits
    };


    static double convert(double value,
                          cwUnitConverter::LengthUnit valuesUnit,
                          cwUnitConverter::LengthUnit to);

    private:
        static double UnitsToMeters[NumberOfUnits];

};

#endif // CWUNITCOVERTER_H
