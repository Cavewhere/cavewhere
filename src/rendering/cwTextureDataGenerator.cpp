//Our includes
#include "cwTextureDataGenerator.h"
#include "cwTextureUploadTask.h"
#include "cwDebug.h"

//Qt includes
#include <QTextureImageData>
using namespace Qt3DRender;


cwTextureDataGenerator::cwTextureDataGenerator()
{

}

cwTextureDataGenerator::cwTextureDataGenerator(const QString project,
                                               const cwImage& image,
                                               int mipmapLevel,
                                               int gen,
                                               Qt3DCore::QNodeId texId) :
    Project(project),
    Image(image),
    MipmapLevel(mipmapLevel),
    Generation(gen),
    TextureId(texId)
{

}

Qt3DRender::QTextureImageDataPtr cwTextureDataGenerator::operator ()()
{
    QTextureImageDataPtr textureData = QTextureImageDataPtr::create();

    textureData->setWidth(0);
    textureData->setHeight(0);
    textureData->setMipLevels(0);
    textureData->setDepth(1);
    textureData->setFaces(1);
    textureData->setLayers(1);
    textureData->setTarget(QOpenGLTexture::Target2D);

    cwTextureUploadTask task;
    task.setThreading(cwTextureUploadTask::NoThreading);
    task.setImage(image());
    task.setProjectFilename(project());
    task.setMipmapLevel(MipmapLevel);
    task.setType(cwTextureUploadTask::DXT1Mipmaps);

    auto result = task.mipmaps().result();
    auto mipmaps = result.mipmaps;
    if(mipmaps.isEmpty()) {
        return textureData;
    }
    Q_ASSERT(mipmaps.size() == 1);

    auto firstMipmap = mipmaps.first();

    int blockSize = 0;
    switch(result.type) {
        case cwTextureUploadTask::DXT1Mipmaps:
        textureData->setFormat(QOpenGLTexture::RGB_DXT1);
        blockSize = 8;
        break;
    case cwTextureUploadTask::OpenGL_RGBA:
        textureData->setFormat(QOpenGLTexture::RGBA8_UNorm);
        blockSize = 4;
        break;
    default:
        Q_ASSERT_X(false, LOCATION_STR, "Unhandled texture format");
    }

    textureData->setWidth(firstMipmap.second.width());
    textureData->setHeight(firstMipmap.second.height());
    textureData->setMipLevels(1);
    textureData->setData(firstMipmap.first, blockSize, true);

    return textureData;
}

bool cwTextureDataGenerator::operator ==(const Qt3DRender::QTextureImageDataGenerator& other) const
{
    auto otherPtr = &other;
    if(otherPtr == nullptr) { return false; }
    const cwTextureDataGenerator *otherFunctor = functor_cast<cwTextureDataGenerator>(&other);
    return (otherFunctor != Q_NULLPTR
                            && otherFunctor->Generation == Generation
                            && otherFunctor->TextureId == TextureId);
}
