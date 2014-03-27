/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSTATIONINFO_H
#define CWSTATIONINFO_H

//Qt includes
#include <QSharedDataPointer>
#include <QMatrix3x3>

class cwStationPositionData;

/**
 * @brief The cwStationInfo class
 *
 * This class stores station infomation about, station's position, cumultive convarience matrix.
 *
 * There is a one staton information object per station.
 */
class cwStationPosition
{
public:
    cwStationPosition();
    cwStationPosition(const cwStationPosition &);
    cwStationPosition &operator=(const cwStationPosition &);
    ~cwStationPosition();

    void setPosition(QVector3D position);
    QVector3D position() const;

    void setConvariance(QMatrix3x3 convariance);
    QMatrix3x3 convariance() const;

private:
    QSharedDataPointer<cwStationPositionData> d;
};

#endif // CWSTATIONINFO_H
