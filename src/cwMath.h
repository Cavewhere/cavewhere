#ifndef CWMATH_H
#define CWMATH_H

/**
  This includes math.h but extends it so the shit work with winblows
  */

#include <math.h>

#ifdef Q_WS_WIN
inline double exp2(double value) {
    return pow(2.0, value);
}

inline double log2(double value) {
    return log(value) / log(2.0);
}

#endif

#endif // CWMATH_H
