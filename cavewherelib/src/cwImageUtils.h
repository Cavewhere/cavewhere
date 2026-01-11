#ifndef CWIMAGEUTILS_H
#define CWIMAGEUTILS_H

#include "CaveWhereLibExport.h"

#include <QByteArray>
#include <QImage>
#include <QStringList>

class CAVEWHERE_LIB_EXPORT cwImageUtils
{
public:
    static QStringList supportedImageFormats();
    static QImage imageWithAutoTransform(QByteArray& data, const QByteArray& format);
};

#endif // CWIMAGEUTILS_H
