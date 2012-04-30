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
class cwCave;

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

    void setParentCave(cwCave* cave);
    cwCave* parentCave() const;

    QMatrix4x4 scaleMatrix() const;
    QMatrix4x4 metersOnPageMatrix() const;

    void addScrap(cwScrap* scrap);
    cwScrap* scrap(int scrap) const;
    QList<cwScrap*> scraps() const;
    void setScraps(QList<cwScrap*> scraps);


signals:
//    void imagePathChanged();
    void originalChanged(int id);
    void iconChanged(int id);
    void imageChanged(cwImage image);
    void rotateChanged(float rotate);

    //For scraps
    void insertedScraps(int begin, int end);
    void beginInsertingScraps(int begin, int end);

    int beginRemovingScraps(int begin, int end);
    void removedScraps(int begin, int end);

    void scrapAdded();
    void scrapsReset();

    //When ever the trip parent has changed
    void parentTripChanged();

public slots:

private:
    cwTrip* ParentTrip;
    cwCave* ParentCave;

    cwImage ImageIds;
    float DisplayRotation;  //!< Display rotation of the notes, don't confuse this with NoteTransform's rotation

    cwNoteTranformation NoteTransformation;
    QList<cwNoteStation> Stations;

    QList<cwScrap*> Scraps;

    void copy(const cwNote& object);
    void setupScrap(cwScrap* scrap);
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

/**
  Sets the parent cave for the note
  */
inline cwCave *cwNote::parentCave() const {
    return ParentCave;
}




#endif // CWNOTE_H