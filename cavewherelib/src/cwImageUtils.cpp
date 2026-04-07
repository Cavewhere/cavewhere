#include "cwImageUtils.h"

#include "cwSvgReader.h"

#include <QBuffer>
#include <QImageReader>

#include <algorithm>
#include <cmath>

QStringList cwImageUtils::supportedImageFormats()
{
    return QStringList({"bmp", "gif", "jpg", "jpeg", "png", "tif", "tiff", "svg", "webp"});
}

QSize cwImageUtils::clampImageSize(const QSize& size)
{
    if (!size.isValid()) {
        return size;
    }

    const qint64 maxPixels = maxImageBytes / bytesPerPixel;
    const qint64 pixelCount = static_cast<qint64>(size.width()) * size.height();
    if (pixelCount <= maxPixels) {
        return size;
    }

    const double scale = std::sqrt(static_cast<double>(maxPixels) / static_cast<double>(pixelCount));
    const int width = std::max(1, static_cast<int>(std::floor(size.width() * scale)));
    const int height = std::max(1, static_cast<int>(std::floor(size.height() * scale)));
    return QSize(width, height);
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
