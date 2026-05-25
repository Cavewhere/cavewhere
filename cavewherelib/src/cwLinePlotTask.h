/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTTASK_H
#define CWLINEPLOTTASK_H

//Our includes
class cwCavingRegion;
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCavingRegionData.h"
class cwScrap;
class cwTrip;
class cwCave;


//Qt includes
#include <QVector3D>
#include <QElapsedTimer>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QFuture>
#include <QMap>
#include <QList>
#include <QPair>
#include <QMultiHash>

//Std includes
#include <optional>

class cwLinePlotTask
{
public:
    struct LinePlotWorker;

    /**
     * \brief Region-level error from the line-plot pipeline.
     *
     * Populated on the worker side when a step fails; consumed on the
     * UI side by cwLinePlotManager which surfaces it as Q_PROPERTYs the
     * CavernOutputPage binds to.
     */
    struct SolveError {
        // Steps mirror the worker's pipeline phases that can actually fail.
        // (Geometry has no failure path today — cwLinePlotGeometry::generate
        // currently never returns an error — so it is intentionally omitted.)
        enum class Step { Export, Cavern, Parse, Validation };
        Step step = Step::Cavern;
        int exitCode = 0;          // populated for Step::Cavern
        QString message;           // human-readable summary
    };
    /**
     * TripDataPtrs, CaveDataPtrs, and RegionDataPtrs, store pointers to original
     * data in this task.  This will allow the task to return the pointer of each object
     * that the station's position has changed.  This is extremely useful for alerting
     * when a specific scrap needs to be updated.
     *
     * It is unsafe to use the pointers in the this data structure.  But are used for
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
        RegionDataPtrs(const cwCavingRegion* region);

        QList<CaveDataPtrs> Caves;
    };

    struct Input {
        cwCavingRegionData regionData;
        class RegionDataPtrs regionPointers;
    };

    class LinePlotCaveData {
    public:
        LinePlotCaveData();

        void setDepth(double depth);
        void setLength(double length);
        void setStationPositions(cwStationPositionLookup positionLookup);
        void setUnconnectedChunkError(QList<cwFindUnconnectedSurveyChunks::Result> results);
        void setNetwork(cwSurveyNetwork network);

        double depth() const;
        double length() const;
        cwStationPositionLookup stationPositions() const;
        QList<cwFindUnconnectedSurveyChunks::Result> unconnectedChunkError() const;
        cwSurveyNetwork network() const;

        bool hasDepthLengthChanged() const;
        bool hasStationPositionsChanged() const;
        bool hasNetworkChanged() const;

    private:
        bool DepthLengthChanged;
        double Depth;
        double Length;
        QList<cwFindUnconnectedSurveyChunks::Result> UnconnectedChunksErrors;

        bool StationPostionsChanged;
        bool NetworkChanged;
        cwStationPositionLookup Lookup;
        cwSurveyNetwork Network;
    };

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

        void clear();

        void setCaveData(QMap<cwCave*, LinePlotCaveData> caveData);
        void setTrip(QSet<cwTrip*> trips);
        void setScraps(QSet<cwScrap*> scraps);
        void setPositions(QVector<QVector3D> positions);
        void setPlotIndexData(QVector<unsigned int> indexData);      

        QMap<cwCave*, LinePlotCaveData> caveData() const;
        QSet<cwTrip*> trips() const;
        QSet<cwScrap*> scraps() const;
        QVector<QVector3D> stationPositions() const;
        QVector<unsigned int> linePlotIndexData() const;

    public:
        void setRegionNetwork(cwSurveyNetwork network);
        cwSurveyNetwork regionNetwork() const;
        bool hasRegionNetworkChanged() const;

        QMap<cwCave*, LinePlotCaveData> Caves;
        QSet<cwTrip*> Trips;
        QSet<cwScrap*> Scraps;
        QVector<QVector3D> StationPositions;
        QVector<unsigned int> LinePlotIndexData;
        cwSurveyNetwork RegionNetwork;
        bool RegionNetworkChanged = false;

        void setSolveError(SolveError error) { LastSolveError = std::move(error); }
        bool hasSolveError() const { return LastSolveError.has_value(); }
        const SolveError& solveError() const { return *LastSolveError; }

        std::optional<SolveError> LastSolveError;

        // Cavern's .log (info + error messages) and netskel's .err
        // (loop-closure stats) — populated whenever cavern ran, regardless of
        // success or failure. CavernOutputPage surfaces them as Q_PROPERTYs on
        // cwLinePlotManager so the user can see implicit-fix notices and loop
        // error stats even on a clean solve.
        QString CavernLog;
        QString LoopClosureStats;

        friend class cwLinePlotTask;
        friend struct LinePlotWorker;
    };

    static Input buildInput(const cwCavingRegion* region);
    static QFuture<LinePlotResultData> run(Input input);



    /**
     * @brief The StationCaveLookup class
     *
     * Stores a lookup for all the stations and scraps in a cave.  This will a station to multiple
     * trips / scraps
     */
    class StationTripScrapLookup {
    public:
        StationTripScrapLookup(cwCave* cave);
        StationTripScrapLookup() { }

