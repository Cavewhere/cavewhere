/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwRHIPointCloud.h"
#include "cwAppearanceOverride.h"
#include "cwFrustum.h"
#include "cwPointCloudAppearance.h"
#include "cwPointCloudClod.h"
#include "cwPointOctree.h"
#include "cwProfileLog.h"
#include "cwRenderFrameStats.h"
#include "cwRenderMaterialState.h"
#include "cwRenderPointCloud.h"
#include "cwRhiItemRenderer.h"
#include "cwRhiFrameRenderer.h"
#include "cwScene.h"
#include "cwTextureResidency.h"

// Qt includes
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QSet>

// Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace {

    // One node's per-instance constants, as PointCloud.vert reads them: the
    // dequantization (nodeOriginScale), the floor (nodeFloor), the two ids
    // the debug views color by and the fade-in counts from (nodeIds), the
    // spacing the builder measured from the node's own points
    // (nodeMeasuredSpacing), where the floor's reveal ramp starts from
    // (nodePreviousFloor, nodeFloorChangedFrame), and the frame a node's fade
    // out of the cut or back into it started on, with which one it is
    // (nodeFade).
    struct NodeConstants {
        std::array<float, 4> originScale;
        float floorSpacing = 0.0f;
        quint32 arrivalFrame = 0;
        quint32 nodeIndex = 0;
        float measuredSpacing = 0.0f;
        float previousFloorSpacing = 0.0f;
        quint32 floorChangedFrame = 0;
        quint32 fadeFrame = 0;
        quint32 fade = 0;
    };

    // The two words of NodeConstants a floor move rewrites beside the floor
    struct FloorRampStart {
        float previousFloorSpacing = 0.0f;
        quint32 floorChangedFrame = 0;
    };

    // The two words of NodeConstants a fade's start or end rewrites
    struct FadeWords {
        quint32 fadeFrame = 0;
        quint32 fade = 0;
    };

    constexpr quint32 kNodeConstantsBytes = sizeof(NodeConstants);
    static_assert(kNodeConstantsBytes == 48, "Three float4s per node slot");

    // Slots in the shared per-instance constants buffer, at 48 bytes each
    // (1.5 MB in all). A view selects at most cw::octree::kMaxDesiredNodes nodes
    // per frame, so at least half the slots are unselected in any frame and an
    // incoming node can always find one to take. Deep enough that the byte budget
    // binds first at every budget the settings allow; the eviction fallback below
    // is what a cloud of tiny nodes still leans on.
    constexpr int kMaxResidentNodes = 32768;
    static_assert(kMaxResidentNodes >= 2 * cw::octree::kMaxDesiredNodes,
                  "An upload must be able to evict an unselected node for its "
                  "constants slot, which needs more slots than one frame's cut.");

    // How far the cold queue may outgrow residency before the stale entries a
    // node's return to the cut left behind are swept out. Compaction is linear
    // and pays for itself over the pushes that triggered it.
    constexpr int kColdQueueSlackFactor = 4;
    constexpr int kColdQueueSlackMinimum = 1024;

    constexpr int kPointsBinding = 0;
    constexpr int kNodeConstantsBinding = 1;

    //One instance per node, so the constants advance once per draw
    constexpr quint32 kInstanceStepRate = 1;
    constexpr int kRootIndex = 0;
    constexpr int kRootLevel = 0;
    constexpr qint64 kBytesPerMegabyte = 1024 * 1024;

    constexpr quint32 kNodeFloorOffset = offsetof(NodeConstants, floorSpacing);
    constexpr quint32 kNodeIdsOffset = offsetof(NodeConstants, arrivalFrame);
    constexpr quint32 kNodeMeasuredSpacingOffset = offsetof(NodeConstants, measuredSpacing);
    constexpr quint32 kNodeFloorRampOffset = offsetof(NodeConstants, previousFloorSpacing);
    constexpr quint32 kNodeFloorChangedFrameOffset = offsetof(NodeConstants, floorChangedFrame);
    constexpr quint32 kNodeFadeOffset = offsetof(NodeConstants, fadeFrame);
    static_assert(offsetof(NodeConstants, fade) == kNodeFadeOffset + sizeof(quint32),
                  "nodeFade reads the fade frame and the fade as one uvec2");
    static_assert(kNodeFloorChangedFrameOffset == kNodeFloorRampOffset + sizeof(float),
                  "A floor ramp's start is written as one pair of words");
    static_assert(offsetof(NodeConstants, nodeIndex) == kNodeIdsOffset + sizeof(quint32),
                  "nodeIds reads the arrival frame and the node index as one uvec2");

    // The node box overlay. A box is a unit cube scaled to the node's cube: 12
    // edges, two ends each, every end carrying how far along its edge it sits
    // so a loading node's box can be dashed.
    constexpr int kCubeEdgeCount = 12;
    constexpr int kNodeBoxVertexCount = 2 * kCubeEdgeCount;
    constexpr int kNodeBoxCornerBinding = 0;
    constexpr int kNodeBoxInstanceBinding = 1;

    struct NodeBoxCorner {
        float x;
        float y;
        float z;
        float alongEdge;
    };

    constexpr std::array<NodeBoxCorner, kNodeBoxVertexCount> kNodeBoxCorners = {{
        // the four edges along x
        {0, 0, 0, 0}, {1, 0, 0, 1}, {0, 1, 0, 0}, {1, 1, 0, 1},
        {0, 0, 1, 0}, {1, 0, 1, 1}, {0, 1, 1, 0}, {1, 1, 1, 1},
        // along y
        {0, 0, 0, 0}, {0, 1, 0, 1}, {1, 0, 0, 0}, {1, 1, 0, 1},
        {0, 0, 1, 0}, {0, 1, 1, 1}, {1, 0, 1, 0}, {1, 1, 1, 1},
        // along z
        {0, 0, 0, 0}, {0, 0, 1, 1}, {1, 0, 0, 0}, {1, 0, 1, 1},
        {0, 1, 0, 0}, {0, 1, 1, 1}, {1, 1, 0, 0}, {1, 1, 1, 1}
    }};

    // One box: the node cube's min corner and side, then its color, with the
    // alpha slot saying whether NodeBox.frag dashes it.
    struct NodeBoxInstance {
        std::array<float, 4> originSize;
        std::array<float, 4> color;
    };

    constexpr float kSolidBox = 0.0f;
    constexpr float kDashedBox = 1.0f;

    constexpr int kNodeBoxGrowthFactor = 2;

    constexpr quint8 kVertexStageMask = 0x1;
    constexpr quint8 kUnusedBinding = 0xFF;

    using cw::profile::elapsedUs;
}

cwRHIPointCloud::cwRHIPointCloud(std::shared_ptr<cwPointOctreePickSet> pickSet) :
    m_streamer(&cwRHIPointCloud::loadNode,
               [](const cwPointOctreeNodeSource& source, int) {
                   return source.byteSize
                          + cwPointOctreePickIndex::estimatedBytes(source.byteSize);
               },
               cwRenderMemoryLedger::Category::PointCloudGeometry),
    m_pickSet(std::move(pickSet)),
    m_maxResidentNodes(kMaxResidentNodes)
{
}

cwRHIPointCloud::~cwRHIPointCloud()
{
    // The only place cancelAll() is allowed: it blocks until the loads in
    // flight finish, and nothing may land on a table that is going away.
    m_streamer.cancelAll();

    // Nothing draws once this object is gone, so nothing may be picked either
    // — and the set would otherwise keep an implicit share of every mirror.
    m_pickSet->publish({}, QBox3D(), 0.0f);

    for (const NodeRecord& node : std::as_const(m_nodes)) {
        delete node.buffer;
    }
    delete m_nodeConstants;
    delete m_perCloudUBO;
    delete m_srb;
    delete m_nodeBoxCube;
    delete m_nodeBoxInstances;
    delete m_nodeBoxSrb;
    // Resources orphaned by a pool growth are owned by the cwAppearanceSlotted
    // base and freed in its destructor (disjoint from m_perCloudUBO/m_srb above).
    // m_pipelines releases its held pipeline references on destruction.
}

void cwRHIPointCloud::initialize(const ResourceUpdateData& data)
{
    if (m_resourcesInitialized) {
        return;
    }

    initializeResources(data);
    m_resourcesInitialized = true;
}

void cwRHIPointCloud::initializeResources(const ResourceUpdateData& data)
{
    auto* rhi = data.renderData.cb->rhi();

    m_nodeConstants = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                     kNodeConstantsBytes * quint32(m_maxResidentNodes));
    m_nodeConstants->create();

    // Popped from the back, so the first node takes slot 0.
    m_freeSlots.reserve(m_maxResidentNodes);
    for (int slot = m_maxResidentNodes - 1; slot >= 0; slot--) {
        m_freeSlots.append(slot);
    }

    // One layout for every point cloud: a node's quantized points step per
    // vertex, its dequantization constants per instance, and the whole node
    // draws as a single instance.
    m_inputLayout.setBindings({
        { cw::octree::kBytesPerPoint, QRhiVertexInputBinding::PerVertex },
        { kNodeConstantsBytes, QRhiVertexInputBinding::PerInstance, kInstanceStepRate }
    });
    m_inputLayout.setAttributes({
        { kPointsBinding, 0, QRhiVertexInputAttribute::UShort4, 0 },        // qpos
        { kNodeConstantsBinding, 1, QRhiVertexInputAttribute::Float4, 0 },  // nodeOriginScale
        { kNodeConstantsBinding, 2, QRhiVertexInputAttribute::Float,
          kNodeFloorOffset },                                               // nodeFloor
        { kNodeConstantsBinding, 3, QRhiVertexInputAttribute::UInt2,
          kNodeIdsOffset },                                                 // nodeIds
        { kNodeConstantsBinding, 4, QRhiVertexInputAttribute::Float,
          kNodeMeasuredSpacingOffset },                                     // nodeMeasuredSpacing
        { kNodeConstantsBinding, 5, QRhiVertexInputAttribute::Float,
          kNodeFloorRampOffset },                                           // nodePreviousFloor
        { kNodeConstantsBinding, 6, QRhiVertexInputAttribute::UInt,
          kNodeFloorChangedFrameOffset },                                   // nodeFloorChangedFrame
        { kNodeConstantsBinding, 7, QRhiVertexInputAttribute::UInt2,
          kNodeFadeOffset }                                                 // nodeFade
    });

    m_nodeBoxInputLayout.setBindings({
        { quint32(sizeof(NodeBoxCorner)), QRhiVertexInputBinding::PerVertex },
        { quint32(sizeof(NodeBoxInstance)), QRhiVertexInputBinding::PerInstance,
          kInstanceStepRate }
    });
    m_nodeBoxInputLayout.setAttributes({
        { kNodeBoxCornerBinding, 0, QRhiVertexInputAttribute::Float4, 0 },  // corner
        { kNodeBoxInstanceBinding, 1, QRhiVertexInputAttribute::Float4,
          quint32(offsetof(NodeBoxInstance, originSize)) },                 // boxOriginSize
        { kNodeBoxInstanceBinding, 2, QRhiVertexInputAttribute::Float4,
          quint32(offsetof(NodeBoxInstance, color)) }                       // boxColor
    });
}

