/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEMESHCHECK_H
#define CWPASSAGETUBEMESHCHECK_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeMesh.h"

//Qt includes
#include <QSet>
#include <QString>
#include <QVector>
#include <QVector3D>

//Std includes
#include <optional>

/**
 * Algorithmic artifact detectors for a passage tube mesh: each one finds a
 * glitch a viewer would see (a hole, an inside-out face, a wall through a
 * wall, a twisted ring) so tests assert "the mesh looks right" in code.
 *
 * Pure functions over value snapshots: callable from any thread.
 *
 * A range is a span of mesh.indices, three per triangle. check() looks at
 * what a viewer sees with every trip visible: the trip ranges, without the
 * trip boundary discs that draw only while a neighbor trip is hidden.
 * Triangle numbers in an Issue count from the start of mesh.indices.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeMeshCheck
{
public:
    cwPassageTubeMeshCheck() = delete;

    static constexpr double kMinTriangleArea = 1e-6;
    static constexpr double kMaxAspectRatio = 50.0;
    static constexpr double kMinSliverAltitude = 0.02;
    static constexpr double kWeldTolerance = 1e-5;
    static constexpr double kMaxCreaseDegrees = 150.0;
    static constexpr double kMaxRadiusRatio = 3.0;
    static constexpr double kWallTolerance = 0.15;
    static constexpr double kCenterlineSampleMeters = 0.25;
    // A centerline point this close to the surface lies on it, which counts
    // as inside: a zero reading puts the wall on the centerline.
    static constexpr double kCenterlineTolerance = 0.01;
    static constexpr int kCapSamples = 32;
    // A cap point this close to another chain's surface lies on it: a hidden
    // cone's base on its through passage's wall.
    static constexpr double kCapSurfaceTolerance = 0.01;

    enum Type {
        OpenEdge,
        InconsistentWinding,
        NonManifoldVertex,
        NegativeVolume,
        DegenerateTriangle,
        SliverTriangle,
        SelfIntersection,
        PassageOverlap,
        NestedShell,
        Crease,
        RadiusJump,
        FrameTwist,
        FoldBack,
        WallMissed,
        CenterlineOutside,
        CapExposed,
        WallBeyondReading
    };

    struct Issue {
        Type type = OpenEdge;
        QVector3D position;
        QString detail;
        int triangle = -1;
        int otherTriangle = -1;
    };

    // One edge-connected piece of a range. genus follows V − E + F = 2 − 2g:
    // 0 for a tree-shaped cave, one more for each independent survey loop.
    struct ComponentStats {
        int vertexCount = 0;
        int edgeCount = 0;
        int triangleCount = 0;
        int genus = 0;
        double volume = 0.0;
    };

    // issues are the glitches a viewer would see. warnings are what a viewer
    // sees as intended: two passages running into each other draw as their
    // union, a hard seam.
    struct Report {
        QVector<Issue> issues;
        int componentCount = 0;
        QVector<ComponentStats> components;
        QVector<Issue> warnings;
    };

    struct Options {
        // User-provided so the default arguments below can construct it
        // before this class is complete.
        Options() {}

        double minTriangleArea = kMinTriangleArea;
        double maxAspectRatio = kMaxAspectRatio;
        double minSliverAltitudeMeters = kMinSliverAltitude;
        double maxCreaseDegrees = kMaxCreaseDegrees;
        double maxRadiusRatio = kMaxRadiusRatio;
        double wallToleranceMeters = kWallTolerance;
    };

    using Range = cwPassageTubeMesh::VertexRange;

    static Range wholeMesh(const cwPassageTubeMesh& mesh);

    // The triangles drawn while every trip is visible: all but the trip
    // boundary discs.
    static Range allTripsVisible(const cwPassageTubeMesh& mesh);

    // OpenEdge, InconsistentWinding, NonManifoldVertex, and NegativeVolume
    // for each closed, consistently wound component, with component stats.
    static Report closed(const cwPassageTubeMesh& mesh, const Range& range);

    // Positive when the triangles wind counter-clockwise seen from outside.
    static double signedVolume(const cwPassageTubeMesh& mesh, const Range& range);

    // A SliverTriangle is both thinner than maxAspectRatio allows and
    // narrower than minSliverAltitudeMeters at its shortest altitude.
    static QVector<Issue> degenerateTriangles(const cwPassageTubeMesh& mesh,
                                              const Range& range,
                                              const Options& options = Options());

    // Each crossing between two triangles. One between triangles of two
    // chains, neither of them in a zip band, is a PassageOverlap; the rest
    // are SelfIntersection, including every crossing of a triangle that
    // debug.triangleChain leaves out.
    static QVector<Issue> selfIntersections(const cwPassageTubeMesh& mesh,
                                            const Range& range,
                                            const cwPassageTubeBuilder::Debug& debug = cwPassageTubeBuilder::Debug());

    // Components, joined through shared vertices, that lie inside another.
    // A pair of components that crosses in selfIntersections is left out:
    // the crossing is the issue.
    static QVector<Issue> nestedShells(const cwPassageTubeMesh& mesh,
                                       const Range& range,
                                       const QVector<Issue>& selfIntersections = {});

    static QVector<Issue> creases(const cwPassageTubeMesh& mesh,
                                  const Range& range,
                                  const QSet<quint32>& intendedCreaseVertices,
                                  const Options& options = Options());

    // RadiusJump, FrameTwist and FoldBack over the builder's rings. Rings a
    // junction grew follow the arm's outline and may jump.
    //
    // WallBeyondReading, once per ring fitted to a station's readings (a
    // station's ring or an elbow's middle ring, never one a junction grew),
    // at its farthest vertex: each vertex lies within wallToleranceMeters of
    // the quadrant ellipse of the station's readings anchored at the station,
    // as the station's legs or the ring see it (wallReaches).
    static QVector<Issue> ringIssues(const cwPassageTubeBuilder::Debug& debug,
                                     const Options& options = Options());

    // Where a ring vertex would lie on its station's readings, and how far
    // past them it reaches; negative inside them.
    struct WallReach {
        QVector3D readingPoint;
        double beyondMeters = 0.0;
    };

    // One entry per vertex of a ring fitted to a station's readings, how far
    // it lies from the readings' quadrant ellipse around the station in the
    // section where it lies nearest: the widest passage readings across each
    // of the chain's legs at the station, seen along that leg (round across a
    // steep leg, as the builder fits a pit), or the ring's own section as
    // the builder slid the readings onto the ring's plane, grown to hold the
    // ring's center. nullopt for a vertex in a quadrant without readings.
    // Empty for every other ring.
    static QVector<std::optional<WallReach>> wallReaches(const cwPassageTubeBuilder::Debug& debug,
                                                         const cwPassageTubeBuilder::Chain& chain,
                                                         const cwPassageTubeBuilder::Ring& ring);

    // Each passage LRUD wall point of every leg endpoint lies on debug.mesh,
    // the side walls square to the chain's direction through the station,
    // apart from the stations where the builder warns BendClamped, the
    // junction stations where an arm has a mouth, and the stations of an arm
    // left inside its through passage.
    static QVector<Issue> wallsOnSurface(const cwPassageTubeBuilder::Debug& debug,
                                         const Options& options = Options());

    // Both stations of every leg of a chain with rings, and points every
    // kCenterlineSampleMeters along it, lie inside the triangles drawn while
    // every trip is visible by a winding number vote of three skewed rays, or
    // within kCenterlineTolerance of one of them. The legs of an arm left
    // inside its through passage have no rings.
    static QVector<Issue> centerlineInside(const cwPassageTubeMesh& mesh,
                                           const cwPassageTubeBuilder::Debug& debug);

    // Each disc in debug.caps drawn while every trip is visible, at a
    // station with two or more legs, that shows: one of kCapSamples points
    // spread over it lies outside the mesh of every other chain, or it
    // coincides with another cap. A dead end's cap, and an arm's cone hidden
    // inside its through passage, are what a viewer expects.
    static QVector<Issue> capsExposed(const cwPassageTubeMesh& mesh,
                                      const cwPassageTubeBuilder::Debug& debug);

    // Runs every check over allTripsVisible; the ring, wall, centerline and
    // cap checks run when debug has chains.
    // PassageOverlap goes to Report::warnings, every other kind to issues.
    static Report check(const cwPassageTubeMesh& mesh,
                        const cwPassageTubeBuilder::Debug& debug = cwPassageTubeBuilder::Debug(),
                        const Options& options = Options());

    static QString typeName(Type type);
    static QString toText(const Report& report);
};

#endif // CWPASSAGETUBEMESHCHECK_H
