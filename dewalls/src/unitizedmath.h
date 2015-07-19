#ifndef UNITIZEDMATH_H
#define UNITIZEDMATH_H

#include "angle.h"
#include "unitizeddouble.h"
#include <cmath>

namespace dewalls
{

inline double usin(UnitizedDouble<Angle> u) {
    return sin(u.get(Angle::radians()));
}

inline UnitizedDouble<Angle> uasin(double value) {
    return UnitizedDouble<Angle>(asin(value), Angle::radians());
}

inline double ucos(UnitizedDouble<Angle> u) {
    return cos(u.get(Angle::radians()));
}

inline UnitizedDouble<Angle> uacos(double value) {
    return UnitizedDouble<Angle>(acos(value), Angle::radians());
}

inline double utan(UnitizedDouble<Angle> u) {
    return tan(u.get(Angle::radians()));
}

inline UnitizedDouble<Angle> uatan(double value) {
    return UnitizedDouble<Angle>(atan(value), Angle::radians());
}

inline UnitizedDouble<Angle> uatan2(double y, double x) {
    return UnitizedDouble<Angle>(atan2(y, x), Angle::radians());
}

template<class T>
inline UnitizedDouble<Angle> uatan2(UnitizedDouble<T> y, UnitizedDouble<T> x) {
    return UnitizedDouble<Angle>(atan2(y.get(y.unit()), x.get(y.unit())), Angle::radians());
}

template<class T>
inline UnitizedDouble<T> usq(UnitizedDouble<T> x) {
    double val = x.get(x.unit());
    return UnitizedDouble<T>(val * val, x.unit());
}

template<class T>
inline UnitizedDouble<T> usqrt(UnitizedDouble<T> x) {
    return UnitizedDouble<T>(sqrt(x.get(x.unit())), x.unit());
}

template<class T>
inline UnitizedDouble<T> uabs(UnitizedDouble<T> x) {
    return UnitizedDouble<T>(abs(x.get(x.unit())), x.unit());
}

}

#endif // UNITIZEDMATH_H

