/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwSurveyNetwork.h"


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

