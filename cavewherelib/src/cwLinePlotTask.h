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
#include "cwLinePlotGeometry.h"
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
#include <QRegularExpression>
#include <QUuid>

//Std includes
#include <optional>
#include <utility>

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
    struct Input {
        cwCavingRegionData regionData;

        // Absolute on-disk attachment directories for caves and trips whose
        // externalCenterline is set. Keyed by cwCave::id() / cwTrip::id() so
        // the exporter can join them with the project-relative entryFile
        // value to write absolute *include paths. Maps are empty when no
        // attachments exist; entries are required when the matching
        // entity's externalCenterline is set or the export step fails fast
        // (see cwSurvexExporterCaveTask::writeExternalInclude).
        QHash<QUuid, QString> caveAttachmentDirs;
        QHash<QUuid, QString> tripAttachmentDirs;

        // CaveWhere-resolved declination (degrees, east-positive) per
        // external trip whose file carries no declination of its own.
        // Resolved on the main thread by buildInput from the live
        // cwTripCalibration (IGRF auto or manual value) — the worker
        // snapshot only carries the raw manual/auto fields. Absent key =
        // file-owned or unknown → the driver emits nothing for that trip.
        QHash<QUuid, double> tripInjectedDeclinations;
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
     * Identifies the caves, trips, and scraps whose station positions changed
     * by their stable cwCave/cwTrip/cwScrap::id() UUID rather than a raw
     * pointer. Only value-type identity crosses the worker boundary; the
     * manager resolves each UUID back to the live object on the main thread.
     *
     * Also holds the new station lookup that the cave should be update with.  We don't update
     * the original cave's data directly because of thread safety.
     */
    class LinePlotResultData {
    public:
        LinePlotResultData() { }

        void clear();

        void setPositions(QVector<QVector3D> positions);
        void setTripVertexRanges(QVector<cwLinePlotGeometry::VertexRange> tripVertexRanges);
        void setTripUuids(QVector<QUuid> tripUuids);

        QVector<QVector3D> stationPositions() const;

        // Per-trip vertex span in stationPositions and the running-id -> stable
        // cwTrip::id mapping (both running-id indexed). Only value-type data
        // crosses the worker boundary; the manager resolves each UUID to a live
        // cwTrip* and binds its visibility proxy to the matching vertex range.
        // See cwLinePlotGeometry for how the ranges are assigned.
        QVector<cwLinePlotGeometry::VertexRange> tripVertexRanges() const;
        QVector<QUuid> tripUuids() const;

    public:
        void setRegionNetwork(cwSurveyNetwork network);
        cwSurveyNetwork regionNetwork() const;
        bool hasRegionNetworkChanged() const;

        QHash<QUuid, LinePlotCaveData> Caves;
        QSet<QUuid> Trips;
        QSet<QUuid> Scraps;
        QVector<QVector3D> StationPositions;
        QVector<cwLinePlotGeometry::VertexRange> TripVertexRanges;
        QVector<QUuid> TripUuids;
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

        // The generated driver .svx text fed to cavern — captured right
        // after export, so it is present even when cavern itself fails.
        // CavernOutputPage's driver-source disclosure binds to the
        // cwLinePlotManager property this populates.
        QString DriverSource;

        // Wall-clock seconds for the whole pipeline (export → cavern →
        // parse → geometry); negative when no solve ran (the default
        // result published on setRegion / no-shots paths). The UI shows
        // the stats suffix only for non-negative values.
        double SolveDurationSeconds = -1.0;

        // Cavern's warning tally for this run (0 when cavern failed
        // before reporting or never ran).
        int CavernWarningCount = 0;

        friend class cwLinePlotTask;
        friend struct LinePlotWorker;
    };

    static Input buildInput(const cwCavingRegion* region);

    // The external-centerline subsystem's contribution to a solve, taken
    // as one value snapshot (cwExternalCenterlineManager::solveInputs())
    // so a solve can never mix dirs and flags from different scans.
    struct ExternalCenterlineInputs {
        QHash<QUuid, QString> caveAttachmentDirs;
        QHash<QUuid, QString> tripAttachmentDirs;
        // Per-owner scan flag; trips flagged false get their resolved
        // declination baked into tripInjectedDeclinations. Owners
        // missing from the map are treated as file-owned.
        QHash<QUuid, bool> fileOwnsDeclination;
    };

    static Input buildInput(const cwCavingRegion* region,
                            const ExternalCenterlineInputs& external);
    static QFuture<LinePlotResultData> run(Input input);

    /**
     * Cave-name encoding contract used by the line-plot driver.
     *
     * cavernCaveNameFor(id) produces the synthetic *begin name written to
     * the worker's intermediate .svx ("cave_<32 hex of QUuid::Id128>"). The
     * matching regex (cavernStationRegex) parses cavern's emitted lines of
     * the form "cave_<32 hex>.<station>" back into a (QUuid, station) pair.
     *
     * Exposed as public statics so the [LinePlot][UuidPrefix] tests can pin
     * the contract: cwSurvexExporterRegion stamps this name into the .svx,
     * cavern echoes it back as a station prefix, and splitLookupByCave
     * recovers the cwCave::id() it came from. The names are never visible to
     * the user - the user-facing exporter (cwSurveyExportManager) operates
     * on the original snapshot before the rewrite and keeps friendly cave
     * names intact.
     *
     * The regex deliberately rejects the previous "<integer>.<station>"
     * form: the integer was a position-in-list with no meaning outside the
     * driver's snapshot, so if cavern ever surfaces such a line in a UUID
     * build it indicates the encoding bypass we want surfaced as a parse
     * miss, not silently folded into cave 0.
     */
    static QString cavernCaveNameFor(const QUuid& caveId);
    static QRegularExpression cavernStationRegex();



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

        // Trip UUIDs, and (tripId, scrapId) pairs, of every trip/scrap that
        // touches stationName. Keyed by stable id() rather than list index so
        // the result never carries a raw pointer across the worker boundary.
        QList<QUuid> trips(QString stationName) const;
        QList<std::pair<QUuid, QUuid>> scraps(QString stationName) const;

    private:
        QMultiHash<QString, QUuid> MapStationToTrip;
        QMultiHash<QString, std::pair<QUuid, QUuid>> MapStationToScrap;
    };

};

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
 * @brief cwLinePlotTask::LinePlotResultData::setPositions
 * @param positions
 */
inline void cwLinePlotTask::LinePlotResultData::setPositions(QVector<QVector3D> positions) {
    StationPositions = std::move(positions);
}

inline void cwLinePlotTask::LinePlotResultData::setTripVertexRanges(
    QVector<cwLinePlotGeometry::VertexRange> tripVertexRanges) {
    TripVertexRanges = std::move(tripVertexRanges);
}

inline void cwLinePlotTask::LinePlotResultData::setTripUuids(QVector<QUuid> tripUuids) {
    TripUuids = std::move(tripUuids);
}

inline QVector<cwLinePlotGeometry::VertexRange>
cwLinePlotTask::LinePlotResultData::tripVertexRanges() const {
    return TripVertexRanges;
}

inline QVector<QUuid> cwLinePlotTask::LinePlotResultData::tripUuids() const {
    return TripUuids;
}

inline QList<QUuid> cwLinePlotTask::StationTripScrapLookup::trips(QString stationName) const
{
    return MapStationToTrip.values(stationName);
}

inline QList<std::pair<QUuid, QUuid>> cwLinePlotTask::StationTripScrapLookup::scraps(QString stationName) const
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
