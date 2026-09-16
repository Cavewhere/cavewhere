/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRHIPOINTCLOUD_H
#define CWRHIPOINTCLOUD_H

// Our includes
#include "cwAppearanceSlotted.h"
#include "cwDiskCacher.h"
#include "cwPointOctreePickIndex.h"
#include "cwPointOctreePickSet.h"
#include "cwPointOctreeSelection.h"
#include "cwPointOctreeSource.h"
#include "cwRHIObject.h"
#include "cwRenderFrameStats.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderPointCloud.h"
#include "cwRhiFrameRenderer.h"
#include "cwTileStreamer.h"

// Std includes
#include <algorithm>
#include <deque>
#include <memory>

// Qt includes
#include <QByteArray>
#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>

class cwRhiItemRenderer;

/**
 * Draws one point cloud out of the octree nodes it streams off disk: the cut
 * through the tree this frame's camera wants (cw::octree::selectNodes), one
 * Immutable vertex buffer per resident node, and one shared per-instance
 * buffer holding every node's dequantization constants.
 *
 * Every method but synchronize() runs on the render thread; synchronize() runs
 * there too, with the GUI thread parked at the sync barrier.
 */
class cwRHIPointCloud : public cwRHIObject, public cwAppearanceSlotted
{
    friend struct CwRhiPointCloudTestAccess;

public:
    //! @a pickSet is the cloud's pick provider, shared with the
    //! cwRenderPointCloud that registered it. Publishing into it after that
    //! object is gone is harmless — the set simply has no reader left.
    explicit cwRHIPointCloud(std::shared_ptr<cwPointOctreePickSet> pickSet);
    ~cwRHIPointCloud() override;

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    bool streamResources(ResourceUpdateData& data, qint64& remainingUploadBytes) override;
    bool residencyReady(const RenderData& jobRenderData) override;
    void releaseStreamedResources() override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

    // Drops the cut nothing draws any more and cancels the loads it asked for,
    // so a cloud that leaves the live view settles instead of streaming in
    // nodes no one will see. An offscreen job's culled tiles leave it alone.
    void gatherCulled(const GatherContext& context) override;
    bool usesPointCloudPass() const override;

    // The octree's root cube, inflated by the sprite radius. The manifest's
    // root cube is in the project's local frame CRS and PointCloud.vert
    // multiplies dequantized positions by the view-projection alone — no model
    // matrix — so the box is already world space. nullopt without a source.
    std::optional<QBox3D> worldBounds() const override;
    cwAppearanceSlotted* appearanceSlots() override { return this; }

    // cwAppearanceSlotted: unpack a cwPointCloudAppearance from the opaque payload
    // and write it (world radius) into one slot of m_perCloudUBO.
    void uploadAppearance(QRhiResourceUpdateBatch* batch, int slot,
                          const cwAppearanceOverride& override) override;

private:
    enum class NodeState {
        Absent,
        Requested,
        Resident,
        Failed
    };

    // One entry of the node table, parallel to the manifest's node list.
    struct NodeRecord {
        NodeState state = NodeState::Absent;
        QRhiBuffer* buffer = nullptr;

        // The very bytes uploaded, kept as an implicit share rather than a
        // second allocation, so Q4's picking can read the node's points
        // without going back to disk. Released with the node.
        QByteArray bytes;

        // The leaf and group boxes over those bytes, built on the worker that
        // loaded them so a pick descends instead of scanning. Released with the
        // node, and counted with it in the CPU ledger.
        cwPointOctreePickIndex index;

        int constantSlot = -1;
        quint64 lastDesiredFrame = 0;

        // The finest sample spacing drawn under this node, in meters — the
        // world-space floor its sprites cover. Holds the value written into the
        // node's constants slot, or the one queued for it in m_pendingFloors.
        float floorSpacing = 0.0f;

        // Where the node sits in m_residentIndices, so releasing it is a
        // swap-remove instead of a search. -1 while the node is not resident.
        int residentPosition = -1;

        // An offscreen job asked for this node through residencyReady(). The
        // live cut cancels the requests it no longer wants, and an export whose
        // camera differs from the live one would lose every load it started;
        // this flag exempts it until the node lands, fails, or is released.
        bool exportRequested = false;
    };

    // One resident node that has left the cut, as of the frame it left in.
    struct ColdEntry {
        quint64 frame = 0;
        int node = -1;
    };

    // What the streamer loads for one node. The cache root travels with it so
    // the loader lambda captures nothing and stays safe on a worker.
    struct cwPointOctreeNodeSource {
        QString cacheRootPath;
        cwDiskCacher::Key key;
        qint64 byteSize = 0;

