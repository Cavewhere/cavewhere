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
    qDebug() << "Update generator:" << ProjectFilename << Image.isValid() << Image.mipmaps().size();
    if(!ProjectFilename.isEmpty() && Image.isValid()) {
        GenerationCount++;
        Generator.reset(new cwTextureDataGenerator(projectFilename(),
                                                   image(),
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
