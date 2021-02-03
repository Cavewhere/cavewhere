//Our includes
#include "cwTextureDataGenerator.h"
#include "cwTextureUploadTask.h"
#include "cwDebug.h"

//Qt includes
#include <QTextureImageData>
using namespace Qt3DRender;

//Async includes
#include "asyncfuture.h"


cwTextureDataGenerator::cwTextureDataGenerator()
{

}

cwTextureDataGenerator::cwTextureDataGenerator(const QByteArray& data,
                                               QSize size,
                                               int mipmapLevel,
                                               cwTextureUploadTask::Format format,
                                               int imageId) :
    Data(data),
    Size(size),
    MipmapLevel(mipmapLevel),
    Format(format),
    ImageId(imageId)
{

}

Qt3DRender::QTextureImageDataPtr cwTextureDataGenerator::operator ()()
{
    QTextureImageDataPtr textureData = QTextureImageDataPtr::create();

    if(Data.isEmpty()) {
        textureData->setWidth(1);
        textureData->setHeight(1);
        textureData->setMipLevels(1);
        textureData->setDepth(1);
        textureData->setFaces(1);
        textureData->setLayers(1);
        textureData->setTarget(QOpenGLTexture::Target2D);
        textureData->setFormat(QOpenGLTexture::RGBA8_UNorm);
        textureData->setPixelFormat(QOpenGLTexture::RGBA);
        textureData->setPixelType(QOpenGLTexture::UInt8);

        QImage redImage(1, 1, QImage::Format_RGBA8888);
        redImage.fill(Qt::red);

        textureData->setData(QByteArray(reinterpret_cast<const char*>(redImage.constBits()), redImage.sizeInBytes()),
                             4,
                             false);

        return textureData;
    }

    textureData->setWidth(Size.width());
    textureData->setHeight(Size.height());
    textureData->setMipLevels(1);
    textureData->setDepth(1);
    textureData->setFaces(1);
    textureData->setLayers(1);
    textureData->setTarget(QOpenGLTexture::Target2D);

    int blockSize = 0;
    bool isCompressed = false;
    switch(Format) {
    case cwTextureUploadTask::DXT1Mipmaps:
        textureData->setFormat(QOpenGLTexture::RGB_DXT1);
        blockSize = 8;
        isCompressed = true;
        break;
    case cwTextureUploadTask::OpenGL_RGBA:
        textureData->setFormat(QOpenGLTexture::RGBA8_UNorm);
        blockSize = 4;
        isCompressed = false;
        break;
    default:
        Q_ASSERT_X(false, LOCATION_STR, "Unhandled texture format");
    }

    textureData->setPixelFormat(QOpenGLTexture::RGBA);
    textureData->setPixelType(QOpenGLTexture::UInt8);
    textureData->setData(Data, blockSize, isCompressed);

    return textureData;
}

bool cwTextureDataGenerator::operator ==(const Qt3DRender::QTextureImageDataGenerator& other) const
{
    auto otherPtr = &other;
    if(otherPtr == nullptr) { return false; }
    const cwTextureDataGenerator *otherFunctor = functor_cast<cwTextureDataGenerator>(&other);

    return (otherFunctor != Q_NULLPTR
            && otherFunctor->Format == Format
            && otherFunctor->Size == Size
            && otherFunctor->MipmapLevel == MipmapLevel
            && otherFunctor->ImageId == ImageId
            && otherFunctor->Data.size() == Data.size());
}
