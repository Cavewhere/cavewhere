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
#include <QSet>
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
 * Pipeline:
 * 1. Shots → chains. A chain is one chunk's run of consecutive shots, so
 *    every station on it has one record. A chain splits where a padded ring
 *    leaves its station; the pieces meet by union.
 * 2. Chains → rings. Every ring stands vertical: the record's left and right
 *    level along the clockwise turn of the plan bisector of the station's
 *    legs (the miter in plan), its up and down plumb. A leg's end rings
 *    advance along its plan run padded at each end (Chain::legPads), so a
 *    plumb leg is a box with thickness; its rings between are the point-wise
 *    lerps of the two.
 * 3. Rings → capped tubes. Each chain closes into one fan-capped tube per
 *    trip it runs through; a loop closes on itself uncapped.
 * 4. Cap slide. An end cap at a station other chains reach slides into the
 *    passages it meets (cwPassageTubeUnion::slideCaps).
 * 5. Union. The tubes of each connected group of chains are joined by a
 *    boolean union (cwPassageTubeUnion), so a junction is where the
 *    passages' walls cross.
 *
 * cwPassageTubeMeshCheck expects the tubes before the union to overlap
 * (PassageOverlap) and the union to be closed, with no self-intersections;
 * a chain tube that crosses itself warns ChainSelfCrossing, and a pocket
 * the union wraps warns EnclosedVoid.
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
    // Rings between a shaft's end section and the apex of its dome.
    static constexpr int kDomeRings = 4;

    // How tall a shaft's round cap stands past its end station.
    enum class ShaftCapHeight {
        Reading,    // the U or D reading the cap points along
        MeanWidth,  // mean(L, R) of the end section, a hemisphere on a round shaft
        Flat        // no dome, a flat fan
    };

    enum class ShaftCapShape {
        Round,  // a quarter ellipse from the end ring to the apex
        Cone    // straight from the end ring to the apex
    };

    struct Options {
        double sampleSpacingMeters = 1.0;
        int ringResolution = 16;
        double defaultRadiusMeters = 0.5;
        // Slides each end cap's vertices into the passages it meets before
        // the union (cwPassageTubeUnion::slideCaps).
        bool slideCaps = true;
        // Scales the pad a leg's end rings stand off its stations (LegPad).
        double padScale = 1.0;
        // A leg is a shaft when it rises more than shaftRiseRatio times its
        // section height and its vertical-ring slab would be thinner along
        // the shot than wide across it by more than shaftThinnessRatio:
        // tan(clino) · (L+R) / (U+D) > shaftThinnessRatio. Prototype knobs.
        double shaftRiseRatio = 1.0;
        double shaftThinnessRatio = 5.0;
        // A shaft stands rings square to its leg, round at mean(L, R) along
        // the shot, as its own chain piece capped by domes; off, every leg
        // keeps vertical rings.
        bool shaftRingsSquare = true;
        ShaftCapHeight shaftCapHeight = ShaftCapHeight::Reading;
        ShaftCapShape shaftCapShape = ShaftCapShape::Round;
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
    // Chain::legs of the leg the ring lies on or ends. section is set on the
    // rings that are a leg's section from its own record.
    struct Ring {
        QVector3D center;
        QVector<double> radii;
        QVector<double> binAngles;
        QVector<double> offsetsAlong;
        Frame frame;
        QVector<bool> measured;
        int chainLeg = 0;
        QVector<QVector3D> vertices;
        std::optional<Section> section;
    };

    // How a chain end is closed: a fan across its end ring, or nothing on a
    // loop.
    enum class EndCap {
        Fan,
        None
    };

    // How far a leg's end rings stand off its stations along its plan run:
    // e·start before its start and e·end past its end, where start and end
    // are pad = Options::padScale · (|dz| / length) · mean(L, R) of each
    // station's drawn walls and e = 1 − min(1, planRun / (start + end)). A
    // padded leg's plan footprint is start + end: a plumb leg stands as deep
    // as it is wide, a level one on its stations. A shaft
    // (Options::shaftRiseRatio, shaftThinnessRatio) stands on its stations:
    // riseRatio is its rise over its section height, thinness is
    // tan(clino) · (L+R) / (U+D), +∞ for a plumb leg.
    struct LegPad {
        double start = 0.0;
        double end = 0.0;
        double e = 0.0;
        bool isShaft = false;
        double riseRatio = 0.0;
        double thinness = 0.0;
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
        // from. triangleCap indexes caps for the triangles of an end fan or of
        // a trip boundary disc, and is -1 for the rest.
        QVector<int> triangleChain;
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
