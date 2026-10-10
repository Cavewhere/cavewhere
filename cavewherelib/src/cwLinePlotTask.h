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
#include "cwFindFloatingSurveys.h"
#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCavingRegionData.h"
#include "cwLinePlotGeometry.h"
#include "cwExternalCenterlineScanner.h"
class cwScrap;
class cwTrip;
class cwSurveyNode;
class cwScopeLabels;


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
        // externalCenterline is set. Keyed by cwSurveyNode::id() / cwTrip::id() so
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

        // Caves and trips whose externalCenterline is set but unreadable
        // by the solve: dependencies reaching outside the project's data
        // root, or an in-project copy that is gone from disk. The driver
        // skips their *include entirely rather than failing the export, so
        // one broken attachment costs its own survey and leaves the
        // rest of the region solvable.
        QSet<QUuid> excludedExternalOwners;

        // Per cave or trip owner, the stations its file fixes itself, in the
        // file's namespace. The driver drops a node fix on one of them, which
        // cavern would otherwise reject with error 46.
        QHash<QUuid, QStringList> externalFixedStations;

        // Per cave or trip owner whose file fixes stations with no input *cs
        // and names its own *cs out, that output system as the file writes
        // it (see cwSurvexExporterRegion::Options::externalOutputSystems).
        QHash<QUuid, QString> externalOutputSystems;

        // The previous solve's region lookup and network
        // (cwCavingRegion::stationPositions / network). The worker diffs its
        // fresh solve against these to decide which stations actually moved.
        // Empty means "assume everything changed", which is both the correct
        // answer for a first solve and a safe default for synthetic callers
        // that build an Input by hand (cwTripLinePlotTask).
        cwStationPositionLookup previousStationPositions;
        cwSurveyNetwork previousNetwork;
    };

    class LinePlotCaveData {
    public:
        LinePlotCaveData();

        void setDepth(double depth);
        void setLength(double length);
        void setSplayTips(cwSplayTipsByStation splayTips);
        void setUnconnectedChunkError(QList<cwFindUnconnectedSurveyChunks::Result> results);

        double depth() const;
        double length() const;
        cwSplayTipsByStation splayTips() const;
        QList<cwFindUnconnectedSurveyChunks::Result> unconnectedChunkError() const;

        bool hasDepthLengthChanged() const;

    private:
        bool DepthLengthChanged;
        double Depth;
        double Length;
        QList<cwFindUnconnectedSurveyChunks::Result> UnconnectedChunksErrors;
        cwSplayTipsByStation SplayTips;
    };

    /**
     * @brief One solve's answer.
     *
     * Identifies the nodes, trips, and scraps whose station positions changed
     * by their stable cwSurveyNode/cwTrip/cwScrap::id() UUID rather than a raw
     * pointer. Only value-type identity crosses the worker boundary; the
     * manager resolves each UUID back to the live object on the main thread.
     *
     * Also holds the region's new station lookup and network, which the
     * manager writes to the live cwCavingRegion on the main thread.
     */
    class LinePlotResultData {
    public:
        LinePlotResultData() { }

        void setPositions(QVector<QVector3D> positions);
        void setTripVertexRanges(QVector<cwLinePlotGeometry::VertexRange> tripVertexRanges);
        void setTripSplayVertexRanges(QVector<cwLinePlotGeometry::VertexRange> ranges);
        void setTripUuids(QVector<QUuid> tripUuids);

        QVector<QVector3D> stationPositions() const;

        // Per-trip vertex span in stationPositions (with the splay sub-span at
        // its tail) and the running-id -> stable cwTrip::id mapping (all
        // running-id indexed). Only value-type data crosses the worker
        // boundary; the manager resolves each UUID to a live cwTrip* and binds
        // its visibility proxy to the matching vertex range. See
        // cwLinePlotGeometry for how the ranges are assigned.
        QVector<cwLinePlotGeometry::VertexRange> tripVertexRanges() const;
        QVector<cwLinePlotGeometry::VertexRange> tripSplayVertexRanges() const;
        QVector<QUuid> tripUuids() const;

    public:
        void setRegionNetwork(cwSurveyNetwork network);
        cwSurveyNetwork regionNetwork() const;
        bool hasRegionNetworkChanged() const;

        //! Every solved station keyed by its full cavern name, set only when
        //! some station moved, appeared, or vanished since the previous solve.
        void setRegionStationPositions(cwStationPositionLookup positions);
        cwStationPositionLookup regionStationPositions() const;
        bool hasRegionStationPositionsChanged() const;

        // Keyed by cwSurveyNode::id(), one entry per node at any depth.
        QHash<QUuid, LinePlotCaveData> Caves;
        QSet<QUuid> Trips;
        QSet<QUuid> Scraps;
        QVector<QVector3D> StationPositions;
        QVector<cwLinePlotGeometry::VertexRange> TripVertexRanges;
        QVector<cwLinePlotGeometry::VertexRange> TripSplayVertexRanges;
        QVector<QUuid> TripUuids;
        cwSurveyNetwork RegionNetwork;
        bool RegionNetworkChanged = false;
        cwStationPositionLookup RegionStationPositions;
        bool RegionStationPositionsChanged = false;

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

        // Every survey this run found floating, from both of the passes that
        // can find one: the pre-solve native check that also populates
        // LinePlotCaveData::unconnectedChunkError, and the post-solve topology
        // check the native pass is blind to. Region-wide rather than per-cave
        // because each record names its own cave, and because a solve that
        // never ran still has an answer for the caves it validated.
        QList<cwFindFloatingSurveys::Result> FloatingSurveys;

        // Whether this run reached the post-solve half of that answer. Every
        // failure path returns before it while the pre-solve half always runs,
        // so a caller can tell "no external scope is floating" from "nobody
        // looked". False by default, like every other flag on this struct: a
        // result no run filled in asserts nothing.
        bool ExternalScopesChecked = false;

        //! The stations of one trip or attached file that cavern could not tie
        //! to its cave. \a ownerId is the trip's id, or the node's for a
        //! file attached to the node itself; \a stations are owner-local and
        //! may repeat.
        struct HangingStations {
            QUuid ownerId;
            QStringList stations;

            bool operator==(const HangingStations& other) const = default;
        };

        //! Every owner holding stations cavern dropped because nothing ties
        //! them to a fixed point, plus the fixed points of attached files that
        //! no survey uses. Filled by the post-solve pass, so it is meaningful
        //! only when ExternalScopesChecked is true.
        QList<HangingStations> Hanging;

        //! A result asserting "there is nothing, and I looked" — what the
        //! manager publishes to wipe state on setRegion, and what a region with
        //! nothing solvable in it produces. Distinct from a default-constructed
        //! result, which asserts nothing and so leaves the last answer standing.
        static LinePlotResultData cleared();

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
        // Owners whose attachment reaches outside the project's data root,
        // or whose in-project copy is gone from disk. The driver writes no
        // *include for them (see Input).
        QSet<QUuid> excludedExternalOwners;
        // Per owner (cave or trip) whose file fixes stations with no input
        // coordinate system of their own, those stations as the file writes
        // them. Read on the main thread when the solve starts, to warn about
        // them in a georeferenced project.
        QHash<QUuid, QStringList> bareFixedStations;
        // Owners whose file fixes stations itself, and those stations (see
        // Input::externalFixedStations).
        QHash<QUuid, QStringList> externalFixedStations;
        // Owners whose file fixes stations bare and names its own *cs out,
        // and that system (see Input::externalOutputSystems).
        QHash<QUuid, QString> externalOutputSystems;
        // Owners whose Compass .mak names a zone and no datum, and those
        // files. Read on the main thread when the solve starts, to say so.
        QHash<QUuid, QList<cwExternalCenterlineScanner::DefaultedDatum>> defaultedDatums;
    };

    static Input buildInput(const cwCavingRegion* region,
                            const ExternalCenterlineInputs& external);
    static QFuture<LinePlotResultData> run(Input input);

    /**
     * @brief The StationCaveLookup class
     *
     * Stores a lookup for all the stations and scraps in one node's own trips.
     * It maps a station to multiple trips / scraps. A source root's lookup
     * holds every trip of its subtree, named as the root's view names them,
     * since decode hands every name under the root to the root.
     */
    class StationTripScrapLookup {
    public:
        StationTripScrapLookup(cwSurveyNode* node, const cwScopeLabels& labels);
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

inline cwLinePlotTask::LinePlotResultData cwLinePlotTask::LinePlotResultData::cleared()
{
    LinePlotResultData result;
    result.ExternalScopesChecked = true;
    return result;
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

inline void cwLinePlotTask::LinePlotResultData::setRegionStationPositions(cwStationPositionLookup positions)
{
    RegionStationPositions = std::move(positions);
    RegionStationPositionsChanged = true;
}

inline cwStationPositionLookup cwLinePlotTask::LinePlotResultData::regionStationPositions() const
{
    return RegionStationPositions;
}

inline bool cwLinePlotTask::LinePlotResultData::hasRegionStationPositionsChanged() const
{
    return RegionStationPositionsChanged;
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

inline void cwLinePlotTask::LinePlotResultData::setTripSplayVertexRanges(
    QVector<cwLinePlotGeometry::VertexRange> ranges) {
    TripSplayVertexRanges = std::move(ranges);
}

inline void cwLinePlotTask::LinePlotResultData::setTripUuids(QVector<QUuid> tripUuids) {
    TripUuids = std::move(tripUuids);
}

inline QVector<cwLinePlotGeometry::VertexRange>
cwLinePlotTask::LinePlotResultData::tripVertexRanges() const {
    return TripVertexRanges;
}

inline QVector<cwLinePlotGeometry::VertexRange>
cwLinePlotTask::LinePlotResultData::tripSplayVertexRanges() const {
    return TripSplayVertexRanges;
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

inline void cwLinePlotTask::LinePlotCaveData::setUnconnectedChunkError(QList<cwFindUnconnectedSurveyChunks::Result> results)
{
    UnconnectedChunksErrors = results;
}

inline double cwLinePlotTask::LinePlotCaveData::depth() const
{
    return Depth;
}

inline double cwLinePlotTask::LinePlotCaveData::length() const
{
    return Length;
}

inline void cwLinePlotTask::LinePlotCaveData::setSplayTips(cwSplayTipsByStation splayTips)
{
    SplayTips = std::move(splayTips);
}

inline cwSplayTipsByStation cwLinePlotTask::LinePlotCaveData::splayTips() const
{
    return SplayTips;
}

inline QList<cwFindUnconnectedSurveyChunks::Result> cwLinePlotTask::LinePlotCaveData::unconnectedChunkError() const
{
    return UnconnectedChunksErrors;
}

inline bool cwLinePlotTask::LinePlotCaveData::hasDepthLengthChanged() const
{
    return DepthLengthChanged;
}

#endif // CWLINEPLOTTASK_H