void cwRHIPointCloud::synchronize(const SynchronizeData& data)
{
    Q_ASSERT(dynamic_cast<cwRenderPointCloud*>(data.object) != nullptr);
    auto* pointCloud = static_cast<cwRenderPointCloud*>(data.object);

    const bool sourceChanged = pointCloud->m_source.isChanged();
    const cwPointOctreeSource source = pointCloud->m_source.value();

    m_renderState = pointCloud->m_renderState;
    pointCloud->m_source.resetChanged();
    pointCloud->m_renderState.resetChanged();

    if (!sourceChanged) {
        return;
    }

    resetNodes(source);
}

void cwRHIPointCloud::resetNodes(const cwPointOctreeSource& source)
{
    // Release under the outgoing source: m_nodes is parallel to its manifest,
    // and the empty pick set that release publishes has to describe the cloud
    // being dropped, not the one arriving.
    releaseStreamedResources();

    m_source = source;
    m_nodeKeyBase = cw::octree::nodeKeyBase(m_source.lazPath, m_source.fingerprint);

    m_nodes.clear();
    m_residentIndices.clear();
    m_residentCount = 0;
    m_coldNodes.clear();
    m_liveAppearanceStale = true;
    if (m_source.manifest) {
        m_nodes.resize(m_source.manifest->nodes.size());
    }

    // The new octree's root bounds are what frames a reset view, so they are
    // published before a single node has landed.
    m_residencyChanged = true;
    publishPickSet();
}

void cwRHIPointCloud::releaseNode(int index)
{
    NodeRecord& node = m_nodes[index];

    // Buffers are deleted here, from synchronize() and from streamResources()
    // — every one of them runs before gather() builds this frame's drawables,
    // so no drawable ever names a buffer that has been freed.
    delete node.buffer;
    node.buffer = nullptr;

    if (node.constantSlot >= 0) {
        m_freeSlots.append(node.constantSlot);
        node.constantSlot = -1;
    }

    // Swap-remove out of the resident list: the node at the back takes this
    // node's place and is told where it moved to.
    if (node.residentPosition >= 0) {
        const int position = node.residentPosition;
        const int moved = m_residentIndices.last();
        m_residentIndices[position] = moved;
        m_nodes[moved].residentPosition = position;
        m_residentIndices.removeLast();
        node.residentPosition = -1;
        m_residentCount = int(m_residentIndices.size());
    }

    m_gpuBytes.setBytes(m_gpuBytes.bytes() - node.bytes.size());
    m_mirrorBytes.setBytes(m_mirrorBytes.bytes() - node.bytes.size() - node.index.byteSize());
    node.bytes = QByteArray();
    node.index = cwPointOctreePickIndex();

    node.state = NodeState::Absent;
    node.exportRequested = false;
    node.floorSpacing = 0.0f;
    node.previousFloorSpacing = 0.0f;
    node.fade = NodeFade::None;
    m_residencyChanged = true;
}

bool cwRHIPointCloud::isRequested(int index) const
{
    return index >= 0 && index < m_nodes.size()
           && m_nodes.at(index).state == NodeState::Requested;
}

void cwRHIPointCloud::requestNode(int index, quint64 priority)
{
    m_streamer.request(quint32(index), nodeSource(index), 0, priority);
    m_nodes[index].state = NodeState::Requested;
    if (!m_requested.contains(index)) {
        m_requested.append(index);
    }

    if (m_profileEnabled) {
        m_profile.requests++;
    }
}

cwRHIPointCloud::cwPointOctreeNodeSource cwRHIPointCloud::nodeSource(int index) const
{
    // nodeName() memoizes a parent table; only the render thread ever asks a
    // published manifest for a name, so that memo has one writer.
    return cwPointOctreeNodeSource {
        m_source.cacheRootPath(),
        cw::octree::nodeKey(m_nodeKeyBase, m_source.manifest->nodeName(index)),
        m_source.manifest->nodes.at(index).byteSize
    };
}

cw::octree::SelectionInput cwRHIPointCloud::selectionInput(const RenderData& renderData,
                                                           const cwFrustum* frustum) const
{
    cw::octree::SelectionInput input;
    input.manifest = m_source.manifest.get();
    input.frustum = frustum;
    input.viewProjection = renderData.viewProjectionMatrix;
    input.absP11 = std::abs(double(renderData.projectionMatrix(1, 1)));
    input.viewportHeightPx = renderData.viewportSize.height();
    input.screenSpaceErrorPx = renderData.budgets.screenSpaceErrorPx;
    input.sseInflation = forest().sseInflation();
    input.maxLevel = renderData.budgets.maxLevel;
    return input;
}

void cwRHIPointCloud::updateResources(const ResourceUpdateData& data)
{
    refreshSseThreshold(data.renderData.budgets);

    if (!m_renderState.isChanged() && !m_liveAppearanceStale && m_perCloudUBO) {
        return;
    }

    auto* rhi = data.renderData.cb->rhi();
    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

    // Per-cloud uniform — the fraction of its own cell a sprite covers, and
    // the refine threshold the shader measures that fraction in. A fixed
    // default (set on cwRenderPointCloud::RenderState) produces consistent
    // sprite sizes across every loaded cloud; the earlier meanSpacingXY * 0.5
    // auto-derivation was unreliable because mean-spacing estimates vary with
    // LAZ density / sampling. The live coverage is overridden via
    // cwLazLayersSceneNode::setSpacingCoverage (P+wheel gesture in the 3D
    // view).
    //
    // Steady state is ONE slot (slot 0 = the live coverage). Per-job appearance
    // overrides acquire transient slots that grow the buffer on demand
    // (cwAppearanceSlotted), so an interactive session pays 0.75 KB/cloud instead
    // of the full kAppearanceSlotCount. resizeAppearanceSlots builds the buffer and
    // writes slot 0; a later coverage change re-writes only slot 0.
    if (!m_perCloudUBO) {
        resizeAppearanceSlots(rhi, batch, 1);
    } else {
        writeLiveAppearanceSlot(batch);
    }

    m_renderState.resetChanged();
}

void cwRHIPointCloud::writeAppearanceSlot(QRhiResourceUpdateBatch* batch, int slot,
                                          float spacingCoverage)
{
    PerCloudUniform uniform = appearanceUniform(spacingCoverage);

    // An export job draws its own camera once, so it draws every node at its
    // settled weight and floor rather than wherever the live view's ramps
    // stand: revealFrames 0 has the shader skip the fades and floor ramps
    if (slot != kLiveAppearanceSlot) {
        uniform.revealFrames = cw::pointcloud::kMinRevealFrames;
    }
    batch->updateDynamicBuffer(m_perCloudUBO, slot * m_perCloudStride,
                               sizeof(PerCloudUniform), &uniform);
}

void cwRHIPointCloud::writeLiveAppearanceSlot(QRhiResourceUpdateBatch* batch)
{
    const cwRenderPointCloud::RenderState& live = m_renderState.value();
    writeAppearanceSlot(batch, kLiveAppearanceSlot, live.spacingCoverage);
    m_liveAppearanceStale = false;
}

void cwRHIPointCloud::writeNodeFloors(QRhiResourceUpdateBatch* batch,
                                      const QVector<int>& drawnNodes,
                                      const QVector<int>& missingNodes, quint64 revealClock)
{
    const cwPointCloudShading& shading = m_renderState.value().shading;
    if (shading.revealMode == cw::pointcloud::RevealMode::Grow) {
        // Floored at the cut the forest asked for, so the sprites keep their
        // target size while children stream: the governor refines a node while
        // its spacing projects wider than sseThresholdPx, so a requested
        // floor projects at or under spacingCoverage * sseThresholdPx and the
        // missing children show as holes between the ancestor's sparse points.
        // Once every requested node is resident this is the drawn cut, so a
        // settled frame matches Cover's. What still moves the floors (the cut
        // following the camera, the governor's inflation) ramps as under Cover.
        // A failed node is not on its way, so its ancestor covers its cell.
        QVector<int> streamingNodes;
        streamingNodes.reserve(missingNodes.size());
        for (const int index : missingNodes) {
            if (m_nodes.at(index).state != NodeState::Failed) {
                streamingNodes.append(index);
            }
        }
        const QVector<double> floors = cw::octree::requestedCutSpacings(
            *m_source.manifest, drawnNodes, streamingNodes, shading.spacingSource);
        for (int i = 0; i < drawnNodes.size(); i++) {
            writeNodeFloor(batch, drawnNodes.at(i), float(floors.at(i)), revealClock);
        }
        return;
    }

    const QVector<double> floors =
        cw::octree::finestDrawnSpacings(*m_source.manifest, drawnNodes, missingNodes,
                                        shading.spacingSource);

    // A missing node's cell has only its nearest drawn ancestor's points in
    // it, so that ancestor's floor has to cover the cell from the first frame
    // or the reveal ramp would open a hole there for revealFrames frames
    const QSet<int> incomplete = shading.revealFrames > 0
                                     ? cw::octree::incompleteDrawnNodes(*m_source.manifest,
                                                                        drawnNodes, missingNodes)
                                     : QSet<int>();

    for (int i = 0; i < drawnNodes.size(); i++) {
        const int node = drawnNodes.at(i);
        const float floor = float(floors.at(i));
        const bool coversMissingCell = incomplete.contains(node)
                                       && floor > m_nodes.at(node).floorSpacing;
        writeNodeFloor(batch, node, floor, revealClock,
                       coversMissingCell ? FloorMove::AtOnce : FloorMove::Ramped);
    }
}

void cwRHIPointCloud::restoreOwnFloorsLeavingCut(QRhiResourceUpdateBatch* batch,
                                                 const cw::octree::Selection& previous,
                                                 quint64 frame, quint64 revealClock)
{
    for (const cw::octree::SelectedNode& selected : previous.nodes) {
        if (selected.node < 0 || selected.node >= m_nodes.size()) {
            continue;
        }

        const NodeRecord& node = m_nodes.at(selected.node);
        if (node.state != NodeState::Resident || node.lastDesiredFrame == frame
            || node.fade == NodeFade::Out) {
            continue;
        }

        writeNodeFloor(batch, selected.node, ownSpacing(selected.node), revealClock);
    }
}

