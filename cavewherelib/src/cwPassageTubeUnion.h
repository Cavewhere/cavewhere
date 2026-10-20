/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEUNION_H
#define CWPASSAGETUBEUNION_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeMesh.h"

//Qt includes
#include <QString>
#include <QUuid>
#include <QVector>
#include <QVector3D>

//Std includes
#include <optional>

/**
 * The boolean union of closed passage tubes, through the manifold library.
 *
 * Each chain is its own closed tube: its rings lofted and a fan across each
 * end. Chains that share a station overlap there, and unite() turns the
 * overlapping tubes into one surface with the inside faces removed.
 *
 * Pure functions over value snapshots: callable from any thread.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeUnion
{
public:
    cwPassageTubeUnion() = delete;

    // A point within this distance of a tube's surface lies on it, and
    // counts as inside when the winding number puts it on or inside.
    static constexpr double kSurfaceToleranceMeters = 0.01;
    // The union collapses edges shorter than this and flips away triangles
    // thinner than this, which manifold leaves where an intersection lands
    // within float rounding of an input edge.
    static constexpr double kMergeToleranceMeters = 0.001;
    // How far inside a tube's surface, square to it, a sliding cap vertex
    // comes to rest.
    static constexpr double kSlideMarginMeters = 0.05;

    // manifold::Manifold::Error, one to one.
    enum class Status {
        NoError,
        NonFiniteVertex,
        NotManifold,
        VertexOutOfBounds,
        PropertiesWrongLength,
        MissingPositionProperties,
        MergeVectorsDifferentLengths,
        MergeIndexOutOfBounds,
        TransformWrongLength,
        RunIndexWrongLength,
        FaceIDWrongLength,
        InvalidConstruction,
        ResultTooLarge,
        InvalidTangents,
        Cancelled
    };

    // Where a union triangle came from: the input tube and the face of it
    // the triangle is part of, as unite's faces name them.
    struct Origin {
        int input = -1;
        int face = -1;
    };

    // A pocket the union enclosed: an inside-out shell, negative volume m³,
    // around centroid.
    struct Void {
        double volume = 0.0;
        QVector3D centroid;
    };

    // mesh is the union of the inputs manifold accepted, as one trip range,
    // with each triangle's origin in triangleOrigins and every enclosed void
    // dropped into voids; volume counts the voids as solid. inputStatus
    // holds one entry per input: an input with an error is left out of the
    // union and named here.
    struct Result {
        cwPassageTubeMesh mesh;
        QVector<Origin> triangleOrigins;
        QVector<Void> voids;
        QVector<Status> inputStatus;
        Status status = Status::NoError;
        double volume = 0.0;
        int genus = 0;

        bool isComplete() const;
    };

    // A closed tube of one run of a chain's rings within one trip: the
    // chain's rings in order, the last repeating the first on a loop. A loop
    // that stays in one trip is one tube without caps (isLoop). Otherwise the
    // tube's triangles from firstCapTriangle on are its two end fans, the
    // start's first, each around the centroid of its end ring. A neighbor
    // trip names
    // the trip of the chain's tube that shares that end ring, where the
    // passage runs on.
    struct ChainTube {
        int chain = -1;
        QUuid tripId;
        cwPassageTubeMesh mesh;
        QVector<int> rings;
        bool isLoop = false;
        int firstCapTriangle = 0;
        std::optional<QUuid> startNeighborTrip;
        std::optional<QUuid> endNeighborTrip;

        // One face per triangle of mesh: kSides, else the end fan it is in,
        // kStartCap or kEndCap.
        QVector<int> faces() const;
        // The center of the end fan of face kStartCap or kEndCap.
        QVector3D capCenter(int face) const;
        // True when the tube runs along the leg debug.legs[legIndex].
        bool holdsLeg(const cwPassageTubeBuilder::Debug& rings, int legIndex) const;
    };

    static constexpr int kSides = 0;
    static constexpr int kStartCap = 1;
    static constexpr int kEndCap = 2;

    using CapVertex = cwPassageTubeBuilder::CapVertex;

    // faces holds one face number per triangle of each tube, all 0 when
    // empty. manifold keeps the edges between faces, so a face is a run of
    // triangles to trace, not each triangle.
    static Result unite(const QVector<cwPassageTubeMesh>& tubes, const QVector<QVector<int>>& faces = {});

    // The surface of tubes that lies inside the solid of around: what their
    // union with around removes, and what closes them while around is
    // hidden. Each triangle's origin names its tube and face, as unite's do;
    // inputStatus names the tubes manifold rejects, which are left out.
    static Result coveredSurface(const QVector<cwPassageTubeMesh>& tubes,
                                 const QVector<QVector<int>>& faces,
                                 const QVector<cwPassageTubeMesh>& around);

    // What manifold says of one tube on its own.
    static Status status(const cwPassageTubeMesh& tube);

    // manifold's volume of a closed tube; 0 when manifold rejects it.
    static double volume(const cwPassageTubeMesh& tube);

    static QString statusName(Status status);

    // A closed, outward-wound tube through rings, each a loop of vertices
    // with the same count: consecutive rings lofted, each end ring closed by
    // a fan around its centroid. With isLoop, the last ring joins the first
    // and nothing is capped.
    static cwPassageTubeMesh loft(const QVector<QVector<QVector3D>>& rings, bool isLoop = false);

    // One tube per run of consecutive rings with vertices and per trip, for
    // every chain of buildChainRings. A chain that runs on from one trip
    // into another is cut at the ring they share, which closes both tubes;
    // one that reads two sections at a station is split between them
    // (splitsBefore).
    static QVector<ChainTube> chainTubes(const cwPassageTubeBuilder::Debug& rings);

    // True when chain's ring is the second of a station's two sections, read
    // by legs of one trip either side of it: the chain's tube ends on the
    // ring before and the next one starts on this one, overlapping there.
    // Where the trip changes at the station, the tube runs on across both
    // and is cut between the trips instead.
    static bool splitsBefore(const cwPassageTubeBuilder::Debug& rings, const cwPassageTubeBuilder::Chain& chain, int ring);

    // The chains joined through shared stations, as lists of chain indices.
    static QVector<QVector<int>> chainComponents(const cwPassageTubeBuilder::Debug& rings);

    // The meshes one after another, as one trip range.
    static cwPassageTubeMesh merged(const QVector<cwPassageTubeMesh>& meshes);

    // True when point lies inside the closed tube by its winding number, or
    // on its surface: within kSurfaceToleranceMeters of it, where the
    // winding number is about one half on a face and less on a convex edge.
    static bool contains(const cwPassageTubeMesh& tube, const QVector3D& point);

    // The end-cap notch: each vertex of a chain's end ring at a station that
    // other chains reach, lying outside all of their tubes. to is from.
    static QVector<CapVertex> capNotch(const cwPassageTubeBuilder::Debug& rings);

    // Slides each notch vertex along its end ring's tangent, forward or back,
    // within the end leg's length, to the nearest point inside another
    // chain's tube at the station: kSlideMarginMeters inside the wall it
    // crosses, or nearer the crossing where the line leaves the tube again.
    // The vertex's bin along that leg lerps to it. A shaft's end stays put,
    // its dome its cap. Returns the notch with where each vertex went.
    static QVector<CapVertex> slideCaps(cwPassageTubeBuilder::Debug& rings);
};

#endif // CWPASSAGETUBEUNION_H
