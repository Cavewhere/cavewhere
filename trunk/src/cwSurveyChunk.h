#ifndef CWSurveyChunk_H
#define CWSurveyChunk_H

//Our includes
#include <cwStationReference.h>
class cwShot;
class cwTrip;
class cwCave;

//Qt
#include <QObject>
#include <QList>
#include <QDeclarativeListProperty>
#include <QVariant>

class cwSurveyChunk : public QObject {
    Q_OBJECT
    Q_ENUMS(DataRole)

public:
    enum Direction {
        Above,
        Below
    };

    enum DataRole {
        StationNameRole,
        StationLeftRole,
        StationRightRole,
        StationUpRole,
        StationDownRole,
        ShotDistanceRole,
        ShotCompassRole,
        ShotBackCompassRole,
        ShotClinoRole,
        ShotBackClinoRole
    };

    cwSurveyChunk(QObject *parent = 0);
    cwSurveyChunk(const cwSurveyChunk& chunk);
    virtual ~cwSurveyChunk();

    bool isValid() const;
    bool canAddShot(const cwStationReference& fromStation, const cwStationReference& toStation, cwShot* shot);

    void setParentTrip(cwTrip* trip);
    cwTrip* parentTrip() const;

    cwCave* parentCave() const;

    void updateStationsWithNewCave();

    QList<cwStationReference> stations() const;
    QList<cwShot*> shots() const;
signals:
    void StationsAdded(int beginIndex, int endIndex);
    void ShotsAdded(int beginIndex, int endIndex);

    void StationsRemoved(int beginIndex, int endIndex);
    void ShotsRemoved(int beginIndex, int endIndex);

    void dataChanged(DataRole mainRole, int index, QVariant data);

public slots:
    int StationCount() const;
    cwStationReference Station(int index) const;

    int ShotCount() const;
    cwShot* Shot(int index) const;

    QPair<cwStationReference, cwStationReference> ToFromStations(const cwShot* shot) const;

    void AppendNewShot();
    void AppendShot(cwStationReference fromStation, cwStationReference toStation, cwShot* shot);

    cwSurveyChunk* SplitAtStation(int stationIndex);

    void InsertStation(int stationIndex, Direction direction);
    void InsertShot(int stationIndex, Direction direction);

    void RemoveStation(int stationIndex, Direction shot);
    bool CanRemoveStation(int stationIndex, Direction shot);

    void RemoveShot(int shotIndex, Direction station);
    bool CanRemoveShot(int shotIndex, Direction station);

    QVariant data(DataRole role, int index) const;
    void setData(DataRole role, int index, QVariant data);

private:
    QList<cwStationReference> Stations;
    QList<cwShot*> Shots;
    cwTrip* ParentTrip;

    bool shotIndexCheck(int index) const { return index >= 0 && index < Shots.count();  }
    bool stationIndexCheck(int index) const { return index >= 0 && index < Stations.count(); }

    void remove(int stationIndex, int shotIndex);
    int index(int index, Direction direction);

    void updateStationsCave(cwStationReference station);

    cwStationReference createNewStation();

    QVariant stationData(DataRole role, int index) const;
    QVariant shotData(DataRole role, int index) const;

};

/**
  \brief Gets the parent trip for this chunk
  */
inline cwTrip* cwSurveyChunk::parentTrip() const {
    return ParentTrip;
}

/**
  \brief Gets all the stations

  You shouldn't modify the station data from this list
  */
inline QList<cwStationReference> cwSurveyChunk::stations() const {
    return Stations;
}

/**
  \brief Gets all the shot date

  You shouldn't modify the shot data from this list
  */
inline QList<cwShot*> cwSurveyChunk::shots() const {
    return Shots;
}


#endif // CWSurveyChunk_H
