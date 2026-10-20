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
 * Pipeline: survey shots → legs → chains → sections → rings → closed tubes
 * → union. A chain is a run of legs skinned as one tube; it passes through
 * degree-2 stations, and exactly one chain passes straight through each
 * junction (Loch's rule), so every other arm starts or ends there.
 *
 * Each leg is the loft between its two stations' sections: its rings, at its
 * stations and evenly along it, are the point-wise lerps of the two. Two legs
 * that read the same section at a station share its ring; where they read
 * different ones, the chain holds both rings there.
 *
 * Each chain's rings close into one capped tube per trip it runs through,
 * and chains that share a station overlap there. The mesh is the boolean
 * union of the tubes of each connected group of chains
 * (cwPassageTubeUnion), so a junction is where the passages' walls cross.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeBuilder
{
public:
    cwPassageTubeBuilder() = delete;

    static constexpr double kMinLegLength = 0.01;
    static constexpr double kMinRadiusMeters = 0.05;
    static constexpr double kMaxRadiusMeters = 100.0;
    // A station whose facing lies more than this off a leg reads its side
    // walls square to that leg.
    static constexpr double kMaxFacingDegrees = 60.0;

    struct Options {
        double sampleSpacingMeters = 1.0;
        int ringResolution = 16;
        double defaultRadiusMeters = 0.5;
        double blendLowDegrees = 45.0;
        double blendHighDegrees = 75.0;
        // Slides each end cap's vertices into the passages it meets before
        // the union (cwPassageTubeUnion::slideCaps).
        bool slideCaps = true;
    };

    // One surveyed shot with both endpoints positioned. LRUD come from the
    // station occurrences on the chunk that owns the shot and give the leg's
    // sections at face value; the passage LRUD are the same readings with
    // shaft depths replaced, and size the legs when chains are chosen.
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

    // A ring's frame: tangent along the chain, square to the ring's plane,
    // and right = tangent × up. inclinationWeight is 0 for a leg gentler than
    // Options::blendLowDegrees and 1 for one steeper than
    // Options::blendHighDegrees.
    struct Frame {
        QVector3D tangent;
        QVector3D up;
        QVector3D right;
        double inclinationWeight = 0.0;
    };

    // A gentle leg's section at one of its stations, which a ring centered on
    // the station is: the quadrant ellipse through the reading points of the
    // record the leg's own chunk took there, at face value. readings are the
    // walls along -right, right, up and -up of the ring's frame, a missing one
    // filled from those beside it; stationRadii are the ellipse's radii
    // around station at the ring's binAngles.
    struct Section {
        cwStationLrud readings;
        QVector<double> stationRadii;
        QVector3D station;
    };

    // A cross-section ring around center with N = Options::ringResolution
    // bins. vertices are the ring's mesh vertices, one per bin: a station's
    // section at its station, and the point-wise lerp of the leg's two
    // sections at the ring's fraction along it between them, so a ring
    // between sections that face different ways is not planar. Bin k lies at
    // polar angle binAngles[k] from frame.right toward frame.up, radii[k] from
    // the center in the plane square to frame.tangent and offsetsAlong[k]
    // along frame.tangent off it; the angles rise with k from binAngles[0] =
    // 0, where frame.right points at vertex 0. measured[k] is true when the
    // readings at both ends of the leg filled the bin, false when it was
    // filled from the walls beside it or borrowed. chainLeg is the index into
    // Chain::legs of the leg the ring lies on or ends. isGrown is true when a
    // junction grew the ring to take an arm's section across to the far wall.
    // section is set on the rings that are a gentle leg's section from its
    // own record.
    struct Ring {
        QVector3D center;
        QVector<double> radii;
        QVector<double> binAngles;
        QVector<double> offsetsAlong;
        Frame frame;
        QVector<bool> measured;
        int chainLeg = 0;
        QVector<QVector3D> vertices;
        bool isGrown = false;
        std::optional<Section> section;
    };

    // How a chain end is closed: a fan at a dead end, a mouth on an arm ending
    // at a junction that another chain passes through, nothing on a loop. An
    // arm whose mouth could not be zipped into the through tube closes with a
    // fan over its mouth instead. A dome closes each of the two chains a
    // hairpin breaks: their end rings sit past the station along their legs,
    // so the two tubes overlap around it and neither shows a disc.
    enum class EndCap {
        Fan,
        Mouth,
        None,
        Dome
    };

    // A point of an arm's mouth on the through surface. column is the arm bin
    // whose wall the point continues plus the fraction of the way toward the
    // next bin's, 0 <= fraction < 1.
    struct MouthPoint {
        QVector3D position;
        double column = 0.0;
    };

    // One corner of a zip triangle: a through ring vertex (ring, column), or
    // point number column of the window's mouth number mouth when ring is
    // kMouthRing.
    struct ZipCorner {
        static constexpr int kMouthRing = -1;

        int ring = kMouthRing;
        int column = 0;
        int mouth = 0;

        bool operator==(const ZipCorner&) const = default;
    };

    // An arm's mouth in a window. mouthCorners holds one corner per mouth
    // point: the point itself, an earlier point of the same mouth it was
    // welded to, or the boundary vertex it was welded to. The arm's strip to
    // its mouth, which belongs to tripId, and the zip band both use them.
    struct WindowMouth {
        int armChain = -1;
        bool isArmStart = false;
        QVector<ZipCorner> mouthCorners;
        QUuid tripId;
    };

    // Where arms join the through chain that holds this window. The through
    // quads in removedQuads, each named by its first ring and first column,
    // are left out; boundaries are the hole's edge loops as (ring, column)
    // through vertices, with the hole on their left in (ring, column) space;
    // zip closes the band between the boundaries and the mouths with
    // triangles that belong to tripId. Arms whose holes share through quads
    // share one window.
    struct JunctionWindow {
        QSet<QPair<int, int>> removedQuads;
        QVector<QVector<QPair<int, int>>> boundaries;
        QVector<std::array<ZipCorner, 3>> zip;
        QVector<WindowMouth> mouths;
        QUuid tripId;
    };

    struct Chain {
        QVector<ChainLeg> legs;
        // In chain order: one ring per station and per sample along each leg;
        // two at a station whose legs read different sections there.
        QVector<Ring> rings;
        // How far a pit dead-end cap sits past the end station: the station's
        // up when the chain ends climbing, its down when it ends descending,
        // scaled by the end leg's inclination weight so level ends sit flush.
        // Only a steep end extends; a gentle end closes on its plumb ring.
        double startCapOffsetMeters = 0.0;
        double endCapOffsetMeters = 0.0;
        // How far past its end station a chain carries its end ring when it
        // ends at a junction as that junction's through: the biggest passage
        // there, which no other leg continues. Every other leg is an arm
        // whose mouth lands on the wall it carries past the station. Zero at
        // other ends.
        double startThroughReachMeters = 0.0;
        double endThroughReachMeters = 0.0;
        // Where an arm meets its through tube: points on the through surface
        // past the arm's first or last ring, in column order, with at least
        // one at each whole column. Empty when the end is not a mouth.
        QVector<MouthPoint> startMouth;
        QVector<MouthPoint> endMouth;
        // The center of the fan that closes a mouth the stitch pass could not
        // zip, inside the through tube; the mouth's centroid when unset.
        std::optional<QVector3D> startFanCenter;
        std::optional<QVector3D> endFanCenter;
        // The holes the arms joining this chain cut into it.
        QVector<JunctionWindow> windows;
        EndCap startCap = EndCap::Fan;
        EndCap endCap = EndCap::Fan;
    };

    // A disc across a chain's tube: the cap at one of its ends or a trip
    // boundary, at station.
    struct Cap {
        int chain = -1;
        QUuid caveId;
        QString station;
    };

    // A vertex of a chain's end ring at a station other chains reach,
    // outside every one of their tubes. to is where the cap slide put it,
    // inside one of them; it equals from when no point along the leg within
    // its length is inside.
    struct CapVertex {
        int chain = -1;
        bool atStart = true;
        int bin = 0;
        QVector3D from;
        QVector3D to;

        bool isInside() const { return from != to; }
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
        // One entry per mesh triangle: the chain whose tube the union took it
        // from. triangleInZipBand is all false. triangleCap indexes caps for
        // the triangles of an end fan other than a dome, or of a trip
        // boundary disc, and is -1 for the rest.
        QVector<int> triangleChain;
        QVector<bool> triangleInZipBand;
        QVector<int> triangleCap;
        QVector<Cap> caps;
        // Each end-ring vertex the cap slide looked at: the ones it moved
        // inside another passage and the ones it left.
        QVector<CapVertex> slidCaps;
    };

    static cwPassageTubeMesh build(const cwCavingRegionData& region,
                                   const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                   const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                                   const Options& options);

    static Debug buildDebug(const cwCavingRegionData& region,
                            const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                            const QHash<QUuid, cwSplayTipsByStation>& splaysByCave,
                            const Options& options);

    // The legs and each chain's placed rings as buildRings leaves them:
    // every chain is its own tube, and no mesh is written.
    static Debug buildChainRings(const cwCavingRegionData& region,
                                 const QHash<QUuid, cwStationPositionLookup>& positionsByCave,
                                 const Options& options);
};

#endif // CWPASSAGETUBEBUILDER_H
