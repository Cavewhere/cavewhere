/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUSEDSTATIONSTASK_H
#define CWUSEDSTATIONSTASK_H

//Our includes
#include "cwTask.h"
#include "cwStation.h"

//Qt includes
#include <QWeakPointer>
#include <QList>
#include <QString>

class cwUsedStationsTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwUsedStationsTask(QObject *parent = 0);

    Q_INVOKABLE void setStationNames(QList< cwStation > stations);

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
        QList<QString> stations() const { return StationsNames; }

    private:
        QString Name;
        QList<QString> StationsNames;

        static bool lessThanForNumericStation(QString left, QString right);
    };


    QList< cwStation > Stations;

    QList<SplitStationName> createSplitStationNames() const;
    QList<SurveyGroup> createSurveyGroups(QList<SplitStationName> stations) const;
    QList<SurveyGroup> groupGroupsByContinousStationNames(QList<SurveyGroup> ungrouped) const;
    QList<QString> groupStrings(QList<SurveyGroup> groups) const;

    static cwUsedStationsTask::SplitStationName splitName(cwStation stationName);


};

/**
  Sets all the station names for the task
  */
inline void cwUsedStationsTask::setStationNames(QList< cwStation > stationNames) {
    Stations = stationNames;
}

#endif // CWUSEDSTATIONSTASK_H
