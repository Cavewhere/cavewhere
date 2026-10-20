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
 * → union. A chain is a run of consecutive shots of one chunk skinned as one
 * tube, so every station on it has one record. Every ring stands vertical:
 * the record's left and right level along the clockwise turn of the plan
 * bisector of the station's legs, its up and down plumb.
 *
 * Each leg is the loft between its two end rings, which advance along its
 * plan run padded at each end (Chain::legPads), so a plumb leg is a box with
 * thickness; its rings are the point-wise lerps of the two. Legs that both
 * place their ring on the station between them share it; elsewhere the chain
 * splits there into pieces that meet by union.
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
    // Two chains' records of one side wall at a station they share that
    // differ by more than this ratio warn JunctionReadingConflict.
    static constexpr double kReadingConflictRatio = 3.0;

    struct Options {
        double sampleSpacingMeters = 1.0;
        int ringResolution = 16;
        double defaultRadiusMeters = 0.5;
        // Slides each end cap's vertices into the passages it meets before
        // the union (cwPassageTubeUnion::slideCaps).
        bool slideCaps = true;
    };

    // One surveyed shot with both endpoints positioned. LRUD come from the
    // station occurrences on the chunk that owns the shot and give the leg's
    // sections at face value.
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
        // The plan direction from → to that the shot's compass reads, for a
        // leg whose stations hold no plan direction between them.
        std::optional<QVector3D> shotFacing;
    };

    // A leg as a chain walks it; reversed means it is walked to → from.
    struct ChainLeg {
        int legIndex = -1;
        bool reversed = false;
    };

    // A ring's frame: tangent level and square to the ring's plane, up
    // plumb, and right = tangent × up, level.
    struct Frame {
        QVector3D tangent;
        QVector3D up;
        QVector3D right;
    };

    // A leg's section at one of its stations, which its end ring there is:
    // the quadrant ellipse through the reading points of the record the leg's
    // own chunk took there, at face value. readings are the
    // walls along -right, right, up and -up of the ring's frame, a missing one
    // filled from those beside it; stationRadii are the ellipse's radii
    // around center at the ring's binAngles. center is the ring's center:
    // station, or station padded along the leg's plan run.
    struct Section {
        cwStationLrud readings;
        QVector<double> stationRadii;
        QVector3D station;
        QVector3D center;
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
    // section is set on the rings that are a leg's section from its own
    // record.
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

    // How a chain end is closed: a fan across its end ring, or nothing on a
    // loop.
    enum class EndCap {
        Fan,
        None
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

    // How far a leg's end rings stand off its stations along its plan run:
    // e·start before its start and e·end past its end, where start and end
    // are pad = mean(L, R) / 2 of each station's drawn walls and
    // e = 1 − min(1, planRun / (start + end)).
    struct LegPad {
        double start = 0.0;
        double end = 0.0;
        double e = 0.0;
    };

    struct Chain {
        QVector<ChainLeg> legs;
        // One per leg.
        QVector<LegPad> legPads;
        // The plan direction each station faces, in walk order, as the run of
        // shots the chain was cut from faces it (cwPassageTubeGeometry::
        // chainFacings); empty for a chain the builder did not cut.
        QVector<QVector3D> facings;
        // In chain order: one ring per station and per sample along each leg.
        QVector<Ring> rings;
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
        // the triangles of an end fan or of a trip boundary disc, and is -1
        // for the rest.
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
