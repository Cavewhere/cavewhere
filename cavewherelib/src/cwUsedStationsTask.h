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

    /**
     * @brief The Settings class,
     * This is the settings for the task, and how it return's data
     */
    class Settings {
    public:
        Settings() :
            Bold(true),
            Abbreviated(false),
            OnlyLargestRange(false)
        {}

        /**
         * @brief bold
         * @return True if the task will add bold sections of text, false, plain text
         */
        bool bold() const { return Bold; }
        void setBold(bool value) { Bold = value; }

        /**
         * @brief abbreviated
         * @return True if the task will abbreviate the results, removing filler words. False
         * if fill words are added
         */
        bool abbreviated() const { return Abbreviated; }
        void setAbbreviated(bool value) { Abbreviated = value; }

        /**
         * @brief onlyLargestRange
         * @return Return's only the group with the most number of stations
         */
        bool onlyLargestRange() const { return OnlyLargestRange; }
        void setOnlyLargestRange(bool value) { OnlyLargestRange = value; }

    private:
        bool Bold;
        bool Abbreviated;
        bool OnlyLargestRange;

    };

    explicit cwUsedStationsTask(QObject *parent = 0);

    Q_INVOKABLE void setStationNames(QList< cwStation > stations);
    Q_INVOKABLE void setSettings(cwUsedStationsTask::Settings settings);

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
        SurveyGroup(QString name, cwUsedStationsTask::Settings settings) :
            Name(name),
            TaskSettings(settings)
        {
        }

        void addStation(QString stationName);
        QList<SurveyGroup> createContinousGroups();

        QString groupString() const;
        QString name() const { return Name; }
        QList<QString> stations() const { return StationsNames; }

    private:
        QString Name;
        QList<QString> StationsNames;
        cwUsedStationsTask::Settings TaskSettings;

        static bool lessThanForNumericStation(QString left, QString right);
    };


    QList< cwStation > Stations;
    Settings TaskSettings;

    QList<SplitStationName> createSplitStationNames() const;
    QList<SurveyGroup> createSurveyGroups(QList<SplitStationName> stations) const;
    QList<SurveyGroup> groupGroupsByContinousStationNames(QList<SurveyGroup> ungrouped) const;
    QList<QString> groupStrings(QList<SurveyGroup> groups) const;

    static cwUsedStationsTask::SplitStationName splitName(cwStation stationName);


};

Q_DECLARE_METATYPE(cwUsedStationsTask::Settings)

/**
  Sets all the station names for the task
  */
inline void cwUsedStationsTask::setStationNames(QList< cwStation > stationNames) {
    Stations = stationNames;
}

inline void cwUsedStationsTask::setSettings(cwUsedStationsTask::Settings settings)
{
    TaskSettings = settings;
}

#endif // CWUSEDSTATIONSTASK_H
