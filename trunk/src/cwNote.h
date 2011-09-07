#ifndef CWNOTE_H
#define CWNOTE_H

//Qt includes
#include <QObject>
#include <QMatrix4x4>

//Our includes
#include "cwImage.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
class cwTrip;

class cwNote : public QObject
{
    Q_OBJECT

//    Q_PROPERTY(QString imagePath READ imagePath WRITE setImagePath NOTIFY imagePathChanged)

    Q_PROPERTY(int original READ original NOTIFY originalChanged)
    Q_PROPERTY(int icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(float rotate READ rotate WRITE setRotate NOTIFY rotateChanged)

    Q_ENUMS(StationDataRole)

public:
    enum StationDataRole {
        StationName,
        StaitonPosition
    };

    explicit cwNote(QObject *parent = 0);
    cwNote(const cwNote& object);
    cwNote& operator=(const cwNote& object);

    void setImage(cwImage image);
    cwImage image() const;

    int original() const;
    int icon() const;

    void setRotate(float degrees);
    float rotate() const;

    void setParentTrip(cwTrip* trip);
    cwTrip* parentTrip() const;

    QMatrix4x4 scaleMatrix() const;

    void addStation(cwNoteStation station);
    Q_INVOKABLE void removeStation(int stationId);
    const QList<cwNoteStation>& stations() const;
    cwNoteStation station(int stationId);
    int numberOfStations() const;
    Q_INVOKABLE QVariant stationData(StationDataRole role, int noteStationIndex) const;
    Q_INVOKABLE void setStationData(StationDataRole role, int noteStationIndex, QVariant value);

    cwNoteTranformation stationNoteTransformation() const;


signals:
//    void imagePathChanged();
    void originalChanged(int id);
    void iconChanged(int id);
    void imageChanged(cwImage image);
    void rotateChanged(float rotate);

    void stationAdded();
    void stationPositionChanged(int noteStationIndex);
    void stationNameChanged(int noteStationIndex);
    void stationRemoved(int index);

public slots:

private:
    cwTrip* ParentTrip;

    cwImage ImageIds;
    float DisplayRotation;  //!< Display rotation of the notes, don't confuse this with NoteTransform's rotation

    cwNoteTranformation NoteTransformation;
    QList<cwNoteStation> Stations;

    void copy(const cwNote& object);

    void updateNoteTransformation();
    QList< QPair <cwNoteStation, cwNoteStation> > noteShots() const;
    QList< cwNoteTranformation > calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots) const;
    cwNoteTranformation calculateShotTransformation(cwNoteStation station1, cwNoteStation station2) const;
    cwNoteTranformation averageTransformations(QList< cwNoteTranformation > shotTransforms);

};

/**
  \brief Gets the parent trip for this chunk
  */
inline cwTrip* cwNote::parentTrip() const {
    return ParentTrip;
}

/**
  \brief Get's the path for the image
  */
inline cwImage cwNote::image() const {
    return ImageIds;
}

inline int cwNote::original() const {
    return ImageIds.original();
}

inline int cwNote::icon() const {
    return ImageIds.icon();
}

/**
  \brief Sets the rotation for the notes
  */
inline float cwNote::rotate() const {
    return DisplayRotation;
}

/**
  \brief Gets the number of stations for a page of notes
  */
inline int cwNote::numberOfStations() const {
    return Stations.count();
}

inline const QList<cwNoteStation>& cwNote::stations() const {
    return Stations;
}

/**
  Gets the station note transformation,  This is the note page real scale and
  rotation!
  */
inline cwNoteTranformation cwNote::stationNoteTransformation() const {
    return NoteTransformation;
}


#endif // CWNOTE_H
