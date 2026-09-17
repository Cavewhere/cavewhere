/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPointCloudForest.h"
#include "cwPointOctreeManifest.h"
#include "cwRenderBudgets.h"
#include "cwRenderMemoryLedger.h"

//Std includes
#include <algorithm>
#include <cmath>

void cwPointCloudForest::setTree(const cwRHIObject* cloud,
                                 const cwPointOctreeManifest* manifest)
{
    if (manifest == nullptr) {
        removeTree(cloud);
        return;
    }

    const int index = indexOfCloud(cloud);
    if (index >= 0) {
        //The cloud is already cut from this tree, so the frame keeps its cut
        if (m_trees.at(index).manifest == manifest) {
            return;
        }
        m_trees[index].manifest = manifest;
    } else {
        m_trees.append(Tree{cloud, manifest, cw::octree::Selection{}});
    }

    forgetLastCut();
}

void cwPointCloudForest::removeTree(const cwRHIObject* cloud)
{
    const int index = indexOfCloud(cloud);
    if (index < 0) {
        return;
    }

    m_trees.remove(index);
    forgetLastCut();

    //The last cloud of the view is gone, so whatever brings one back starts
    //where a first frame starts rather than at the inflation the view left
    if (m_trees.isEmpty()) {
        m_sseInflation = 1.0;
        m_relaxedSteps = 1;
        m_relaxProbeFrame = 0;
        m_availableBytes = 0;
    }
}

int cwPointCloudForest::indexOfCloud(const cwRHIObject* cloud) const
{
    for (int i = 0; i < m_trees.size(); i++) {
        if (m_trees.at(i).cloud == cloud) {
            return i;
        }
    }
    return -1;
}

void cwPointCloudForest::forgetLastCut()
{
    //The cut names trees by position, and the manifests the probe would
    //re-select from may be the ones going away
    m_selected = cw::octree::ForestSelection{};
    m_lastInput = cw::octree::ForestInput{};
    m_desiredBytesRelaxed = -1;
    m_pointCappedRelaxed = false;

    for (Tree& tree : m_trees) {
        tree.selection = cw::octree::Selection{};
    }
}

bool cwPointCloudForest::anyNodeSelected() const
{
    for (const cw::octree::Selection& selection : m_selected.trees) {
        if (!selection.nodes.isEmpty()) {
            return true;
        }
    }
    return false;
}

void cwPointCloudForest::advance(const cwRenderBudgets& budgets)
{
    const auto* ledger = cwRenderMemoryLedger::instance();
    const qint64 total = ledger->totalBytes(cwRenderMemoryLedger::Residency::Gpu);

    //What is left for the clouds: everything that is not point cloud geometry
    //comes off the top by what it holds. The clouds share the rest, which is
    //why no other cloud's demand comes off it any more.
    const qint64 cloudGeometryBytes =
        ledger->bytes(cwRenderMemoryLedger::Category::PointCloudGeometry,
                      cwRenderMemoryLedger::Residency::Gpu);
    const qint64 availableBytes =
        std::max<qint64>(0, budgets.gpuBudgetBytes - (total - cloudGeometryBytes));

    //No cloud drew, so there is no cut to measure. The governor holds where the
    //last drawn frame left it rather than relaxing on the bytes of a cut nobody
    //selected, which would hand the view back the whole unrelaxed cut in the
    //first frame a cloud returns.
    if (!anyNodeSelected()) {
        return;
    }

    cw::octree::InflationInput inflation;
    inflation.current = m_sseInflation;
    inflation.desiredBytes = m_selected.bytes;
    inflation.desiredBytesRelaxed = m_desiredBytesRelaxed;
    inflation.availableBytes = availableBytes;
    inflation.pointCapped = m_selected.pointCapped;
    inflation.pointCappedRelaxed = m_pointCappedRelaxed;
    inflation.relaxedSteps = m_relaxedSteps;
    m_sseInflation = cw::octree::nextSseInflation(inflation);

    //What the next probe measures its relaxations against
    m_availableBytes = availableBytes;

    probeRelaxedCut();
}

void cwPointCloudForest::probeRelaxedCut()
{
    m_desiredBytesRelaxed = -1;
    m_pointCappedRelaxed = false;
    m_relaxedSteps = 1;

    if (m_sseInflation <= 1.0) {
        m_relaxProbeFrame = 0;
        return;
    }

    m_relaxProbeFrame++;
    if (m_relaxProbeFrame < cw::octree::kSseRelaxProbeFrames) {
        return;
    }
    m_relaxProbeFrame = 0;

    if (m_lastInput.trees.isEmpty()) {
        return;
    }

    // The step down is only taken on evidence: the cut one step finer, selected
    // for real rather than estimated, and only on a probe frame because
    // selection is not free. A probe keeps going while the finer cut still fits
    // what the forest had last frame, so a deep inflation comes back in one
    // probe rather than one probe per step. nextSseInflation() checks the
    // deepest level again against this frame's bytes before taking it.
    cw::octree::ForestInput relaxed = m_lastInput;
    double level = m_sseInflation;
    int steps = 0;

    while (level > 1.0) {
        level = std::max(1.0, level / cw::octree::kSseInflationStep);
        relaxed.sseInflation = level;

        const cw::octree::ForestSelection probe = cw::octree::selectForest(relaxed);
        steps++;

        const bool fits = !probe.pointCapped
                          && double(probe.bytes)
                                 < double(m_availableBytes) * (1.0 - cw::octree::kSseRelaxMargin);

        if (!fits) {
            //The first level probed is the one the governor rules on when none fit
            if (steps == 1) {
                m_desiredBytesRelaxed = probe.bytes;
                m_pointCappedRelaxed = probe.pointCapped;
            }
            return;
        }

        m_desiredBytesRelaxed = probe.bytes;
        m_pointCappedRelaxed = false;
        m_relaxedSteps = steps;
    }
}

void cwPointCloudForest::select(const cwRHIObject::RenderData& renderData,
                                const cwFrustum& frustum,
                                const std::function<bool(const cwRHIObject*)>& draws)
{
    m_lastFrustum = frustum;

    cw::octree::ForestInput input;
    input.frustum = &m_lastFrustum;
    input.viewProjection = renderData.viewProjectionMatrix;
    input.absP11 = std::abs(double(renderData.projectionMatrix(1, 1)));
    input.viewportHeightPx = renderData.viewportSize.height();
    input.screenSpaceErrorPx = renderData.budgets.screenSpaceErrorPx;
    input.sseInflation = m_sseInflation;
    input.maxPoints = renderData.budgets.pointBudget;

    //The trees of the cut, in registration order, and where each one's slice
    //goes back to
    QVector<int> cutTrees;
    cutTrees.reserve(m_trees.size());

    for (int i = 0; i < m_trees.size(); i++) {
        Tree& tree = m_trees[i];
        tree.selection = cw::octree::Selection{};

        if (!draws(tree.cloud)) {
            continue;
        }

        cutTrees.append(i);
        input.trees.append(cw::octree::ForestTree{tree.manifest});
    }

    m_selected = cw::octree::selectForest(input);
    m_lastInput = input;

    for (int i = 0; i < cutTrees.size(); i++) {
        m_trees[cutTrees.at(i)].selection = m_selected.trees.at(i);
    }
}

const cw::octree::Selection& cwPointCloudForest::selectionFor(const cwRHIObject* cloud) const
{
    static const cw::octree::Selection kNothingSelected;

    const int index = indexOfCloud(cloud);
    return index >= 0 ? m_trees.at(index).selection : kNothingSelected;
}