float cwRHIPointCloud::ownSpacing(int index) const
{
    const cwPointOctreeManifest& manifest = *m_source.manifest;
    const cwPointOctreeNode& node = manifest.nodes.at(index);

    // A leaf under resident ancestors is, with them, every point of its cell.
    // Resident stands in for drawn here: this floor is the one a node takes
    // on upload and on leaving the cut, before or without a selection that
    // says which ancestors draw. A live frame's writeNodeFloors() overwrites
    // it from the selection on the same batch; a job with no live gather,
    // such as an export, keeps it, so there a resident ancestor outside the
    // cut still lets its leaf draw at native spacing alone.
    const auto holdsRawData = [&]() {
        if (!node.isLeaf()) {
            return false;
        }
        const QVector<int>& parents = manifest.parents();
        for (int ancestor = parents.at(index); ancestor >= 0; ancestor = parents.at(ancestor)) {
            if (m_nodes.at(ancestor).state != NodeState::Resident) {
                return false;
            }
        }
        return true;
    };

    return float(cw::clod::ownFloorSpacing(manifest.spacing(node.level),
                                           double(node.measuredSpacing),
                                           m_renderState.value().shading.spacingSource,
                                           holdsRawData()));
}

void cwRHIPointCloud::writeNodeFloor(QRhiResourceUpdateBatch* batch, int index,
                                     float floorSpacing, quint64 revealClock, FloorMove move)
{
    // Only resident nodes reach here, so the node holds the slot the floor
    // goes into
    NodeRecord& node = m_nodes[index];
    if (floorSpacing == node.floorSpacing) {
        return;
    }

    // The ramp starts from the floor the node drew with last frame, so a move
    // that interrupts a ramp carries on from where that one had got to. A
    // node arriving this frame has drawn nothing yet, and its fade-in carries
    // the change, so it takes its first floor at once.
    if (node.arrivalFrame == revealClock || move == FloorMove::AtOnce) {
        node.previousFloorSpacing = 0.0f;
    } else if (node.floorChangedFrame < revealClock) {
        const quint32 lastDrawnAge = quint32(revealClock - node.floorChangedFrame - 1);
        node.previousFloorSpacing = float(cw::clod::revealedFloor(
            double(node.previousFloorSpacing), double(node.floorSpacing), lastDrawnAge,
            m_renderState.value().shading.revealFrames));
    }
    node.floorSpacing = floorSpacing;
    node.floorChangedFrame = revealClock;

    const quint32 slotOffset = quint32(node.constantSlot) * kNodeConstantsBytes;
    batch->updateDynamicBuffer(m_nodeConstants, slotOffset + kNodeFloorOffset,
                               sizeof(float), &node.floorSpacing);

    const FloorRampStart rampStart{node.previousFloorSpacing, quint32(revealClock)};
    batch->updateDynamicBuffer(m_nodeConstants, slotOffset + kNodeFloorRampOffset,
                               sizeof(FloorRampStart), &rampStart);
}

bool cwRHIPointCloud::nodeRevealing(const NodeRecord& node, quint64 revealClock) const
{
    const int revealFrames = m_renderState.value().shading.revealFrames;
    const auto rampingSince = [&](quint64 since) {
        return cw::clod::revealing(cw::clod::revealAge(revealClock, since), revealFrames);
    };

    const bool arriving = rampingSince(node.arrivalFrame);
    const bool floorMoving = node.previousFloorSpacing > 0.0f
                             && node.previousFloorSpacing != node.floorSpacing
                             && rampingSince(node.floorChangedFrame);
    const bool fading = node.fade != NodeFade::None && rampingSince(node.fadeFrame);
    return arriving || floorMoving || fading;
}

void cwRHIPointCloud::writeFade(QRhiResourceUpdateBatch* batch, int index, NodeFade fade,
                                quint64 fadeFrame)
{
    NodeRecord& node = m_nodes[index];
    node.fade = fade;
    node.fadeFrame = fadeFrame;

    const FadeWords words{quint32(fadeFrame), quint32(fade)};
    batch->updateDynamicBuffer(m_nodeConstants,
                               quint32(node.constantSlot) * kNodeConstantsBytes + kNodeFadeOffset,
                               sizeof(FadeWords), &words);
}

double cwRHIPointCloud::fadeShare(const NodeRecord& node, quint64 revealClock) const
{
    const int revealFrames = m_renderState.value().shading.revealFrames;
    const quint32 age = cw::clod::revealAge(revealClock, node.fadeFrame);
    switch (node.fade) {
    case NodeFade::Out:
        return cw::clod::departureScale(age, revealFrames);
    case NodeFade::In:
        return cw::clod::revealScale(age, revealFrames);
    case NodeFade::None:
        break;
    }
    return 1.0;
}

void cwRHIPointCloud::startFade(QRhiResourceUpdateBatch* batch, int index, NodeFade fade,
                                quint64 revealClock)
{
    // The share of its weight the last frame drew the node at: one turning
    // around mid-fade carries on from where its fade had got to, one coming
    // back into the cut otherwise starts from nothing, and one leaving it
    // otherwise starts from all of it
    const NodeRecord& node = m_nodes.at(index);
    const bool turning = node.fade != NodeFade::None && node.fade != fade;
    const double fadedShare = turning ? fadeShare(node, revealClock - 1)
                                      : (fade == NodeFade::Out ? 1.0 : 0.0);

    // Under Grow the share is the sprite's area, so one leaving before its
    // arrival finished growing shrinks from the area it had reached
    const cwPointCloudShading& shading = m_renderState.value().shading;
    const int revealFrames = shading.revealFrames;
    const bool leavingGrowing = fade == NodeFade::Out
                                && shading.revealMode == cw::pointcloud::RevealMode::Grow;
    const double share =
        leavingGrowing
            ? std::min(fadedShare,
                       cw::clod::revealScale(cw::clod::revealAge(revealClock - 1, node.arrivalFrame),
                                             revealFrames))
            : fadedShare;

    const quint32 age = fade == NodeFade::In ? cw::clod::fadeInAgeFor(share, revealFrames)
                                             : cw::clod::fadeOutAgeFor(share, revealFrames);
    writeFade(batch, index, fade, revealClock > age ? revealClock - age : 0);
}

void cwRHIPointCloud::closeOverReleasedNode(QRhiResourceUpdateBatch* batch, int index,
                                            quint64 frame)
{
    // Grow lets a hole show rather than snap a floor over it, so a released
    // node's ancestor keeps ramping toward its new floor
    if (m_renderState.value().shading.revealMode == cw::pointcloud::RevealMode::Grow) {
        return;
    }

    const QVector<int>& parents = m_source.manifest->parents();
    for (int ancestor = parents.at(index); ancestor >= 0; ancestor = parents.at(ancestor)) {
        const NodeRecord& covering = m_nodes.at(ancestor);
        if (covering.state != NodeState::Resident || covering.lastDesiredFrame != frame) {
            continue;
        }

        // Only a rising floor: a falling one is shrinking over children of
        // its own that are fading in
        const bool rising = covering.previousFloorSpacing > 0.0f
                            && covering.floorSpacing > covering.previousFloorSpacing;
        if (rising) {
            NodeRecord& node = m_nodes[ancestor];
            node.previousFloorSpacing = 0.0f;
            const FloorRampStart rampStart{0.0f, quint32(node.floorChangedFrame)};
            batch->updateDynamicBuffer(m_nodeConstants,
                                       quint32(node.constantSlot) * kNodeConstantsBytes
                                           + kNodeFloorRampOffset,
                                       sizeof(FloorRampStart), &rampStart);
        }
        return;
    }
}

void cwRHIPointCloud::updateFades(QRhiResourceUpdateBatch* batch,
                                  const cw::octree::Selection& previous,
                                  const QVector<int>& drawnNodes, quint64 frame,
                                  quint64 revealClock)
{
    QSet<int> previousNodes;
    previousNodes.reserve(previous.nodes.size());
    for (const cw::octree::SelectedNode& selected : previous.nodes) {
        previousNodes.insert(selected.node);
    }

    // A node coming back into the cut already resident fades in as an
    // arrival does, so a cached node shows up over the ramp too; one still
    // fading out turns around where it stands
    for (const int index : drawnNodes) {
        if (!previousNodes.contains(index) && m_nodes.at(index).arrivalFrame != revealClock) {
            startFade(batch, index, NodeFade::In, revealClock);
        }
    }

    // The nodes the last frame drew that this cut dropped start fading out,
    // so the cells they covered thin away over the frames their ancestors'
    // floors take to grow over them, the arrival ramp run backwards. One
    // already gone leaves nothing to fade, so its ancestor covers its cells
    // at once.
    for (const cw::octree::SelectedNode& selected : previous.nodes) {
        if (selected.node < 0 || selected.node >= m_nodes.size()) {
            continue;
        }
        const NodeRecord& node = m_nodes.at(selected.node);
        if (node.lastDesiredFrame == frame || node.fade == NodeFade::Out) {
            continue;
        }
        if (node.state == NodeState::Resident) {
            startFade(batch, selected.node, NodeFade::Out, revealClock);
            m_departingNodes.append(selected.node);
        } else {
            closeOverReleasedNode(batch, selected.node, frame);
        }
    }

    // A fade-out ends when its node turned back into the cut, was evicted
    // (its ancestor then covers its cells at once), or has faded to nothing;
    // the node then sizes by its own spacing again, as every resident node
    // outside the cut does
    const int revealFrames = m_renderState.value().shading.revealFrames;
    const auto ended = [&](int index) {
        const NodeRecord& node = m_nodes.at(index);
        if (node.state != NodeState::Resident) {
            closeOverReleasedNode(batch, index, frame);
            return true;
        }
        if (node.fade != NodeFade::Out) {
            return true;
        }
        if (cw::clod::revealing(cw::clod::revealAge(revealClock, node.fadeFrame),
                                revealFrames)) {
            return false;
        }
        finishDeparture(batch, index, revealClock);
        return true;
    };
    m_departingNodes.erase(std::remove_if(m_departingNodes.begin(), m_departingNodes.end(),
                                          ended),
                           m_departingNodes.end());
}

void cwRHIPointCloud::finishDeparture(QRhiResourceUpdateBatch* batch, int index,
                                      quint64 revealClock)
{
    writeFade(batch, index, NodeFade::None, revealClock);
    writeNodeFloor(batch, index, ownSpacing(index), revealClock);
}

void cwRHIPointCloud::endDepartures(QRhiResourceUpdateBatch* batch, quint64 revealClock)
{
    for (const int index : std::as_const(m_departingNodes)) {
        const NodeRecord& node = m_nodes.at(index);
        if (node.state == NodeState::Resident && node.fade == NodeFade::Out) {
            finishDeparture(batch, index, revealClock);
        }
    }
    m_departingNodes.clear();
}

