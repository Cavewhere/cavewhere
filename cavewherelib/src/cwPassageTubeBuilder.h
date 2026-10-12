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
#include <QPair>
#include <QSet>
#include <QString>
#include <QUuid>
#include <QVector>
#include <QVector3D>

//Std includes
#include <array>
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
 *
 * At a junction, cwPassageTubeJunction trims each arm where it meets the
 * through tube, grows the through rings to take the arm's section across to
 * the far wall, and zips the arm's mouth into a window cut in the through
 * surface, so the junction is one closed surface.
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
    // station occurrences on the chunk that owns the shot; the passage LRUD
    // are the same readings with shaft depths replaced, and feed the rings.
    // A facing is the unit horizontal direction its station's LRUD were read
    // looking along: the chord between the station's chunk neighbors. The
    // reading's right wall lies clockwise of it.
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
        cwStationLrud fromPassageLrud;
        cwStationLrud toPassageLrud;
        QVector3D fromFacing;
        QVector3D toFacing;
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

    // A cross-section ring around center with N = Options::ringResolution
    // bins. Bin k lies at polar angle binAngles[k] from frame.right toward
    // frame.up, radii[k] from the center; the angles rise with k from
    // binAngles[0] = 0 and keep each bin in the quadrant of 2πk/N, so a round
    // ring's bins sit at 2πk/N. measured[k] is true when survey data filled
    // the bin, false when it was interpolated or borrowed from another ring.
    // chainLeg is the index into Chain::legs of the leg the ring lies on or
    // ends. vertices are the ring's mesh vertices, one per bin, pulled onto
    // the previous ring's plane where they would fold behind it. isGrown is
    // true when a junction grew the ring to take an arm's section across to
    // the far wall.
    struct Ring {
        QVector3D center;
        QVector<double> radii;
        QVector<double> binAngles;
        Frame frame;
        QVector<bool> measured;
        int chainLeg = 0;
        QVector<QVector3D> vertices;
        bool isGrown = false;
    };

    // How a chain end is closed: a fan at a dead end, a mouth on an arm ending
    // at a junction that another chain passes through, nothing on a loop. An
    // arm whose mouth could not be zipped into the through tube closes with a
    // fan over its mouth instead.
    enum class EndCap {
        Fan,
        Mouth,
        None
    };

    // One corner of a zip triangle: a through ring vertex (ring, column), or
    // the arm's mouth vertex at column when ring is kMouthRing.
    struct ZipCorner {
        static constexpr int kMouthRing = -1;

        int ring = kMouthRing;
        int column = 0;

        bool operator==(const ZipCorner&) const = default;
    };

    // Where an arm joins the through chain that holds this window. The through
    // quads in removedQuads, each named by its first ring and first column, are
    // left out; boundary is the hole's edge as (ring, column) through vertices,
    // counter-clockwise in (ring, column) space; zip closes the band between
    // boundary and the arm's mouth with triangles that belong to tripId.
    // mouthCorners holds one corner per arm column: the mouth vertex itself, an
    // earlier mouth vertex it was welded to, or the boundary vertex it was
    // welded to. The arm's strip to its mouth and the zip band both use them.
    struct JunctionWindow {
        QSet<QPair<int, int>> removedQuads;
        QVector<QPair<int, int>> boundary;
        QVector<std::array<ZipCorner, 3>> zip;
        QVector<ZipCorner> mouthCorners;
        int armChain = -1;
        bool isArmStart = false;
        QUuid tripId;
    };

    struct Chain {
        QVector<ChainLeg> legs;
        // In chain order: one ring per station and per sample along each leg.
        QVector<Ring> rings;
        // How far a pit dead-end cap sits past the end station: the station's
        // up when the chain ends climbing, its down when it ends descending,
        // scaled by the end leg's inclination weight so level ends sit flush.
        // Only a steep end extends; a gentle end closes on its plumb ring.
        double startCapOffsetMeters = 0.0;
        double endCapOffsetMeters = 0.0;
        // Where an arm meets its through tube: one point per column on the
        // through surface, past the arm's first or last ring. Empty when the
        // end is not a mouth.
        QVector<QVector3D> startMouth;
        QVector<QVector3D> endMouth;
        // The center of the fan that closes a mouth the stitch pass could not
        // zip, inside the through tube; the mouth's centroid when unset.
        std::optional<QVector3D> startFanCenter;
        std::optional<QVector3D> endFanCenter;
        // The holes the arms joining this chain cut into it.
        QVector<JunctionWindow> windows;
        EndCap startCap = EndCap::Fan;
        EndCap endCap = EndCap::Fan;
    };

    // Diagnostics: the intermediate results of the pipeline, for inspecting
    // how the builder read the survey.
    struct Debug {
        QVector<Leg> legs;
        QVector<Chain> chains;
        QVector<cwPassageTubeMesh::Warning> warnings;
        cwPassageTubeMesh mesh;
        // Mesh vertices where a sharp fold is by design: the centers of fan
        // caps and trip boundaries.
        QSet<quint32> intendedCreaseVertices;
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
