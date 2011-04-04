#ifndef CWUNITCOVERTER_H
#define CWUNITCOVERTER_H

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

#endif // CWUNITCOVERTER_H
