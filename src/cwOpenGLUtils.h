#ifndef CWOPENGLUTILS_H
#define CWOPENGLUTILS_H

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwOpenGLUtils
{
public:
    cwOpenGLUtils();

    static QImage toGLTexture(QImage image);
};

#endif // CWOPENGLUTILS_H
