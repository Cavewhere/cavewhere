
//Our includes
#include "cwCropImageTask.h"
#include "cwImageData.h"
#include "cwDebug.h"
#include "cwProject.h"
#include "cwAddImageTask.h"

//Qt includes
#include <QByteArray>
#include <QBuffer>
#include <QImageWriter>

cwCropImageTask::cwCropImageTask(QObject* parent) : cwProjectIOTask(parent) {
//    AddImageTask = new cwAddImageTask(this);
//    AddImageTask->setParentTask(this);

}

/**
  Sets the original image that'll be cropped
  */
void cwCropImageTask::setOriginal(cwImage image) {
    Original = image;
}

/**
  Sets the cropping region.  The region should be in normalized image coordinates, ie from
  [0.0 to 1.0]. If the coordinates are out of bounds, this will clamp them. to 0.0 to 1.0
  */
void cwCropImageTask::setRectF(QRectF cropTo) {
    CropRect = cropTo;
}

/**
  This gets the finished cropped image.  This should be called when the task has completed
  */
cwImage cwCropImageTask::croppedImage() const {
    return CroppedImage;
}

/**
  \brief This does the the cropping
  */
void cwCropImageTask::runTask() {

    ImageProvider.setProjectPath(DatabasePath);
//    AddImageTask->setDatabaseFilename(DatabasePath);

    bool connected = connectToDatabase("CropImageTask");
    if(connected) {
        tryCroppingImage();

        Database.close();
    }

    done();
}

/**
  This crops an image in the database

  This will take imageId, extra the image data, crop the image, and then save image agian to
  a different id.  This id is returned.
  */
cwImageData cwCropImageTask::cropImage(int imageId) const {
    cwImageData imageData = ImageProvider.data(imageId);

    QByteArray croppedData;
    QRect cropArea = mapNormalizedToIndex(CropRect, imageData.size());
    QSize croppedSize = cropArea.size();
    QByteArray croppedFormat = imageData.format();
    int dotPerMeter = imageData.dotsPerMeter();

    //If custom DXT1 format
    if(imageData.format() == cwProjectImageProvider::Dxt1_GZ_Extension) {
        //Crop the DXT1 data
        croppedData = cropDxt1Image(imageData.data(), imageData.size(), cropArea);
    } else {
        //Some other format that QImage can handle
        QImage image = QImage::fromData(imageData.data(), imageData.format());
        QImage croppedImage = image.copy(cropArea);

        QBuffer buffer(&croppedData);
        QImageWriter writer(&buffer, croppedFormat);
        writer.write(croppedImage);
    }

    if(croppedData.isEmpty()) {
        qDebug() << "Can't crop image id:" << imageId << LOCATION;
        return cwImageData();
    }

    //Add the cropped image data to the database
    cwImageData croppedImageData(croppedSize, dotPerMeter, croppedFormat, croppedData);
    return croppedImageData;
}

/**
  This converts normalized rect into a index rect in terms of pixels.

  This size is the size of the original image.  This is useful for converting normalize rectangle
  into a rectangle.
  */
QRect cwCropImageTask::mapNormalizedToIndex(QRectF normalized, QSize size) const {
    int left = qRound(normalized.left() * size.width());
    int right = qRound(normalized.right() * size.width());
    int top = qRound((1.0 - normalized.bottom()) * size.height());
    int bottom = qRound((1.0 - normalized.top()) * size.height());

    return QRect(QPoint(left, top), QPoint(right, bottom));
}

/**
    \brief This crops the dxt1Data to the cropArea

    This returns the cropped dxt data
  */
QByteArray cwCropImageTask::cropDxt1Image(const QByteArray &dxt1Data, QSize size, QRect cropArea) const {
    if(dxt1Data.size() % 8 != 0) {
        qDebug() << "This isn't Dxt1 format, can't crop dxt1 data" << LOCATION;
        return QByteArray();
    }

    //Number of blocks in each direction, a block represents 4x4 pixel block
    int numBlockSizeX = size.width() / 4;
    int numBlockSizeY = size.height() / 4;

    //Over shoot size?
    if(size.width() % 4 != 0) {
        numBlockSizeX += 1;
    }

    if(size.height() % 4 != 0) {
        numBlockSizeY += 1;
    }

    //Make sure the number of blocks add up to the originial, a block is 8 bytes
    int sizeOfBlock = 8; //8 bytes
    if(numBlockSizeX * numBlockSizeY * sizeOfBlock != dxt1Data.size()) {
        qDebug() << "dxt1 data size mismatch" << LOCATION;
        return QByteArray();
    }

    //Convert cropArea to dxt1 blocks
    int x = cropArea.x() / 4;
    int y = cropArea.y() / 4;
    int width = cropArea.width() / 4;
    int height = cropArea.height() / 4;
    if(width % 4 == 0) { width += 1; }
    if(height % 4 == 0) { height += 1; }
    QByteArray cropDxt1Image(width * height, 0); //Create a completely black image

    width = qMin(width, numBlockSizeX - x); //Make sure the width is in the bounds of the dxt1 format


    //For each scan line in the dxt formate
    for(int row = 0; row < height && row + y < numBlockSizeY; row++) {
        int fromIndex = numBlockSizeX * (row + y) + x;
        int toIndex = row * width;

        Q_ASSERT(fromIndex <= dxt1Data.size() - width);
        Q_ASSERT(toIndex <= cropDxt1Image.size() - width);

        //Copy each scanline
        memcpy(&(cropDxt1Image.data()[toIndex]), &(dxt1Data.constData()[fromIndex]), width);
    }

    return cropDxt1Image;
}

/**
  This tries to crop cwImage and save it back to a cwImage.
  */
void cwCropImageTask::tryCroppingImage()
{
    cwImageData originalData = cropImage(Original.original());
    cwImageData iconData = cropImage(Original.icon());

    //For all the mipmaps
    QList<cwImageData> mipMapImageData;
    foreach(int mipmapId, Original.mipmaps()) {
        mipMapImageData.append(cropImage(mipmapId));
    }

    //Commit all the images to the project database
    bool good = beginTransation();
    if(!good) { return; }

    //Add all the images to the database
    int originalId = cwProject::addImage(Database, originalData);
    int iconId = cwProject::addImage(Database, iconData);

    QList<int> mipmapIds;
    mipmapIds.reserve(Original.mipmaps().size());
    foreach(cwImageData mipmap, mipMapImageData) {
        int mipmapId = cwProject::addImage(Database, mipmap);
        mipmapIds.append(mipmapId);
    }

    endTransation();

    CroppedImage.setMipmaps(mipmapIds);
    CroppedImage.setOriginal(originalId);
    CroppedImage.setIcon(iconId);
    CroppedImage.setOriginalSize(originalData.size());
    CroppedImage.setOriginalDotsPerMeter(originalData.dotsPerMeter());
}
