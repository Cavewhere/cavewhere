#ifndef CWNOTESTATION_H
#define CWNOTESTATION_H

//Qt includes
#include <QSharedData>
#include <QSharedDataPointer>
#include <QPointF>
#include <QString>
#include <QHash>

class cwNoteStation
{
public:
    cwNoteStation();

    void setPositionOnNote(QPointF point);
    QPointF positionOnNote() const;

    void setName(QString name);
    QString name() const;

    bool isValid() const;

    bool operator ==(const cwNoteStation& station) const;

private:
    class PrivateData : public QSharedData {

    public:
        PrivateData() { }

        QPointF PositionOnNote;
        QString StationName;
    };

    QSharedDataPointer<PrivateData> Data;
};

inline uint qHash(const cwNoteStation& note) {
    return qHash(note.name());
}

inline bool cwNoteStation::operator ==(const cwNoteStation& otherStation) const {
    return otherStation.name() == name() && otherStation.positionOnNote() == positionOnNote();
}

/**
  Gets the position of the station on the page of notes.

  The point is normalized from 0.0 to 1.0.
  */
inline QPointF cwNoteStation::positionOnNote() const {
    return Data->PositionOnNote;
}

/**
  \brief Gets the station's name
  */
inline void cwNoteStation::setName(QString name) {
    Data->StationName = name;
}

/**
  \brief Sets the station's name
  */
inline QString cwNoteStation::name() const {
    return Data->StationName;
}

/**
  \brief Returns true if the note station has a name
  */
inline bool cwNoteStation::isValid() const {
    return !name().isEmpty();
}


#endif // CWNOTESTATION_H
