/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwUsedStationsTask.h"
#include "cwDebug.h"

//Qt includes
#include <QRegularExpression>
#include <QMap>
#include <QDebug>
#include <QtConcurrentMap>

cwUsedStationsTask::cwUsedStationsTask(QObject *parent) :
    cwTask(parent)
{
}

/**
  \brief This will concatenate like station names

  Example of results:
  A Suvrey, Stations 1 to 1000
  Stations 1 to 1000
  Station 324

  This will emit the results in usedStations.  The results will be
  sorted from number blocks then survey letter
  */
void cwUsedStationsTask::runTask() {

    if(!isRunning()) { done(); return; }

    //Split the station names into survey and station number
    QList<cwUsedStationsTask::SplitStationName> splitStations = createSplitStationNames();

    if(!isRunning()) { done(); return; }

    //Group the stations by survey
    QList<SurveyGroup> surveyGroups = createSurveyGroups(splitStations);

    if(!isRunning()) { done(); return; }

    //Re group the survey groups, such that concurring station come together
    surveyGroups = groupGroupsByContinousStationNames(surveyGroups);

    if(!isRunning()) { done(); return; }

    //Convert the surveygroups into strings
    QList<QString> groupedStations = groupStrings(surveyGroups);

    if(!isRunning()) { done(); return; }

    //Return the groupedStations if anyone is listening
    emit usedStations(groupedStations);

    //We are done
    done();


}

/**
 * This is a threaded helper function to createSplitStationNames
 */
cwUsedStationsTask::SplitStationName cwUsedStationsTask::splitName(cwStation station) {
    const static QRegularExpression namePartRegExp("\\d+\\D*$");
    QRegularExpressionMatch match = namePartRegExp.match(station.name());
    int index = match.hasMatch() ? match.capturedStart(0) : 0;

    QString surveyName = station.name().left(index).toUpper();
    QString stationName = station.name().mid(index);

    return cwUsedStationsTask::SplitStationName(surveyName, stationName);
}

/**
  \brief Splits the StationNames into the survey and StationNamuber
  */
QList<cwUsedStationsTask::SplitStationName> cwUsedStationsTask::createSplitStationNames() const {
    return QtConcurrent::blockingMapped(Stations, splitName);
}

/**
  \brief Group the stations by survey station name in splitStations
  */
QList<cwUsedStationsTask::SurveyGroup> cwUsedStationsTask::createSurveyGroups(QList<cwUsedStationsTask::SplitStationName> splitStations) const {
    QMap<QString, SurveyGroup> groupLookup;
    foreach(SplitStationName station, splitStations) {
        if(groupLookup.contains(station.surveyName())) {
            //Add it to existing group
            SurveyGroup& group = groupLookup[station.surveyName()];
            group.addStation(station.stationName());
        } else {
            //Create a group
            SurveyGroup group(station.surveyName(), TaskSettings);
            group.addStation(station.stationName());
            groupLookup[group.name()] = group;
        }
    }

    return groupLookup.values();
}

/**
  Re group the survey groups, such that concurring station come together
  */
QList<cwUsedStationsTask::SurveyGroup> cwUsedStationsTask::groupGroupsByContinousStationNames(QList<cwUsedStationsTask::SurveyGroup> ungrouped) const {

    QList<cwUsedStationsTask::SurveyGroup> grouped;

    //Create contiguous groups for each suvrey group
    foreach(cwUsedStationsTask::SurveyGroup currentGroup, ungrouped) {
        grouped.append(currentGroup.createContinousGroups());
    }

    return grouped;
}

void cwUsedStationsTask::SurveyGroup::addStation(QString stationName) {
    StationsNames.append(stationName);
}


/**
  This function assumes that the groups stations are stored,

  This returns the groupString in a rich text format
  */
