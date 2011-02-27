#ifndef CWUSEDSTATIONSTASK_H
#define CWUSEDSTATIONSTASK_H

//Our includes
#include <cwTask.h>
class cwStation;

//Qt includes
#include <QWeakPointer>
#include <QList>
#include <QString>

class cwUsedStationsTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwUsedStationsTask(QObject *parent = 0);

    Q_INVOKABLE void setStationNames(QList< QString > fullStationNames);

protected:
    virtual void runTask();

signals:

    //Return all the useds stations set by setStations
    void usedStations(QList<QString>);

public slots:

private:

    class SplitStationName {
    public:
        SplitStationName() { }
        SplitStationName(QString survey, QString stationNumber) {
            SurveyName = survey;
            StationNumber = stationNumber;
        }

        QString surveyName() const { return SurveyName; }
        QString stationName() const { return StationNumber; }

    private:
        QString SurveyName;
        QString StationNumber;
    };

    class SurveyGroup {
    public:
        SurveyGroup() { }
        SurveyGroup(QString name) {
            Name = name;
        }

        void addStation(QString stationName);
        QList<SurveyGroup> createContinousGroups();

        QString groupString() const;
        QString name() const { return Name; }
        QList<QString> stations() const { return Stations; }

    private:
        QString Name;
        QList<QString> Stations;

        static bool lessThanForNumericStation(QString left, QString right);
    };


    QList< QString > StationNames;

    QList<SplitStationName> createSpliteStationNames() const;
    QList<SurveyGroup> createSurveyGroups(QList<SplitStationName> stations) const;
    QList<SurveyGroup> groupGroupsByContinousStationNames(QList<SurveyGroup> ungrouped) const;
    QList<QString> groupStrings(QList<SurveyGroup> groups) const;

    static cwUsedStationsTask::SplitStationName splitName(QString stationName);


};

/**
  Sets all the station names for the task
  */
inline void cwUsedStationsTask::setStationNames(QList< QString > stationNames) {
    StationNames = stationNames;
}

#endif // CWUSEDSTATIONSTASK_H
