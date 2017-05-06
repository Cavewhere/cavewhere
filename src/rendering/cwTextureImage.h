#ifndef CWTEXTUREIMAGE_H
#define CWTEXTUREIMAGE_H

//Qt includes
#include <Qt3DCore/QNode>
#include <Qt3DRender/QTextureImage>

//Our includes
#include "cwImage.h"

class cwTextureImage : public Qt3DRender::QTextureImage
{
    Q_OBJECT

    Q_PROPERTY(QString projectFilename READ projectFilename WRITE setProjectFilename NOTIFY projectFilenameChanged)
    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)

public:
    cwTextureImage(Qt3DCore::QNode* parent = nullptr);

    cwImage image() const;
    void setImage(cwImage image);

    QString projectFilename() const;
    void setProjectFilename(QString projectFilename);

    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const;

private:
    void updateGenartor();

    cwImage Image; //!<
    QString ProjectFilename; //!<
    qint64 GenerationCount {0};
    Qt3DRender::QTextureImageDataGeneratorPtr Generator;

signals:
    void projectFilenameChanged();
    void imageChanged();



};

/**
* @brief cwTextureImage::projectFilename
* @return
*/
inline QString cwTextureImage::projectFilename() const {
    return ProjectFilename;
}

/**
* @brief cwTextureImage::image
* @return
*/
inline cwImage cwTextureImage::image() const {
    return Image;
}
#endif // CWTEXTUREIMAGE_H
