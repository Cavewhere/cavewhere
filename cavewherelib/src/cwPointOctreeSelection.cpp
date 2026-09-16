// cwPointOctreeSelection.cpp
#include "cwPointOctreeSelection.h"

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

    //Orders the walk coarsest-projecting first, ties broken by node index so
    //the cut is the same for the same manifest and camera
    struct CoarsestFirst
    {
        bool operator()(const SelectedNode& left, const SelectedNode& right) const
        {
            if(left.projectedSpacingPx != right.projectedSpacingPx) {
                return left.projectedSpacingPx < right.projectedSpacingPx;
            }
            return left.node > right.node;
        }
    };
}

Selection selectCut(const SelectionInput& input)
{
    Selection selection;

    const cwPointOctreeManifest* manifest = input.manifest;
    if(manifest == nullptr || manifest->nodes.isEmpty() || input.maxNodes <= 0) {
        return selection;
    }

    const auto take = [manifest, &selection](const SelectedNode& node)
    {
        const cwPointOctreeNode& entry = manifest->nodes.at(node.node);
        selection.nodes.append(node);
        selection.points += entry.pointCount;
        selection.bytes += entry.byteSize;
    };

    //Without a camera the root is all this view can honestly ask for
    if(input.absP11 <= 0.0 || input.viewportHeightPx <= 0) {
        take({kRootIndex, 0.0});
        return selection;
    }

    const auto projectedSpacing = [manifest, &input](int index)
    {
        return cw::sse::projectedPixels(manifest->spacing(manifest->nodes.at(index).level),
                                        manifest->nodeBounds(index),
                                        input.viewProjection,
                                        input.absP11,
                                        input.viewportHeightPx);
    };

    const double refineThreshold = refineThresholdPx(input.screenSpaceErrorPx,
                                                     input.sseInflation);

    std::priority_queue<SelectedNode, std::vector<SelectedNode>, CoarsestFirst> heap;
    heap.push({kRootIndex, projectedSpacing(kRootIndex)});

    while(!heap.empty() && selection.nodes.size() < input.maxNodes) {
        const SelectedNode current = heap.top();
        heap.pop();

        if(input.frustum != nullptr
           && !input.frustum->intersects(manifest->nodeBounds(current.node))) {
            continue;
        }

        //The root goes in whatever it costs, so a view always has something to
        //draw; from the second node on the point budget is a hard cap, and the
        //coarsest-first order makes the cut a prefix of the one asked for.
        if(!selection.nodes.isEmpty()
           && selection.points + manifest->nodes.at(current.node).pointCount > input.maxPoints) {
            selection.pointCapped = true;
            break;
        }

        take(current);

        if(current.projectedSpacingPx > refineThreshold) {
            for(int child : manifest->nodes.at(current.node).children) {
                if(child >= 0 && child < manifest->nodes.size()) {
                    heap.push({child, projectedSpacing(child)});
                }
            }
        }
    }

    return selection;
}

QVector<SelectedNode> selectNodes(const SelectionInput& input)
{
    return selectCut(input).nodes;
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
