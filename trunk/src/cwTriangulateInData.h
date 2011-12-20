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




private:
    class PrivateData : public QSharedData {
    public:
        cwImage NoteImage;
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
#endif // CWTRIANGULATEINDATA_H
