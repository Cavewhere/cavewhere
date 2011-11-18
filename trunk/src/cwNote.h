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
class cwScrap;

class cwNote : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int original READ original NOTIFY originalChanged)
    Q_PROPERTY(int icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(cwImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(float rotate READ rotate WRITE setRotate NOTIFY rotateChanged)

public:
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
//    QMatrix4x4 noteTransformationMatrix() const;

    void addScrap(cwScrap* scrap);
    cwScrap* scrap(int scrap);
    QList<cwScrap*> scraps();


signals:
//    void imagePathChanged();
    void originalChanged(int id);
    void iconChanged(int id);
    void imageChanged(cwImage image);
    void rotateChanged(float rotate);

    //For scraps
    void scrapAdded();

    //When ever the trip parent has changed
    void parentTripChanged();

public slots:

private:
    cwTrip* ParentTrip;

    cwImage ImageIds;
    float DisplayRotation;  //!< Display rotation of the notes, don't confuse this with NoteTransform's rotation

    cwNoteTranformation NoteTransformation;
    QList<cwNoteStation> Stations;

    QList<cwScrap*> Scraps;

    void copy(const cwNote& object);

//    //For note station transformation
//    void updateNoteTransformation();
//    QList< QPair <cwNoteStation, cwNoteStation> > noteShots() const;
//    QList< cwNoteTranformation > calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots) const;
//    cwNoteTranformation calculateShotTransformation(cwNoteStation station1, cwNoteStation station2) const;
//    cwNoteTranformation averageTransformations(QList< cwNoteTranformation > shotTransforms);

    //Guess station name
//    void guessStationName(cwNoteStation& station);

};

Q_DECLARE_METATYPE(cwNote*)

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





#endif // CWNOTE_H
