// cwPointOctreeSelection.cpp
#include "cwPointOctreeSelection.h"

//Qt includes
#include <QHash>

//Our includes
#include "cwFrustum.h"
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

    return selectForest(forest).trees.at(0);
}

QVector<SelectedNode> selectNodes(const SelectionInput& input)
{
    return selectCut(input).nodes;
}

QVector<int> finestDrawnLevels(const cwPointOctreeManifest& manifest,
                               const QVector<int>& drawnNodes)
{
    QHash<int, int> finest;
    finest.reserve(drawnNodes.size());
    for(int node : drawnNodes) {
        finest.insert(node, manifest.nodes.at(node).level);
    }

    //Every drawn node lifts its own level into the drawn ancestors above it, so
    //each of them ends at the deepest level drawn anywhere under it.
    const QVector<int>& parents = manifest.parents();
    for(int node : drawnNodes) {
        const int level = manifest.nodes.at(node).level;
        for(int ancestor = parents.at(node); ancestor >= 0; ancestor = parents.at(ancestor)) {
            auto entry = finest.find(ancestor);
            if(entry != finest.end()) {
                if(entry.value() >= level) {
                    break;
                }
                entry.value() = level;
            }
        }
    }

    QVector<int> levels;
    levels.reserve(drawnNodes.size());
    for(int node : drawnNodes) {
        levels.append(finest.value(node));
    }
    return levels;
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
