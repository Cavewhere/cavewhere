#ifndef CWIMAGEUTILS_H
#define CWIMAGEUTILS_H

#include "CaveWhereLibExport.h"

#include <QByteArray>
#include <QImage>
#include <QImageIOHandler>
#include <QSize>
#include <QStringList>
#include <QTransform>

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

    //! Returns a QTransform that maps normalized [0,1] coords from
    //! pre-EXIF-rotation space to post-EXIF-rotation space.
    static QTransform transformForOrientation(QImageIOHandler::Transformations t);

    //! Returns a QTransform that re-normalizes coords from \a wrongSize
    //! normalization to \a correctSize normalization, accounting for
    //! CaveWhere's Y-flip convention in toNormalized().
    static QTransform reNormalizationTransform(const QSize& wrongSize, const QSize& correctSize);
};

#endif // CWIMAGEUTILS_H
