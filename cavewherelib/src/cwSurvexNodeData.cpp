#include "cwSurvexNodeData.h"
#include "cwStation.h"

cwSurvexNodeData::cwSurvexNodeData(QObject *parent) : QObject(parent)
{

}

/**
 * @brief cwSurvexBlockData::addLRUDChunk
 * This creates a new LRUD chunk
 */
void cwSurvexNodeData::addLRUDChunk()
{
    LRUDChunks.append(cwSurvexLRUDChunk());
}

/**
 * @brief cwSurvexNodeData::addSplay
 * @param stationName - The named station the splay was shot from
 * @param splay - The wall shot
 *
 * Buffers the splay until the block's chunks are complete
 */
void cwSurvexNodeData::addSplay(const QString &stationName, const cwShotMeasurement &splay)
{
    cwStation& station = Splays[cwStation::canonicalKey(stationName)];
    if(!station.isValid()) {
        station.setName(stationName);
    }
    station.addSplay(splay);
}

/**
* @brief cwSurvexBlockData::equatedStations
 * @return
 */
QStringList cwSurvexNodeData::equatedStations(QString stationName) const
{
    QStringList equatedStations;
    foreach(QStringList stations, EqualStations) {
        if(stations.contains(stationName)) {
            equatedStations.append(stations);
        }
    }

    return equatedStations;
}

/**
 * @brief cwSurvexBlockData::addExportStations
 * @param exportStations
 *
 * Appends the export stations to the export station list
 */
void cwSurvexNodeData::addExportStations(QStringList exportStations)
{
    foreach(QString station, exportStations) {
        ExportStations.insert(station);
    }
}