cwRHIPointCloud::PerCloudUniform
cwRHIPointCloud::appearanceUniform(float spacingCoverage) const
{
    // The root has no coarser level to carry the surface once its own points
    // thin away, so it is the widest gap cw::clod ever thins toward.
    const float rootSpacing =
        m_source.manifest ? float(m_source.manifest->spacing(kRootLevel)) : 0.0f;
    const cwPointCloudShading& shading = m_renderState.value().shading;
    PerCloudUniform uniform;
    uniform.spacingCoverage = spacingCoverage;
    uniform.sseThresholdPx = m_sseThresholdPx;
    uniform.rootSpacing = rootSpacing;
    uniform.maxPointSizePx = shading.maxPointSizePx;
    uniform.thinningMode = qint32(shading.thinningMode);
    uniform.sizeMode = qint32(shading.sizeMode);
    uniform.floorMode = qint32(shading.floorMode);
    uniform.spacingSource = qint32(shading.spacingSource);
    uniform.fixedPointSizePx = shading.fixedPointSizePx;
    uniform.debugView = qint32(shading.debugView);
    uniform.revealFrames = qint32(shading.revealFrames);
    uniform.revealMode = qint32(shading.revealMode);
    // Truncated alongside the arrival frames in the node constants, so the
    // unsigned difference the shader takes stays right across the wrap.
    uniform.currentFrame = quint32(currentFrame());
    return uniform;
}

void cwRHIPointCloud::refreshSseThreshold(const cwRenderBudgets& budgets)
{
    // The same threshold selectCut() refines against, so the sprites cover the
    // spacing the cut is aiming for: PointCloud.vert divides it back out by
    // each vertex's own pixels-per-meter, which is how a far sprite grows to
    // cover its own coarser cell while the near one stays where the wheel put
    // it.
    const float threshold = float(cw::octree::refineThresholdPx(budgets.screenSpaceErrorPx,
                                                                forest().sseInflation()));
    if (threshold == m_sseThresholdPx) {
        return;
    }

    m_sseThresholdPx = threshold;
    m_liveAppearanceStale = true;
}

void cwRHIPointCloud::uploadAppearance(QRhiResourceUpdateBatch* batch, int slot,
                                       const cwAppearanceOverride& override)
{
    // Unpack the cloud's own appearance schema. Every field falls back on its
    // own, so a missing payload — or one that overrides only a sibling field —
    // still draws the rest at the live values.
    const auto* appearance = override.payload<cwPointCloudAppearance>();
    const cwPointCloudAppearance requested = appearance ? *appearance
                                                        : cwPointCloudAppearance();
    const cwRenderPointCloud::RenderState& live = m_renderState.value();
    writeAppearanceSlot(batch, slot,
                        requested.spacingCoverage.value_or(live.spacingCoverage));
}

void cwRHIPointCloud::resizeAppearanceSlots(QRhi* rhi, QRhiResourceUpdateBatch* batch,
                                            int slotCount)
{
    if (m_perCloudStride == 0) {
        m_perCloudStride = rhi->ubufAligned(sizeof(PerCloudUniform));
    }

    // Retire the old buffer + SRB rather than freeing them in place: a draw
    // recorded earlier this frame (the live cloud) still binds them and is read at
    // submit. The base frees them next frame (flushRetiredAppearanceResources),
    // after this frame is submitted. Dropping the SRB forces ensureShaderResources
    // to rebuild it against the new buffer on the next gather.
    retireAppearanceResource(m_perCloudUBO);
    retireAppearanceResource(m_srb);
    m_srb = nullptr;

    m_perCloudUBO = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                   m_perCloudStride * quint32(slotCount));
    m_perCloudUBO->create();
    m_appearanceSlotCapacity = slotCount;

    // Carry the live appearance into the fresh buffer's slot 0 so a non-overriding
    // draw (the live view, or an offscreen job with no override for this cloud)
    // reads the right radius from it.
    writeLiveAppearanceSlot(batch);
}

bool cwRHIPointCloud::streamResources(ResourceUpdateData& data, qint64& remainingUploadBytes)
{
    m_profileEnabled = lcProfileRender().isDebugEnabled();
    QElapsedTimer streamTimer;
    if (m_profileEnabled) {
        streamTimer.start();
    }

    m_streamer.setMaxPendingCpuBytes(data.renderData.budgets.cpuBudgetBytes);

    if (m_source.isNull()) {
        return false;
    }

    // The cut of the frame about to gather spans this cloud's tree. Releasing
    // takes the tree out so the governor settles while the view is hidden, and
    // this is where a view that came back puts it in again.
    if (m_frame) {
        m_frame->pointCloudForest().setTree(this, m_source.manifest.get());
    }

    auto* rhi = data.renderData.cb->rhi();
    if (!rhi || !m_nodeConstants) {
        return m_streamer.hasWork();
    }

    QElapsedTimer uploadTimer;
    if (m_profileEnabled) {
        uploadTimer.start();
    }

    // Only while nothing is waiting on the upload budget. A payload the streamer
    // has handed over is off its CPU cap and counts against no ledger until it
    // is uploaded, so draining faster than the budget uploads would let the
    // queue grow to the whole cut. Held back, the streamer keeps them inside
    // setMaxPendingCpuBytes and m_readyQueue never holds more than one batch.
    if (m_readyQueue.isEmpty()) {
        const QVector<NodeStreamer::Result> ready = m_streamer.takeReady();
        for (const NodeStreamer::Result& result : ready) {
            const int index = int(result.itemId);
            if (!isRequested(index)) {
                // The cut moved on while the load ran: the payload dies with
                // the local vector.
                continue;
            }

            if (!result.error.isEmpty()) {
                qWarning() << "Point octree node" << m_source.manifest->nodeName(index)
                           << "of" << m_source.lazPath << "failed to load:" << result.error;
                m_nodes[index].state = NodeState::Failed;
                m_nodes[index].exportRequested = false;
                continue;
            }

            m_readyQueue.append(result);
        }
    }

    bool uploadedThisFrame = false;
    m_waitingForSlot = false;
    QVector<NodeStreamer::Result> deferred;
    for (const NodeStreamer::Result& result : std::as_const(m_readyQueue)) {
        const int index = int(result.itemId);
        if (!isRequested(index)) {
            continue;
        }

        if (data.renderData.budgets.evictionHold && m_freeSlots.isEmpty()) {
            // Held with the slot pool full: the node stays Requested and its
            // payload waits here, so it is decoded once rather than dropped and
            // asked for again every frame.
            deferred.append(result);
            m_waitingForSlot = true;
            continue;
        }

        if (!cw::residency::takeFromBudget(remainingUploadBytes, result.payload.bytes.size(),
                                           uploadedThisFrame)) {
            // The streamer already handed this over, so it waits here for the
            // next frame rather than being asked for a second time.
            deferred.append(result);
            continue;
        }

        if (uploadNode(rhi, data.resourceUpdateBatch, index, result.payload,
                       data.renderData.budgets.evictionHold)) {
            uploadedThisFrame = true;
        }
    }
    m_readyQueue = deferred;

    QElapsedTimer partTimer;
    if (m_profileEnabled) {
        m_profile.upload.add(elapsedUs(uploadTimer));
        partTimer.start();
    }
    enforceGpuBudget(data.renderData.budgets);
    if (m_profileEnabled) {
        m_profile.enforceBudget.add(elapsedUs(partTimer));
        m_profile.gpuBudgetBytes = data.renderData.budgets.gpuBudgetBytes;
        partTimer.restart();
    }

    // The forest steps the inflation at the top of the frame, before any cloud
    // streams, so this reads the same threshold the cut gather() picks a moment
    // from now refines to. Sizing this frame's sprites against a stale
    // threshold would flash fat sprites (or holes) on every step the governor
    // takes. The batch is still submitted after gather(), so the write lands in
    // time for this frame's draws.
    refreshSseThreshold(data.renderData.budgets);

    // The residency age view and the reveal ramp count from the current
    // frame, so their uniform moves every frame.
    const cwPointCloudShading& shading = m_renderState.value().shading;
    if (shading.debugView == cw::pointcloud::DebugView::ResidencyAge
        || shading.revealFrames > 0) {
        m_liveAppearanceStale = true;
    }

    // updateResources() runs only on the frames the render object changed, so
    // this is where a refine threshold the view or the inflation governor moved
    // on its own reaches the shader — one write per change, none on the frames
    // between.
    if (m_liveAppearanceStale && m_perCloudUBO) {
        writeLiveAppearanceSlot(data.resourceUpdateBatch);
    }

    writeNodeBoxes(rhi, data.resourceUpdateBatch);

    publishPickSet();
    if (m_profileEnabled) {
        m_profile.publishPick.add(elapsedUs(partTimer));
        partTimer.restart();
    }

    publishPointCloudStats();
    if (m_profileEnabled) {
        m_profile.publishStats.add(elapsedUs(partTimer));
        m_profile.stream.add(elapsedUs(streamTimer));
        m_profile.streamFrames++;
        maybeFlushProfileBlock();
    }

    // The reveal ramp is left out: whether a ramp still has frames to draw is
    // only known once this frame's gather has drawn it, which
    // hasStreamingWork() reports.
    return hasLoadingWork();
}

bool cwRHIPointCloud::hasStreamingWork() const
{
    return m_revealing || hasLoadingWork();
}

bool cwRHIPointCloud::hasLoadingWork() const
{
    // A queue waiting on the eviction hold for a slot moves only once the hold
    // is let go, and letting go asks for its own frame through budgetsChanged.
    if (m_waitingForSlot) {
        return false;
    }

    return !m_readyQueue.isEmpty() || m_streamer.hasWork();
}

void cwRHIPointCloud::publishPointCloudStats()
{
    cwRenderFrameStats::PointCloud counts;
    counts.residentNodes = m_residentCount;

    // streamResources runs before gather, so this is the cut the last frame drew.
    counts.selectedNodes = int(m_selected.nodes.size());
    counts.selectedPoints = m_selected.points;
    counts.nodeLoadsInFlight = m_streamer.pending().loads + int(m_readyQueue.size());
    counts.sseInflation = forest().sseInflation();
    counts.pickMirrorBytes = m_mirrorBytes.bytes();

    if (m_profileEnabled) {
        m_profile.residentNodes = counts.residentNodes;
        m_profile.pendingLoads = counts.nodeLoadsInFlight;
        m_profile.sseInflation = counts.sseInflation;
    }

    // The HUD watches the stats' revision, so a settled cloud republishing the
    // same numbers would wake it 60 times a second for nothing.
    if (m_statsPublished && counts == m_publishedStats) {
        return;
    }
    m_publishedStats = counts;
    m_statsPublished = true;

    cwRenderFrameStats::instance()->publishPointCloud(counts);
}

