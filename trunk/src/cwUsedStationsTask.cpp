//Our includes
#include "cwUsedStationsTask.h"


//Qt includes
#include <QRegExp>
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
    QList<cwUsedStationsTask::SplitStationName> splitStations = createSpliteStationNames();

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

cwUsedStationsTask::SplitStationName cwUsedStationsTask::splitName(QString stationName) {
    QRegExp splitStationsReg("((?:[a-z]|[A-Z])*)(\\w+)");
    if(splitStationsReg.exactMatch(stationName)) {
        QString surveyName = splitStationsReg.cap(1).toUpper();
        QString stationName = splitStationsReg.cap(2);

        cwUsedStationsTask::SplitStationName splitStation(surveyName, stationName);
        return splitStation;
    } else {
        qDebug() << "Can't match: " << stationName;
    }
    return cwUsedStationsTask::SplitStationName();
}

/**
  \brief Splits the StationNames into the survey and StationNamuber
  */
QList<cwUsedStationsTask::SplitStationName> cwUsedStationsTask::createSpliteStationNames() const {


    //QFuture<cwUsedStationsTask::SplitStationName> future =

    return QtConcurrent::blockingMapped(StationNames, splitName);

//    QList<cwUsedStationsTask::SplitStationName> splitStations;

//    QRegExp splitStationsReg("((?:[a-z]|[A-Z])*)(\\w+)");
//    foreach(QString stationName, StationNames) {
//        if(splitStationsReg.exactMatch(stationName)) {
//            QString surveyName = splitStationsReg.cap(1);
//            QString stationName = splitStationsReg.cap(2);

//            cwUsedStationsTask::SplitStationName splitStation(surveyName, stationName);
//            splitStations.append(splitStation);
//        } else {
//            qDebug() << "Can't match: " << stationName;
//        }
//    }

   // return splitStations;
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
            SurveyGroup group(station.surveyName());
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
    Stations.append(stationName);
}


/**
  This function assumes that the groups stations are stored,

  This returns the groupString in a rich text format
  */
QString cwUsedStationsTask::SurveyGroup::groupString() const {

    Q_ASSERT(!Stations.isEmpty());
    QString groupString;
    if(!Name.isEmpty()) {
        groupString += QString("<b>%1</b> Survey, ").arg(Name);
    }

    if(Stations.size() == 1) {
        //Only one station
        groupString += QString("Station <b>%1</b>").arg(Stations.first());
    } else {
        //More than one station
        groupString += QString("Stations <b>%1</b> to <b>%2</b>").arg(Stations.first(), Stations.last());
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
    foreach(QString station, Stations) {
        station.toInt(&okay);
        if(okay) {
            numericStations.append(station);
        } else {
            nonNumericStations.append(station);
        }
    }

    //Sort the non-numeric stations by string
    qSort(numericStations);

    //Sort the numericStations by number, use stable sort because items are mostely sorted
    qStableSort(numericStations.begin(), numericStations.end(), lessThanForNumericStation);

    //Group the numericStations into contious groups
    QList<SurveyGroup> continousGroups;
    if(!numericStations.isEmpty()) {
        QString firstStation = numericStations.first();
        int previousStation = firstStation.toInt();

        SurveyGroup currentGroup(Name);
        currentGroup.addStation(firstStation);

        for(int i = 1; i < numericStations.size(); i++) {
            QString stationNumber = numericStations.at(i);
            if(previousStation + 1 != stationNumber.toInt()) {
                //Add the current group to the continous groups
                continousGroups.append(currentGroup);

                //Create a new group
                currentGroup = SurveyGroup(Name);
                //currentGroup.addStation(stationNumber);
            }
            //Add the station to this group
            currentGroup.addStation(stationNumber);
            previousStation = stationNumber.toInt();
        }

        //Add the current group
        continousGroups.append(currentGroup);
    }


    //Add the non-numeric stations
    foreach(QString nonNumericStation, nonNumericStations) {
        SurveyGroup group(Name);
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
    QList<QString> groupStrings;
    foreach(SurveyGroup group, groups) {
        groupStrings.append(group.groupString());
    }
    return groupStrings;
}
