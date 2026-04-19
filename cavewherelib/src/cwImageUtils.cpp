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

QTransform cwImageUtils::transformForOrientation(QImageIOHandler::Transformations t)
{
    QTransform tr;
    switch (t) {
    case QImageIOHandler::TransformationNone:
        break;
    case QImageIOHandler::TransformationMirror:
        // (x,y) -> (1-x, y)
        tr.translate(1.0, 0.0);
        tr.scale(-1.0, 1.0);
        break;
    case QImageIOHandler::TransformationFlip:
        // (x,y) -> (x, 1-y)
        tr.translate(0.0, 1.0);
        tr.scale(1.0, -1.0);
        break;
    case QImageIOHandler::TransformationRotate180:
        // (x,y) -> (1-x, 1-y)
        tr.translate(1.0, 1.0);
        tr.scale(-1.0, -1.0);
        break;
    case QImageIOHandler::TransformationRotate90:
        // (x,y) -> (y, 1-x)
        tr.translate(0.0, 1.0);
        tr.rotate(-90);
        break;
    case QImageIOHandler::TransformationMirrorAndRotate90:
        // (x,y) -> (1-y, 1-x)
        tr.translate(1.0, 1.0);
        tr.rotate(90);
        tr.scale(-1.0, 1.0);
        break;
    case QImageIOHandler::TransformationFlipAndRotate90:
        // (x,y) -> (y, x)
        return QTransform(0.0, 1.0, 1.0, 0.0, 0.0, 0.0);
    case QImageIOHandler::TransformationRotate270:
        // (x,y) -> (1-y, x)
        tr.translate(1.0, 0.0);
        tr.rotate(90);
        break;
    }
    return tr;
}

QTransform cwImageUtils::reNormalizationTransform(const QSize& wrongSize, const QSize& correctSize)
{
    const double sx = double(wrongSize.width()) / correctSize.width();
    const double sy = double(wrongSize.height()) / correctSize.height();
    // Maps: x' = x*sx, y' = y*sy + (1-sy)
    QTransform t;
    t.translate(0.0, 1.0 - sy);
    t.scale(sx, sy);
    return t;
}
