// cwPointOctreeSelection.cpp
#include "cwPointOctreeSelection.h"

//Our includes
#include "cwFrustum.h"
#include "cwPointOctreeManifest.h"
#include "cwScreenSpace.h"

//Std includes
#include <algorithm>
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

QVector<SelectedNode> selectNodes(const SelectionInput& input)
{
    QVector<SelectedNode> selected;

    const cwPointOctreeManifest* manifest = input.manifest;
    if(manifest == nullptr || manifest->nodes.isEmpty() || input.maxNodes <= 0) {
        return selected;
    }

    //Without a camera the root is all this view can honestly ask for
    if(input.absP11 <= 0.0 || input.viewportHeightPx <= 0) {
        selected.append({kRootIndex, 0.0});
        return selected;
    }

    const auto projectedSpacing = [manifest, &input](int index)
    {
        return cw::sse::projectedPixels(manifest->spacing(manifest->nodes.at(index).level),
                                        manifest->nodeBounds(index),
                                        input.viewProjection,
                                        input.absP11,
                                        input.viewportHeightPx);
    };

    const double refineThreshold = input.screenSpaceErrorPx * input.sseInflation;

    std::priority_queue<SelectedNode, std::vector<SelectedNode>, CoarsestFirst> heap;
    heap.push({kRootIndex, projectedSpacing(kRootIndex)});

    while(!heap.empty() && selected.size() < input.maxNodes) {
        const SelectedNode current = heap.top();
        heap.pop();

        if(input.frustum != nullptr
           && !input.frustum->intersects(manifest->nodeBounds(current.node))) {
            continue;
        }

        selected.append(current);

        if(current.projectedSpacingPx > refineThreshold) {
            for(int child : manifest->nodes.at(current.node).children) {
                if(child >= 0 && child < manifest->nodes.size()) {
                    heap.push({child, projectedSpacing(child)});
                }
            }
        }
    }

    return selected;
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
        return left < right;
    });

    qint64 reclaimed = 0;
    for(int index : candidates) {
        plan.append(index);
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

    if(input.overBudgetWithNothingEvictable) {
        return std::min(current * kSseInflationStep, kMaxSseInflation);
    }

    if(input.underBudgetByMargin) {
        return std::max(current / kSseInflationStep, 1.0);
    }

    return current;
}

}
