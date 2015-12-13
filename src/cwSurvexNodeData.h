#ifndef CWSURVEXNODEDATA_H
#define CWSURVEXNODEDATA_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QSet>
#include "cwSurvexLRUDChunk.h"

class cwSurvexNodeData : public QObject
{
    Q_OBJECT
public:
    friend class cwSurvexImporter;
    friend class cwSurvexGlobalData;

    explicit cwSurvexNodeData(QObject *parent = 0);

    void addToEquated(QStringList stationNames);
    QStringList equatedStations(QString fullStationName) const;

    void addExportStations(QStringList exportStations);
    QStringList exportStations() const;

    void addLRUDChunk();

private:
    QList<cwSurvexLRUDChunk> LRUDChunks;
    QList<QStringList> EqualStations;  //Each entry hold a list of station names's that are the same.
    QSet<QString> ExportStations; //Holds a station name that is exported for equates

    //For caves, used station names, and equating stations
    QMap<QString, QString> EquateMap;  //All stations get added to the map
};

/**
 * @brief cwSurvexBlockData::exportStations
 * @return All the export stations in this block
 */
inline QStringList cwSurvexNodeData::exportStations() const
{
    return QStringList(ExportStations.toList());
}

/**
 * @brief cwSurvexBlockData::addToEquated
 * @param adds a list of stationNames that are equal to each other.
 */
inline void cwSurvexNodeData::addToEquated(QStringList stationNames) {
    EqualStations.append(stationNames);
}



#endif // CWSURVEXNODEDATA_H
