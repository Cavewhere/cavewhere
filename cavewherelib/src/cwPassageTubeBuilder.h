/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEBUILDER_H
#define CWPASSAGETUBEBUILDER_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwCavingRegionData.h"
#include "cwPassageTubeMesh.h"
#include "cwStationPositionLookup.h"

//Qt includes
#include <QHash>
#include <QString>
#include <QUuid>
#include <QVector>
#include <QVector3D>

//Std includes
#include <optional>

/**
 * One station occurrence's left/right/up/down in meters. A dimension that was
 * left empty, or that exceeds cwPassageTubeBuilder::kMaxRadiusMeters, is
 * std::nullopt; a set whose four values are all zero or empty is entirely
 * std::nullopt, and a lone zero in an otherwise measured set becomes
 * cwPassageTubeBuilder::kMinRadiusMeters.
 */
struct cwStationLrud {
    std::optional<double> left;
    std::optional<double> right;
    std::optional<double> up;
    std::optional<double> down;

    bool hasAny() const { return left || right || up || down; }
};

/**
 * Builds the passage tube mesh for a caving region from the solved station
 * positions, LRUD, and splay tips.
 *
 * Pure compute over value snapshots: callable from any thread.
 *
 * Pipeline: survey shots → legs → chains → frames → rings → mesh. A chain is
 * a run of legs skinned as one tube; it passes through degree-2 stations, and
 * exactly one chain passes straight through each junction (Loch's rule), so
 * every other arm starts or ends there.
 *
 * Each chain is sampled at its stations and evenly along its legs. A sample's
 * cross-section frame keeps world up in the section plane for gentle passage
 * and rolls toward a rotation-minimizing frame as the passage steepens into a
 * pit, so rings stay continuous through vertical shots.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeBuilder
{
public:
    cwPassageTubeBuilder() = delete;

    static constexpr double kMinLegLength = 0.01;
    static constexpr double kMinRadiusMeters = 0.05;
    static constexpr double kMaxRadiusMeters = 100.0;

    struct Options {
        double sampleSpacingMeters = 1.0;
        int ringResolution = 16;
        double defaultRadiusMeters = 0.5;
        double blendLowDegrees = 45.0;
        double blendHighDegrees = 75.0;
        double subdivideBendDegrees = 60.0;
    };

    // One surveyed shot with both endpoints positioned. LRUD come from the
    // station occurrences on the chunk that owns the shot.
    struct Leg {
        QUuid caveId;
        QUuid tripId;
        int chunkIndex = -1;
        int shotIndex = -1;
        QString from;
        QString to;
        QVector3D fromPosition;
        QVector3D toPosition;
        cwStationLrud fromLrud;
        cwStationLrud toLrud;
    };

    // A leg as a chain walks it; reversed means it is walked to → from.
    struct ChainLeg {
        int legIndex = -1;
        bool reversed = false;
    };

    // A cross-section frame. right = tangent × up; inclinationWeight is 0 for
    // passage gentler than Options::blendLowDegrees and 1 for passage steeper
    // than Options::blendHighDegrees, where up follows the chain rather than
    // world up.
    struct Frame {
        QVector3D tangent;
        QVector3D up;
        QVector3D right;
        double inclinationWeight = 0.0;
    };

    // A cross-section ring around center. Bin k lies at angle 2πk/N from
    // frame.right toward frame.up, N = Options::ringResolution; measured[k]
    // is true when survey data filled the bin, false when it was interpolated
    // or borrowed from another ring.
    struct Ring {
        QVector3D center;
        QVector<double> radii;
        Frame frame;
        QVector<bool> measured;
    };

    struct Chain {
        QVector<ChainLeg> legs;
        // In chain order: one ring per station and per sample along each leg.
        QVector<Ring> rings;
        // How far a pit dead-end cap sits past the end station: the station's
        // up when the chain ends climbing, its down when it ends descending,
        // scaled by the end's inclination weight so level ends sit flush.
        double startCapOffsetMeters = 0.0;
        double endCapOffsetMeters = 0.0;
    };

    // Diagnostics: the intermediate results of the pipeline, for inspecting
    // how the builder read the survey.
    struct Debug {
        QVector<Leg> legs;
        QVector<Chain> chains;
        QVector<cwPassageTubeMesh::Warning> warnings;
    };

    static cwPassageTubeMesh build(const cwCavingRegionData& region,
                                   const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                   const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                   const Options& options);

    static Debug buildDebug(const cwCavingRegionData& region,
                            const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                            const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                            const Options& options);
};

#endif // CWPASSAGETUBEBUILDER_H
