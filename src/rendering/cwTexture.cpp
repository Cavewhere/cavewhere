//Our include
#include "cwTexture.h"
#include "cwTextureImage.h"
#include "cwTextureUploadTask.h"
#include "cwTextureDataGenerator.h"
#include "cwAsyncFuture.h"
#include "cwOpenGLSettings.h"
#include "cwDebug.h"

//Async includes
#include "asyncfuture.h"

cwTexture::cwTexture(Qt3DCore::QNode *parent) :
    Qt3DCore::QNode(parent),
    TextureFuture(this),
    Texture(createTexture())
{
    auto settings = cwOpenGLSettings::instance();
    connect(settings, &cwOpenGLSettings::useDXT1CompressionChanged, this, &cwTexture::updateTextures);
    connect(settings, &cwOpenGLSettings::useMipmapsChanged, this, &cwTexture::updateTextures);

    TextureFuture.onFutureChanged([this](){
        FutureToken.addJob({TextureFuture.future(), "Updating Texture"});
    });
}

void cwTexture::setProject(const QString &project)
{
    if(Project != project) {
        Project = project;
        updateTextures();
    }
}


void cwTexture::setImage(const cwImage &image)
{
    if(Image != image && image.isOriginalValid()) {
        Image = image;
        updateTextures();
    }
}

void cwTexture::setFutureToken(cwFutureManagerToken token)
{
    FutureToken = token;
}

void cwTexture::updateTextures()
{

    if(Project.isEmpty() || !Image.isOriginalValid()) {
        return;
    }

    auto run = [this]() {
        auto newTexture = createTexture();

        cwTextureUploadTask task;
        task.setImage(Image);
        task.setProjectFilename(Project);
        task.setFormat(cwTextureUploadTask::format());
        auto future = task.mipmaps();

        return AsyncFuture::observe(future).context(this, [future, newTexture, this]() {
            auto result = future.result();

            if(result.mipmaps.isEmpty()) {
                qDebug() << "Image data is empty" << LOCATION;
                return;
            }

            auto numberOfMipmaps =  useMipmaps() ? result.mipmaps.size() : 1;

            for(int i = 0; i < numberOfMipmaps; i++) {
                auto mipmap = result.mipmaps.at(i);
                auto textureImage = new cwTextureImage(this); // static_cast<cwTextureImage*>(textureImages().at(i));

                textureImage->setMipLevel(i);
                auto generator = new cwTextureDataGenerator(
                            mipmap.first, //Data
                            mipmap.second, //Size
                            i, //Mipmap level
                            result.format,
                            Image.original()
                            );

                textureImage->setDataGenerator(generator);
                newTexture->addTextureImage(textureImage);
            }

            setTexture(newTexture);
        },
        [newTexture]() {
            delete newTexture;
        }).future();
    };

    TextureFuture.restart(run);

//    cwAsyncFuture::restart(&TextureFuture, run);
}

bool cwTexture::useMipmaps() const
{
    return cwOpenGLSettings::instance()->useMipmaps();
}

Qt3DRender::QTexture2D *cwTexture::createTexture()
{
    auto texture = new Qt3DRender::QTexture2D(this);
    auto settings = cwOpenGLSettings::instance();

    auto updateMagFilter = [settings, texture]() {
        switch(settings->magFilter()) {
        case settings->MagLinear:
            texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
            break;
        case settings->MagNearest:
            texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Nearest);
            break;
        }
    };

    auto updateMinFilter = [settings, texture]() {
        if(settings->useMipmaps()) {
            switch(settings->minFilter()) {
            case settings->MinLinear:
                texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                break;
            case settings->MinLinear_Mipmap_Linear:
                texture->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
                break;
            case settings->MinNearest_Mipmap_Linear:
                texture->setMinificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapLinear);
                break;
            }
        } else {
            texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        }
    };

    auto updateUseMipmaps = [settings, texture] {
        if(settings->useMipmaps() && cwTextureUploadTask::format() == cwTextureUploadTask::OpenGL_RGBA) {
            texture->setGenerateMipMaps(true);
        } else {
            texture->setGenerateMipMaps(false);
        }
    };

    auto updateAnisotropy = [settings, texture]() {
        auto maxAnisotropy = settings->useAnisotropy() ? 16.0f : 1.0f;
        texture->setMaximumAnisotropy(maxAnisotropy);
    };


    if(settings->useDXT1Compression()) {
        //We can't update texture parameter with compressed textures
        //regenerate the texture, qt3d seem to be bugging with compressed textures
        auto updateFullTexture = [this]() {
            updateTextures();
        };
        connect(settings, &cwOpenGLSettings::magFilterChanged, texture, updateFullTexture);
        connect(settings, &cwOpenGLSettings::minFilterChanged, texture, updateFullTexture);
        connect(settings, &cwOpenGLSettings::useAnisotropyChanged, texture, updateFullTexture);
    } else {
        connect(settings, &cwOpenGLSettings::magFilterChanged, texture, updateMagFilter);
        connect(settings, &cwOpenGLSettings::minFilterChanged, texture, updateMinFilter);
        connect(settings, &cwOpenGLSettings::useAnisotropyChanged, texture, updateAnisotropy);
    }

    updateMagFilter();
    updateMinFilter();
    updateAnisotropy();
    updateUseMipmaps();

    texture->wrapMode()->setX(Qt3DRender::QTextureWrapMode::ClampToEdge);
    texture->wrapMode()->setY(Qt3DRender::QTextureWrapMode::ClampToEdge);

    return texture;
}

void cwTexture::setTexture(Qt3DRender::QTexture2D *texture)
{
    auto oldTexture = Texture;
    Texture = std::move(texture);
    emit textureChanged();
    oldTexture->deleteLater();
}