bool cwRHIPointCloud::uploadNode(QRhi* rhi, QRhiResourceUpdateBatch* batch, int index,
                                 const cwPointOctreeNodePayload& payload, bool evictionHold)
{
    const QByteArray& bytes = payload.bytes;
    const int slot = takeConstantSlot(evictionHold);
    if (slot < 0) {
        // Nothing could be freed for it; it goes back to being asked for.
        m_nodes[index].state = NodeState::Absent;
        m_nodes[index].exportRequested = false;
        return false;
    }

    // A node holds at most kLeafMaxPoints or kSampleGridResolution^3 points at
    // kBytesPerPoint each, both far under cw::kMaxRhiBufferBytes.
    auto* buffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                  quint32(bytes.size()));
    buffer->create();
    // By-value QByteArray: a refcount bump instead of a deep copy.
    batch->uploadStaticBuffer(buffer, bytes);

    const QBox3D bounds = m_source.manifest->nodeBounds(index);
    const QVector3D minimum = bounds.minimum();
    const float quantizationStep =
        float(double(bounds.maximum().x() - minimum.x()) / double(cw::octree::kQuantMax));
    // Until a live gather says what is drawn under it, a node stands for its
    // own points alone, so its own sample spacing is the gap to close. The
    // live gather that follows this frame's uploads works out the floor of the
    // node and of every ancestor it completes, and writes them on this same
    // batch. Until a live gather writes its floor, this value is what an
    // export job draws it with, and what the slot keeps through a culled frame.
    const float nodeOwnSpacing = ownSpacing(index);
    NodeConstants constants;
    constants.originScale = {minimum.x(), minimum.y(), minimum.z(), quantizationStep};
    constants.floorSpacing = nodeOwnSpacing;
    // The frame the shader's reveal ramp counts this arrival's fade-in from,
    // the counter streamResources() writes the uniform's currentFrame with.
    const quint64 arrivalFrame = currentFrame();
    constants.arrivalFrame = quint32(arrivalFrame);
    constants.nodeIndex = quint32(index);
    constants.measuredSpacing = m_source.manifest->nodes.at(index).measuredSpacing;
    constants.floorChangedFrame = quint32(arrivalFrame);
    batch->updateDynamicBuffer(m_nodeConstants, quint32(slot) * kNodeConstantsBytes,
                               kNodeConstantsBytes, &constants);

    NodeRecord& node = m_nodes[index];
    node.floorSpacing = nodeOwnSpacing;
    node.arrivalFrame = arrivalFrame;
    node.previousFloorSpacing = 0.0f;
    node.floorChangedFrame = arrivalFrame;
    node.fade = NodeFade::None;
    node.fadeFrame = 0;
    node.buffer = buffer;
    node.bytes = bytes;
    node.index = payload.index;
    node.constantSlot = slot;
    node.state = NodeState::Resident;
    node.exportRequested = false;
    node.residentPosition = int(m_residentIndices.size());
    m_residentIndices.append(index);
    m_residentCount = int(m_residentIndices.size());

    // A node that landed after the cut moved past it is already evictable, and
    // nothing else will offer it: gather only records the nodes the previous cut
    // held.
    if (node.lastDesiredFrame != currentFrame()) {
        recordColdNode(index, node.lastDesiredFrame);
    }

    m_gpuBytes.setBytes(m_gpuBytes.bytes() + bytes.size());
    m_mirrorBytes.setBytes(m_mirrorBytes.bytes() + bytes.size() + payload.index.byteSize());

    m_residencyChanged = true;

    if (m_profileEnabled) {
        m_profile.uploads++;
    }
    return true;
}

void cwRHIPointCloud::recordColdNode(int index, quint64 frame)
{
    // The root is pinned: it is in every cut, so it never goes cold.
    if (index == kRootIndex) {
        return;
    }

    // Departures arrive newest-stamp last, which is what keeps the front the
    // coldest. A node that landed after its cut had already moved on is the
    // exception: it goes to the cold end when it is colder than the front, and
    // otherwise lands out of order, which costs eviction order, never
    // correctness — every entry in the queue is evictable.
    if (!m_coldNodes.empty() && frame < m_coldNodes.front().frame) {
        m_coldNodes.push_front({frame, index});
    } else {
        m_coldNodes.push_back({frame, index});
    }
}

void cwRHIPointCloud::recordColdNodes(const cw::octree::Selection& previous, quint64 frame)
{
    for (const cw::octree::SelectedNode& selected : previous.nodes) {
        if (selected.node < 0 || selected.node >= m_nodes.size()) {
            continue;
        }

        const NodeRecord& node = m_nodes.at(selected.node);
        if (node.state != NodeState::Resident || node.lastDesiredFrame == frame) {
            continue;
        }

        recordColdNode(selected.node, node.lastDesiredFrame);
    }

    const int slack = std::max(kColdQueueSlackMinimum,
                               kColdQueueSlackFactor * m_residentCount);
    if (int(m_coldNodes.size()) > slack) {
        compactColdNodes();
    }
}

bool cwRHIPointCloud::coldEntryStale(const ColdEntry& entry) const
{
    const NodeRecord& node = m_nodes.at(entry.node);
    return node.state != NodeState::Resident || node.lastDesiredFrame != entry.frame;
}

void cwRHIPointCloud::compactColdNodes()
{
    const auto stale = [this](const ColdEntry& entry) { return coldEntryStale(entry); };
    m_coldNodes.erase(std::remove_if(m_coldNodes.begin(), m_coldNodes.end(), stale),
                      m_coldNodes.end());
}

bool cwRHIPointCloud::evictColdestNode()
{
    while (!m_coldNodes.empty()) {
        const ColdEntry entry = m_coldNodes.front();
        m_coldNodes.pop_front();

        if (coldEntryStale(entry)) {
            continue;
        }

        releaseNode(entry.node);
        return true;
    }

    return false;
}

int cwRHIPointCloud::takeConstantSlot(bool evictionHold)
{
    if (m_freeSlots.isEmpty() && !evictionHold) {
        QElapsedTimer evictTimer;
        if (m_profileEnabled) {
            evictTimer.start();
        }

        const bool evicted = evictColdestNode();

        if (m_profileEnabled) {
            m_profile.slotEvictUs += elapsedUs(evictTimer);
            if (evicted) {
                m_profile.evictions++;
            }
        }
    }

    if (m_freeSlots.isEmpty()) {
        return -1;
    }

    const int slot = m_freeSlots.last();
    m_freeSlots.removeLast();
    return slot;
}

void cwRHIPointCloud::publishPickSet()
{
    if (!m_residencyChanged) {
        return;
    }
    m_residencyChanged = false;

    QVector<cwPointOctreePickSet::Node> nodes;
    QBox3D rootBounds;
    float pickRadius = 0.0f;

    if (m_source.manifest) {
        rootBounds = m_source.manifest->nodeBounds(kRootIndex);
        pickRadius = m_source.manifest->meanSpacingXY
                     * cwRenderPointCloud::PointPickRadiusScale;

        nodes.reserve(m_residentIndices.size());
        for (const int index : std::as_const(m_residentIndices)) {
            // The very bytes the node uploaded, and the index built over them —
            // implicit shares, not copies.
            nodes.append({m_source.manifest->nodeBounds(index), m_nodes.at(index).bytes,
                          m_nodes.at(index).index});
        }
    }

    m_pickSet->publish(std::move(nodes), rootBounds, pickRadius);
}

QVector<cw::octree::NodeResidency> cwRHIPointCloud::residencyStats() const
{
    QElapsedTimer timer;
    if (m_profileEnabled) {
        timer.start();
    }

    const quint64 frame = currentFrame();

    // The resident nodes alone: on a big cloud that is a few thousand entries
    // against a manifest of a hundred thousand.
    QVector<cw::octree::NodeResidency> stats;
    stats.reserve(m_residentIndices.size());
    for (const int index : m_residentIndices) {
        const NodeRecord& node = m_nodes.at(index);

        cw::octree::NodeResidency residency;
        residency.node = index;
        residency.resident = true;
        residency.selectedThisFrame = node.lastDesiredFrame == frame;
        residency.lastDesiredFrame = node.lastDesiredFrame;
        residency.bytes = node.bytes.size();
        residency.pinned = index == kRootIndex;
        stats.append(residency);
    }

    if (m_profileEnabled) {
        m_profile.residencyStatsUs += elapsedUs(timer);
    }
    return stats;
}

quint64 cwRHIPointCloud::currentFrame() const
{
    return m_frame ? m_frame->frameCounter() : 0;
}

const cwPointCloudForest& cwRHIPointCloud::forest() const
{
    static const cwPointCloudForest kNoForest;
    return m_frame ? m_frame->pointCloudForest() : kNoForest;
}

void cwRHIPointCloud::enforceGpuBudget(const cwRenderBudgets& budgets)
{
    const auto* ledger = cwRenderMemoryLedger::instance();
    const qint64 overshoot = ledger->totalBytes(cwRenderMemoryLedger::Residency::Gpu)
                             - budgets.gpuBudgetBytes;

    if (overshoot > 0 && !budgets.evictionHold) {
        const quint64 frame = currentFrame();
        const QVector<cw::octree::NodeResidency> stats = residencyStats();
        for (const int index : cw::octree::planNodeEvictions(stats, overshoot)) {
            // Once the nodes the cut dropped run out, the planner offers the
            // ones it still wants. Taking those would only re-request and
            // re-upload them next frame, so they stay and the cut gets coarser
            // instead.
            if (m_nodes.at(index).lastDesiredFrame == frame) {
                continue;
            }

            releaseNode(index);

            if (m_profileEnabled) {
                m_profile.evictions++;
            }
        }
    }

    //After the evictions, so the block reports what the frame ends up holding
    if (m_profileEnabled) {
        m_profile.gpuBytes = ledger->totalBytes(cwRenderMemoryLedger::Residency::Gpu);
    }
}

