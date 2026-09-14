// cwPointOctreeSelection.h
#pragma once

//Qt includes
#include <QMatrix4x4>
#include <QVector>

//Std includes
#include <limits>

//Our includes
#include "CaveWhereLibExport.h"

class cwPointOctreeManifest;
class cwFrustum;

/**
 * The node-selection and eviction policy of the point octree renderer. Every
 * function here is pure and touches no RHI state, so a test or a worker can
 * call it on any thread.
 */
namespace cw::octree {

    //Caps the selection walk, and with it the draw count per view per frame
    constexpr int kMaxDesiredNodes = 4096;

    //Anti-churn: how fast the screen space error grows and shrinks under pressure
    constexpr double kSseInflationStep = 1.25;
    constexpr double kMaxSseInflation = 16.0;

    //How far under the budget share the relaxed cut must fit before inflation steps back down
    constexpr double kSseRelaxMargin = 0.10;

    //How often a view inflated above 1 re-selects the cut one step finer to see
    //whether it fits again. Selection is cheap but not free, so this is a probe
    //rather than a per-frame answer.
    constexpr int kSseRelaxProbeFrames = 10;

    //Live priority = projectedSpacingPx * kPriorityScale
    constexpr quint64 kPriorityScale = 1024;

    //residencyReady() requests outrank every live request but the root
    constexpr quint64 kExportPriority = std::numeric_limits<quint64>::max() - 1;

    //The root always loads first
    constexpr quint64 kRootPriority = std::numeric_limits<quint64>::max();

    /**
     * Everything selectNodes() needs to cut the tree for one view of one frame.
     *
     * A null frustum culls nothing, which is what a test without a camera
     * wants. viewProjection is the clip-space corrected matrix, so row 3 is the
     * w row and an orthographic camera keeps w = 1.
     */
    struct CAVEWHERE_LIB_EXPORT SelectionInput
    {
        const cwPointOctreeManifest* manifest = nullptr;
        const cwFrustum* frustum = nullptr;
        QMatrix4x4 viewProjection;
        double absP11 = 0.0;            //|projectionMatrix()(1, 1)|
        int viewportHeightPx = 0;       //Physical pixels
        double screenSpaceErrorPx = 1.5;
        double sseInflation = 1.0;
        int maxNodes = kMaxDesiredNodes;

        //Points this view may draw. The cut stops refining once the next node
        //would push it past this, which bounds frame time in the unit frame
        //time follows.
        qint64 maxPoints = std::numeric_limits<qint64>::max();
    };

    //One node of the cut, with the on-screen spacing of its points that put it there
    struct CAVEWHERE_LIB_EXPORT SelectedNode
    {
        int node = -1;
        double projectedSpacingPx = 0.0;
    };

    //! One cut through the tree, with what drawing it costs
    struct CAVEWHERE_LIB_EXPORT Selection
    {
        QVector<SelectedNode> nodes;
        qint64 points = 0;
        qint64 bytes = 0;

        //! maxPoints stopped the walk, so the cut is coarser than the camera asked for
        bool pointCapped = false;
    };

    /**
     * The cut through the tree this view wants this frame, coarsest-projecting
     * node first and the root always at index 0.
     *
     * A node refines while its points are farther apart than
     * screenSpaceErrorPx * sseInflation on screen; a node outside the frustum
     * takes its whole subtree with it. An unknown camera (absP11 or
     * viewportHeightPx of zero) selects the root alone, so a view that has not
     * been rendered yet still holds the cloud's presence.
     *
     * The root is always taken, even when it alone holds more points than
     * maxPoints; the cap applies from the second node on. The cut stays valid
     * either way, because the tree is additive: every selected node's ancestors
     * are selected too, so stopping early only leaves the rest coarser.
     */
    CAVEWHERE_LIB_EXPORT Selection selectCut(const SelectionInput& input);

    //! selectCut()'s nodes alone, for the callers that need nothing else
    CAVEWHERE_LIB_EXPORT QVector<SelectedNode> selectNodes(const SelectionInput& input);

    //What the planner knows about one resident node
    struct CAVEWHERE_LIB_EXPORT NodeResidency
    {
        //Where the node sits in the caller's node table, which is what the plan
        //names. The list itself holds only the resident nodes, so its own order
        //says nothing about the tree.
        int node = -1;
        bool resident = false;
        bool selectedThisFrame = false;
        quint64 lastDesiredFrame = 0;
        qint64 bytes = 0;
        bool pinned = false;            //The root, which never leaves
    };

    /**
     * NodeResidency::node of each resident node to release until overshootBytes
     * is covered.
     *
     * Nodes the cut dropped go first, oldest lastDesiredFrame first, then the
     * selected ones in the same order; a pinned node is never chosen. The plan
     * stops as soon as the overshoot is covered, and comes back partial when
     * the resident set cannot cover it — the caller then raises its inflation
     * through nextSseInflation().
     */
    CAVEWHERE_LIB_EXPORT QVector<int> planNodeEvictions(const QVector<NodeResidency>& nodes,
                                                        qint64 overshootBytes);

    /**
     * What this view's cut costs against what it is allowed, which is what the
     * inflation follows — residency says nothing about it, because LRU keeps
     * residency at the budget whatever the cut asks for.
     */
    struct CAVEWHERE_LIB_EXPORT InflationInput
    {
        double current = 1.0;
        qint64 desiredBytes = 0;            //!< Bytes of the cut at current
        qint64 desiredBytesRelaxed = -1;    //!< Bytes of the cut one step finer, -1 when not probed
        qint64 availableBytes = 0;          //!< The budget share left for this view
        bool pointCapped = false;           //!< The cut at current hit the point budget
        bool pointCappedRelaxed = false;    //!< The probed cut hit it too

        //! Steps between current and the probed cut, so one probe can undo several
        int relaxedSteps = 1;
    };

    /**
     * The view's next screen-space-error multiplier: a step coarser while the
     * cut costs more than the view is allowed, relaxedSteps steps finer once the
     * probed cut fits with kSseRelaxMargin to spare, and unchanged otherwise.
     * Stays within [1, kMaxSseInflation].
     */
    CAVEWHERE_LIB_EXPORT double nextSseInflation(const InflationInput& input);
}