        bool operator==(const cwPointOctreeNodeSource& other) const
        {
            return byteSize == other.byteSize
                   && cacheRootPath == other.cacheRootPath
                   && key.id == other.key.id
                   && key.path == other.key.path
                   && key.checksum == other.key.checksum;
        }
    };

    using NodeStreamer = cwTileStreamer<cwPointOctreeNodeSource, cwPointOctreeNodePayload>;

    // The streamer's loader, on a cwConcurrent worker. Static and capturing
    // nothing: everything it touches travels on the source, and cwDiskCacher
    // is thread-safe.
    static Monad::Result<cwPointOctreeNodePayload> loadNode(const cwPointOctreeNodeSource& source,
                                                            int level);

    void initializeResources(const ResourceUpdateData& data);
    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;

    // Throws away the whole node table and everything it holds, for a source
    // that no longer describes the same octree.
    void resetNodes(const cwPointOctreeSource& source);

    // Frees one resident node: its buffer, its constants slot, and its mirror.
    void releaseNode(int index);

    // True while the node is one the streamer has an open load for, so a
    // landed payload still belongs to it.
    bool isRequested(int index) const;

    // Asks the streamer for the node and records the open request.
    void requestNode(int index, quint64 priority);

    cwPointOctreeNodeSource nodeSource(int index) const;

    cw::octree::SelectionInput selectionInput(const RenderData& renderData,
                                              const cwFrustum* frustum) const;

    // Creates the node's vertex buffer, writes its dequantization constants,
    // and marks it resident. False when no constants slot could be freed for
    // it, which leaves the node Absent to be asked for again.
    bool uploadNode(QRhi* rhi, QRhiResourceUpdateBatch* batch, int index,
                    const cwPointOctreeNodePayload& payload);

    // A free constants slot, evicting the coldest resident node outside the cut
    // to make one when the stack is empty. -1 when nothing could be freed.
    int takeConstantSlot();

    // Records every node the previous cut held that this frame's cut dropped,
    // keyed by the frame it was last wanted in, which is what evictColdestNode
    // draws on.
    void recordColdNodes(const cw::octree::Selection& previous, quint64 frame);

    // Remembers @a index as evictable as of @a frame. The root is pinned and
    // never recorded.
    void recordColdNode(int index, quint64 frame);

    // True once @a entry's node has come back into the cut or been released, so
    // the entry says nothing about it any more.
    bool coldEntryStale(const ColdEntry& entry) const;

    // Drops the stale entries, so the queue stays proportional to residency.
    void compactColdNodes();

    // Releases the coldest resident node outside the cut, freeing its constants
    // slot. False when the queue holds nothing still evictable.
    bool evictColdestNode();

    // Hands the pick set the nodes that are resident now. Called once per frame
    // at most, from the places residency changes.
    void publishPickSet();

    //! Publishes this frame's node residency, cut size, and SSE inflation for
    //! the HUD, and only when one of them moved
    void publishPointCloudStats();

    // Cancels every open request that @a frame's cut did not ask for, and
    // returns how many were canceled. A request an offscreen job flagged for
    // export survives.
    int cancelRequestsNotDesiredThisFrame(quint64 frame);

    // Every kSseRelaxProbeFrames frames while inflated, re-selects @a input one
    // step finer and records what that cut would cost, which is the only
    // evidence nextSseInflation() steps down on.
    void probeRelaxedCut(const cw::octree::SelectionInput& input);

    QVector<cw::octree::NodeResidency> residencyStats() const;
    void enforceGpuBudget(const cwRenderBudgets& budgets);

    // The frame the cut was last stamped with. streamResources runs before
    // gather, so it still names that frame for everything before the next cut.
    quint64 currentFrame() const;

    // What one cw.profile.render line summarizes: kProfileBlockFrames frames of
    // render-thread timings and counters. Filled only while the category is on.
    struct ProfileBlock {
        // One timed span across the block: enough for its mean and its worst frame.
        struct Span {
            qint64 totalUs = 0;
            qint64 maxUs = 0;

            void add(qint64 microseconds)
            {
                totalUs += microseconds;
                maxUs = std::max(maxUs, microseconds);
            }

            double meanUs(int frames) const
            {
                return frames > 0 ? double(totalUs) / double(frames) : 0.0;
            }
        };

        // gather() and streamResources() run once a frame each, but either can
        // return early, so each side counts its own frames and each mean is
        // divided by the count that belongs to it.
        int frames = 0;
        int streamFrames = 0;

        Span gather;
        Span selectNodes;
        Span requestLoop;
        Span cancelLoop;
        Span stream;
        Span upload;
        Span publishPick;
        Span publishStats;
        Span enforceBudget;

