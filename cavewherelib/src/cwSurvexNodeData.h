#ifndef CWSURVEXNODEDATA_H
#define CWSURVEXNODEDATA_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QSet>
#include "cwSurvexLRUDChunk.h"
#include "cwStation.h"

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

    void addSplay(const QString& stationName, const cwShotMeasurement& splay);

private:
    QList<cwSurvexLRUDChunk> LRUDChunks;

    //A station's splays can appear before the leg that introduces the station,
    //so they're buffered here while the block parses and attached once its
    //chunks are complete. Keyed by cwStation::canonicalKey so "A4" and "a4"
    //collect on the same station, which keeps its as-written name for warnings.
    QMap<QString, cwStation> Splays;

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
    return QStringList(ExportStations.begin(), ExportStations.end());
}

/**
 * @brief cwSurvexBlockData::addToEquated
 * @param adds a list of stationNames that are equal to each other.
 */
inline void cwSurvexNodeData::addToEquated(QStringList stationNames) {
    EqualStations.append(stationNames);
}



#endif // CWSURVEXNODEDATA_H
