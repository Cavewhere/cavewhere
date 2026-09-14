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

// Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {

    // {nodeMin.x, nodeMin.y, nodeMin.z, nodeSize / kQuantMax} as four floats
    constexpr int kNodeConstantsFloats = 4;
    constexpr quint32 kNodeConstantsBytes = kNodeConstantsFloats * sizeof(float);

    // Slots in the shared per-instance constants buffer, at 16 bytes each. A
    // view selects at most cw::octree::kMaxDesiredNodes nodes per frame, so at
    // least half the slots are unselected in any frame and an incoming node can
    // always find one to take.
    constexpr int kMaxResidentNodes = 8192;
    static_assert(kMaxResidentNodes >= 2 * cw::octree::kMaxDesiredNodes,
                  "An upload must be able to evict an unselected node for its "
                  "constants slot, which needs more slots than one frame's cut.");

    constexpr int kPointsBinding = 0;
    constexpr int kNodeConstantsBinding = 1;

    //One instance per node, so the constants advance once per draw
    constexpr quint32 kInstanceStepRate = 1;
    constexpr int kRootIndex = 0;
    constexpr qint64 kBytesPerMegabyte = 1024 * 1024;

    using cw::profile::elapsedUs;
}

cwRHIPointCloud::cwRHIPointCloud(std::shared_ptr<cwPointOctreePickSet> pickSet) :
    m_streamer(&cwRHIPointCloud::loadNode,
               [](const cwPointOctreeNodeSource& source, int) {
                   return source.byteSize
                          + cwPointOctreePickIndex::estimatedBytes(source.byteSize);
               },
               cwRenderMemoryLedger::Category::PointCloudGeometry),
    m_pickSet(std::move(pickSet))
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
                                     kNodeConstantsBytes * quint32(kMaxResidentNodes));
    m_nodeConstants->create();

    // Popped from the back, so the first node takes slot 0.
    m_freeSlots.reserve(kMaxResidentNodes);
    for (int slot = kMaxResidentNodes - 1; slot >= 0; slot--) {
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
        { kNodeConstantsBinding, 1, QRhiVertexInputAttribute::Float4, 0 }   // nodeOriginScale
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

    m_nodes.clear();
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

    m_gpuBytes.setBytes(m_gpuBytes.bytes() - node.bytes.size());
    m_mirrorBytes.setBytes(m_mirrorBytes.bytes() - node.bytes.size() - node.index.byteSize());
    node.bytes = QByteArray();
    node.index = cwPointOctreePickIndex();

    node.state = NodeState::Absent;
    node.exportRequested = false;
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
    cwPointOctreeNodeSource source;
    source.cacheRootPath = m_source.cacheRootPath();
    // nodeName() memoizes a parent table; only the render thread ever asks a
    // published manifest for a name, so that memo has one writer.
    source.key = cw::octree::nodeKey(m_source.lazPath, m_source.fingerprint,
                                     m_source.manifest->nodeName(index));
    source.byteSize = m_source.manifest->nodes.at(index).byteSize;
    return source;
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
    input.sseInflation = m_sseInflation;
    return input;
}

void cwRHIPointCloud::updateResources(const ResourceUpdateData& data)
{
    if (!m_renderState.isChanged() && m_perCloudUBO) {
        return;
    }

    auto* rhi = data.renderData.cb->rhi();
    QRhiResourceUpdateBatch* batch = data.resourceUpdateBatch;

    // Per-cloud uniform — world-space sprite radius in meters. A fixed default
    // (set on cwRenderPointCloud::RenderState) produces consistent sprite sizes
    // across every loaded cloud; the earlier meanSpacingXY * 0.5 auto-derivation
    // was unreliable because mean-spacing estimates vary with LAZ density /
    // sampling. The live radius is overridden via cwLazLayersSceneNode::
    // setWorldRadius (P+wheel gesture in the 3D view, and sink_repatcher
    // --point-radius for offline renders).
    //
    // Steady state is ONE slot (slot 0 = the live radius). Per-job appearance
    // overrides acquire transient slots that grow the buffer on demand
    // (cwAppearanceSlotted), so an interactive session pays 0.75 KB/cloud instead
    // of the full kAppearanceSlotCount. resizeAppearanceSlots builds the buffer and
    // writes slot 0; a later live-radius change re-writes only slot 0.
    if (!m_perCloudUBO) {
        resizeAppearanceSlots(rhi, batch, 1);
    } else {
        writeAppearanceSlot(batch, kLiveAppearanceSlot, m_renderState.value().worldRadius);
    }

    m_renderState.resetChanged();
}

