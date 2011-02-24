#ifndef CWSurveyChunk_H
#define CWSurveyChunk_H

//Our includes
class cwStationReference;
class cwShot;
class cwTrip;
class cwCave;

//Qt
#include <QObject>
#include <QList>
#include <QDeclarativeListProperty>

class cwSurveyChunk : public QObject {
    Q_OBJECT

public:
    enum Direction {
        Above,
        Below
    };

    cwSurveyChunk(QObject *parent = 0);
    cwSurveyChunk(const cwSurveyChunk& chunk);

    bool isValid() const;
    bool canAddShot(cwStationReference* fromStation, cwStationReference* toStation, cwShot* shot);

    void setParentTrip(cwTrip* trip);
    cwTrip* parentTrip() const;

    cwCave* parentCave() const;

    void updateStationsWithNewCave();

signals:
    void StationsAdded(int beginIndex, int endIndex);
    void ShotsAdded(int beginIndex, int endIndex);

    void StationsRemoved(int beginIndex, int endIndex);
    void ShotsRemoved(int beginIndex, int endIndex);

public slots:
    int StationCount() const;
    cwStationReference* Station(int index) const;

    int ShotCount() const;
    cwShot* Shot(int index) const;

    QPair<cwStationReference*, cwStationReference*> ToFromStations(const cwShot* shot) const;

    void AppendNewShot();
    void AppendShot(cwStationReference* fromStation, cwStationReference* toStation, cwShot* shot);


    cwSurveyChunk* SplitAtStation(int stationIndex);

    void InsertStation(int stationIndex, Direction direction);
    void InsertShot(int stationIndex, Direction direction);

    void RemoveStation(int stationIndex, Direction shot);
    bool CanRemoveStation(int stationIndex, Direction shot);

    void RemoveShot(int shotIndex, Direction station);
    bool CanRemoveShot(int shotIndex, Direction station);

private:
    QList<cwStationReference*> Stations;
    QList<cwShot*> Shots;
    cwTrip* ParentTrip;

    bool shotIndexCheck(int index) const { return index >= 0 && index < Shots.count();  }
    bool stationIndexCheck(int index) const { return index >= 0 && index < Stations.count(); }

    void remove(int stationIndex, int shotIndex);
    int index(int index, Direction direction);

    void updateStationsCave(cwStationReference* station);

    cwStationReference* createNewStation();

};

/**
  \brief Gets the parent trip for this chunk
  */
inline cwTrip* cwSurveyChunk::parentTrip() const {
    return ParentTrip;
}


#endif // CWSurveyChunk_H
