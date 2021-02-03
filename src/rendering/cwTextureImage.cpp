//Our includes
#include "cwTextureImage.h"
#include "cwTextureDataGenerator.h"
#include "cwTextureUploadTask.h"

//Async Includes
#include "asyncfuture.h"

cwTextureImage::cwTextureImage(Qt3DCore::QNode* parent) :
    Qt3DRender::QTextureImage(parent),
    Generator(new cwTextureDataGenerator())
{



}


/**
 * @brief cwTextureImage::dataGenerator
 * @return
 */
Qt3DRender::QTextureImageDataGeneratorPtr cwTextureImage::dataGenerator() const
{
    return Generator;
}

void cwTextureImage::setDataGenerator(cwTextureDataGenerator *generator)
{
    if(!(*Generator == *generator)) {
        Generator.reset(generator);
        notifyDataGeneratorChanged();
    }
}
