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
#include "cwDiskCacher.h"
#include "cwImageProvider.h"
#include "cwGlobals.h"
#include "cwTextureUploadTask.h"
#include "cwTrackedImage.h"

//Qt includes
#include <QRectF>
#include <QString>
#include <QDir>

/**
  \brief This will crop a cwImage using normalize coordinates of the original
  */
class CAVEWHERE_LIB_EXPORT cwCropImageTask : public QObject
{
    Q_OBJECT

public:
    /**
     * What one crop produces: the PNG crop written to the image cache, plus the
     * key of the UASTC .ktx2 entry encoded from the same pixels. compressedKey's
     * id is empty when the encode failed, and callers then stay on the
     * uncompressed image.
     */
    struct Result {
        cwTrackedImagePtr image;
        cwDiskCacher::Key compressedKey;
    };

    cwCropImageTask(QObject* parent = nullptr);

    //Inputs
    void setOriginal(cwImage image);
    void setRectF(QRectF cropTo);
    void setFormatType(cwTextureUploadTask::Format format);
    void setDataRootDir(const QDir& dataRootDir);

    QFuture<Result> crop();

protected:
    virtual void runTask();

private:
    //Inputs
    cwImage Original;
    QRectF CropRect;
    cwTextureUploadTask::Format Format = cwTextureUploadTask::Unknown;
    QDir DataRootDir;

    //Output
    cwImage CroppedImage;

    static QRect mapNormalizedToIndex(QRectF normalized, QSize size);
    static QRect nearestDXT1Rect(QRect rect);
};

#endif // CWCROPIMAGETASK_H
