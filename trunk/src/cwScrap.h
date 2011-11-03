#ifndef CWSCRAP_H
#define CWSCRAP_H

//Qt includes
#include <QObject>
#include <QVector2D>
#include <QVector>
#include <QPolygonF>

//Our includes
#include <cwNoteTranformation.h>
#include <cwNoteStation.h>
class cwNote;

/**
  cwScrap holds a polygon of points that represents a scrap

  Points can be added or removed from the scrap.  All the points will be in
  normalize note coordinates system.
  */
class cwScrap : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwNoteTranformation* noteTransformation READ noteTransformation NOTIFY noteTransformationChanged)

    Q_ENUMS(StationDataRole)
public:

    enum StationDataRole {
        StationName,
        StationPosition
    };

    explicit cwScrap(QObject *parent = 0);

    void setParentNote(cwNote* trip);
    cwNote* parentNote() const;

    void addPoint(QPointF point);
    void insertPoint(int index, QPointF point);
    void removePoint(int index);
    QVector<QPointF> points();
    int numberOfPoints() const;

    void addStation(cwNoteStation station);
    Q_INVOKABLE void removeStation(int stationId);
    const QList<cwNoteStation>& stations() const;
    cwNoteStation station(int stationId);
    int numberOfStations() const;
    Q_INVOKABLE QVariant stationData(StationDataRole role, int noteStationIndex) const;
    Q_INVOKABLE void setStationData(StationDataRole role, int noteStationIndex, QVariant value);

    cwNoteTranformation* noteTransformation() const;

    QString guessNeighborStationName(const cwNoteStation& previousStation, QPointF stationNotePosition);

signals:
    //For scrap outline
    void insertedPoints(int begin, int end);
    void removedPoints(int begin, int end);

    //For stations
    void stationAdded();
    void stationPositionChanged(int noteStationIndex);
    void stationNameChanged(int noteStationIndex);
    void stationRemoved(int index);

    void noteTransformationChanged();

private:
    //The outline of the scrap, in normalized points
    QPolygonF OutlinePoints;

    //The stations that the scrap owns
    cwNoteTranformation* NoteTransformation;
    QList<cwNoteStation> Stations;

    //The parent trip, this is for referencing the stations
    cwNote* ParentNote;

    //Clamps a pointF that's in note coordinates to the scrap
    QPointF clampToScrap(QPointF point);
    bool pointOnLine(QLineF line, QPointF point);

    //For note station transformation, automatic calculation
//    void updateNoteTransformation();
//    QList< QPair <cwNoteStation, cwNoteStation> > noteShots() const;
//    QList< cwNoteTranformation > calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots) const;
//    cwNoteTranformation calculateShotTransformation(cwNoteStation station1, cwNoteStation station2) const;
//    cwNoteTranformation averageTransformations(QList< cwNoteTranformation > shotTransforms);

private slots:
    void updateStationsWithNewCave();

};

Q_DECLARE_METATYPE(cwScrap*)

/**
  \brief Adds a point to the scrap at the end of the scrap line
  */
inline void cwScrap::addPoint(QPointF point) {
    insertPoint(OutlinePoints.size(), point);
}

/**
  \brief Gets all the bound points in the scrap
  */
inline QVector<QPointF> cwScrap::points() {
    return OutlinePoints;
}

/**
  \brief Gets number of points in the scrap
  */
inline int cwScrap::numberOfPoints() const {
    return OutlinePoints.size();
}

/**
  \brief Gets the number of stations for a page of notes
  */
inline int cwScrap::numberOfStations() const {
    return Stations.count();
}

/**
  \brief Gets all the stations that the scrap owns
  */
inline const QList<cwNoteStation>& cwScrap::stations() const {
    return Stations;
}

/**
  Gets the station note transformation,  This is the note page real scale and
  rotation!
  */
inline cwNoteTranformation* cwScrap::noteTransformation() const {
    return NoteTransformation;
}

/**
  Returns the parent trip
  */
inline cwNote* cwScrap::parentNote() const {
    return ParentNote;
}


#endif // CWSCRAP_H
