// cwSurveyNetwork.cpp

#include "cwSurveyNetwork.h"

// single struct holding both adjacency and optional position
struct StationData {
    QStringList neighborList;
    QVector3D stationPosition;
    bool positionIsSet;

    StationData() : neighborList(), stationPosition(), positionIsSet(false) {}

    bool operator==(const StationData& other) const {
        return neighborList == other.neighborList
               && positionIsSet == other.positionIsSet
               && stationPosition == other.stationPosition;
    }

    bool operator!=(const StationData& other) const { return !operator==(other); }
};

class cwSurveyNetworkData : public QSharedData {
public:
    QHash<QString, StationData> stationDataMap;
};

cwSurveyNetwork::cwSurveyNetwork() : m_data(new cwSurveyNetworkData) {}

cwSurveyNetwork::cwSurveyNetwork(const cwSurveyNetwork& other) : m_data(other.m_data) {}

cwSurveyNetwork& cwSurveyNetwork::operator=(const cwSurveyNetwork& other) {
    if (this != &other) {
        m_data.operator=(other.m_data);
    }
    return *this;
}

cwSurveyNetwork::~cwSurveyNetwork() {}

void cwSurveyNetwork::clear() { m_data->stationDataMap.clear(); }

void cwSurveyNetwork::addShot(const QString& fromStation, const QString& toStation) {
    QString uppercaseFrom = fromStation.toUpper();
    QString uppercaseTo = toStation.toUpper();

    if (uppercaseFrom.isEmpty() || uppercaseTo.isEmpty()) {
        return;
    }

    StationData& fromStationData = m_data->stationDataMap[uppercaseFrom];
    StationData& toStationData = m_data->stationDataMap[uppercaseTo];

    if (!fromStationData.neighborList.contains(uppercaseTo)) {
        fromStationData.neighborList.append(uppercaseTo);
    }
    if (!toStationData.neighborList.contains(uppercaseFrom)) {
        toStationData.neighborList.append(uppercaseFrom);
    }
}

QStringList cwSurveyNetwork::neighbors(const QString& stationName) const {
    QString uppercaseName = stationName.toUpper();
    auto dataIterator = m_data->stationDataMap.constFind(uppercaseName);
    if (dataIterator != m_data->stationDataMap.constEnd()) {
        return dataIterator->neighborList;
    }
    return {};
}

QStringList cwSurveyNetwork::stations() const { return m_data->stationDataMap.keys(); }

bool cwSurveyNetwork::isEmpty() const { return m_data->stationDataMap.isEmpty(); }

void cwSurveyNetwork::setPosition(const QString& stationName, const QVector3D& stationPosition) {
    QString uppercaseName = stationName.toUpper();
    StationData& stationData = m_data->stationDataMap[uppercaseName];
    stationData.stationPosition = stationPosition;
    stationData.positionIsSet = true;
}

QVector3D cwSurveyNetwork::position(const QString& stationName) const {
    QString uppercaseName = stationName.toUpper();
    auto dataIterator = m_data->stationDataMap.constFind(uppercaseName);
    if (dataIterator != m_data->stationDataMap.constEnd() && dataIterator->positionIsSet) {
        return dataIterator->stationPosition;
    }
    return QVector3D();
}

bool cwSurveyNetwork::hasPosition(const QString& stationName) const {
    QString uppercaseName = stationName.toUpper();
    auto dataIterator = m_data->stationDataMap.constFind(uppercaseName);
    return (dataIterator != m_data->stationDataMap.constEnd() && dataIterator->positionIsSet);
}

QStringList cwSurveyNetwork::changedStations(const cwSurveyNetwork& firstNetwork, const cwSurveyNetwork& secondNetwork) {
    if (firstNetwork.m_data == secondNetwork.m_data) {
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
    return m_data == other.m_data
           || m_data->stationDataMap == other.m_data->stationDataMap
           || changedStations(*this, other).isEmpty();
}
