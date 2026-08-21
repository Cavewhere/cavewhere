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
#include <QList>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

//Half a turn around the compass, which is what reversing a bearing adds
constexpr double cwHalfTurnDegrees = 180.0;

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

//! The middle of \a values, averaging the two middle ones on an even count, and
//! 0.0 for an empty list — callers that must tell "no values" apart from a
//! median that happens to be zero guard first. Takes its copy so the caller's
//! order is left intact; pass an rvalue when the list is built for this.
inline double cwMedian(QList<double> values) {
    if(values.isEmpty()) {
        return 0.0;
    }

    std::sort(values.begin(), values.end());
    const qsizetype middle = values.size() / 2;
    if(values.size() % 2 == 1) {
        return values.at(middle);
    }

    return 0.5 * (values.at(middle - 1) + values.at(middle));
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
