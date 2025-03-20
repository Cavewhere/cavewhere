#ifndef CWOPENGLUTILS_H
#define CWOPENGLUTILS_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QRgb>

class CAVEWHERE_LIB_EXPORT cwOpenGLUtils
{
public:
    cwOpenGLUtils();

    static QImage toGLTexture(QImage image);

    static QRgb toQRgba(unsigned int color);

    //Does sum of the square differenc between color changes
    static int fuzzyCompareColors(QColor c1, QColor c2);
};

#endif // CWOPENGLUTILS_H
