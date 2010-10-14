#ifndef CWSurveyChunk_H
#define CWSurveyChunk_H

//Our includes
class cwStation;
class cwShot;

//Qt
#include <QObject>
#include <QList>
#include <QDeclarativeListProperty>

class cwSurveyChunk : public QObject {
    Q_OBJECT

//    Q_PROPERTY(QDeclarativeListProperty<cwStation> Stations READ qmlStations);

public:
    cwSurveyChunk(QObject *parent = 0);

    bool IsValid();
    bool CanAddShot(cwStation* fromStation, cwStation* toStation, cwShot* shot);

signals:
    void StationsAdded(int beginIndex, int endIndex);
    void ShotsAdded(int beginIndex, int endIndex);
//    void StationAdded(); //cwStation* station, int index);
//    void ShotAdded(); //cwShot* shot, int index);

public slots:
    int StationCount();
    cwStation* Station(int index);

    int ShotCount();
    cwShot* Shot(int index);

    void AddNewShot();
    void AddShot(cwStation* fromStation, cwStation* toStation, cwShot* shot);

private:
    QList<cwStation*> Stations;
    QList<cwShot*> Shots;

    bool ShotIndexCheck(int index) { return index >= 0 && index < Shots.count();  }
    bool StationIndexCheck(int index) { return index >= 0 && index < Stations.count(); }

};

#endif // CWSurveyChunk_H
