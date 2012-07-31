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
#include <cwTriangulatedData.h>
class cwNote;
class cwCave;

/**
  cwScrap holds a polygon of points that represents a scrap

  Points can be added or removed from the scrap.  All the points will be in
  normalize note coordinates system.
  */
class cwScrap : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwNoteTranformation* noteTransformation READ noteTransformation NOTIFY noteTransformationChanged)
    Q_PROPERTY(bool calculateNoteTransform READ calculateNoteTransform WRITE setCalculateNoteTransform NOTIFY calculateNoteTransformChanged)

    Q_ENUMS(StationDataRole)
public:

    enum StationDataRole {
        StationName,
        StationPosition
    };

    explicit cwScrap(QObject *parent = 0);
    cwScrap(const cwScrap& other);
    const cwScrap& operator =(const cwScrap& other);

    void setParentNote(cwNote* trip);
    cwNote* parentNote() const;

    void setParentCave(cwCave* cave);
    cwCave* parentCave() const;

    void addPoint(QPointF point);
    Q_INVOKABLE void insertPoint(int index, QPointF point);
    Q_INVOKABLE void removePoint(int index);
    QVector<QPointF> points() const;
    QPolygonF polygon() const;
    void setPoints(QVector<QPointF> points);
    Q_INVOKABLE void setPoint(int index, QPointF point);
    Q_INVOKABLE int numberOfPoints() const;
    Q_INVOKABLE bool isClosed() const;
    Q_INVOKABLE void close();

    void addStation(cwNoteStation station);
    Q_INVOKABLE void removeStation(int stationId);
    const QList<cwNoteStation>& stations() const;
    void setStations(QList<cwNoteStation> stations);
    cwNoteStation station(int stationId);
    int numberOfStations() const;
    Q_INVOKABLE QVariant stationData(StationDataRole role, int noteStationIndex) const;
    Q_INVOKABLE void setStationData(StationDataRole role, int noteStationIndex, QVariant value);

    cwNoteTranformation* noteTransformation() const;

    bool calculateNoteTransform() const;
    void setCalculateNoteTransform(bool calculateNoteTransform);

    QString guessNeighborStationName(const cwNoteStation& previousStation, QPointF stationNotePosition);

    QMatrix4x4 mapWorldToNoteMatrix(cwNoteStation stationOffset);

    void setTriangulationData(cwTriangulatedData data);
    cwTriangulatedData triangulationData() const;

public slots:
    void updateNoteTransformation();

signals:
    //For scrap outline
    void insertedPoints(int begin, int end);
    void removedPoints(int begin, int end);
    void pointChanged(int begin, int end);
    void pointsReset();
    void closeChanged();

    //For stations
    void stationAdded();
    void stationPositionChanged(int begin, int end);
    void stationNameChanged(int noteStationIndex);
    void stationRemoved(int index);
    void stationsReset();

    void noteTransformationChanged();
    void calculateNoteTransformChanged();

private:
    //The outline of the scrap, in normalized points
    QPolygonF OutlinePoints;

    //The stations that the scrap owns
    QList<cwNoteStation> Stations;

    //The note transform, this is used for guessing the station name's for the user
    cwNoteTranformation* NoteTransformation;
    bool CalculateNoteTransform; //!< If true this will automatically calculate the note transform

    //The parent trip, this is for referencing the stations
    cwNote* ParentNote;
    cwCave* ParentCave;

    //For rendering, points in note coordinates
    cwTriangulatedData TriangulationData;

    //Clamps a pointF that's in note coordinates to the scrap
    QPointF clampToScrap(QPointF point);
    bool pointOnLine(QLineF line, QPointF point);

    //For note station transformation, automatic calculation
    QList< QPair <cwNoteStation, cwNoteStation> > noteShots() const;
    QList< cwNoteTranformation > calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots) const;
    cwNoteTranformation calculateShotTransformation(cwNoteStation station1, cwNoteStation station2) const;
    cwNoteTranformation averageTransformations(QList< cwNoteTranformation > shotTransforms);

    const cwScrap& copy(const cwScrap& other);

private slots:
//    void updateStationsWithNewCave();

};

Q_DECLARE_METATYPE(cwScrap*)

/**
  \brief Adds a point to the scrap at the end of the scrap line
  */
inline void cwScrap::addPoint(QPointF point) {
    insertPoint(OutlinePoints.size(), point);
}

/**
  \brief Gets all the bound points in the scrappoi
  */
inline QVector<QPointF> cwScrap::points() const {
    return OutlinePoints;
}

/**
 * @brief cwScrap::polygon
 * @return The polygon that repesents the scrap
 *
 * This is the same as the OutlinePoints
 */
inline QPolygonF cwScrap::polygon() const {
    return QPolygonF(OutlinePoints);
}

/**
  \brief Gets number of points in the scrap
  */
inline int cwScrap::numberOfPoints() const {
    return OutlinePoints.size();
}

/**
 * @brief cwScrap::isClosed
 * @return Returns true if the the scrap geometry is closed, ie. the last point
 * equal to first point
 */
inline bool cwScrap::isClosed() const {
    if(numberOfPoints() <= 2) { return false; }
    return polygon().isClosed();
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



/**
Gets calculateNoteTransform
*/
inline bool cwScrap::calculateNoteTransform() const {
    return CalculateNoteTransform;
}

/**
    \brief Gets the parent cave
  */
inline cwCave *cwScrap::parentCave() const {
    return ParentCave;
}

/**
  \brief Sets the triangulation data
  */
inline void cwScrap::setTriangulationData(cwTriangulatedData data) {
    TriangulationData = data;
}

/**
  \brief Gets the triangulation data
  */
inline cwTriangulatedData cwScrap::triangulationData() const {
    return TriangulationData;
}


#endif // CWSCRAP_H
