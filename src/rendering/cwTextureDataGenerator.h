#ifndef CWTEXTUREGENERATOR_H
#define CWTEXTUREGENERATOR_H

//Qt includes
#include <QTextureImageDataGenerator>
#include <QNode>
#include <QString>
#include <QAbstractTexture>

//Our includes
#include "cwImage.h"
#include "cwTextureUploadTask.h"

class cwTextureDataGenerator : public Qt3DRender::QTextureImageDataGenerator
{
public:
    cwTextureDataGenerator();
    cwTextureDataGenerator(const QByteArray& data,
                           QSize size,
                           int mipmapLevel,
                           cwTextureUploadTask::Format format,
                           int imageId);

    Qt3DRender::QTextureImageDataPtr operator ()() Q_DECL_FINAL;
    bool operator ==(const QTextureImageDataGenerator& other) const Q_DECL_FINAL;

    QT3D_FUNCTOR(cwTextureDataGenerator)

private:

    QByteArray Data;
    QSize Size;
    int MipmapLevel;
    cwTextureUploadTask::Format Format;
    int ImageId = -1;


};



#endif // CWTEXTUREGENERATOR_H
