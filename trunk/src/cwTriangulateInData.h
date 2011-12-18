#ifndef CWTRIANGULATEINDATA_H
#define CWTRIANGULATEINDATA_H

//Qt includes
#include <QSharedData>
#include <QPolygonF>

//Qt includes
#include "cwImage.h"
#include "cwTriangulateStation.h"

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

private:
    class PrivateData : public QSharedData {
    public:
        cwImage noteImage;
        QPolygonF outline;
        QList<cwTriangulateStation> stations;
    };

    QSharedDataPointer<PrivateData> Data;
};


/**
Get NoteImage
*/
inline cwImage cwTriangulateInData::noteImage() const {
    return Data->noteImage;
}

/**
Sets NoteImage
*/
inline void cwTriangulateInData::setNoteImage(cwImage noteImage) {
    Data->noteImage = noteImage;
}

/**
Get variableName
*/
inline QPolygonF cwTriangulateInData::outline() const {
    return Data->outline;
}

/**
Sets variableName
*/
inline void cwTriangulateInData::setOutline(QPolygonF outline) {
    Data->outline = outline;
}

/**
  Get variableName
  */
inline QList<cwTriangulateStation> cwTriangulateInData::stations() const {
    return Data->stations;
}

/**
  Sets variableName
  */
inline void cwTriangulateInData::setStations(QList<cwTriangulateStation> stations) {
    Data->stations = stations;
}
#endif // CWTRIANGULATEINDATA_H
