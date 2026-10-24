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
#include <QUuid>
#include <QSet>

//Our includes
#include "cwGlobals.h"
#include "cwDistanceReading.h"
#include "cwShotMeasurement.h"
enum class cwErrorTypeId : int;

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

    QUuid id() const { return m_data->m_id; }
    void setId(const QUuid id) { m_data->m_id = id; }

    QString name() const;
    bool setName(QString Name);

    cwDistanceReading left() const;
    void setLeft(const cwDistanceReading& left) { m_data->m_left = left; }

    cwDistanceReading right() const;
    void setRight(const cwDistanceReading& right) { m_data->m_right = right; }

    cwDistanceReading up() const;
    void setUp(const cwDistanceReading& up) { m_data->m_up = up; }

    cwDistanceReading down() const;
    void setDown(const cwDistanceReading& down) { m_data->m_down = down; }

    // Splays are wall shots taken from this station with no named destination.
    // Front sights only — a splay is never backsighted.
    QList<cwShotMeasurement> splays() const;
    void setSplays(const QList<cwShotMeasurement>& splays) { m_data->m_splays = splays; }
    void addSplay(const cwShotMeasurement& splay) { m_data->m_splays.append(splay); }
    const cwShotMeasurement& splayAt(int index) const { return m_data->m_splays.at(index); }
    int splayCount() const { return m_data->m_splays.size(); }

    // cwErrorTypeId values the user ignored on this station's cells (#174)
    QSet<int> ignoredWarnings() const;
    void setIgnoredWarnings(const QSet<int>& ids);
    bool isWarningIgnored(cwErrorTypeId id) const;

    void setData(QVariant data, DataRoles role);
    QVariant data(DataRoles role) const;

    bool isValid() const { return !m_data->m_name.isEmpty(); }

    bool operator ==(const cwStation& station) const;
    bool operator !=(const cwStation& station) const;

    static bool nameIsValid(QString stationName);

    // Canonical form for station-name equality and hash keys. Case-insensitive
    // everywhere the pipeline compares stations (cavern, cwSurveyNetwork,
    // cwStationPositionLookup). Use this instead of ad-hoc toLower/toUpper.
    static QString canonicalKey(const QString &stationName) { return stationName.toLower(); }

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData();
        PrivateData(QString name);

        QUuid m_id;

        QString m_name;

        cwDistanceReading m_left;
        cwDistanceReading m_right;
        cwDistanceReading m_up;
        cwDistanceReading m_down;

        QList<cwShotMeasurement> m_splays;

        QSet<int> m_ignoredWarnings;
    };

    QSharedDataPointer<PrivateData> m_data;
};

inline size_t qHash(const cwStation& station) {
    return qHash(cwStation::canonicalKey(station.name()));
}

inline QString cwStation::name() const { return m_data->m_name; }
inline cwDistanceReading cwStation::left() const { return m_data->m_left; }
inline cwDistanceReading cwStation::right() const { return m_data->m_right; }
inline cwDistanceReading cwStation::up() const { return m_data->m_up; }
inline cwDistanceReading cwStation::down() const { return m_data->m_down; }
inline QList<cwShotMeasurement> cwStation::splays() const { return m_data->m_splays; }

inline bool cwStation::operator ==(const cwStation &station) const
{
    //Matches qHash(), which hashes the canonical key, so QSet/QHash stay consistent
    return canonicalKey(m_data->m_name) == canonicalKey(station.name());
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
