#ifndef CWTEXTUREGENERATOR_H
#define CWTEXTUREGENERATOR_H

//Qt includes
#include <QTextureImageDataGenerator>
#include <QNode>
#include <QString>


//Our includes
#include "cwImage.h"

class cwTextureDataGenerator : public Qt3DRender::QTextureImageDataGenerator
{
public:
    cwTextureDataGenerator();
    cwTextureDataGenerator(const QString project, const cwImage& image, int gen, Qt3DCore::QNodeId texId);

    QString project() const;
    cwImage image() const;

    Qt3DRender::QTextureImageDataPtr operator ()() Q_DECL_FINAL;
    bool operator ==(const QTextureImageDataGenerator& other) const Q_DECL_FINAL;

    QT3D_FUNCTOR(cwTextureDataGenerator)

private:
    QString Project;
    cwImage Image;
    int Generation = -1;
    Qt3DCore::QNodeId TextureId;


};

inline QString cwTextureDataGenerator::project() const
{
    return Project;
}

inline cwImage cwTextureDataGenerator::image() const
{
    return Image;
}



#endif // CWTEXTUREGENERATOR_H
