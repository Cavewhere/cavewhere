#ifndef CWCROPIMAGETASK_H
#define CWCROPIMAGETASK_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"
#include "cwProjectImageProvider.h"
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
//    void setDatabaseFilename(QString filename);

    //Output
    cwImage croppedImage() const;

protected:
    virtual void runTask();

private:
    //Inputs
    cwImage Original;
    QRectF CropRect;
//    QString ProjectPath;
//    QString DatabasePath;

    //Output
    cwImage CroppedImage;

    //For extracting image data from the database
    cwProjectImageProvider ImageProvider;

    //For writting the cropped image
//    cwAddImageTask* AddImageTask;

    cwImageData cropImage(int imageId) const;
    QRect mapNormalizedToIndex(QRectF normalized, QSize size) const;

    QByteArray cropDxt1Image(const QByteArray& dxt1Data, QSize size, QRect cropArea) const;

private slots:
    void tryCroppingImage();

};

#endif // CWCROPIMAGETASK_H
