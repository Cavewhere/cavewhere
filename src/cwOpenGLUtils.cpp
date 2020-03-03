#include "cwOpenGLUtils.h"

cwOpenGLUtils::cwOpenGLUtils()
{

}

QImage cwOpenGLUtils::toGLTexture(QImage image)
{
    return image.convertToFormat(QImage::Format_RGBA8888).mirrored();
}
