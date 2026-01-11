#include "cwImageUtils.h"

#include "cwSvgReader.h"

#include <QBuffer>
#include <QImageReader>

QStringList cwImageUtils::supportedImageFormats()
{
    return QStringList({"bmp", "gif", "jpg", "jpeg", "png", "tif", "tiff", "svg", "webp"});
}

QImage cwImageUtils::imageWithAutoTransform(QByteArray& data, const QByteArray& format)
{
    if (cwSvgReader::isSvg(format)) {
        return cwSvgReader::toImage(data, format);
    }

    QBuffer stream(&data);
    QImageReader imageReader(&stream, format);
    imageReader.setAutoTransform(true);
    return imageReader.read();
}
