#ifndef CWTEXTUREIMAGE_H
#define CWTEXTUREIMAGE_H

//Qt includes
#include <Qt3DCore/QNode>
#include <Qt3DRender/QTextureImage>

//Our includes
#include "cwGlobals.h"
class cwTextureDataGenerator;

class CAVEWHERE_LIB_EXPORT cwTextureImage : public Qt3DRender::QTextureImage
{
    Q_OBJECT

public:
    cwTextureImage(Qt3DCore::QNode* parent = nullptr);

    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const;
    void setDataGenerator(cwTextureDataGenerator* generator);

private:
    Qt3DRender::QTextureImageDataGeneratorPtr Generator;

};
#endif // CWTEXTUREIMAGE_H
