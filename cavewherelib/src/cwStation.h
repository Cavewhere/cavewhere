/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef cwStation_H
#define cwStation_H

//Qt includes
#include <QSharedDataPointer>

//Our includes
#include "cwGlobals.h"
#include "cwDistanceReading.h"

class CAVEWHERE_LIB_EXPORT cwStation {

public:
    enum DataRoles {
        NameRole,
        LeftRole,
        RightRole,
        UpRole,
        DownRole
    };

    cwStation();
    cwStation(const cwStation& station);
    cwStation(QString name);

    QString name() const;
    bool setName(QString Name);

    cwDistanceReading left() const;
    void setLeft(const cwDistanceReading& left) { Data->m_left = left; }

    cwDistanceReading right() const;
    void setRight(const cwDistanceReading& right) { Data->m_right = right; }

    cwDistanceReading up() const;
    void setUp(const cwDistanceReading& up) { Data->m_up = up; }

    cwDistanceReading down() const;
    void setDown(const cwDistanceReading& down) { Data->m_down = down; }

    void setData(QVariant data, DataRoles role);
    QVariant data(DataRoles role) const;

    bool isValid() const { return !Data->Name.isEmpty(); }

    bool operator ==(const cwStation& station) const;
    bool operator !=(const cwStation& station) const;

    static bool nameIsValid(QString stationName);

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData();
        PrivateData(QString name);

        QString Name;

        cwDistanceReading m_left;
        cwDistanceReading m_right;
        cwDistanceReading m_up;
        cwDistanceReading m_down;
    };

    QSharedDataPointer<PrivateData> Data;
};

inline size_t qHash(const cwStation& station) {
    return qHash(station.name().toLower());
}

inline QString cwStation::name() const { return Data->Name; }
inline cwDistanceReading cwStation::left() const { return Data->m_left; }
inline cwDistanceReading cwStation::right() const { return Data->m_right; }
inline cwDistanceReading cwStation::up() const { return Data->m_up; }
inline cwDistanceReading cwStation::down() const { return Data->m_down; }

inline bool cwStation::operator ==(const cwStation &station) const
{
    return station.name().compare(station.name()) == 0;
}

inline bool cwStation::operator !=(const cwStation &station) const
{
    return ! operator ==(station);
}

inline QDebug operator<<(QDebug debug, const cwStation &station)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << station.name();
    return debug;
}



#endif // cwStation_H
