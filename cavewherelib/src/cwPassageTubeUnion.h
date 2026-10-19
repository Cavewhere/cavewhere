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
#include <QVector>
#include <QVector3D>

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
    // counts as outside.
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

    // mesh is the union of the inputs manifold accepted, as one trip range.
    // inputStatus holds one entry per input: an input with an error is left
    // out of the union and named here.
    struct Result {
        cwPassageTubeMesh mesh;
        QVector<Status> inputStatus;
        Status status = Status::NoError;
        double volume = 0.0;
        int genus = 0;

        bool isComplete() const;
    };

    // A closed tube of one run of a chain's rings.
    struct ChainTube {
        int chain = -1;
        cwPassageTubeMesh mesh;
    };

    // A cap vertex at a station other chains reach, outside every one of
    // their tubes. to is where the slide put it, inside one of them; it
    // equals from when no point along the leg within its length is inside.
    struct CapVertex {
        int chain = -1;
        bool atStart = true;
        int bin = 0;
        QVector3D from;
        QVector3D to;

        bool isInside() const { return from != to; }
    };

    static Result unite(const QVector<cwPassageTubeMesh>& tubes);

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

    // One tube per run of consecutive rings with vertices, for every chain
    // of buildChainRings.
    static QVector<ChainTube> chainTubes(const cwPassageTubeBuilder::Debug& rings);

    // The chains joined through shared stations, as lists of chain indices.
    static QVector<QVector<int>> chainComponents(const cwPassageTubeBuilder::Debug& rings);

    // The meshes one after another, as one trip range.
    static cwPassageTubeMesh merged(const QVector<cwPassageTubeMesh>& meshes);

    // True when point lies inside the closed tube by its winding number and
    // farther than kSurfaceToleranceMeters from its surface.
    static bool contains(const cwPassageTubeMesh& tube, const QVector3D& point);

    // The end-cap notch: each vertex of a chain's end ring at a station that
    // other chains reach, lying outside all of their tubes. to is from.
    static QVector<CapVertex> capNotch(const cwPassageTubeBuilder::Debug& rings);

    // Slides each notch vertex along its end ring's tangent, forward or back,
    // within the end leg's length, to the nearest point inside another
    // chain's tube at the station: kSlideMarginMeters inside the wall it
    // crosses, or nearer the crossing where the line leaves the tube again.
    // The vertex's bin along that leg lerps to it. Returns the notch with
    // where each vertex went.
    static QVector<CapVertex> slideCaps(cwPassageTubeBuilder::Debug& rings);
};

#endif // CWPASSAGETUBEUNION_H