        QList<int> trips(QString stationName) const;
        QList<QPair<int, int> > scraps(QString stationName) const;

    private:
        QMultiHash<QString, int> MapStationToTrip; //Multi map of a station to multiple trips indexes
        QMultiHash<QString, QPair<int, int> > MapStationToScrap; //Multi map of a station to multiple scraps indexes (first index is cave, then scrap index)
    };

};

/**
 * @brief cwLinePlotTask::LinePlotResultData::caveData
 * @return The external cave pointer that's should be updated with cwStationPositionLookup
 *
 * This functions aren't thread safe!! You should only call these if the task isn't running
 */
inline QMap<cwCave *, cwLinePlotTask::LinePlotCaveData> cwLinePlotTask::LinePlotResultData::caveData() const
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

inline void cwLinePlotTask::LinePlotResultData::setRegionNetwork(cwSurveyNetwork network)
{
    RegionNetwork = std::move(network);
    RegionNetworkChanged = true;
}

inline cwSurveyNetwork cwLinePlotTask::LinePlotResultData::regionNetwork() const
{
    return RegionNetwork;
}

inline bool cwLinePlotTask::LinePlotResultData::hasRegionNetworkChanged() const
{
    return RegionNetworkChanged;
}

/**
 * @brief cwLinePlotTask::LinePlotResultData::setCaveData
 * @param caveData
 */
inline void cwLinePlotTask::LinePlotResultData::setCaveData(QMap<cwCave *, LinePlotCaveData> caveData) {
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

inline QList<int> cwLinePlotTask::StationTripScrapLookup::trips(QString stationName) const
{
    return MapStationToTrip.values(stationName);
}

inline QList<QPair<int, int> > cwLinePlotTask::StationTripScrapLookup::scraps(QString stationName) const
{
    return MapStationToScrap.values(stationName);
}

inline void cwLinePlotTask::LinePlotCaveData::setDepth(double depth)
{
    Depth = depth;
    DepthLengthChanged = true;
}

inline void cwLinePlotTask::LinePlotCaveData::setLength(double length)
{
    Length = length;
    DepthLengthChanged = true;
}

inline void cwLinePlotTask::LinePlotCaveData::setStationPositions(cwStationPositionLookup positionLookup)
{
    Lookup = positionLookup;
    StationPostionsChanged = true;
}

inline void cwLinePlotTask::LinePlotCaveData::setUnconnectedChunkError(QList<cwFindUnconnectedSurveyChunks::Result> results)
{
    UnconnectedChunksErrors = results;
}

inline void cwLinePlotTask::LinePlotCaveData::setNetwork(cwSurveyNetwork network)
{
    Network = network;
    NetworkChanged = true;
}

inline double cwLinePlotTask::LinePlotCaveData::depth() const
{
    return Depth;
}

inline double cwLinePlotTask::LinePlotCaveData::length() const
{
    return Length;
}

inline cwStationPositionLookup cwLinePlotTask::LinePlotCaveData::stationPositions() const
{
    return Lookup;
}

inline QList<cwFindUnconnectedSurveyChunks::Result> cwLinePlotTask::LinePlotCaveData::unconnectedChunkError() const
{
    return UnconnectedChunksErrors;
}

inline cwSurveyNetwork cwLinePlotTask::LinePlotCaveData::network() const
{
    return Network;
}

inline bool cwLinePlotTask::LinePlotCaveData::hasDepthLengthChanged() const
{
    return DepthLengthChanged;
}

inline bool cwLinePlotTask::LinePlotCaveData::hasStationPositionsChanged() const
{
    return StationPostionsChanged;
}

inline bool cwLinePlotTask::LinePlotCaveData::hasNetworkChanged() const
{
    return NetworkChanged;
}

#endif // CWLINEPLOTTASK_H
