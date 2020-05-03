//Our includes
#include "cwTextureImage.h"
#include "cwTextureDataGenerator.h"

cwTextureImage::cwTextureImage(Qt3DCore::QNode* parent) :
    Qt3DRender::QTextureImage(parent),
    Generator(new cwTextureDataGenerator())
{



}

/**
* @brief cwTextureImage::setProjectFilename
* @param projectFilename
*/
void cwTextureImage::setProjectFilename(QString projectFilename) {
    if(ProjectFilename != projectFilename) {
        ProjectFilename = projectFilename;
        updateGenartor();
        emit projectFilenameChanged();
    }
}

/**
 * @brief cwTextureImage::dataGenerator
 * @return
 */
Qt3DRender::QTextureImageDataGeneratorPtr cwTextureImage::dataGenerator() const
{
    return Generator;

}

/**
 * @brief cwTextureImage::updateGenartor
 *
 * Updates the generator for the texture map
 */
void cwTextureImage::updateGenartor()
{
    if(!ProjectFilename.isEmpty() && Image.isMipmapsValid()) {
        GenerationCount++;
        Generator.reset(new cwTextureDataGenerator(projectFilename(),
                                                   image(),
                                                   mipLevel(),
                                                   GenerationCount,
                                                   id()));
        notifyDataGeneratorChanged();
    }
}

/**
* @brief cwTextureImage::setImage
* @param image
*/
void cwTextureImage::setImage(cwImage image) {
    if(Image != image) {
        Image = image;
        updateGenartor();
        emit imageChanged();
    }
}