void cwRHIPointCloud::flushProfileBlock()
{
    constexpr int kMeanDigits = 1;
    constexpr int kInflationDigits = 3;
    constexpr int kMegabyteDigits = 1;

    const int frames = m_profile.frames;
    const int streamFrames = m_profile.streamFrames;

    QVector<int>& cutSizes = m_profile.cutSizes;
    std::sort(cutSizes.begin(), cutSizes.end());
    const int cutMedian = cutSizes.isEmpty() ? 0 : cutSizes.at(cutSizes.size() / 2);
    const int cutMax = cutSizes.isEmpty() ? 0 : cutSizes.last();

    QVector<qint64>& pointCounts = m_profile.pointCounts;
    std::sort(pointCounts.begin(), pointCounts.end());
    const qint64 pointsMedian =
        pointCounts.isEmpty() ? 0 : pointCounts.at(pointCounts.size() / 2);
    const qint64 pointsMax = pointCounts.isEmpty() ? 0 : pointCounts.last();

    const auto span = [](QLatin1StringView name, const ProfileBlock::Span& timed,
                        int spanFrames) {
        return QStringLiteral(" %1MeanUs=%2 %1MaxUs=%3")
            .arg(name)
            .arg(timed.meanUs(spanFrames), 0, 'f', kMeanDigits)
            .arg(timed.maxUs);
    };

    QVector<qint64>& frameUs = m_profile.frameUs;
    std::sort(frameUs.begin(), frameUs.end());
    const auto at = [&frameUs](double fraction) {
        if (frameUs.isEmpty()) {
            return qint64(0);
        }
        const qsizetype index = qsizetype(fraction * double(frameUs.size() - 1) + 0.5);
        return frameUs.at(std::clamp(index, qsizetype(0), frameUs.size() - 1));
    };
    constexpr double kMedianFraction = 0.5;
    constexpr double k95thFraction = 0.95;
    const qint64 frameMedianUs = at(kMedianFraction);
    const qint64 frame95Us = at(k95thFraction);
    const qint64 frameMaxUs = frameUs.isEmpty() ? 0 : frameUs.last();

    constexpr double kMicrosecondsPerMillisecond = 1000.0;
    const auto milliseconds = [](qint64 microseconds) {
        return double(microseconds) / kMicrosecondsPerMillisecond;
    };

    const auto megabytes = [](qint64 bytes) {
        return QString::number(double(bytes) / double(kBytesPerMegabyte), 'f', kMegabyteDigits);
    };

    QString line = QStringLiteral("render frame=%1 frames=%2 streamFrames=%3")
                       .arg(currentFrame())
                       .arg(frames)
                       .arg(streamFrames);
    line += span(QLatin1StringView("gather"), m_profile.gather, frames);
    line += span(QLatin1StringView("selectNodes"), m_profile.selectNodes, frames);
    line += span(QLatin1StringView("requestLoop"), m_profile.requestLoop, frames);
    line += span(QLatin1StringView("cancelLoop"), m_profile.cancelLoop, frames);
    line += span(QLatin1StringView("stream"), m_profile.stream, streamFrames);
    line += span(QLatin1StringView("upload"), m_profile.upload, streamFrames);
    line += span(QLatin1StringView("publishPick"), m_profile.publishPick, streamFrames);
    line += span(QLatin1StringView("publishStats"), m_profile.publishStats, streamFrames);
    line += span(QLatin1StringView("enforceBudget"), m_profile.enforceBudget, streamFrames);
    line += QStringLiteral(" cutMed=%1 cutMax=%2 resident=%3")
                .arg(cutMedian).arg(cutMax).arg(m_profile.residentNodes);
    line += QStringLiteral(" requests=%1 cancels=%2 uploads=%3 evictions=%4 pendingLoads=%5")
                .arg(m_profile.requests)
                .arg(m_profile.cancels)
                .arg(m_profile.uploads)
                .arg(m_profile.evictions)
                .arg(m_profile.pendingLoads);
    line += QStringLiteral(" sse=%1 gpuMb=%2 budgetMb=%3")
                .arg(m_profile.sseInflation, 0, 'f', kInflationDigits)
                .arg(megabytes(m_profile.gpuBytes))
                .arg(megabytes(m_profile.gpuBudgetBytes));
    line += QStringLiteral(" pointsMed=%1 pointsMax=%2 slotEvictUs=%3 residencyStatsUs=%4")
                .arg(pointsMedian)
                .arg(pointsMax)
                .arg(m_profile.slotEvictUs)
                .arg(m_profile.residencyStatsUs);
    line += QStringLiteral(" frameMsMed=%1 frameMsP95=%2 frameMsMax=%3")
                .arg(milliseconds(frameMedianUs), 0, 'f', kMeanDigits)
                .arg(milliseconds(frame95Us), 0, 'f', kMeanDigits)
                .arg(milliseconds(frameMaxUs), 0, 'f', kMeanDigits);

    cw::profile::write(lcProfileRender(), line);

    m_profile = ProfileBlock{};
}

void cwRHIPointCloud::maybeFlushProfileBlock()
{
    // gather() closes the block, so both sides of the frame are in it. The
    // second test is for a gather that keeps returning early: the stream side
    // still reports rather than piling one block on the next.
    const bool blockFull = m_profile.frames >= cw::profile::kProfileBlockFrames
                           || m_profile.streamFrames - m_profile.frames
                                  >= cw::profile::kProfileBlockFrames;
    if (blockFull) {
        flushProfileBlock();
    }
}

bool cwRHIPointCloud::residencyReady(const RenderData& jobRenderData)
{
    if (m_source.isNull()) {
        return true;
    }

    const cwFrustum frustum =
        cwFrustum::fromViewProjection(jobRenderData.viewProjectionMatrix);
    const QVector<cw::octree::SelectedNode> selected =
        cw::octree::selectNodes(selectionInput(jobRenderData, &frustum));

    bool ready = true;
    for (const cw::octree::SelectedNode& node : selected) {
        if (node.node < 0 || node.node >= m_nodes.size()) {
            continue;
        }

        switch (m_nodes.at(node.node).state) {
        case NodeState::Resident:
            break;
        case NodeState::Failed:
            // It will never land, so waiting on it would only burn the job's
            // frame budget until the gate gives up.
            break;
        case NodeState::Absent:
            // Every node the job wants is asked for, even once one has reported
            // the job unready, so the whole cut is in flight by the next frame.
            requestNode(node.node, cw::octree::kExportPriority);
            m_nodes[node.node].exportRequested = true;
            ready = false;
            break;
        case NodeState::Requested:
            // The live cut may have started this one; the job still needs it,
            // so the live cut may no longer take it away.
            m_nodes[node.node].exportRequested = true;
            ready = false;
            break;
        }
    }

    return ready;
}

void cwRHIPointCloud::releaseStreamedResources()
{
    // Per node rather than cancelAll(): that one waits for the disk reads in
    // flight, and the render thread is the wrong place to wait. The streamer
    // bumps each node's generation, so a load already in flight lands on nothing.
    for (const int index : std::as_const(m_requested)) {
        m_streamer.cancel(quint32(index));
        if (m_nodes.at(index).state == NodeState::Requested) {
            m_nodes[index].state = NodeState::Absent;
        }
        m_nodes[index].exportRequested = false;
    }
    m_requested.clear();
    m_readyQueue.clear();
    m_waitingForSlot = false;
    m_revealing = false;
    m_departingNodes.clear();
    m_nodeBoxCount = 0;

    // releaseNode swap-removes out of the back, so taking the back each time
    // walks the list once and leaves it empty.
    while (!m_residentIndices.isEmpty()) {
        releaseNode(m_residentIndices.last());
    }
    m_coldNodes.clear();

    // The set holds an implicit share of every mirror it was published, so it
    // has to let go here too, or a hidden view's bytes would outlive the nodes.
    publishPickSet();

    // The view that left is gone; whatever brings it back starts from the root.
    m_selected = cw::octree::Selection{};
    if (m_frame) {
        m_frame->pointCloudForest().removeTree(this);
    }

    publishPointCloudStats();
}

int cwRHIPointCloud::cancelRequestsNotDesiredThisFrame(quint64 frame)
{
    // Nodes asked for by an earlier cut that this one dropped. A pass over the
    // handful of open requests, not over the whole node table.
    int canceled = 0;
    for (int i = m_requested.size() - 1; i >= 0; i--) {
        const int index = m_requested.at(i);
        if (m_nodes.at(index).state != NodeState::Requested) {
            m_requested.removeAt(i);
            continue;
        }

        if (m_nodes.at(index).exportRequested) {
            // An offscreen job is waiting on it. Canceling would throw away a
            // load in flight and the job would ask for it again next frame,
            // which on a slow disk is a wait that never ends.
            continue;
        }

        if (m_nodes.at(index).lastDesiredFrame != frame) {
            m_streamer.cancel(quint32(index));
            m_nodes[index].state = NodeState::Absent;
            m_requested.removeAt(i);
            canceled++;

            if (m_profileEnabled) {
                m_profile.cancels++;
            }
        }
    }

    return canceled;
}

void cwRHIPointCloud::gatherCulled(const GatherContext& context)
{
    // The cut and the requests belong to the view the user is watching, the
    // same way gather() only lets the live frame touch them. An export job
    // renders one camera of its own out of band, and a tile of it that misses
    // this cloud says nothing about what the live view still wants.
    if (!context.liveFrame || m_source.isNull()) {
        return;
    }

    // Nothing draws this cloud, so its last cut is stale: publishPointCloudStats
    // would keep reporting a cut no one can see.
    const cw::octree::Selection previousSelection = m_selected;
    m_selected = cw::octree::Selection{};
    m_revealing = false;
    if (!m_departingNodes.isEmpty() && context.resourceUpdateBatch) {
        endDepartures(context.resourceUpdateBatch, m_frame->frameCounter() - 1);
    }

    // gather() never ran, so no node carries this frame's stamp: every node the
    // dropped cut held just went cold, and every open request that no export is
    // waiting on is one this frame does not want.
    const quint64 frame = currentFrame();
    recordColdNodes(previousSelection, frame);
    const int canceled = cancelRequestsNotDesiredThisFrame(frame);

    if (!previousSelection.nodes.isEmpty() || canceled > 0) {
        m_residencyChanged = true;
    }
}

