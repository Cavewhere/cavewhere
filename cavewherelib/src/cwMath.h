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
#include <cmath>

inline double cwWrapDegrees360(double degrees) {
    if(!std::isfinite(degrees)) {
        return 0.0;
    }

    double wrapped = std::fmod(degrees, 360.0);
    if(wrapped < 0.0) {
        wrapped += 360.0;
    }

    if(wrapped >= 360.0) {
        wrapped -= 360.0;
    }

    return wrapped;
}

// #ifdef Q_OS_WIN //Need this for x86 windows
// inline double exp2(double value) {
//     return pow(2.0, value);
// }

// inline double log2(double value) {
//     return log(value) / log(2.0);
// }

// #endif

#endif // CWMATH_H