void cwRHIPointCloud::writeAppearanceSlot(QRhiResourceUpdateBatch* batch, int slot,
                                          float worldRadius)
{
    const PerCloudUniform uniform{ worldRadius, {0.0f, 0.0f, 0.0f} };
    batch->updateDynamicBuffer(m_perCloudUBO, slot * m_perCloudStride,
                               sizeof(PerCloudUniform), &uniform);
}

void cwRHIPointCloud::uploadAppearance(QRhiResourceUpdateBatch* batch, int slot,
                                       const cwAppearanceOverride& override)
{
    // Unpack the cloud's own appearance schema. A missing payload (or one that
    // omits the radius) renders at the live radius, so an override that only
    // tweaks a future field still draws at the right size.
    const auto* appearance = override.payload<cwPointCloudAppearance>();
    const float radius = (appearance && appearance->worldRadius.has_value())
        ? appearance->worldRadius.value()
        : m_renderState.value().worldRadius;
    writeAppearanceSlot(batch, slot, radius);
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
    writeAppearanceSlot(batch, kLiveAppearanceSlot, m_renderState.value().worldRadius);
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

    auto* rhi = data.renderData.cb->rhi();
    if (!rhi || !m_nodeConstants) {
        return m_streamer.hasWork();
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
    QVector<NodeStreamer::Result> deferred;
    for (const NodeStreamer::Result& result : std::as_const(m_readyQueue)) {
        const int index = int(result.itemId);
        if (!isRequested(index)) {
            continue;
        }

        if (!cw::residency::takeFromBudget(remainingUploadBytes, result.payload.bytes.size(),
                                           uploadedThisFrame)) {
            // The streamer already handed this over, so it waits here for the
            // next frame rather than being asked for a second time.
            deferred.append(result);
            continue;
        }

        if (uploadNode(rhi, data.resourceUpdateBatch, index, result.payload)) {
            uploadedThisFrame = true;
        }
    }
    m_readyQueue = deferred;

    QElapsedTimer partTimer;
    if (m_profileEnabled) {
        partTimer.start();
    }
    enforceGpuBudget(data.renderData.budgets);
    if (m_profileEnabled) {
        m_profile.enforceBudget.add(elapsedUs(partTimer));
        m_profile.gpuBudgetBytes = data.renderData.budgets.gpuBudgetBytes;
        partTimer.restart();
    }

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

    return !m_readyQueue.isEmpty() || m_streamer.hasWork();
}

void cwRHIPointCloud::publishPointCloudStats() const
{
    cwRenderFrameStats::PointCloud counts;

    for (const NodeRecord& node : m_nodes) {
        if (node.state == NodeState::Resident) {
            ++counts.residentNodes;
        }
    }

    // streamResources runs before gather, so this is the cut the last frame drew.
    counts.selectedNodes = int(m_selected.nodes.size());
    counts.selectedPoints = m_selected.points;
    counts.nodeLoadsInFlight = m_streamer.pending().loads + int(m_readyQueue.size());
    counts.sseInflation = m_sseInflation;

    if (m_profileEnabled) {
        m_profile.residentNodes = counts.residentNodes;
        m_profile.pendingLoads = counts.nodeLoadsInFlight;
        m_profile.sseInflation = counts.sseInflation;
    }

    cwRenderFrameStats::instance()->publishPointCloud(counts);
}

bool cwRHIPointCloud::uploadNode(QRhi* rhi, QRhiResourceUpdateBatch* batch, int index,
                                 const cwPointOctreeNodePayload& payload)
{
    const QByteArray& bytes = payload.bytes;
    const int slot = takeConstantSlot(bytes.size());
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
    const std::array<float, kNodeConstantsFloats> constants {
        minimum.x(), minimum.y(), minimum.z(), quantizationStep
    };
    batch->updateDynamicBuffer(m_nodeConstants, quint32(slot) * kNodeConstantsBytes,
                               kNodeConstantsBytes, constants.data());

    NodeRecord& node = m_nodes[index];
    node.buffer = buffer;
    node.bytes = bytes;
    node.index = payload.index;
    node.constantSlot = slot;
    node.state = NodeState::Resident;
    node.exportRequested = false;

    m_gpuBytes.setBytes(m_gpuBytes.bytes() + bytes.size());
    m_mirrorBytes.setBytes(m_mirrorBytes.bytes() + bytes.size() + payload.index.byteSize());

    m_residencyChanged = true;

    if (m_profileEnabled) {
        m_profile.uploads++;
    }
    return true;
}

int cwRHIPointCloud::takeConstantSlot(qint64 incomingBytes)
{
    if (m_freeSlots.isEmpty()) {
        QElapsedTimer evictTimer;
        if (m_profileEnabled) {
            evictTimer.start();
        }

        const QVector<int> plan =
            cw::octree::planNodeEvictions(residencyStats(), incomingBytes);
        for (const int index : plan) {
            releaseNode(index);
        }

        if (m_profileEnabled) {
            m_profile.slotEvictUs += elapsedUs(evictTimer);
            m_profile.evictions += int(plan.size());
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

        for (int i = 0; i < m_nodes.size(); i++) {
            if (m_nodes.at(i).state != NodeState::Resident) {
                continue;
            }

            // The very bytes the node uploaded, and the index built over them —
            // implicit shares, not copies.
            nodes.append({m_source.manifest->nodeBounds(i), m_nodes.at(i).bytes,
                          m_nodes.at(i).index});
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

    const quint64 frame = m_frame ? m_frame->frameCounter() : 0;

    QVector<cw::octree::NodeResidency> stats;
    stats.reserve(m_nodes.size());
    for (int i = 0; i < m_nodes.size(); i++) {
        const NodeRecord& node = m_nodes.at(i);

        cw::octree::NodeResidency residency;
        residency.resident = node.state == NodeState::Resident;
        // streamResources runs before gather, so the counter still sits on the
        // frame the cut was last stamped with.
        residency.selectedThisFrame = node.lastDesiredFrame == frame;
        residency.lastDesiredFrame = node.lastDesiredFrame;
        residency.bytes = node.bytes.size();
        residency.pinned = i == kRootIndex;
        stats.append(residency);
    }

    if (m_profileEnabled) {
        m_profile.residencyStatsUs += elapsedUs(timer);
    }
    return stats;
}

void cwRHIPointCloud::enforceGpuBudget(const cwRenderBudgets& budgets)
{
    const auto* ledger = cwRenderMemoryLedger::instance();
    const qint64 overshoot = ledger->totalBytes(cwRenderMemoryLedger::Residency::Gpu)
                             - budgets.gpuBudgetBytes;

    if (overshoot > 0) {
        const QVector<cw::octree::NodeResidency> stats = residencyStats();
        for (const int index : cw::octree::planNodeEvictions(stats, overshoot)) {
            // Once the nodes the cut dropped run out, the planner offers the
            // ones it still wants. Taking those would only re-request and
            // re-upload them next frame, so they stay and the cut gets coarser
            // instead.
            if (stats.at(index).selectedThisFrame) {
                continue;
            }

            releaseNode(index);

            if (m_profileEnabled) {
                m_profile.evictions++;
            }
        }
    }

    const qint64 total = ledger->totalBytes(cwRenderMemoryLedger::Residency::Gpu);

    // What is left for this cloud: everything that is not point cloud geometry
    // comes off the top by what it holds, and the other clouds by what their
    // cuts want. Their residency is no guide — LRU keeps it at the budget
    // whatever they are drawing.
    const PointCloudDemand others =
        m_frame ? m_frame->pointCloudDemandExcluding(this) : PointCloudDemand{};
    const qint64 cloudGeometryBytes =
        ledger->bytes(cwRenderMemoryLedger::Category::PointCloudGeometry,
                      cwRenderMemoryLedger::Residency::Gpu);
    const qint64 availableBytes = std::max<qint64>(
        0, budgets.gpuBudgetBytes - (total - cloudGeometryBytes) - others.bytes);

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

    if (m_profileEnabled) {
        m_profile.gpuBytes = total;
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

    const auto megabytes = [](qint64 bytes) {
        return QString::number(double(bytes) / double(kBytesPerMegabyte), 'f', kMegabyteDigits);
    };

    QString line = QStringLiteral("render frame=%1 frames=%2 streamFrames=%3")
                       .arg(m_frame ? m_frame->frameCounter() : 0)
                       .arg(frames)
                       .arg(streamFrames);
    line += span(QLatin1StringView("gather"), m_profile.gather, frames);
    line += span(QLatin1StringView("selectNodes"), m_profile.selectNodes, frames);
    line += span(QLatin1StringView("requestLoop"), m_profile.requestLoop, frames);
    line += span(QLatin1StringView("cancelLoop"), m_profile.cancelLoop, frames);
    line += span(QLatin1StringView("stream"), m_profile.stream, streamFrames);
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

    for (int i = 0; i < m_nodes.size(); i++) {
        if (m_nodes.at(i).state == NodeState::Resident) {
            releaseNode(i);
        }
    }

    // The set holds an implicit share of every mirror it was published, so it
    // has to let go here too, or a hidden view's bytes would outlive the nodes.
    publishPickSet();

    // The view that left is gone; whatever brings it back starts from the root.
    m_selected = cw::octree::Selection{};
    m_sseInflation = 1.0;
    m_desiredBytesRelaxed = -1;
    m_pointCappedRelaxed = false;
    m_relaxedSteps = 1;
    m_relaxProbeFrame = 0;
    m_availableBytes = 0;
    if (m_frame) {
        m_frame->clearPointCloudDemand(this);
    }

    publishPointCloudStats();
}

void cwRHIPointCloud::probeRelaxedCut(const cw::octree::SelectionInput& input)
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

    // The step down is only taken on evidence: the cut one step finer, selected
    // for real rather than estimated, and only on a probe frame because
    // selection is not free. A probe keeps going while the finer cut still fits
    // the share this cloud had last frame, so a deep inflation comes back in one
    // probe rather than one probe per step. nextSseInflation() checks the
    // deepest level again against this frame's share before taking it.
    cw::octree::SelectionInput relaxed = input;
    double level = m_sseInflation;
    int steps = 0;

    while (level > 1.0) {
        level = std::max(1.0, level / cw::octree::kSseInflationStep);
        relaxed.sseInflation = level;

        const cw::octree::Selection probe = cw::octree::selectCut(relaxed);
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

bool cwRHIPointCloud::gather(const GatherContext& context, QVector<PipelineBatch>& batches)
{
    if (context.renderPass != RenderPass::PointCloud) {
        return false;
    }

    // A cloud that draws nothing this frame gives its share of the view's
    // budgets back at once, so the clouds that do draw get the whole of it.
    const auto standDown = [this]()
    {
        if (m_frame) {
            m_frame->clearPointCloudDemand(this);
        }
        return false;
    };

    if (m_source.isNull()) {
        return standDown();
    }

    const RenderData& renderData = *context.renderData;
    if (!ensurePipeline(renderData)) {
        return standDown();
    }

    auto* pipeline = m_pipelineRecord ? m_pipelineRecord->pipeline : nullptr;
    if (!pipeline || !m_srb || !m_nodeConstants) {
        return standDown();
    }

    m_profileEnabled = lcProfileRender().isDebugEnabled();
    QElapsedTimer gatherTimer;
    QElapsedTimer partTimer;
    if (m_profileEnabled) {
        gatherTimer.start();
        partTimer.start();
    }

    cw::octree::SelectionInput input = selectionInput(renderData, context.frustum);

    // The point budget and the relax probe belong to the view the user is
    // watching. An export job renders its own camera once, at the detail it
    // asked for, and leaves the live frame's governor alone.
    if (context.liveFrame) {
        // The point budget is the whole view's, so this cloud may only ask for
        // its share of what the other clouds' cuts left of it.
        input.maxPoints = m_frame->pointBudgetShare(this, renderData.budgets.pointBudget);
    }

    m_selected = cw::octree::selectCut(input);

    if (context.liveFrame) {
        probeRelaxedCut(input);
        m_frame->setPointCloudDemand(this, {m_selected.points, m_selected.bytes});
    }

    if (m_profileEnabled) {
        m_profile.selectNodes.add(elapsedUs(partTimer));
        m_profile.cutSizes.append(int(m_selected.nodes.size()));
        m_profile.pointCounts.append(m_selected.points);
        partTimer.restart();
    }

    const quint64 frame = m_frame->frameCounter();

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

    for (const cw::octree::SelectedNode& selected : std::as_const(m_selected.nodes)) {
        if (selected.node < 0 || selected.node >= m_nodes.size()) {
            continue;
        }

        NodeRecord& node = m_nodes[selected.node];
        node.lastDesiredFrame = frame;

        switch (node.state) {
        case NodeState::Resident: {
            Drawable drawable;
            drawable.type = Drawable::Type::NonIndexed;
            drawable.vertexBindings = {
                QRhiCommandBuffer::VertexInput(node.buffer, 0),
                QRhiCommandBuffer::VertexInput(m_nodeConstants,
                                               quint32(node.constantSlot) * kNodeConstantsBytes)
            };
            drawable.vertexCount = m_source.manifest->nodes.at(selected.node).pointCount;
            drawable.instanceCount = 1;
            drawable.bindings = m_srb;
            drawable.globalCameraBinding = 0; // binding 0 = global camera UBO (dynamic offset)
            drawable.appearanceBinding = 1;
            drawable.appearanceUniformOffset = appearanceOffset;
            drawables.append(drawable);
            break;
        }
        case NodeState::Absent: {
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
            break;
        }
    }

    if (m_profileEnabled) {
        m_profile.requestLoop.add(elapsedUs(partTimer));
        partTimer.restart();
    }

    // Nodes asked for by an earlier cut that this one dropped. A pass over the
    // handful of open requests, not over the whole node table.
    for (int i = m_requested.size() - 1; i >= 0; i--) {
        const int index = m_requested.at(i);
        if (m_nodes.at(index).state != NodeState::Requested) {
            m_requested.removeAt(i);
            continue;
        }

        if (m_nodes.at(index).exportRequested) {
            // An offscreen job is waiting on it. Cancelling would throw away a
            // load in flight and the job would ask for it again next frame,
            // which on a slow disk is a wait that never ends.
            continue;
        }

        if (m_nodes.at(index).lastDesiredFrame != frame) {
            m_streamer.cancel(quint32(index));
            m_nodes[index].state = NodeState::Absent;
            m_requested.removeAt(i);

            if (m_profileEnabled) {
                m_profile.cancels++;
            }
        }
    }

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

    // Each point draws as a sprite of worldRadius meters around its position,
    // so the drawn cloud reaches that far past the root cube.
    const float worldRadius = m_renderState.value().worldRadius;
    const QVector3D padding(worldRadius, worldRadius, worldRadius);
    const QBox3D root = m_source.manifest->nodeBounds(kRootIndex);
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
    key.textureBinding = 0xFF;
    key.globalStages = 0x1;
    key.perDrawStages = 0x1;
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

    const cwDiskCacher cacher{QDir(source.cacheRootPath)};
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
