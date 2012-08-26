#ifndef CWLINEPLOTTASK_H
#define CWLINEPLOTTASK_H

//Our includes
#include "cwTask.h"
class cwCavingRegion;
class cwLinePlotGeometryTask;
#include "cwStationPositionLookup.h"
class cwSurvexExporterRegionTask;
class cwCavernTask;
class cwPlotSauceTask;
class cwPlotSauceXMLTask;
class cwScrap;
class cwTrip;
class cwCave;

//Qt includes
#include <QTemporaryFile>
#include <QVector3D>
#include <QTime>
#include <QVector>
#include <QSet>

class cwLinePlotTask : public cwTask
{
    Q_OBJECT
public:

    /**
     * @brief The CaveStationData class
     *
     * Holds a pointer to the cave's data that has been changed. This is the pointer to the
     * orginial cave, trips, and scraps. in the main thread
     *
     * Also holds the new station lookup that the cave should be update with.  We don't update
     * the original cave's data directly because of thread safety.
     */
    class LinePlotResultData {
    public:
        LinePlotResultData() { }

        void setCaveData(QMap<cwCave*, cwStationPositionLookup> caveData);
        void setTrip(QSet<cwTrip*> trips);
        void setScraps(QSet<cwScrap*> scraps);
        void setPositions(QVector<QVector3D> positions);
        void setPlotIndexData(QVector<unsigned int> indexData);

        QMap<cwCave*, cwStationPositionLookup> caveData() const;
        QSet<cwTrip*> trips() const;
        QSet<cwScrap*> scraps() const;
        QVector<QVector3D> stationPositions() const;
        QVector<unsigned int> linePlotIndexData() const;

    private:
        QMap<cwCave*, cwStationPositionLookup> Caves;
        QSet<cwTrip*> Trips;
        QSet<cwScrap*> Scraps;
        QVector<QVector3D> StationPositions;
        QVector<unsigned int> LinePlotIndexData;

        friend class cwLinePlotTask;
    };

    explicit cwLinePlotTask(QObject *parent = 0);

    LinePlotResultData linePlotData() const;

signals:

protected:
    virtual void runTask();

public slots:
    void setData(const cwCavingRegion &region);

private slots:
    void exportData();
    void runCavern();
    void convertToXML();
    void readXML();
    void generateCenterlineGeometry();
    void linePlotTaskComplete();

    //For setting up all the station positions
    void updateStationPositionForCaves(const cwStationPositionLookup& stationPostions);

private:
    /**
     * TripDataPtrs, CaveDataPtrs, and RegionDataPtrs, store pointers to original
     * data in this task.  This will allow the task to return the pointer of each object
     * that the station's position has changed.  This is extremely useful for alerting
     * when a specific scrap needs to be updated.
     *
     * It is unsafe to use the points in the this data structure.  But are used for
     * book keeping only.
     *
     * This could possibly be replaced by id's in the future
     */
    class TripDataPtrs {
    public:
        TripDataPtrs() {}
        TripDataPtrs(cwTrip* trip);

        cwTrip* Trip;
        QList<cwScrap*> Scraps;
    };

    class CaveDataPtrs {
    public:
        CaveDataPtrs() {}
        CaveDataPtrs(cwCave* cave);

        cwCave* Cave;
        QList<TripDataPtrs> Trips;
    };

    class RegionDataPtrs {
    public:
        RegionDataPtrs() {}
        RegionDataPtrs(const cwCavingRegion& region);

        QList<CaveDataPtrs> Caves;
    };

    //The region data
    cwCavingRegion* Region; //Local copy of the region, we can modify this
    RegionDataPtrs RegionOriginalPointers; //Allows use to notify the which of the original data has changed
    QVector<cwStationPositionLookup> CaveStationLookups; //Copies of all the cave station lookups that are going to be modified

    //The temparary survex file
    QTemporaryFile* SurvexFile;
    cwSurvexExporterRegionTask* SurvexExporter;

    //Sub tasks
    cwCavernTask* CavernTask;
    cwPlotSauceTask* PlotSauceTask;
    cwPlotSauceXMLTask* PlotSauceParseTask;
    cwLinePlotGeometryTask* CenterlineGeometryTask;

    //What's returned
    LinePlotResultData Result;

    //For performance testing
    QTime Time;

    void encodeCaveNames();
    void initializeCaveStationLookups();
    void setStationAsChanged(int caveIndex, QString stationName);
};

/**
 * @brief cwLinePlotTask::LinePlotResultData::caveData
 * @return The external cave pointer that's should be updated with cwStationPositionLookup
 *
 * This functions aren't thread safe!! You should only call these if the task isn't running
 */
inline QMap<cwCave *, cwStationPositionLookup> cwLinePlotTask::LinePlotResultData::caveData() const
{
    return Caves;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::trips
 * @return A list of all external trips that there station positions have changed
 *
 * This functions aren't thread safe!! You should only call these if the task isn't running
 */
inline QSet<cwTrip *> cwLinePlotTask::LinePlotResultData::trips() const
{
    return Trips;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::scraps
 * @return A list of all the scraps that the position has changed
 *
 * This functions aren't thread safe!! You should only call these if the task isn't running
 */
inline QSet<cwScrap *> cwLinePlotTask::LinePlotResultData::scraps() const
{
    return Scraps;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::stationPositions
 *
 * This returns all the positions of the points.  This is used strictly for rendering.
 * This functions aren't thread safe!! You should only call these if the task isn't running
 */
inline QVector<QVector3D> cwLinePlotTask::LinePlotResultData::stationPositions() const
{
    return StationPositions;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::linePlotIndexData
 * @return Returns all the plot line data indexes.  This is to construct a line array
 *
 *  This functions aren't thread safe!! You should only call these if the task isn't running
 */
inline QVector<unsigned int> cwLinePlotTask::LinePlotResultData::linePlotIndexData() const
{
    return LinePlotIndexData;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::setCaveData
 * @param caveData
 */
inline void cwLinePlotTask::LinePlotResultData::setCaveData(QMap<cwCave*, cwStationPositionLookup> caveData) {
    Caves = caveData;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::setTrip
 * @param trips
 */
inline void cwLinePlotTask::LinePlotResultData::setTrip(QSet<cwTrip*> trips) {
    Trips = trips;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::setScraps
 * @param scraps
 */
inline void cwLinePlotTask::LinePlotResultData::setScraps(QSet<cwScrap*> scraps) {
    Scraps = scraps;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::setPositions
 * @param positions
 */
inline void cwLinePlotTask::LinePlotResultData::setPositions(QVector<QVector3D> positions) {
    StationPositions = positions;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::setPlotIndexData
 * @param indexData
 */
inline void cwLinePlotTask::LinePlotResultData::setPlotIndexData(QVector<unsigned int> indexData) {
    LinePlotIndexData = indexData;
}

/**
 * @brief cwLinePlotTask::linePlotData
 * @return The resulting line plot data from the task.
 *
 * This will include, the 3D geometry of the lineplot and the objects that need to be updated
 * based on the station data that has changed
 */
inline cwLinePlotTask::LinePlotResultData cwLinePlotTask::linePlotData() const
{
    return Result;
}

#endif // CWLINEPLOTTASK_H
