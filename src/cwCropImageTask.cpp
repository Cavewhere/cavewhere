
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

//TODO: REMOVE for testing only
#include <QFile>

cwCropImageTask::cwCropImageTask(QObject* parent) : cwProjectIOTask(parent) {
    AddImageTask = new cwAddImageTask(this);
    AddImageTask->setParentTask(this);

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
 * @brief cwCropImageTask::setMipmapOnly
 * @param mipmapOnly - This make the crop image task only create images with mipmaps
 */
void cwCropImageTask::setMipmapOnly(bool mipmapOnly)
{
    AddImageTask->setMipmapsOnly(mipmapOnly);
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

    cwImageData imageData = ImageProvider.data(Original.original());
    QRect cropArea = mapNormalizedToIndex(CropRect, imageData.size());
    QImage image = QImage::fromData(imageData.data(), imageData.format());


    if(image.size().isEmpty()) {
        qDebug() << "Can't crop an image with no size";
        stop();
        done();
        return;
    }

    QImage croppedImage = image.copy(cropArea);
//    qDebug() << "image:" << Original.original() << image << imageData.format() << croppedImage.size() << imageData.size() << CropRect << cropArea;

    QList<QImage> images;
    images.append(croppedImage);

    AddImageTask->setDatabaseFilename(databaseFilename());
    AddImageTask->setNewImages(images);
    AddImageTask->start();

    Q_ASSERT(AddImageTask->isReady());

    //We need to make sure we're still running, because the AddImageTask
    //May have stopped, if the user stopped this task.  If the user stopped
    //this task, the addImageTask may return no images.
    if(isRunning()) {
        QList<cwImage> croppedImages = AddImageTask->images();
        Q_ASSERT(croppedImages.size() == 1);

        CroppedImage = croppedImages.first();
    }

    done();
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


