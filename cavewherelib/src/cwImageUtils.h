#ifndef CWIMAGEUTILS_H
#define CWIMAGEUTILS_H

#include "CaveWhereLibExport.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QStringList>

class CAVEWHERE_LIB_EXPORT cwImageUtils
{
public:
    static QStringList supportedImageFormats();
    static QImage imageWithAutoTransform(QByteArray& data, const QByteArray& format);

    //! The maximum image size in bytes (256 MB). Images exceeding this are
    //! scaled down by clampImageSize().
    static constexpr qint64 maxImageBytes = 256ll * 1024 * 1024;
    static constexpr qint64 bytesPerPixel = 4;

    //! Scale \a size down proportionally so that width*height*bytesPerPixel
    //! fits within maxImageBytes. Returns \a size unchanged when it already fits.
    static QSize clampImageSize(const QSize& size);
};

#endif // CWIMAGEUTILS_H