        // Block totals rather than per-frame spans: both run many times a frame.
        qint64 slotEvictUs = 0;
        qint64 residencyStatsUs = 0;

        // One entry per frame, for the block's median cut size.
        QVector<int> cutSizes;

        // Wall microseconds between consecutive live gathers, one entry per
        // frame, for the block's median and 95th-percentile frame time. The
        // first frame of a block has no predecessor and adds nothing.
        QVector<qint64> frameUs;

        int requests = 0;
        int cancels = 0;
        int uploads = 0;
        int evictions = 0;

        // State the block reports as it stood on its last frame.
        int residentNodes = 0;
        int pendingLoads = 0;
        double sseInflation = 1.0;
        qint64 gpuBytes = 0;
        qint64 gpuBudgetBytes = 0;

        // Points in the cut, one entry per frame, for the block's median and max.
        QVector<qint64> pointCounts;
    };

    //! Writes the block as one cw.profile.render line and starts the next one.
    void flushProfileBlock();

    //! Flushes once either side of the frame has filled the block.
    void maybeFlushProfileBlock();

    // cwAppearanceSlotted: grow m_perCloudUBO to @a slotCount slots, deferring
    // deletion of the prior buffer + SRB (m_retiredBuffers/m_retiredSrbs, flushed
    // next updateResources) so draws already recorded this frame keep valid
    // pointers. Re-writes the live slot 0 onto @a batch.
    void resizeAppearanceSlots(QRhi* rhi, QRhiResourceUpdateBatch* batch,
                               int slotCount) override;

    // Write one PerCloudUniform into @a slot.
    void writeAppearanceSlot(QRhiResourceUpdateBatch* batch, int slot,
                             float spacingCoverage);

    // Write the live render state into slot 0 and mark it current.
    void writeLiveAppearanceSlot(QRhiResourceUpdateBatch* batch);

    // Take the refine threshold the shader sizes against from @a budgets and
    // this cloud's inflation, marking the live slot behind when it moved.
    void refreshSseThreshold(const cwRenderBudgets& budgets);

    // Mirrors the PerCloudBlock declaration in PointCloud.vert, which sizes
    // every sprite as
    // max(spacingCoverage * nodeFloor.x * pixelsPerMeter(w),
    //     spacingCoverage * sseThresholdPx)
    // pixels, the floor coming per instance. The two live floats carry two
    // explicit pad floats so the struct is std140's 16 bytes, which is what the
    // block rounds up to.
    struct PerCloudUniform {
        float spacingCoverage = 0.0f;
        float sseThresholdPx = 0.0f;
        float padding0 = 0.0f;
        float padding1 = 0.0f;
    };
    static_assert(sizeof(PerCloudUniform) == 4 * sizeof(float),
                  "PerCloudBlock in PointCloud.vert rounds to std140's 16 "
                  "bytes; the C++ struct has to match it, padding included.");

    // The block the shader reads for @a spacingCoverage, carrying this frame's
    // refine threshold along with it.
    PerCloudUniform appearanceUniform(float spacingCoverage) const;

    // Writes every floor the last live gather queued, and empties the queue.
    void flushNodeFloors(QRhiResourceUpdateBatch* batch);

    // Takes each drawn node's floor from the finest level drawn under it and
    // queues the ones that moved for the next resource pass.
    void refreshNodeFloors(const QVector<int>& drawnNodes);

    bool m_resourcesInitialized = false;

    // One layout for the whole class: binding 0 is a node's quantized points,
    // binding 1 the shared per-instance constants. Built once in initialize().
    QRhiVertexInputLayout m_inputLayout;

    // Per-cloud uniform block (binding 1): the spacing rule the shader sizes
    // against, one aligned slot per appearance slot, bound with a dynamic
    // offset so an offscreen job can render the cloud at an overridden
    // coverage without disturbing the live view (slot 0). Steady state is ONE
    // slot (the live coverage); the pool
    // (cwAppearanceSlotted) grows it on demand to the concurrent-override high-water
    // mark, so an interactive session pays one slot per cloud, not kAppearanceSlotCount.
    // m_perCloudStride is the aligned byte size of one slot, also the dynamic-offset
    // granularity.
    QRhiBuffer* m_perCloudUBO = nullptr;
    quint32 m_perCloudStride = 0;
    QRhiShaderResourceBindings* m_srb = nullptr;

    cwPointOctreeSource m_source;

    // What picking reads. Set from the render object at construction; the
    // render thread republishes it whenever residency moves, which
    // m_residencyChanged coalesces to one publish per frame.
    std::shared_ptr<cwPointOctreePickSet> m_pickSet;
    bool m_residencyChanged = false;

