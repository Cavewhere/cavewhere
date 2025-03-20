/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIANGULATEDATA_H
#define CWTRIANGULATEDATA_H

//Our includes
#include "cwTrackedImage.h"
#include "cwTextureUploadTask.h"

//Qt includes
#include <QSharedData>
#include <QVector>
#include <QVector3D>
#include <QVector2D>

class cwTriangulatedData
{
public:
    cwTriangulatedData();

    cwImage croppedImage() const;
    cwTrackedImagePtr croppedImagePtr() const;
    void setCroppedImage(cwTrackedImagePtr croppedImage);

    cwTextureUploadTask::UploadResult croppedImageData() const;
    void setCroppedImageData(const cwTextureUploadTask::UploadResult& imageData);

    QVector<QVector3D> points() const;
    void setPoints(QVector<QVector3D> points);

    QVector<QVector2D> texCoords() const;
    void setTexCoords(QVector<QVector2D> texCoords);

    QVector<uint> indices() const;
    void setIndices(QVector<uint> indices);

    QVector<QVector3D> leadPoints() const;
    void setLeadPoints(QVector<QVector3D> points);

    bool isStale() const;
    void setStale(bool isStale);

    bool isNull() const;

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData() :
            Stale(false)
        {}

        cwTrackedImagePtr croppedImage = cwTrackedImagePtr::create();
        cwTextureUploadTask::UploadResult croppedImageData;
        QVector<QVector3D> points;
        QVector<QVector2D> texCoords;
        QVector<uint> indices;
        QVector<QVector3D> leadPoints;
        bool Stale;
    };

    QSharedDataPointer<PrivateData> Data;
};

/**
Get variableName
*/
inline cwImage cwTriangulatedData::croppedImage() const {
    return *Data->croppedImage;
}

inline cwTrackedImagePtr cwTriangulatedData::croppedImagePtr() const
{
    return Data->croppedImage;
}

/**
Sets variableName
*/
inline void cwTriangulatedData::setCroppedImage(cwTrackedImagePtr croppedImage) {
    Data->croppedImage = croppedImage;
}

inline cwTextureUploadTask::UploadResult cwTriangulatedData::croppedImageData() const
{
    return Data->croppedImageData;
}

inline void cwTriangulatedData::setCroppedImageData(const cwTextureUploadTask::UploadResult &imageData)
{
    Data->croppedImageData = imageData;
}

/**
  Get variableName
  */
inline QVector<QVector3D> cwTriangulatedData::points() const {
    return Data->points;
}

/**
  Sets variableName
  */
inline void cwTriangulatedData::setPoints(QVector<QVector3D> points) {
    Data->points = points;
}

/**
Get variableName
*/
inline QVector<QVector2D> cwTriangulatedData::texCoords() const {
    return Data->texCoords;
}

/**
Sets variableName
*/
inline void cwTriangulatedData::setTexCoords(QVector<QVector2D> texCoords) {
    Data->texCoords = texCoords;
}

/**
  Get variableName
  */
inline QVector<uint> cwTriangulatedData::indices() const {
    return Data->indices;
}

/**
  Sets variableName
  */
inline void cwTriangulatedData::setIndices(QVector<uint> indices) {
    Data->indices = indices;
}

/**
 * @brief cwTriangulatedData::leadPoints
 * @return The carpeted lead points for the scrap
 */
inline QVector<QVector3D> cwTriangulatedData::leadPoints() const
{
    return Data->leadPoints;
}

/**
 * @brief cwTriangulatedData::setLeadPoints
 * @param points - The lead points for the scrap.
 *
 * This should only be set by the cwTriangulateTask
 */
inline void cwTriangulatedData::setLeadPoints(QVector<QVector3D> points)
{
    Data->leadPoints = points;
}


/**
 * @brief cwTriangulatedData::stale
 * @return True if the data is old and should be recalculated
 */
inline bool cwTriangulatedData::isStale() const
{
    return Data->Stale;
}

/**
 * @brief cwTriangulatedData::setStale
 * @param isStale
 *
 * This is used by cwScrapManager to keep track, if the triangulatedData is old or not.
 * This is set to true if it should be recalculated and false if it shouldn't. It is
 * useful to keep track of this incase cavewhere shuts down before the scrap calculation
 * have been completed.
 */
inline void cwTriangulatedData::setStale(bool isStale)
{
    Data->Stale = isStale;
}
#endif // CWTRIANGULATEDATA_H
