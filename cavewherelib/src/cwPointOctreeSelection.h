// cwPointOctreeSelection.h
#pragma once

//Qt includes
#include <QMatrix4x4>
#include <QSet>
#include <QVector>

//Std includes
#include <limits>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPointCloudShading.h"

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

    //A maxLevel that leaves the cut free to refine as deep as the tree goes
    constexpr int kUnlimitedLevel = -1;

    //Anti-churn: how fast the screen space error grows and shrinks under pressure
    constexpr double kSseInflationStep = 1.25;
    constexpr double kMaxSseInflation = 16.0;

    //A slewed inflation within this log-space gap of the governor's target has
    //arrived, and takes the target itself. About a 1% ratio, which no sprite
    //shows, so the slew's exponential tail ends rather than keeping the frame
    //loop drawing and the governor's target held.
    constexpr double kSseSlewArrivalLogGap = 1e-2;

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
     * The projected spacing the cut refines to: a node refines while its points
     * project farther apart than this. PointCloud.vert sizes its sprites
     * against the same number, so the cut and the sprites follow one rule.
     */
    constexpr double refineThresholdPx(double screenSpaceErrorPx, double sseInflation)
    {
        return screenSpaceErrorPx * sseInflation;
    }

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

        //The deepest level the cut refines to, kUnlimitedLevel for no cap
        int maxLevel = kUnlimitedLevel;
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

    //! One tree of a forest cut: the manifest the walk refines through
    struct CAVEWHERE_LIB_EXPORT ForestTree
    {
        const cwPointOctreeManifest* manifest = nullptr;

        bool operator==(const ForestTree& other) const = default;
    };

    /**
     * Everything selectForest() needs: the camera fields of SelectionInput,
     * once, and the trees the cut spans. maxNodes and maxPoints hold across the
     * whole forest, so the trees share one cap instead of each guessing at a
     * share of it.
     */
    struct CAVEWHERE_LIB_EXPORT ForestInput
    {
        QVector<ForestTree> trees;
        const cwFrustum* frustum = nullptr;
        QMatrix4x4 viewProjection;
        double absP11 = 0.0;            //|projectionMatrix()(1, 1)|
        int viewportHeightPx = 0;       //Physical pixels
        double screenSpaceErrorPx = 1.5;
        double sseInflation = 1.0;
        int maxNodes = kMaxDesiredNodes;
        qint64 maxPoints = std::numeric_limits<qint64>::max();
        int maxLevel = kUnlimitedLevel;
    };

    //! One forest cut: a Selection per input tree, in input order, plus the totals
    struct CAVEWHERE_LIB_EXPORT ForestSelection
    {
        QVector<Selection> trees;
        qint64 points = 0;
        qint64 bytes = 0;

        //! maxPoints stopped the walk, so the cut is coarser than the camera asked for
        bool pointCapped = false;
    };

    /**
     * The one cut every tree of @a input shares this frame, coarsest-projecting
     * node first across the whole forest.
     *
     * Every tree's root is taken before the point cap applies, so a view always
     * has something to draw of every tree it can see. Nodes of different trees
     * that project the same land at the same depth, which is what keeps the
     * sprites one size across a file boundary. A tree with a null or empty
     * manifest, and a tree whose root the frustum culls, comes back as an empty
     * Selection.
     */
    CAVEWHERE_LIB_EXPORT ForestSelection selectForest(const ForestInput& input);

    /**
     * The level whose spacing each of @a drawnNodes floors its sprites at, one
     * entry per input node and in the same order. Every index has to name a
     * node of @a manifest.
     *
     * @a drawnNodes is the resident subset of one frame's cut — what the frame
     * really draws, rather than what the cut asked for — and @a missingNodes
     * is the rest of that cut, the nodes still streaming. The tree is
     * additive, so a drawn node draws its own points over whatever its drawn
     * descendants draw, and the answer for it is the deepest level any of them
     * sits at, or its own level when none of them is drawn.
     *
     * A drawn node is incomplete while any missing node has it as its nearest
     * drawn ancestor: it is all that covers that missing node's cell, so it
     * keeps its own level until its children land. An incomplete node also
     * caps what its subtree lifts into the drawn nodes above it, which get at
     * most its level from that branch. Without this, one small deep leaf that
     * lands early would shrink the root's sprites across every octant whose
     * children have not arrived yet, and open holes there.
     *
     * The renderer writes these floors a frame after the gather that worked
     * them out, so the protection takes effect the frame after the cut changes.
     */
    CAVEWHERE_LIB_EXPORT QVector<int> finestDrawnLevels(const cwPointOctreeManifest& manifest,
                                                        const QVector<int>& drawnNodes,
                                                        const QVector<int>& missingNodes = {});

    /**
     * The nodes of @a drawnNodes that are the nearest drawn ancestor of some
     * node of @a missingNodes: all that covers that missing node's cell, which
     * finestDrawnLevels() keeps at its own level until its children land.
     */
    CAVEWHERE_LIB_EXPORT QSet<int> incompleteDrawnNodes(const cwPointOctreeManifest& manifest,
                                                        const QVector<int>& drawnNodes,
                                                        const QVector<int>& missingNodes);

    /**
     * The floor of every node in @a drawnNodes, in meters: the spacing of the
     * node finestDrawnLevels() names for it, as cw::clod::drawSpacing gives it
     * under @a source. Under Grid that is manifest.spacing() of the level
     * finestDrawnLevels() returns, value for value; under Measured it is the
     * larger of that node's grid spacing and its measured spacing, and where
     * several nodes at that deepest level feed one floor, the narrowest of
     * them. Either way a floor is at most its own node's spacing.
     */
    CAVEWHERE_LIB_EXPORT QVector<double> finestDrawnSpacings(const cwPointOctreeManifest& manifest,
                                                             const QVector<int>& drawnNodes,
                                                             const QVector<int>& missingNodes,
                                                             cw::pointcloud::SpacingSource source);

    /**
     * The floor of every node in @a drawnNodes under RevealMode::Grow, in
     * meters: the finest spacing of the cut the forest asked for under it,
     * drawn or still streaming, under @a source. It is finestDrawnSpacings()
     * over @a drawnNodes and @a missingNodes together, all treated as drawn,
     * so a missing child floors its drawn ancestor at the child's spacing
     * rather than leaving the ancestor at its own. With @a missingNodes empty
     * it is finestDrawnSpacings() value for value.
     */
    CAVEWHERE_LIB_EXPORT QVector<double> requestedCutSpacings(const cwPointOctreeManifest& manifest,
                                                              const QVector<int>& drawnNodes,
                                                              const QVector<int>& missingNodes,
                                                              cw::pointcloud::SpacingSource source);

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
