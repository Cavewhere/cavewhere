#ifndef CWCROPIMAGETASK_H
#define CWCROPIMAGETASK_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"
#include "cwImageProvider.h"
#include "cwProjectIOTask.h"
class cwAddImageTask;

//Qt includes
#include <QRectF>
#include <QString>

/**
  \brief This will crop a cwImage using normalize coordinates of the original
  */
class cwCropImageTask : public cwProjectIOTask
{
    Q_OBJECT

public:
    cwCropImageTask(QObject* parent = 0);

    //Inputs
    void setOriginal(cwImage image);
    void setRectF(QRectF cropTo);
    void setMipmapOnly(bool mipmapOnly);

    //Output
    cwImage croppedImage() const;

protected:
    virtual void runTask();

private:
    //Inputs
    cwImage Original;
    QRectF CropRect;

    //Output
    cwImage CroppedImage;

    //For extracting image data from the database
    cwImageProvider ImageProvider;

    //For writting the cropped image
    cwAddImageTask* AddImageTask;

    QRect mapNormalizedToIndex(QRectF normalized, QSize size) const;

};

#endif // CWCROPIMAGETASK_H
