#ifndef CWTRIANGULATEDATA_H
#define CWTRIANGULATEDATA_H

//Our includes
#include "cwImage.h"

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
    void setCroppedImage(cwImage croppedImage);

    QVector<QVector3D> points() const;
    void setPoints(QVector<QVector3D> points);

    QVector<QVector2D> texCoords() const;
    void setTexCoords(QVector<QVector2D> texCoords);

    QVector<int> indices() const;
    void setIndices(QVector<int> indices);

private:
    class PrivateData : public QSharedData {
    public:
        cwImage croppedImage;
        QVector<QVector3D> points;
        QVector<QVector2D> texCoords;
        QVector<int> indices;
    };

    QSharedDataPointer<PrivateData> Data;
};

/**
Get variableName
*/
inline cwImage cwTriangulatedData::croppedImage() const {
    return Data->croppedImage;
}

/**
Sets variableName
*/
inline void cwTriangulatedData::setCroppedImage(cwImage croppedImage) {
    Data->croppedImage = croppedImage;
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
inline QVector<int> cwTriangulatedData::indices() const {
    return Data->indices;
}

/**
  Sets variableName
  */
inline void cwTriangulatedData::setIndices(QVector<int> indices) {
    Data->indices = indices;
}
#endif // CWTRIANGULATEDATA_H
