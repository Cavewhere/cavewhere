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

    Q_PROPERTY(cwTrip* parentTrip READ parentTrip WRITE setParentTrip NOTIFY parentTripChanged)

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

    bool hasStation(QString stationName) const;
    QSet<cwStationReference> neighboringStations(QString stationName) const;

    Q_INVOKABLE bool isStationRole(cwSurveyChunk::DataRole role) const;
    Q_INVOKABLE bool isShotRole(cwSurveyChunk::DataRole role) const;

    Q_INVOKABLE QString guessLastStationName() const;
    QString guessNextStation(QString stationName) const;

signals:
    void parentTripChanged();

    void stationsAdded(int beginIndex, int endIndex);
    void shotsAdded(int beginIndex, int endIndex);

    void stationsRemoved(int beginIndex, int endIndex);
    void shotsRemoved(int beginIndex, int endIndex);

    void dataChanged(cwSurveyChunk::DataRole mainRole, int index, QVariant data);

public slots:
    int stationCount() const;
    cwStationReference station(int index) const;
    QList<int> indicesOfStation(QString stationName) const;

    int shotCount() const;
    cwShot* shot(int index) const;

    QPair<cwStationReference, cwStationReference> toFromStations(const cwShot* shot) const;

    void appendNewShot();
    void appendShot(cwStationReference fromStation, cwStationReference toStation, cwShot* shot);

    cwSurveyChunk* splitAtStation(int stationIndex);

    void insertStation(int stationIndex, Direction direction);
    void insertShot(int stationIndex, Direction direction);

    void removeStation(int stationIndex, Direction shot);
    bool canRemoveStation(int stationIndex, Direction shot);

    void removeShot(int shotIndex, Direction station);
    bool canRemoveShot(int shotIndex, Direction station);

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

    void setStationData(DataRole role, int index, const QVariant &data);
    void setShotData(DataRole role, int index, const QVariant &data);

};

Q_DECLARE_METATYPE(cwSurveyChunk*)

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
