/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyNetwork.h"

//Qt includes
#include <QSet>

class cwSurveyNetworkData : public QSharedData
{
public:

    QHash<QString, QStringList> Network;
};

cwSurveyNetwork::cwSurveyNetwork() : data(new cwSurveyNetworkData)
{

}

cwSurveyNetwork::cwSurveyNetwork(const cwSurveyNetwork &rhs) : data(rhs.data)
{

}

cwSurveyNetwork &cwSurveyNetwork::operator=(const cwSurveyNetwork &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwSurveyNetwork::~cwSurveyNetwork()
{

}

/**
 * Clears all the entries in the station network
 */
void cwSurveyNetwork::clear()
{
    data->Network.clear();
}

/**
 * Adds a new shot to the network. With from station name to the to station name.
 * If the shot already exists in the network this will do nothing. The station names are
 * incasesesitive.
 */
void cwSurveyNetwork::addShot(QString from, QString to) {
    from = from.toUpper();
    to = to.toUpper();

    if(from.isEmpty() || to.isEmpty()) { return; }

    if(!data->Network.contains(from)) {
        data->Network.insert(from, QStringList());
    }

    if(!data->Network.contains(to)) {
        data->Network.insert(to, QStringList());
    }

    QStringList& fromNeighbors = data->Network[from];
    QStringList& toNeighbors = data->Network[to];

    if(!fromNeighbors.contains(to)) {
        fromNeighbors.append(to);
    }

    if(!toNeighbors.contains(from)) {
        toNeighbors.append(from);
    }
}

/**
 * Returns all the neighboring stations at stationName. If stationName doesn't
 * exist in the network or has no neighboring stations, this returns an empty
 * list.
 */
QStringList cwSurveyNetwork::neighbors(QString stationName) const
{
    stationName = stationName.toUpper();
    return data->Network.value(stationName);
}

QStringList cwSurveyNetwork::stations() const
{
    return data->Network.keys();
}

bool cwSurveyNetwork::isEmpty() const
{
    return data->Network.isEmpty();
}

/**
 * Returns a list of change station names between two cwSurveyNetwork n1 and n2.
 *
 * A station is consider changed if it only exists in one of the networks or
 * if the stations neighbors don't match.
 *
 * Passing two of the same networks will return an emtpy list of stations
 */
QStringList cwSurveyNetwork::changedStations(const cwSurveyNetwork &n1, const cwSurveyNetwork &n2)
{
    if(n1.data == n2.data) {
        return QStringList();
    }

    auto stations1 = n1.stations().toSet();
    auto stations2 = n2.stations().toSet();

    auto newStations1 = stations1 - stations2;
    auto newStations2 = stations2 - stations1;
    auto sameStations = stations1 & stations2;

    QStringList changedStations = newStations1.toList() + newStations2.toList();

    for(auto station : sameStations) {
        auto neighbors1 = n1.neighbors(station).toSet();
        auto neighbors2 = n2.neighbors(station).toSet();
        if(neighbors1 != neighbors2) {
            changedStations.append(station);
        }
    }

    return changedStations;
}

/**
 * Returns true if the survey network is the same.
 */
bool cwSurveyNetwork::operator==(const cwSurveyNetwork &other) const
{
    return data == other.data || data->Network == other.data->Network;
}

