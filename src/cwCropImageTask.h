/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCROPIMAGETASK_H
#define CWCROPIMAGETASK_H

//Our includes
#include "cwTask.h"
#include "cwImage.h"
#include "cwImageProvider.h"
#include "cwProjectIOTask.h"
#include "cwGlobals.h"
#include "cwTextureUploadTask.h"
class cwAddImageTask;

//Qt includes
#include <QRectF>
#include <QString>
#include <QtConcurrent>

/**
  \brief This will crop a cwImage using normalize coordinates of the original
  */
class CAVEWHERE_LIB_EXPORT cwCropImageTask : public cwProjectIOTask
{
    Q_OBJECT

public:
    cwCropImageTask(QObject* parent = nullptr);

    //Inputs
    void setOriginal(cwImage image);
    void setRectF(QRectF cropTo);
    void setMipmapOnly(bool mipmapOnly);
    void setFormatType(cwTextureUploadTask::Format format);

    QFuture<cwImage> crop();

protected:
    virtual void runTask();

private:
    //Inputs
    cwImage Original;
    QRectF CropRect;
    cwTextureUploadTask::Format Format = cwTextureUploadTask::Unknown;

    //Output
    cwImage CroppedImage;

    static QRect mapNormalizedToIndex(QRectF normalized, QSize size);
    static QRect nearestDXT1Rect(QRect rect);
};

#endif // CWCROPIMAGETASK_H

