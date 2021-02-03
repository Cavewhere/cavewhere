#ifndef CWTEXTURE_H
#define CWTEXTURE_H

//Qt includes
#include <Qt3DRender/QTexture>
#include <QtConcurrent>

//Our inculdes
#include "cwGlobals.h"
#include "cwImage.h"
#include "cwAsyncFuture.h"
#include "cwFutureManagerToken.h"

class CAVEWHERE_LIB_EXPORT cwTexture : public Qt3DCore::QNode
{
    Q_OBJECT

    Q_PROPERTY(QString project READ project WRITE setProject NOTIFY projectChanged);
    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(Qt3DRender::QTexture2D* texture READ texture NOTIFY textureChanged);

public:
    cwTexture(Qt3DCore::QNode *parent = nullptr);

    bool isValid() const;

    QString project() const;
    void setProject(const QString& project);

    cwImage image() const;
    void setImage(const cwImage& image);

    void setFutureToken(cwFutureManagerToken token);

    Qt3DRender::QTexture2D* texture() const;

    QFuture<void> future() const;

signals:
    void projectChanged();
    void imageChanged();
    void textureChanged();

private:
    QString Project;
    cwImage Image;
    cwFutureManagerToken FutureToken;
    cwAsyncFuture::Restarter<void> TextureFuture;
    Qt3DRender::QTexture2D* Texture;

    void updateTextures();
    bool useMipmaps() const;

    Qt3DRender::QTexture2D* createTexture();
    void setTexture(Qt3DRender::QTexture2D* texture);

};

inline QString cwTexture::project() const
{
    return Project;
}

inline cwImage cwTexture::image() const
{
    return Image;
}

inline Qt3DRender::QTexture2D *cwTexture::texture() const
{
    return Texture;
}

inline QFuture<void> cwTexture::future() const
{
    return TextureFuture.future();
}

#endif // CWTEXTURE_H
