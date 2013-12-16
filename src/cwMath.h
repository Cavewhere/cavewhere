/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMATH_H
#define CWMATH_H

/**
  This includes math.h but extends it so the shit work with winblows
  */

#include <math.h>
#include <QtGlobal>

#ifdef Q_OS_WIN //Need this for x86 windows
inline double exp2(double value) {
    return pow(2.0, value);
}

inline double log2(double value) {
    return log(value) / log(2.0);
}

#endif

inline double pi() {
    return acos(-1);
}

inline double degreeToRadians() {
    return pi() / 180.0;
}

inline double radianToDegrees() {
    return pi() / 180.0;
}

/**
 * @brief roundToDecimal
 * @param value
 * @param decimal
 * @return
 *
 *  This rounds value to the nearest decimal.
 *
 *  For example roundToDecimal(23.42362, 3) will return 23.424
 */
inline double roundToDecimal(double value, int decimal = 0) {
    double multiple = pow(10.0, decimal);
    return qRound(value * multiple) / multiple;
}


#endif // CWMATH_H
