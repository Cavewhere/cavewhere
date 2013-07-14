/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIANGULATEINDATA_H
#define CWTRIANGULATEINDATA_H

//Qt includes
#include <QSharedData>
#include <QPolygonF>

//Qt includes
#include "cwImage.h"
#include "cwTriangulateStation.h"
#include "cwNoteTranformation.h"

class cwTriangulateInData
{
public:
    cwTriangulateInData();

    cwImage noteImage() const;
    void setNoteImage(cwImage noteImage);

    QPolygonF outline() const;
    void setOutline(QPolygonF outline);

    QList<cwTriangulateStation> stations() const;
    void setStations(QList<cwTriangulateStation> stations);

    cwNoteTranformation noteTransform() const;
    void setNoteTransform(cwNoteTranformation noteTransform);

    double noteImageResolution() const;
    void setNoteImageResolution(double dotsPerMeter);

private:
    class PrivateData : public QSharedData {
    public:
        cwImage NoteImage;
        double DotPerMeter;
        QPolygonF Outline;
        cwNoteTranformation NoteTransform;
        QList<cwTriangulateStation> Stations;
    };

    QSharedDataPointer<PrivateData> Data;
};


/**
Get NoteImage
*/
inline cwImage cwTriangulateInData::noteImage() const {
    return Data->NoteImage;
}

/**
Sets NoteImage
*/
inline void cwTriangulateInData::setNoteImage(cwImage noteImage) {
    Data->NoteImage = noteImage;
}

/**
Get variableName
*/
inline QPolygonF cwTriangulateInData::outline() const {
    return Data->Outline;
}

/**
Sets variableName
*/
inline void cwTriangulateInData::setOutline(QPolygonF outline) {
    Data->Outline = outline;
}

/**
  Get variableName
  */
inline QList<cwTriangulateStation> cwTriangulateInData::stations() const {
    return Data->Stations;
}

/**
  Sets variableName
  */
inline void cwTriangulateInData::setStations(QList<cwTriangulateStation> stations) {
    Data->Stations = stations;
}

/**
  Get variableName
  */
inline cwNoteTranformation cwTriangulateInData::noteTransform() const {
    return Data->NoteTransform;
}

/**
  Sets variableName
  */
inline void cwTriangulateInData::setNoteTransform(cwNoteTranformation noteTransform) {
    Data->NoteTransform = noteTransform;
}

/**
 * @brief cwTriangulateInData::noteImageResolution
 * @return Return the resolution of the note image, in DotsPerMeter
 */
inline double cwTriangulateInData::noteImageResolution() const
{
    return Data->DotPerMeter;
}

/**
 * @brief cwTriangulateInData::setNoteImageResolution
 * @param dotsPerMeter - Sets the resolution
 */
inline void cwTriangulateInData::setNoteImageResolution(double dotsPerMeter)
{
    Data->DotPerMeter = dotsPerMeter;
}

#endif // CWTRIANGULATEINDATA_H
