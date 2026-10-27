// cwSurveyNetwork.cpp

#include "cwSurveyNetwork.h"
#include "cwStation.h"


void cwSurveyNetwork::clear() { stationDataMap.clear(); }

void cwSurveyNetwork::addShot(const QString& fromStation, const QString& toStation) {
    addShotKeys(cwStation::canonicalKey(fromStation), cwStation::canonicalKey(toStation));
}

void cwSurveyNetwork::addNeighbor(const QString& station, const QString& neighbor) {
    addNeighborKey(cwStation::canonicalKey(station), cwStation::canonicalKey(neighbor));
}

void cwSurveyNetwork::addShotKeys(const QString& fromKey, const QString& toKey) {
    if (fromKey.isEmpty() || toKey.isEmpty()) {
        return;
    }

    {
        //This reference is only valid in this scope, only acces stationDataMap once in this scope (prevents use after free)
        StationData& fromStationData = stationDataMap[fromKey];
        if (!fromStationData.neighborList.contains(toKey)) {
            fromStationData.neighborList.append(toKey);
        }
    }

    {
        //This reference is only valid in this scope, only acces stationDataMap once in this scope (prevents use after free)
        StationData& toStationData = stationDataMap[toKey];
        if (!toStationData.neighborList.contains(fromKey)) {
            toStationData.neighborList.append(fromKey);
        }
    }
}

void cwSurveyNetwork::addNeighborKey(const QString& stationKey, const QString& neighborKey) {
    if (stationKey.isEmpty() || neighborKey.isEmpty()) {
        return;
    }

    StationData& stationData = stationDataMap[stationKey];
    if (!stationData.neighborList.contains(neighborKey)) {
        stationData.neighborList.append(neighborKey);
    }
}

QHash<QString, cwSurveyNetwork::StationData>::const_iterator cwSurveyNetwork::find(const QString& stationName) const {
    const auto exact = stationDataMap.constFind(stationName);
    if (exact != stationDataMap.constEnd()) {
        return exact;
    }
    const QString canonical = cwStation::canonicalKey(stationName);
    if (canonical == stationName) {
        return stationDataMap.constEnd();
    }
    return stationDataMap.constFind(canonical);
}

QStringList cwSurveyNetwork::neighbors(const QString& stationName) const {
    const auto dataIterator = find(stationName);
    if (dataIterator != stationDataMap.constEnd()) {
        return dataIterator->neighborList;
    }
    return {};
}

QStringList cwSurveyNetwork::stations() const { return stationDataMap.keys(); }

int cwSurveyNetwork::stationCount() const { return stationDataMap.size(); }

bool cwSurveyNetwork::hasStation(const QString& stationName) const {
    const QString trimmed = stationName.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    return find(trimmed) != stationDataMap.constEnd();
}

bool cwSurveyNetwork::isEmpty() const { return stationDataMap.isEmpty(); }

void cwSurveyNetwork::setPosition(const QString& stationName, const QVector3D& stationPosition) {
    setPositionAtKey(cwStation::canonicalKey(stationName), stationPosition);
}

QVector3D cwSurveyNetwork::position(const QString& stationName) const {
    const auto dataIterator = find(stationName);
    if (dataIterator != stationDataMap.constEnd() && dataIterator->positionIsSet) {
        return dataIterator->stationPosition;
    }
    return QVector3D();
}

bool cwSurveyNetwork::hasPosition(const QString& stationName) const {
    const auto dataIterator = find(stationName);
    return (dataIterator != stationDataMap.constEnd() && dataIterator->positionIsSet);
}

void cwSurveyNetwork::setPositionAtKey(const QString& key, const QVector3D& position) {
    StationData& stationData = stationDataMap[key];
    stationData.stationPosition = position;
    stationData.positionIsSet = true;
}

QString cwSurveyNetwork::key(const QString& stationName) const {
    const auto dataIterator = find(stationName);
    return dataIterator != stationDataMap.constEnd() ? dataIterator.key() : QString();
}

cwSurveyNetwork cwSurveyNetwork::scoped(const QString& prefix) const
{
    if (prefix.isEmpty()) {
        return *this;
    }

    const QString key = cwStation::canonicalKey(prefix);

    cwSurveyNetwork network;
    for (auto it = stationDataMap.constBegin(); it != stationDataMap.constEnd(); ++it) {
        if (!it.key().startsWith(key)) {
            continue;
        }
        const QString station = it.key().sliced(key.size());
        if (station.isEmpty()) {
            continue;
        }
        for (const QString& neighbor : it.value().neighborList) {
            if (neighbor.startsWith(key)) {
                network.addShotKeys(station, neighbor.sliced(key.size()));
            } else {
                network.addNeighborKey(station, neighbor);
            }
        }
        if (it.value().positionIsSet) {
            network.setPositionAtKey(station, it.value().stationPosition);
        }
    }
    return network;
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
