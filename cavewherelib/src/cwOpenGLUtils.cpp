#include "cwOpenGLUtils.h"

cwOpenGLUtils::cwOpenGLUtils()
{

}

QImage cwOpenGLUtils::toGLTexture(QImage image)
{
    return image.convertToFormat(QImage::Format_RGBA8888).mirrored();
}

int cwOpenGLUtils::fuzzyCompareColors(QColor c1, QColor c2) {
    QVector<int> channelDiff = {
        c1.red() - c2.red(),
        c1.green() - c2.green(),
        c1.blue() - c2.blue(),
        c1.alpha() - c2.alpha()
    };
    std::transform(channelDiff.begin(), channelDiff.end(), channelDiff.begin(), [](int value) { return value * value; });
    int sum = std::accumulate(channelDiff.begin(), channelDiff.end(), 0);
    return sum;
}

QRgb cwOpenGLUtils::toQRgba(unsigned int color) {
    //Get shifty
    int a = (color & 0xFF000000) >> 24;
    int b = (color & 0x00FF0000) >> 16;
    int g = (color & 0x0000FF00) >> 8;
    int r = (color & 0x000000FF);
    return qRgba(r, g, b, a);
}