QString cwUsedStationsTask::SurveyGroup::groupString() const {

    Q_ASSERT(!StationsNames.isEmpty());
    QString groupString;
    if(!Name.isEmpty()) {
        QString argsString;
        argsString += TaskSettings.bold() ? QString("<b>%1</b>") : QString("%1");
        argsString += TaskSettings.abbreviated() ? " " : " Survey, ";
        groupString += argsString.arg(Name);
    }

    if(StationsNames.size() == 1) {
        //Only one station
        QString station = StationsNames.first();

        QString argsString;
        argsString += TaskSettings.abbreviated() ? "" : "Station ";
        argsString += TaskSettings.bold() ? QString(" <b>%1</b>") : QString("%1");

        if(station.isEmpty() && Name.isEmpty()) { return QString(); }
        if(station.isEmpty()) {
            groupString = argsString.arg(Name); //The group string is really the station
        } else {
            groupString += argsString.arg(station);
        }

    } else {
        //More than one station
        QString argsString;
        argsString += TaskSettings.abbreviated() ? "" : "Stations ";
        argsString += TaskSettings.bold() ? QString(" <b>%1</b>") : QString("%1");
        argsString += TaskSettings.abbreviated() ? "-" : " to ";
        argsString += TaskSettings.bold() ? QString(" <b>%2</b>") : QString("%2");
        groupString += argsString.arg(StationsNames.first(), StationsNames.last());
    }

    return groupString;
}

/**
  \brief This creates contigous group of survey groups

  It groups the survey by consecutive station names.  This will sort the names
  */
QList<cwUsedStationsTask::SurveyGroup> cwUsedStationsTask::SurveyGroup::createContinousGroups() {

    QList<QString> nonNumericStations; //Can't be sorted
    QList<QString> numericStations; //Can be storted

    //Seperate the numeric from the nonNumeric
    bool okay;
    foreach(QString station, StationsNames) {
        station.toInt(&okay);
        if(okay) {
            numericStations.append(station);
        } else {
            nonNumericStations.append(station);
        }
    }

    //Sort the non-numeric stations by string
    std::sort(numericStations.begin(), numericStations.end());

    //Sort the numericStations by number, use stable sort because items are mostely sorted
    std::stable_sort(numericStations.begin(), numericStations.end(), lessThanForNumericStation);

    //Group the numericStations into contious groups
    QList<SurveyGroup> continousGroups;
    if(!numericStations.isEmpty()) {
        QString firstStation = numericStations.first();
        int previousStation = firstStation.toInt();

        SurveyGroup currentGroup(Name, TaskSettings);
        currentGroup.addStation(firstStation);

        for(int i = 1; i < numericStations.size(); i++) {
            QString stationNumber = numericStations.at(i);
            if(previousStation != stationNumber.toInt()) {
                if(previousStation + 1 != stationNumber.toInt()) {
                    //Add the current group to the continous groups
                    continousGroups.append(currentGroup);

                    //Create a new group
                    currentGroup = SurveyGroup(Name, TaskSettings);
                    currentGroup.addStation(stationNumber);
                }
                //Add the station to this group
                currentGroup.addStation(stationNumber);
            }

            previousStation = stationNumber.toInt();
        }
        //Add the current group
        continousGroups.append(currentGroup);
    }


    //Add the non-numeric stations
    foreach(QString nonNumericStation, nonNumericStations) {
        SurveyGroup group(Name, TaskSettings);
        group.addStation(nonNumericStation);
        continousGroups.append(group);
    }

    return continousGroups;
}

/**
  Compares to numeric stations (on that only are numbers) to each other.

  If the left is less than the right this return true else it returns false
  */
bool cwUsedStationsTask::SurveyGroup::lessThanForNumericStation(QString left, QString right) {
    int leftStationNumber = left.toInt();
    int rightStationNumber = right.toInt();
    return leftStationNumber < rightStationNumber;
}

/**
  \brief Converts each group into a string
  */
QList<QString> cwUsedStationsTask::groupStrings(QList<SurveyGroup> groups) const {

    if(TaskSettings.onlyLargestRange()) {
        SurveyGroup largestGroup;
        foreach(const SurveyGroup& group, groups) {
            if(largestGroup.stations().size() < group.stations().size()) {
                //Found a large group
                largestGroup = group;
            }
        }
        return QStringList() << largestGroup.groupString();
    }


    QStringList groupStrings;
    foreach(SurveyGroup group, groups) {
        QString groupString = group.groupString();
        if(!groupString.isEmpty()) {
            groupStrings.append(groupString);
        }
    }
    return groupStrings;
}

