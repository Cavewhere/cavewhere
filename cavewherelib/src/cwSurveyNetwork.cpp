// cwSurveyNetwork.cpp

#include "cwSurveyNetwork.h"


void cwSurveyNetwork::clear() { stationDataMap.clear(); }

void cwSurveyNetwork::addShot(const QString& fromStation, const QString& toStation) {
    QString from = fromStation.toLower();
    QString to = toStation.toLower();

    if (from.isEmpty() || to.isEmpty()) {
        return;
    }

    {
        //This reference is only valid in this scope, only acces stationDataMap once in this scope (prevents use after free)
        StationData& fromStationData = stationDataMap[from];
        if (!fromStationData.neighborList.contains(to)) {
            fromStationData.neighborList.append(to);
        }
    }

    {
        //This reference is only valid in this scope, only acces stationDataMap once in this scope (prevents use after free)
        StationData& toStationData = stationDataMap[to];
        if (!toStationData.neighborList.contains(from)) {
            toStationData.neighborList.append(from);
        }
    }
}

QStringList cwSurveyNetwork::neighbors(const QString& stationName) const {
    QString uppercaseName = stationName.toLower();
    auto dataIterator = stationDataMap.constFind(uppercaseName);
    if (dataIterator != stationDataMap.constEnd()) {
        return dataIterator->neighborList;
    }
    return {};
}

QStringList cwSurveyNetwork::stations() const { return stationDataMap.keys(); }

bool cwSurveyNetwork::isEmpty() const { return stationDataMap.isEmpty(); }

void cwSurveyNetwork::setPosition(const QString& stationName, const QVector3D& stationPosition) {
    QString uppercaseName = stationName.toLower();
    StationData& stationData = stationDataMap[uppercaseName];
    stationData.stationPosition = stationPosition;
    stationData.positionIsSet = true;
}

QVector3D cwSurveyNetwork::position(const QString& stationName) const {
    QString uppercaseName = stationName.toLower();
    auto dataIterator = stationDataMap.constFind(uppercaseName);
    if (dataIterator != stationDataMap.constEnd() && dataIterator->positionIsSet) {
        return dataIterator->stationPosition;
    }
    return QVector3D();
}

bool cwSurveyNetwork::hasPosition(const QString& stationName) const {
    QString uppercaseName = stationName.toLower();
    auto dataIterator = stationDataMap.constFind(uppercaseName);
    return (dataIterator != stationDataMap.constEnd() && dataIterator->positionIsSet);
}

QStringList cwSurveyNetwork::changedStations(const cwSurveyNetwork& firstNetwork, const cwSurveyNetwork& secondNetwork) {
    if (firstNetwork.stationDataMap == secondNetwork.stationDataMap) {
        return {};
    }

    const QSet<QString> firstStationSet = cw::toSet(firstNetwork.stations());
    const QSet<QString> secondStationSet = cw::toSet(secondNetwork.stations());

    const QSet<QString> stationsOnlyInFirst = firstStationSet - secondStationSet;
    const QSet<QString> stationsOnlyInSecond = secondStationSet - firstStationSet;
    const QSet<QString> commonStationSet = firstStationSet & secondStationSet;

    QStringList changedList;
    changedList.reserve(stationsOnlyInFirst.count() + stationsOnlyInSecond.count());

    for (const QString& stationName : stationsOnlyInFirst) {
        changedList.append(stationName);
    }

    for (const QString& stationName : stationsOnlyInSecond) {
        changedList.append(stationName);
    }

    for (const QString& stationName : commonStationSet) {
        QSet<QString> neighborsInFirst = cw::toSet(firstNetwork.neighbors(stationName));
        QSet<QString> neighborsInSecond = cw::toSet(secondNetwork.neighbors(stationName));
        if (neighborsInFirst != neighborsInSecond) {
            changedList.append(stationName);
        }
    }
    return changedList;
}

bool cwSurveyNetwork::operator==(const cwSurveyNetwork& other) const {
    return stationDataMap == other.stationDataMap
           || changedStations(*this, other).isEmpty();
}