bool cwRHIPointCloud::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (context.renderPass == RenderPass::Overlay) {
        return gatherNodeBoxes(context, batches);
    }

    if (context.renderPass != RenderPass::PointCloud) {
        return false;
    }

    if (m_source.isNull()) {
        return false;
    }

    const RenderData& renderData = *context.renderData;
    if (!ensurePipeline(renderData)) {
        return false;
    }

    auto* pipeline = m_pipelineRecord ? m_pipelineRecord->pipeline : nullptr;
    if (!pipeline || !m_srb || !m_nodeConstants) {
        return false;
    }

    m_profileEnabled = lcProfileRender().isDebugEnabled();
    QElapsedTimer gatherTimer;
    QElapsedTimer partTimer;
    if (m_profileEnabled) {
        gatherTimer.start();
        partTimer.start();

        // The whole frame, as the render thread lives it: one live gather to
        // the next. An offscreen job renders its own camera out of band and
        // says nothing about the view's frame rate.
        if (context.liveFrame) {
            if (m_frameIntervalTimer.isValid()) {
                m_profile.frameUs.append(elapsedUs(m_frameIntervalTimer));
            }
            m_frameIntervalTimer.restart();
        }
    } else {
        // The next frame after the category comes on has no predecessor to
        // measure against, so it starts a fresh interval rather than reporting
        // however long the category was off.
        m_frameIntervalTimer.invalidate();
    }

    cw::octree::SelectionInput input = selectionInput(renderData, context.frustum);

    // The cut this frame replaces, which is where the nodes going cold are
    // found. The node list is implicitly shared, so this costs a refcount.
    const cw::octree::Selection previousSelection = m_selected;

    // The live view's cut spans every visible cloud under one cap, so this
    // cloud takes the slice gatherScene already selected for it. An export job
    // renders its own camera once, at the detail it asked for, under no point
    // budget at all, and leaves the forest alone.
    m_selected = context.liveFrame ? forest().selectionFor(this)
                                   : cw::octree::selectCut(input);

    if (m_profileEnabled) {
        m_profile.selectNodes.add(elapsedUs(partTimer));
        m_profile.cutSizes.append(int(m_selected.nodes.size()));
        m_profile.pointCounts.append(m_selected.points);
        partTimer.restart();
    }

    const quint64 frame = m_frame->frameCounter();

    // The frame the shader's reveal ramp reads this draw at: the counter as
    // streamResources() stamped arrivals and wrote the uniform's currentFrame
    // with, before gatherScene() advanced it for this gather.
    const quint64 revealClock = frame - 1;
    const int revealFrames = m_renderState.value().shading.revealFrames;

    // binding 1 = per-cloud appearance UBO (dynamic offset). The job picks the
    // appearance slot; resolve it to this cloud's byte offset. Clamp to the slots
    // actually allocated so an out-of-range slot falls back to the live appearance
    // (slot 0) rather than reading past the buffer. std::max guards the upper bound
    // so the clamp range never inverts even if capacity were 0 (clamp UB otherwise).
    const int maxAppearanceSlot = std::max(0, appearanceSlotCapacity() - 1);
    const int appearanceSlot = std::clamp(context.appearanceSlot, 0, maxAppearanceSlot);
    const quint32 appearanceOffset = quint32(appearanceSlot) * m_perCloudStride;

    QVector<Drawable> drawables;
    drawables.reserve(m_selected.nodes.size());

    const auto nodeDrawable = [&](int index) {
        const NodeRecord& node = m_nodes.at(index);
        Drawable drawable;
        drawable.type = Drawable::Type::NonIndexed;
        drawable.vertexBindings = {
            QRhiCommandBuffer::VertexInput(node.buffer, 0),
            QRhiCommandBuffer::VertexInput(m_nodeConstants,
                                           quint32(node.constantSlot) * kNodeConstantsBytes)
        };
        drawable.vertexCount = m_source.manifest->nodes.at(index).pointCount;
        drawable.instanceCount = 1;
        drawable.bindings = m_srb;
        drawable.globalCameraBinding = 0; // binding 0 = global camera UBO (dynamic offset)
        drawable.appearanceBinding = 1;
        drawable.appearanceUniformOffset = appearanceOffset;
        return drawable;
    };

    //The cut split by residency, which is what each node's floor is taken from
    QVector<int> drawnNodes;
    drawnNodes.reserve(m_selected.nodes.size());
    QVector<int> missingNodes;

    for (const cw::octree::SelectedNode& selected : std::as_const(m_selected.nodes)) {
        if (selected.node < 0 || selected.node >= m_nodes.size()) {
            continue;
        }

        NodeRecord& node = m_nodes[selected.node];
        node.lastDesiredFrame = frame;

        switch (node.state) {
        case NodeState::Resident: {
            const cwPointOctreeNode& drawn = m_source.manifest->nodes.at(selected.node);
            drawnNodes.append(selected.node);

            // Still in the cut, so the floors above are what they would be
            // without minLevel: only the drawing is left out.
            if (drawn.level < renderData.budgets.minLevel) {
                break;
            }

            drawables.append(nodeDrawable(selected.node));
            break;
        }
        case NodeState::Absent: {
            missingNodes.append(selected.node);

            // The root always loads first; everything else queues behind the
            // coarsest thing still missing. kPriorityScale keeps sub-pixel
            // differences in spacing apart in the integer priority.
            const quint64 priority = selected.node == kRootIndex
                ? cw::octree::kRootPriority
                : quint64(selected.projectedSpacingPx * cw::octree::kPriorityScale);
            requestNode(selected.node, priority);
            break;
        }
        case NodeState::Requested:
        case NodeState::Failed:
            missingNodes.append(selected.node);
            break;
        }
    }

    // The world-space floor under the threshold rule, taken per node from the
    // finest spacing drawn under it rather than from the node's own: the cut is
    // additive, so a refined region draws its coarse ancestors too, and their
    // own spacing would blow their sprites up past the region they sit in.
    // Taking it from what is drawn is what holds the surface together where
    // the point cap left the cut coarser than the threshold describes. While
    // children stream, a node whose children are still missing keeps its own
    // spacing, since it alone covers their cells (under RevealMode::Grow it
    // takes the requested cut's floor instead and lets them show as holes,
    // see writeNodeFloors()). The floors ride the batch
    // this frame's draws consume, so a node draws at the floor this cut gives
    // it on the frame the cut changes and on the frame its last child lands. A
    // resident node leaving the cut goes back to its own spacing. An export
    // job renders its own camera, so it leaves the live view's floors where
    // the live frame put them.
    // A live gather without the frame's batch would freeze every floor at the
    // spacing uploadNode() landed it with, so it is a caller bug.
    if (context.liveFrame) {
        Q_ASSERT_X(context.resourceUpdateBatch, "cwRHIPointCloud::gather",
                   "a live gather needs the frame's resource batch to write node floors");
        if (context.resourceUpdateBatch) {
            if (!drawnNodes.isEmpty()) {
                writeNodeFloors(context.resourceUpdateBatch, drawnNodes, missingNodes,
                                revealClock);
            }

            // Ahead of the restore, which leaves a fading node's floor where
            // the last frame drew it
            if (revealFrames > 0) {
                updateFades(context.resourceUpdateBatch, previousSelection, drawnNodes, frame,
                            revealClock);
            } else if (!m_departingNodes.isEmpty()) {
                endDepartures(context.resourceUpdateBatch, revealClock);
            }
            restoreOwnFloorsLeavingCut(context.resourceUpdateBatch, previousSelection, frame,
                                       revealClock);
        }

        for (const int index : std::as_const(m_departingNodes)) {
            if (m_source.manifest->nodes.at(index).level >= renderData.budgets.minLevel) {
                drawables.append(nodeDrawable(index));
            }
        }

        // A ramp still short of its end needs the next frame drawn, so the loop
        // stops on the frame the last one lands
        const auto stillRamping = [&](int index) {
            return nodeRevealing(m_nodes.at(index), revealClock);
        };
        m_revealing = revealFrames > 0
                      && (std::any_of(drawnNodes.cbegin(), drawnNodes.cend(), stillRamping)
                          || std::any_of(m_departingNodes.cbegin(), m_departingNodes.cend(),
                                         stillRamping));
    }

    // Every node this frame's cut holds now carries this frame's stamp, so the
    // ones the previous cut held that still carry an older one are exactly the
    // ones that just went cold.
    recordColdNodes(previousSelection, frame);

    if (m_profileEnabled) {
        m_profile.requestLoop.add(elapsedUs(partTimer));
        partTimer.restart();
    }

    cancelRequestsNotDesiredThisFrame(frame);

    if (m_profileEnabled) {
        m_profile.cancelLoop.add(elapsedUs(partTimer));
    }

    const bool hasDrawables = !drawables.isEmpty();
    if (hasDrawables) {
        cwRHIObject::PipelineState state;
        state.pipeline = pipeline;
        state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, pipeline);

        auto& batch = acquirePipelineBatch(batches, state);
        batch.drawables.append(drawables);
    }

    if (m_profileEnabled) {
        m_profile.gather.add(elapsedUs(gatherTimer));
        m_profile.frames++;
        maybeFlushProfileBlock();
    }

    return hasDrawables;
}

bool cwRHIPointCloud::usesPointCloudPass() const
{
    // A cloud with a manifest always draws its root, so the EDL pass has to run
    // from the frame the source arrives — not from the frame a node lands, or
    // the root would never be composited. The caller
    // (cwRhiFrameRenderer::anyCloudVisible) ANDs in this object's snapshot
    // visibility.
    return !m_source.isNull();
}

std::optional<QBox3D> cwRHIPointCloud::worldBounds() const
{
    if (m_source.isNull()) {
        return std::nullopt;
    }

    // A point draws as a sprite covering spacingCoverage of the spacing the
    // cut refines to at its own depth. A cloud framed in the view is drawn no
    // coarser than its root, so padding by coverage of the root's spacing
    // covers every sprite a view that can see the cloud draws.
    const cwRenderPointCloud::RenderState& state = m_renderState.value();
    const QBox3D root = m_source.manifest->nodeBounds(kRootIndex);
    const float rootSpacing = float(m_source.manifest->spacing(kRootLevel));
    const float radius = state.spacingCoverage * rootSpacing;
    const QVector3D padding(radius, radius, radius);
    return QBox3D(root.minimum() - padding, root.maximum() + padding);
}

bool cwRHIPointCloud::ensurePipeline(const RenderData& data)
{
    if (!m_resourcesInitialized) {
        return false;
    }

    if (!data.renderer) {
        return false;
    }

    // The frame's command buffer, not the renderer's QQuickRhiItem: a pass
    // routed into the EDL offscreen draws through the same QRhi, and an
    // offscreen job has no item to ask.
    QRhi* rhi = data.cb ? data.cb->rhi() : nullptr;
    if (!rhi || !data.renderPassDescriptor) {
        return false;
    }

    const quint32 globalStride = data.renderer->globalUniformBufferStride();
    const quint32 perCloudStride = rhi->ubufAligned(sizeof(PerCloudUniform));

    const auto key = buildPipelineKey(data.renderPassDescriptor, data.sampleCount);

    auto createFn = [this, key, globalStride, perCloudStride](QRhi* localRhi) -> cwRhiPipelineRecord* {
        if (!localRhi) {
            return nullptr;
        }

        auto* record = new cwRhiPipelineRecord;
        record->pipeline = localRhi->newGraphicsPipeline();

        QShader vs = loadShader(":/shaders/PointCloud.vert.qsb");
        QShader fs = loadShader(":/shaders/PointCloud.frag.qsb");

        record->pipeline->setShaderStages({
            { QRhiShaderStage::Vertex, vs },
            { QRhiShaderStage::Fragment, fs }
        });

        record->pipeline->setDepthTest(true);
        record->pipeline->setDepthWrite(true);
        record->pipeline->setSampleCount(key.sampleCount);
        record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
        record->pipeline->setVertexInputLayout(m_inputLayout);
        record->pipeline->setTopology(QRhiGraphicsPipeline::Points);

        QRhiGraphicsPipeline::TargetBlend blendState;
        blendState.enable = false;
        record->pipeline->setTargetBlends({ blendState });

        record->layout = localRhi->newShaderResourceBindings();
        record->layout->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, nullptr, globalStride),
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, QRhiShaderResourceBinding::VertexStage, nullptr, perCloudStride),
        });
        record->layout->create();

        record->pipeline->setShaderResourceBindings(record->layout);
        record->pipeline->setRenderPassDescriptor(key.renderPass);
        record->pipeline->create();

        return record;
    };

    m_pipelineRecord = m_pipelines.acquire(m_frame, key, [&]() {
        return m_frame->acquirePipeline(key, rhi, createFn);
    });

    if (!m_pipelineRecord) {
        return false;
    }

    if (!ensureShaderResources(rhi, data.renderer)) {
        return false;
    }

    return true;
}

