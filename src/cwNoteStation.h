#ifndef CWNOTESTATION_H
#define CWNOTESTATION_H

//Qt includes
#include <QSharedData>
#include <QSharedDataPointer>
#include <QPointF>

//Our includes
#include <cwStation.h>

class cwNoteStation
{
public:
    cwNoteStation();

    void setStation(const cwStation& station);
    cwStation station() const;

    void setPositionOnNote(QPointF point);
    QPointF positionOnNote() const;

//    void setCave(cwCave* cave);
//    cwCave* cave() const;

    void setName(QString name);
    QString name() const;

    bool isValid() const;

    bool operator ==(const cwNoteStation& station) const;

private:
    class PrivateData : public QSharedData {

    public:
        PrivateData() { }

        QPointF PositionOnNote;
        cwStation Station;
    };

    QSharedDataPointer<PrivateData> Data;
};

inline uint qHash(const cwNoteStation& note) {
    return qHash(note.station());
}

inline bool cwNoteStation::operator ==(const cwNoteStation& otherStation) const {
    return otherStation.station() == station() && otherStation.positionOnNote() == positionOnNote();
}

/**
  Gets the station
  */
inline cwStation cwNoteStation::station() const {
    return Data->Station;
}

/**
  Gets the position of the station on the page of notes.

  The point is normalized from 0.0 to 1.0.
  */
inline QPointF cwNoteStation::positionOnNote() const {
    return Data->PositionOnNote;
}

///**
//  \brief Sets the cave for the station
//  */
//inline void cwNoteStation::setCave(cwCave* cave) {
//    Data->Station.setCave(cave);
//}

///**
//  \brief Gets the cave for the station
//  */
//inline cwCave* cwNoteStation::cave() const {
//    return Data->Station.cave();
//}

/**
  \brief Gets the station's name
  */
inline void cwNoteStation::setName(QString name) {
    Data->Station.setName(name);
}

/**
  \brief Sets the station's name
  */
inline QString cwNoteStation::name() const {
    return Data->Station.name();
}

/**
  \brief Returns true if the note station has a name
  */
inline bool cwNoteStation::isValid() const {
    return !name().isEmpty();
}


#endif // CWNOTESTATION_H
