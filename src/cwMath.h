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

#endif // CWMATH_H
