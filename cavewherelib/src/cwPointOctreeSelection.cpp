// cwPointOctreeSelection.cpp
#include "cwPointOctreeSelection.h"

//Qt includes
#include <QHash>
#include <QSet>

//Our includes
#include "cwFrustum.h"
#include "cwPointCloudClod.h"
#include "cwPointOctreeManifest.h"
#include "cwScreenSpace.h"

//Std includes
#include <algorithm>
#include <cmath>
#include <queue>
#include <vector>

namespace cw::octree {

namespace {

    constexpr int kRootIndex = 0;

    //A node of the forest walk: which tree it came from, and where in it
    struct ForestNode
    {
        SelectedNode node;
        int tree = -1;
    };

    //Orders the walk coarsest-projecting first, ties broken by node index and
    //only then by tree index, so the cut is the same for the same forest and
    //camera. Node index before tree index matters under an orthographic camera,
    //where every same-level node of every tree projects to the same spacing and
    //so ties exactly: the trees then advance in lockstep and a binding cap is
    //spent evenly across a file boundary, instead of draining tree 0 first and
    //leaving its neighbor a level coarser.
    struct CoarsestFirst
    {
        bool operator()(const ForestNode& left, const ForestNode& right) const
        {
            if(left.node.projectedSpacingPx != right.node.projectedSpacingPx) {
                return left.node.projectedSpacingPx < right.node.projectedSpacingPx;
            }
            if(left.node.node != right.node.node) {
                return left.node.node > right.node.node;
            }
            return left.tree > right.tree;
        }
    };
}

ForestSelection selectForest(const ForestInput& input)
{
    ForestSelection forest;
    forest.trees.resize(input.trees.size());

    if(input.maxNodes <= 0) {
        return forest;
    }

    //A tree with nothing to draw stays in place as an empty Selection, so
    //every caller can read its slice back by the index it handed in
    const auto manifestOf = [&input](int tree) -> const cwPointOctreeManifest*
    {
        const cwPointOctreeManifest* manifest = input.trees.at(tree).manifest;
        return manifest != nullptr && !manifest->nodes.isEmpty() ? manifest : nullptr;
    };

    int takenNodes = 0;
    const auto take = [&](int tree, const SelectedNode& node)
    {
        const cwPointOctreeNode& entry = manifestOf(tree)->nodes.at(node.node);
        Selection& selection = forest.trees[tree];
        selection.nodes.append(node);
        selection.points += entry.pointCount;
        selection.bytes += entry.byteSize;
        forest.points += entry.pointCount;
        forest.bytes += entry.byteSize;
        takenNodes++;
    };

    //Without a camera the roots are all this view can honestly ask for
    if(input.absP11 <= 0.0 || input.viewportHeightPx <= 0) {
        for(int tree = 0; tree < input.trees.size() && takenNodes < input.maxNodes; tree++) {
            if(manifestOf(tree) != nullptr) {
                take(tree, {kRootIndex, 0.0});
            }
        }
        return forest;
    }

    const auto projectedSpacing = [&](int tree, int index)
    {
        const cwPointOctreeManifest* manifest = manifestOf(tree);
        return cw::sse::projectedPixels(manifest->spacing(manifest->nodes.at(index).level),
                                        manifest->nodeBounds(index),
                                        input.viewProjection,
                                        input.absP11,
                                        input.viewportHeightPx);
    };

    const auto culled = [&](int tree, int index)
    {
        return input.frustum != nullptr
               && !input.frustum->intersects(manifestOf(tree)->nodeBounds(index));
    };

    const double refineThreshold = refineThresholdPx(input.screenSpaceErrorPx,
                                                     input.sseInflation);

    std::priority_queue<ForestNode, std::vector<ForestNode>, CoarsestFirst> heap;

    const auto pushChildren = [&](int tree, const SelectedNode& node)
    {
        if(node.projectedSpacingPx <= refineThreshold) {
            return;
        }
        const cwPointOctreeManifest* manifest = manifestOf(tree);
        if(input.maxLevel >= 0 && manifest->nodes.at(node.node).level >= input.maxLevel) {
            return;
        }
        for(int child : manifest->nodes.at(node.node).children) {
            if(child >= 0 && child < manifest->nodes.size()) {
                heap.push({{child, projectedSpacing(tree, child)}, tree});
            }
        }
    };

    //Every root goes in whatever it costs, so a view always has something to
    //draw of every tree it can see; the point cap applies from there on
    for(int tree = 0; tree < input.trees.size() && takenNodes < input.maxNodes; tree++) {
        if(manifestOf(tree) == nullptr || culled(tree, kRootIndex)) {
            continue;
        }

        const SelectedNode root{kRootIndex, projectedSpacing(tree, kRootIndex)};
        take(tree, root);
        pushChildren(tree, root);
    }

    while(!heap.empty() && takenNodes < input.maxNodes) {
        const ForestNode current = heap.top();
        heap.pop();

        if(culled(current.tree, current.node.node)) {
            continue;
        }

        //The coarsest-first order makes the cut a prefix of the one asked for,
        //so the cap leaves every tree at a valid, coarser cut of its own
        const cwPointOctreeNode& entry =
            manifestOf(current.tree)->nodes.at(current.node.node);
        if(forest.points + entry.pointCount > input.maxPoints) {
            forest.pointCapped = true;
            forest.trees[current.tree].pointCapped = true;
            break;
        }

        take(current.tree, current.node);
        pushChildren(current.tree, current.node);
    }

    return forest;
}

Selection selectCut(const SelectionInput& input)
{
    ForestInput forest;
    forest.trees.append({input.manifest});
    forest.frustum = input.frustum;
    forest.viewProjection = input.viewProjection;
    forest.absP11 = input.absP11;
    forest.viewportHeightPx = input.viewportHeightPx;
    forest.screenSpaceErrorPx = input.screenSpaceErrorPx;
    forest.sseInflation = input.sseInflation;
    forest.maxNodes = input.maxNodes;
    forest.maxPoints = input.maxPoints;
    forest.maxLevel = input.maxLevel;

    return selectForest(forest).trees.at(0);
}

QVector<SelectedNode> selectNodes(const SelectionInput& input)
{
    return selectCut(input).nodes;
}

namespace {

//! What one drawn node floors its sprites at: the level of the finest node
//! drawn under it and that node's spacing under the frame's source
struct DrawnFloor {
    int level = 0;
    double spacing = 0.0;
    bool incomplete = false;
};

//! The walk finestDrawnLevels() documents, carrying each floor's spacing
//! beside its level. Where two nodes at the same deepest level feed one
//! floor, the narrower spacing wins, so the floor is the finest drawn. A
//! floor is at most its node's own spacing, so one sparse deep leaf (a few
//! stray returns measured wide) never draws its ancestor's dense points
//! larger than the ancestor's own spacing would. Under Grid both rules are
//! no-ops: a deeper level's grid spacing is never wider, and one level has
//! one grid spacing.
QVector<DrawnFloor> finestDrawnFloors(const cwPointOctreeManifest& manifest,
                                      const QVector<int>& drawnNodes,
                                      const QVector<int>& missingNodes,
                                      cw::pointcloud::SpacingSource source)
{
    const QVector<int>& parents = manifest.parents();

    const auto ownFloor = [&](int node) {
        const cwPointOctreeNode& entry = manifest.nodes.at(node);
        return DrawnFloor {entry.level,
                           cw::clod::floorSpacing(manifest.spacing(entry.level),
                                                  double(entry.measuredSpacing), source),
                           false};
    };

    QHash<int, DrawnFloor> drawn;
    drawn.reserve(drawnNodes.size());
    for(int node : drawnNodes) {
        drawn.insert(node, ownFloor(node));
    }

    const auto nearestDrawnAncestor = [&](int node) {
        for(int ancestor = parents.at(node); ancestor >= 0; ancestor = parents.at(ancestor)) {
            const auto entry = drawn.find(ancestor);
            if(entry != drawn.end()) {
                return entry;
            }
        }
        return drawn.end();
    };

    //A missing node leaves a hole that only its nearest drawn ancestor covers,
    //at that ancestor's own spacing
    for(int node : incompleteDrawnNodes(manifest, drawnNodes, missingNodes)) {
        drawn[node].incomplete = true;
    }

    //Deepest first, so every node holds its final value before it lifts it
    //into the nearest drawn node above it. A complete node passes on the
    //deepest level drawn under it; an incomplete one passes on its own level,
    //which caps what its subtree lifts into the nodes above it.
    QVector<int> deepestFirst = drawnNodes;
    std::stable_sort(deepestFirst.begin(), deepestFirst.end(), [&manifest](int left, int right) {
        return manifest.nodes.at(left).level > manifest.nodes.at(right).level;
    });
    for(int node : std::as_const(deepestFirst)) {
        DrawnFloor& entry = drawn[node];
        if(entry.incomplete) {
            const DrawnFloor own = ownFloor(node);
            entry.level = own.level;
            entry.spacing = own.spacing;
        }

        const auto ancestor = nearestDrawnAncestor(node);
        if(ancestor == drawn.end()) {
            continue;
        }
        if(entry.level > ancestor->level) {
            ancestor->level = entry.level;
            ancestor->spacing = entry.spacing;
        } else if(entry.level == ancestor->level) {
            ancestor->spacing = std::min(ancestor->spacing, entry.spacing);
        }
    }

    QVector<DrawnFloor> floors;
    floors.reserve(drawnNodes.size());
    for(int node : drawnNodes) {
        DrawnFloor floor = drawn.value(node);
        floor.spacing = std::min(floor.spacing, ownFloor(node).spacing);
        floors.append(floor);
    }
    return floors;
}

}

QVector<int> finestDrawnLevels(const cwPointOctreeManifest& manifest,
                               const QVector<int>& drawnNodes,
                               const QVector<int>& missingNodes)
{
    const QVector<DrawnFloor> floors =
        finestDrawnFloors(manifest, drawnNodes, missingNodes, cw::pointcloud::SpacingSource::Grid);
    QVector<int> levels;
    levels.reserve(floors.size());
    for(const DrawnFloor& floor : floors) {
        levels.append(floor.level);
    }
    return levels;
}

QSet<int> incompleteDrawnNodes(const cwPointOctreeManifest& manifest,
                               const QVector<int>& drawnNodes,
                               const QVector<int>& missingNodes)
{
    QSet<int> incomplete;
    if(missingNodes.isEmpty()) {
        return incomplete;
    }

    const QSet<int> drawn(drawnNodes.cbegin(), drawnNodes.cend());
    const QVector<int>& parents = manifest.parents();
    for(int missing : missingNodes) {
        for(int ancestor = parents.at(missing); ancestor >= 0; ancestor = parents.at(ancestor)) {
            if(drawn.contains(ancestor)) {
                incomplete.insert(ancestor);
                break;
            }
        }
    }
    return incomplete;
}

QVector<double> finestDrawnSpacings(const cwPointOctreeManifest& manifest,
                                    const QVector<int>& drawnNodes,
                                    const QVector<int>& missingNodes,
                                    cw::pointcloud::SpacingSource source)
{
    const QVector<DrawnFloor> floors =
        finestDrawnFloors(manifest, drawnNodes, missingNodes, source);
    QVector<double> spacings;
    spacings.reserve(floors.size());
    for(const DrawnFloor& floor : floors) {
        spacings.append(floor.spacing);
    }
    return spacings;
}

QVector<double> requestedCutSpacings(const cwPointOctreeManifest& manifest,
                                     const QVector<int>& drawnNodes,
                                     const QVector<int>& missingNodes,
                                     cw::pointcloud::SpacingSource source)
{
    //Treated as drawn with nothing missing, so no node counts as incomplete and
    //each requested node lifts its spacing into the drawn nodes above it. The
    //drawn nodes lead the list, so their floors lead the answer.
    const QVector<int> requestedNodes = drawnNodes + missingNodes;
    QVector<double> spacings = finestDrawnSpacings(manifest, requestedNodes, {}, source);
    spacings.resize(drawnNodes.size());
    return spacings;
}

QVector<int> planNodeEvictions(const QVector<NodeResidency>& nodes, qint64 overshootBytes)
{
    QVector<int> plan;
    if(overshootBytes <= 0) {
        return plan;
    }

    QVector<int> candidates;
    candidates.reserve(nodes.size());
    for(int i = 0; i < nodes.size(); i++) {
        const NodeResidency& node = nodes.at(i);
        if(!node.resident || node.pinned || node.bytes <= 0) {
            continue;
        }
        candidates.append(i);
    }

    std::sort(candidates.begin(), candidates.end(), [&nodes](int left, int right) {
        const NodeResidency& leftNode = nodes.at(left);
        const NodeResidency& rightNode = nodes.at(right);
        if(leftNode.selectedThisFrame != rightNode.selectedThisFrame) {
            return rightNode.selectedThisFrame;
        }
        if(leftNode.lastDesiredFrame != rightNode.lastDesiredFrame) {
            return leftNode.lastDesiredFrame < rightNode.lastDesiredFrame;
        }
        return leftNode.node < rightNode.node;
    });

    qint64 reclaimed = 0;
    for(int index : candidates) {
        plan.append(nodes.at(index).node);
        reclaimed += nodes.at(index).bytes;
        if(reclaimed >= overshootBytes) {
            break;
        }
    }

    return plan;
}

double nextSseInflation(const InflationInput& input)
{
    const double current = std::clamp(input.current, 1.0, kMaxSseInflation);

    //The cut costs more than this view is allowed, in bytes or in points
    if(input.desiredBytes > input.availableBytes || input.pointCapped) {
        return std::min(current * kSseInflationStep, kMaxSseInflation);
    }

    //A cut one step finer was probed and fits with room to spare
    if(current > 1.0
       && input.desiredBytesRelaxed >= 0
       && !input.pointCappedRelaxed
       && double(input.desiredBytesRelaxed)
              < double(input.availableBytes) * (1.0 - kSseRelaxMargin)) {
        const double steps = std::max(1, input.relaxedSteps);
        return std::max(current / std::pow(kSseInflationStep, steps), 1.0);
    }

    return current;
}

}
