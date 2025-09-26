#ifndef CWNOTELIDAR_H
#define CWNOTELIDAR_H

// Qt includes
#include <QObject>
#include <QList>
#include <QUrl>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Our includes
#include "cwGlobals.h"
#include "cwNoteLiDARStation.h"
class cwTrip;
class cwCave;

class CAVEWHERE_LIB_EXPORT cwNoteLiDAR : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NoteLiDAR)

    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
    // Q_PROPERTY(QList<cwNoteLiDARStation> stations READ stations WRITE setStations NOTIFY stationsChanged)

public:
    explicit cwNoteLiDAR(QObject* parent = nullptr);

    // Property accessors
    QString filename() const;
    void setFilename(const QString& path);

    void setParentTrip(cwTrip* trip);
    Q_INVOKABLE cwTrip* parentTrip() const { return m_parentTrip; }

    void setParentCave(cwCave* cave);
    cwCave* parentCave() const { return m_parentCave; }


    // Stations API
    void addStation(const cwNoteLiDARStation& station);
    Q_INVOKABLE void removeStation(int stationId);         // removes by index
    const QList<cwNoteLiDARStation>& stations() const;
    void setStations(const QList<cwNoteLiDARStation>& stations);
    Q_INVOKABLE cwNoteLiDARStation station(int stationId) const;

signals:
    void filenameChanged();
    // void stationsChanged();

private:
    QString m_filename;                         // glTF binary file path (.glb)
    QList<cwNoteLiDARStation> m_stations;       // stations associated with this LiDAR note

    cwTrip* m_parentTrip = nullptr;
    cwCave* m_parentCave = nullptr;
};

// Q_DECLARE_METATYPE(cwNoteLiDARStation)

#endif // CWNOTELIDAR_H
