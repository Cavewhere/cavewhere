//Our includes
#include "cwTextureDataGenerator.h"
#include "cwTextureUploadTask.h"

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
    textureData->setFormat(QOpenGLTexture::RGB_DXT1);
    textureData->setTarget(QOpenGLTexture::Target2D);

    cwTextureUploadTask task;
    task.setUsingThreadPool(false); //This is already runnig on the thread pool for qt3d
    task.setImage(image());
    task.setProjectFilename(project());
    task.setMipmapLevel(MipmapLevel);
    task.start();

    if(task.mipmaps().isEmpty()) {
        return textureData;
    }

    auto mipmaps = task.mipmaps();
    Q_ASSERT(mipmaps.size() == 1);

    auto firstMipmap = mipmaps.first();

    textureData->setWidth(firstMipmap.second.width());
    textureData->setHeight(firstMipmap.second.height());
    textureData->setMipLevels(1);
    textureData->setData(firstMipmap.first, 8, true);

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
