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

/**
 * Algorithmic artifact detectors for a passage tube mesh: each one finds a
 * glitch a viewer would see (a hole, an inside-out face, a wall through a
 * wall, a twisted ring) so tests assert "the mesh looks right" in code.
 *
 * Pure functions over value snapshots: callable from any thread.
 *
 * A range is a span of mesh.indices, three per triangle. Topology checks run
 * per trip range, since each trip closes its own tube; intersection checks
 * run over the whole mesh. Triangle numbers in an Issue count from the start
 * of mesh.indices.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeMeshCheck
{
public:
    cwPassageTubeMeshCheck() = delete;

    static constexpr double kMinTriangleArea = 1e-6;
    static constexpr double kMaxAspectRatio = 50.0;
    static constexpr double kWeldTolerance = 1e-5;
    static constexpr double kMaxCreaseDegrees = 150.0;
    static constexpr double kMaxRadiusRatio = 3.0;
    static constexpr double kWallTolerance = 0.15;

    enum Type {
        OpenEdge,
        InconsistentWinding,
        NonManifoldVertex,
        NegativeVolume,
        DegenerateTriangle,
        SliverTriangle,
        SelfIntersection,
        NestedShell,
        Crease,
        RadiusJump,
        FrameTwist,
        FoldBack,
        WallMissed
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

    struct Report {
        QVector<Issue> issues;
        int componentCount = 0;
        QVector<ComponentStats> components;
    };

    struct Options {
        // User-provided so the default arguments below can construct it
        // before this class is complete.
        Options() {}

        double minTriangleArea = kMinTriangleArea;
        double maxAspectRatio = kMaxAspectRatio;
        double maxCreaseDegrees = kMaxCreaseDegrees;
        double maxRadiusRatio = kMaxRadiusRatio;
        double wallToleranceMeters = kWallTolerance;
    };

    using Range = cwPassageTubeMesh::VertexRange;

    static Range wholeMesh(const cwPassageTubeMesh& mesh);

    // OpenEdge, InconsistentWinding, NonManifoldVertex, and NegativeVolume
    // for each closed, consistently wound component, with component stats.
    static Report closed(const cwPassageTubeMesh& mesh, const Range& range);

    // Positive when the triangles wind counter-clockwise seen from outside.
    static double signedVolume(const cwPassageTubeMesh& mesh, const Range& range);

    static QVector<Issue> degenerateTriangles(const cwPassageTubeMesh& mesh,
                                              const Range& range,
                                              const Options& options = Options());

    static QVector<Issue> selfIntersections(const cwPassageTubeMesh& mesh, const Range& range);

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

    // RadiusJump, FrameTwist and FoldBack over the builder's rings.
    static QVector<Issue> ringIssues(const cwPassageTubeBuilder::Debug& debug,
                                     const Options& options = Options());

    // Each passage LRUD wall point of every leg endpoint lies on debug.mesh.
    static QVector<Issue> wallsOnSurface(const cwPassageTubeBuilder::Debug& debug,
                                         const Options& options = Options());

    // Runs every check; the ring and wall checks run when debug has chains.
    static Report check(const cwPassageTubeMesh& mesh,
                        const cwPassageTubeBuilder::Debug& debug = cwPassageTubeBuilder::Debug(),
                        const Options& options = Options());

    static QString typeName(Type type);
    static QString toText(const Report& report);
};

#endif // CWPASSAGETUBEMESHCHECK_H
