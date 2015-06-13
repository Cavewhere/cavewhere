#ifndef UNITIZEDMATH_H
#define UNITIZEDMATH_H

#include "angle.h"
#include "unitizeddouble.h"
#include <cmath>

namespace dewalls
{

inline double usin(UnitizedDouble<Angle> u) {
    return sin(u.get(Angle::radians));
}

inline UnitizedDouble<Angle> uasin(double value) {
    return UnitizedDouble<Angle>(asin(value), Angle::radians);
}

inline double ucos(UnitizedDouble<Angle> u) {
    return cos(u.get(Angle::radians));
}

inline UnitizedDouble<Angle> uacos(double value) {
    return UnitizedDouble<Angle>(acos(value), Angle::radians);
}

inline double utan(UnitizedDouble<Angle> u) {
    return tan(u.get(Angle::radians));
}

inline UnitizedDouble<Angle> uatan(double value) {
    return UnitizedDouble<Angle>(atan(value), Angle::radians);
}

inline UnitizedDouble<Angle> uatan2(double y, double x) {
    return UnitizedDouble<Angle>(atan2(y, x), Angle::radians);
}

}

#endif // UNITIZEDMATH_H

