#ifndef DEWALLS_ANGLE_H
#define DEWALLS_ANGLE_H

#include <QString>
#include "unittype.h"

namespace dewalls {

class Angle : public UnitType<Angle>
{
public:
    static const Angle * const type;
    static const Unit<Angle> * const degrees;
    static const Unit<Angle> * const deg;
    static const Unit<Angle> * const radians;
    static const Unit<Angle> * const rad;
    static const Unit<Angle> * const gradians;
    static const Unit<Angle> * const grad;
    static const Unit<Angle> * const percentGrade;
    static const Unit<Angle> * const percent;
private:
    Angle();
};

} // namespace dewalls

#endif // DEWALLS_ANGLE_H