bool cwRHIPointCloud::ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer)
{
    if (!renderer) {
        return false;
    }

    if (m_srb) {
        if (m_pipelineRecord && m_pipelineRecord->layout &&
            !m_pipelineRecord->layout->isLayoutCompatible(m_srb)) {
            delete m_srb;
            m_srb = nullptr;
        } else {
            return true;
        }
    }

    if (!rhi || !m_perCloudUBO) {
        return false;
    }

    m_srb = rhi->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, QRhiShaderResourceBinding::VertexStage, renderer->globalUniformBuffer(), renderer->globalUniformBufferStride()),
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(1, QRhiShaderResourceBinding::VertexStage, m_perCloudUBO, m_perCloudStride),
    });
    m_srb->create();

    if (m_pipelineRecord && m_pipelineRecord->layout) {
        Q_ASSERT(m_pipelineRecord->layout->isLayoutCompatible(m_srb));
    }

    return true;
}

void cwRHIPointCloud::purgePipelinesFor(QRhiRenderPassDescriptor* descriptor)
{
    cwRHIObject::purgePipelinesFor(descriptor);
    m_nodeBoxPipelines.purgeFor(descriptor);
    m_nodeBoxPipelineRecord = nullptr;
}

void cwRHIPointCloud::writeNodeBoxes(QRhi* rhi, QRhiResourceUpdateBatch* batch)
{
    m_nodeBoxCount = 0;
    if (!m_renderState.value().shading.showNodeBoxes || !m_source.manifest) {
        return;
    }

    if (!m_nodeBoxCube) {
        m_nodeBoxCube = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                       quint32(sizeof(kNodeBoxCorners)));
        m_nodeBoxCube->create();
        batch->uploadStaticBuffer(m_nodeBoxCube, kNodeBoxCorners.data());
    }

    const cwPointOctreeManifest& manifest = *m_source.manifest;
    const auto box = [&manifest](int index, cw::pointcloud::Rgb color, float pattern) {
        const QBox3D bounds = manifest.nodeBounds(index);
        const QVector3D minimum = bounds.minimum();
        return NodeBoxInstance{
            {minimum.x(), minimum.y(), minimum.z(), bounds.maximum().x() - minimum.x()},
            {color.red, color.green, color.blue, pattern}};
    };

    // The last cut, which streamResources still names: every node of it by its
    // level, dashed until it lands, and then every resident node it left out,
    // which is what eviction takes from.
    QVector<NodeBoxInstance> boxes;
    boxes.reserve(m_selected.nodes.size() + m_residentIndices.size());
    const quint64 frame = currentFrame();
    for (const cw::octree::SelectedNode& selected : std::as_const(m_selected.nodes)) {
        if (selected.node < 0 || selected.node >= m_nodes.size()) {
            continue;
        }
        const NodeState state = m_nodes.at(selected.node).state;
        if (state == NodeState::Failed) {
            continue;
        }
        const cw::pointcloud::Rgb color =
            cw::pointcloud::levelColor(manifest.nodes.at(selected.node).level);
        boxes.append(box(selected.node, color,
                         state == NodeState::Resident ? kSolidBox : kDashedBox));
    }
    for (const int index : std::as_const(m_residentIndices)) {
        if (m_nodes.at(index).lastDesiredFrame != frame) {
            boxes.append(box(index, cw::pointcloud::kEvictableNodeBoxColor, kSolidBox));
        }
    }

    if (boxes.isEmpty()) {
        return;
    }

    if (boxes.size() > m_nodeBoxCapacity) {
        // Deleting defers the native release past the frames still reading it
        delete m_nodeBoxInstances;
        m_nodeBoxCapacity = std::max(int(boxes.size()), kNodeBoxGrowthFactor * m_nodeBoxCapacity);
        m_nodeBoxInstances = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                            quint32(m_nodeBoxCapacity * sizeof(NodeBoxInstance)));
        m_nodeBoxInstances->create();
    }

    batch->updateDynamicBuffer(m_nodeBoxInstances, 0,
                               quint32(boxes.size() * sizeof(NodeBoxInstance)),
                               boxes.constData());
    m_nodeBoxCount = int(boxes.size());
}

bool cwRHIPointCloud::gatherNodeBoxes(const GatherContext& context,
                                      QVector<PipelineBatch>& batches)
{
    // The boxes describe the live view's cut, so an export draws none
    if (!context.liveFrame || m_nodeBoxCount == 0 || !m_nodeBoxInstances) {
        return false;
    }

    if (!ensureNodeBoxPipeline(*context.renderData)) {
        return false;
    }

    Drawable drawable;
    drawable.type = Drawable::Type::NonIndexed;
    drawable.vertexBindings = {
        QRhiCommandBuffer::VertexInput(m_nodeBoxCube, 0),
        QRhiCommandBuffer::VertexInput(m_nodeBoxInstances, 0)
    };
    drawable.vertexCount = kNodeBoxVertexCount;
    drawable.instanceCount = quint32(m_nodeBoxCount);
    drawable.bindings = m_nodeBoxSrb;
    drawable.globalCameraBinding = 0;

    cwRHIObject::PipelineState state;
    state.pipeline = m_nodeBoxPipelineRecord->pipeline;
    state.sortKey = cwRHIObject::makeSortKey(context.objectOrder, state.pipeline);
    acquirePipelineBatch(batches, state).drawables.append(drawable);
    return true;
}

bool cwRHIPointCloud::ensureNodeBoxPipeline(const RenderData& data)
{
    QRhi* rhi = data.cb ? data.cb->rhi() : nullptr;
    if (!m_resourcesInitialized || !data.renderer || !rhi || !data.renderPassDescriptor) {
        return false;
    }

    cwRhiPipelineKey key;
    key.renderPass = data.renderPassDescriptor;
    key.sampleCount = data.sampleCount;
    key.vertexShader = QStringLiteral(":/shaders/NodeBox.vert.qsb");
    key.fragmentShader = QStringLiteral(":/shaders/NodeBox.frag.qsb");
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::None);
    key.globalBinding = 0;
    key.globalStages = kVertexStageMask;
    key.textureBinding = kUnusedBinding;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Lines);

    const quint32 globalStride = data.renderer->globalUniformBufferStride();
    auto createFn = [this, key, globalStride](QRhi* localRhi) -> cwRhiPipelineRecord* {
        if (!localRhi) {
            return nullptr;
        }

        auto* record = new cwRhiPipelineRecord;
        record->pipeline = localRhi->newGraphicsPipeline();
        record->pipeline->setShaderStages({
            { QRhiShaderStage::Vertex, loadShader(key.vertexShader) },
            { QRhiShaderStage::Fragment, loadShader(key.fragmentShader) }
        });

        // An overlay: every box shows, whatever the scene puts in front of it
        record->pipeline->setDepthTest(false);
        record->pipeline->setDepthWrite(false);
        record->pipeline->setSampleCount(key.sampleCount);
        record->pipeline->setCullMode(QRhiGraphicsPipeline::None);
        record->pipeline->setVertexInputLayout(m_nodeBoxInputLayout);
        record->pipeline->setTopology(QRhiGraphicsPipeline::Lines);

        record->layout = localRhi->newShaderResourceBindings();
        record->layout->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                0, QRhiShaderResourceBinding::VertexStage, nullptr, globalStride)
        });
        record->layout->create();

        record->pipeline->setShaderResourceBindings(record->layout);
        record->pipeline->setRenderPassDescriptor(key.renderPass);
        record->pipeline->create();
        return record;
    };

    m_nodeBoxPipelineRecord = m_nodeBoxPipelines.acquire(m_frame, key, [&]() {
        return m_frame->acquirePipeline(key, rhi, createFn);
    });
    if (!m_nodeBoxPipelineRecord || !m_nodeBoxPipelineRecord->pipeline) {
        return false;
    }

    if (m_nodeBoxSrb && m_nodeBoxPipelineRecord->layout
        && !m_nodeBoxPipelineRecord->layout->isLayoutCompatible(m_nodeBoxSrb)) {
        delete m_nodeBoxSrb;
        m_nodeBoxSrb = nullptr;
    }

    if (!m_nodeBoxSrb) {
        m_nodeBoxSrb = rhi->newShaderResourceBindings();
        m_nodeBoxSrb->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                0, QRhiShaderResourceBinding::VertexStage, data.renderer->globalUniformBuffer(),
                globalStride)
        });
        m_nodeBoxSrb->create();
    }
    return true;
}

cwRhiPipelineKey cwRHIPointCloud::buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                                   int sampleCount) const
{
    cwRhiPipelineKey key;
    key.renderPass = renderPassDescriptor;
    key.sampleCount = sampleCount;
    key.vertexShader = QStringLiteral(":/shaders/PointCloud.vert.qsb");
    key.fragmentShader = QStringLiteral(":/shaders/PointCloud.frag.qsb");
    key.cullMode = static_cast<quint8>(cwRenderMaterialState::CullMode::None);
    key.frontFace = static_cast<quint8>(cwRenderMaterialState::FrontFace::CCW);
    key.blendMode = static_cast<quint8>(cwRenderMaterialState::BlendMode::None);
    key.depthTest = 1;
    key.depthWrite = 1;
    key.globalBinding = 0;
    key.perDrawBinding = 1;
    key.textureBinding = kUnusedBinding;
    key.globalStages = kVertexStageMask;
    key.perDrawStages = kVertexStageMask;
    key.textureStages = 0;
    key.hasPerDraw = 1;
    key.topology = static_cast<quint8>(QRhiGraphicsPipeline::Points);
    return key;
}

Monad::Result<cwPointOctreeNodePayload> cwRHIPointCloud::loadNode(
    const cwPointOctreeNodeSource& source, int /*level*/)
{
    const bool profiling = lcProfileLoad().isDebugEnabled();
    QElapsedTimer timer;
    if (profiling) {
        timer.start();
    }

    const cwDiskCacher cacher{source.cacheRootPath};
    const QByteArray bytes = cacher.entry(source.key);

    if (profiling) {
        cw::profile::write(lcProfileLoad(), QStringLiteral("load us=%1 bytes=%2")
                                               .arg(elapsedUs(timer))
                                               .arg(bytes.size()));
    }

    if (bytes.size() != source.byteSize) {
        return Monad::Result<cwPointOctreeNodePayload>(
            QStringLiteral("read %1 bytes where the manifest says %2")
                .arg(bytes.size()).arg(source.byteSize));
    }

    // The index costs one pass over bytes this worker just read, so the render
    // thread never pays for it and a pick never rebuilds it.
    return cwPointOctreeNodePayload {bytes, cwPointOctreePickIndex::build(bytes)};
}
