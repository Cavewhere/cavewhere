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

    //How far under the budget the ledger must sit before inflation steps back down
    constexpr double kSseRelaxMargin = 0.10;

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
    };

    //One node of the cut, with the on-screen spacing of its points that put it there
    struct CAVEWHERE_LIB_EXPORT SelectedNode
    {
        int node = -1;
        double projectedSpacingPx = 0.0;
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
     */
    CAVEWHERE_LIB_EXPORT QVector<SelectedNode> selectNodes(const SelectionInput& input);

    //What the planner knows about one resident node
    struct CAVEWHERE_LIB_EXPORT NodeResidency
    {
        bool resident = false;
        bool selectedThisFrame = false;
        quint64 lastDesiredFrame = 0;
        qint64 bytes = 0;
        bool pinned = false;            //The root, which never leaves
    };

    /**
     * Indices of the resident nodes to release until overshootBytes is covered.
     *
     * Nodes the cut dropped go first, oldest lastDesiredFrame first, then the
     * selected ones in the same order; a pinned node is never chosen. The plan
     * stops as soon as the overshoot is covered, and comes back partial when
     * the resident set cannot cover it — the caller then raises its inflation
     * through nextSseInflation().
     */
    CAVEWHERE_LIB_EXPORT QVector<int> planNodeEvictions(const QVector<NodeResidency>& nodes,
                                                        qint64 overshootBytes);

    //What the budget did to this view since the last frame
    struct CAVEWHERE_LIB_EXPORT InflationInput
    {
        double current = 1.0;
        bool overBudgetWithNothingEvictable = false;
        bool underBudgetByMargin = false;
    };

    /**
     * The view's next screen-space-error multiplier: a step coarser while the
     * budget is over and nothing is evictable, a step finer once the ledger has
     * room to spare, and unchanged otherwise. Stays within [1, kMaxSseInflation].
     */
    CAVEWHERE_LIB_EXPORT double nextSseInflation(const InflationInput& input);
}