    // Parallel to m_source.manifest->nodes.
    QVector<NodeRecord> m_nodes;

    // Every node in NodeState::Resident, exactly once and in no order, so the
    // residency walks read this instead of the whole table — on a big cloud, two
    // orders of magnitude shorter. NodeRecord::residentPosition points back, so
    // releasing a node is a swap-remove. m_residentCount is its size, kept beside
    // it so the per-frame stats need no walk at all.
    QVector<int> m_residentIndices;
    int m_residentCount = 0;

    // The resident nodes outside the current cut, coldest first: every node
    // leaving one frame's cut carries that frame's stamp, so pushing at the back
    // keeps the queue sorted. Entries are invalidated lazily, against the node
    // itself, rather than by searching the queue.
    std::deque<ColdEntry> m_coldNodes;

    // What the last publishPointCloudStats() sent, so a frame that moved nothing
    // leaves the HUD's revision standing still. m_statsPublished is false until
    // the first publish, so a cloud that opens on the default numbers still
    // states them rather than leaving another cloud's on the singleton.
    cwRenderFrameStats::PointCloud m_publishedStats;
    bool m_statsPublished = false;

    // Indices of the nodes in NodeState::Requested, so cancelling the ones the
    // cut dropped is a pass over a handful of entries, not the whole table.
    QVector<int> m_requested;

    NodeStreamer m_streamer;

    // Loads the budget refused this frame. The streamer already handed them
    // over, so they wait here rather than being asked for again.
    QVector<NodeStreamer::Result> m_readyQueue;

    // {nodeMin.xyz, nodeSize / kQuantMax} and {floorSpacing, 0, 0, 0} per
    // resident node, one 32 byte slot each, bound per instance at the drawing
    // node's slot offset.
    QRhiBuffer* m_nodeConstants = nullptr;
    QVector<int> m_freeSlots;

    // Slots the constants buffer is built with, kMaxResidentNodes in the app.
    // A shallower pool is what makes the eviction fallback reachable, which is
    // how a test exercises it without a cloud of a hundred thousand nodes.
    int m_maxResidentNodes;

    // This view's screen-space-error multiplier: raised while the cut it wants
    // outruns this view's share of the GPU budget or the point budget, lowered
    // once a probed cut one step finer fits again.
    double m_sseInflation = 1.0;

    // The probe behind the step down: the bytes of the cut one step finer and
    // whether the point budget capped it, refreshed every
    // kSseRelaxProbeFrames frames while inflated. -1 means not probed.
    qint64 m_desiredBytesRelaxed = -1;
    bool m_pointCappedRelaxed = false;
    int m_relaxedSteps = 1;
    int m_relaxProbeFrame = 0;

    // This cloud's share of the GPU byte budget as of the last enforceGpuBudget,
    // which is what the next probe measures its relaxations against.
    qint64 m_availableBytes = 0;

    cwLedgeredBytes m_gpuBytes {cwRenderMemoryLedger::Category::PointCloudGeometry,
                                cwRenderMemoryLedger::Residency::Gpu};
    cwLedgeredBytes m_mirrorBytes {cwRenderMemoryLedger::Category::PointCloudGeometry,
                                   cwRenderMemoryLedger::Residency::Cpu};

    // This frame's cut, from gather()'s selectCut(): its nodes, and what
    // drawing it would cost in points and bytes.
    cw::octree::Selection m_selected;

    // The projected spacing the cut refines to — the view's screenSpaceErrorPx
    // times this cloud's inflation — which is what PointCloud.vert turns back
    // into a world spacing at each vertex's own depth. Render-thread only.
    float m_sseThresholdPx = float(cw::budgets::kDefaultScreenSpaceErrorPx);

    // Nodes whose floor moved on the last live gather, waiting for a resource
    // pass to write it into their constants slot. The same one-frame lag the
    // per-cloud floor had before it went per instance.
    QVector<int> m_pendingFloors;

    // m_sseThresholdPx moved, so slot 0 is behind the view; the next resource
    // pass rewrites it.
    bool m_liveAppearanceStale = false;

    // cw.profile.render, read once per gather() and once per streamResources()
    // and consulted by the helpers they call, so a disabled category costs one
    // branch. The block is mutable because the const helpers time themselves.
    bool m_profileEnabled = false;
    mutable ProfileBlock m_profile;

    // Started on each live gather, so the next one reads the frame's period.
    QElapsedTimer m_frameIntervalTimer;

    cwTracked<cwRenderPointCloud::RenderState> m_renderState;
};

#endif // CWRHIPOINTCLOUD_H
