/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStationPosition.h"

//Qt includes
#include <QSharedData>
#include <QVector3D>

class cwStationPositionData : public QSharedData {
public:
    QVector3D Position;
    QMatrix3x3 Convariance;
};

cwStationPosition::cwStationPosition() : d(new cwStationPositionData)
{
}

cwStationPosition::cwStationPosition(const cwStationPosition &rhs) : d(rhs.d)
{
}

cwStationPosition &cwStationPosition::operator=(const cwStationPosition &rhs)
{
    if (this != &rhs)
        d.operator=(rhs.d);
    return *this;
}

cwStationPosition::~cwStationPosition()
{
}

/**
 * @brief cwStationInfo::setPosition
 * @param position
 *
 * Returns the position of the station
 */
void cwStationPosition::setPosition(QVector3D position)
{
    d->Position = position;
}

/**
 * @brief cwStationInfo::position
 * @return Returns the position of the station
 */
QVector3D cwStationPosition::position() const
{
    return d->Position;
}

/**
 * @brief cwStationInfo::setConvariance
 * @param convariance
 * Sets the cumulative convariance
 */
void cwStationPosition::setConvariance(QMatrix3x3 convariance)
{
    d->Convariance = convariance;
}

/**
 * Returns the convariance that this station, this is the cumulative convariance
 * from the enterance of the cave. If there's more than one entrance, this is the
 * convariance of the loop.
 */
QMatrix3x3 cwStationPosition::convariance() const
{
    return d->Convariance;
}
