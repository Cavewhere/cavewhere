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
#include <array>
#include <deque>
#include <memory>

// Qt includes
#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
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

    // The loads gather() just asked for count too, and so does a drawn node
    // whose reveal ramp (cwPointCloudShading::revealFrames) has frames left
    // to draw. A queue waiting on the eviction hold for a slot is left for the
    // hold's release.
    bool hasStreamingWork() const override;

    bool residencyReady(const RenderData& jobRenderData) override;
    void releaseStreamedResources() override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;

    // Drops the cut nothing draws any more and cancels the loads it asked for,
    // so a cloud that leaves the live view settles instead of streaming in
    // nodes no one will see. An offscreen job's culled tiles leave it alone.
    void gatherCulled(const GatherContext& context) override;
    bool usesPointCloudPass() const override;
    void purgePipelinesFor(QRhiRenderPassDescriptor* descriptor) override;

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

    // A resident node's fade under the reveal ramp (cwPointCloudShading::
    // revealFrames), apart from its arrival's. The values are the ones
    // PointCloud.vert reads from nodeFade.
    enum class NodeFade : quint32 {
        None = 0,
        Out = 1,
        In = 2
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

        // The world-space floor this node's sprites cover, in meters, as its
        // constants slot holds it — what the GPU draws the node with. The live
        // gather writes it here and into the slot on the frame it works it
        // out, on the batch that frame's draws consume.
        float floorSpacing = 0.0f;

        // The reveal ramp's state (cwPointCloudShading::revealFrames): the
        // frame the node became resident, which its fade-in counts from, and
        // the floor it drew with before floorSpacing last moved and the frame
        // that move happened, which its floor ramp counts from. The constants
        // slot holds the same values for the shader.
        quint64 arrivalFrame = 0;
        float previousFloorSpacing = 0.0f;
        quint64 floorChangedFrame = 0;

        // How the node's weight fades on top of its arrival's fade-in, from
        // fadeFrame on: Out after it left the live cut, still drawing while
        // its ancestors' floors grow over the cells it covered, and In after
        // it came back into the cut already resident. The constants slot
        // holds both.
        quint64 fadeFrame = 0;
        NodeFade fade = NodeFade::None;

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
                    const cwPointOctreeNodePayload& payload, bool evictionHold);

    // A free constants slot, evicting the coldest resident node outside the cut
    // to make one when the stack is empty unless @a evictionHold. -1 when
    // nothing could be freed.
    int takeConstantSlot(bool evictionHold);

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

    QVector<cw::octree::NodeResidency> residencyStats() const;
    void enforceGpuBudget(const cwRenderBudgets& budgets);

    // The frame the cut was last stamped with. streamResources runs before
    // gather, so it still names that frame for everything before the next cut.
    quint64 currentFrame() const;

    // The frame's forest: the cut every visible cloud shares and the governor
    // that sizes it. An empty forest stands in for a cloud no frame renderer
    // has taken yet, which reads as an uninflated view of nothing.
    const cwPointCloudForest& forest() const;

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

    // Mirrors the PerCloudBlock declaration in PointCloud.vert, which at the
    // default knobs sizes every sprite as
    // max(spacingCoverage * nodeFloor * pixelsPerMeter(w),
    //     spacingCoverage * sseThresholdPx)
    // pixels, clamped to maxPointSizePx, the floor coming per instance, and
    // thins each node against the spacing the cut is aiming for, never wider
    // than rootSpacing — the coarsest level the file has. The second std140 row
    // carries the cwPointCloudShading modes that switch those rules, the third
    // the debug view, the frame the residency age view counts from, the
    // spacing source every node sizes and thins by, and the frames the reveal
    // ramp spans; the fourth what an arrival does over that ramp, padded out
    // to a whole row.
    struct PerCloudUniform {
        float spacingCoverage = 0.0f;
        float sseThresholdPx = 0.0f;
        float rootSpacing = 0.0f;
        float maxPointSizePx = 0.0f;
        qint32 thinningMode = 0;
        qint32 sizeMode = 0;
        qint32 floorMode = 0;
        float fixedPointSizePx = 0.0f;
        qint32 debugView = 0;
        quint32 currentFrame = 0;
        qint32 spacingSource = 0;
        qint32 revealFrames = 0;
        qint32 revealMode = 0;
        std::array<qint32, 3> padding {};
    };
    static_assert(sizeof(PerCloudUniform) == 16 * sizeof(float),
                  "PerCloudBlock in PointCloud.vert is four std140 rows of 16 "
                  "bytes; the C++ struct has to match it field for field.");

    // The block the shader reads for @a spacingCoverage, carrying this frame's
    // refine threshold along with it.
    PerCloudUniform appearanceUniform(float spacingCoverage) const;

    // Takes each drawn node's floor from cw::octree::finestDrawnSpacings over
    // the cut's resident @a drawnNodes and its still-streaming @a missingNodes,
    // and writes the ones that moved onto @a batch, ahead of this frame's draws.
    // @a revealClock is the frame the shader reads this draw at, which a moved
    // floor's ramp counts from.
    void writeNodeFloors(QRhiResourceUpdateBatch* batch, const QVector<int>& drawnNodes,
                         const QVector<int>& missingNodes, quint64 revealClock);

    // Writes its own spacing for every resident node of @a previous that this
    // frame's cut dropped (it carries no @a frame stamp) and that is not
    // fading out under the reveal ramp, so an export job that
    // draws it out of the live cut sees the floor uploadNode() lands a fresh
    // arrival with, rather than one its children gave it. A culled frame leaves
    // the floors alone, so the cloud comes back at the sprite size it had.
    void restoreOwnFloorsLeavingCut(QRhiResourceUpdateBatch* batch,
                                    const cw::octree::Selection& previous, quint64 frame,
                                    quint64 revealClock);

    // @a index's own floor spacing under the shading's spacing source
    // (cw::clod::ownFloorSpacing), the floor a node draws with while nothing
    // under it counts: a leaf whose ancestors are all resident holds the raw
    // data of its cell with them.
    float ownSpacing(int index) const;

    // How a floor reaches its new value under the reveal ramp: Ramped
    // travels there over revealFrames frames, AtOnce takes it on this frame
    enum class FloorMove {
        Ramped,
        AtOnce
    };

    // Writes @a floorSpacing into @a index's constants slot unless it already
    // holds it, along with where its reveal ramp starts: the floor the node
    // drew with on the frame before @a revealClock, or none for @a move
    // AtOnce.
    void writeNodeFloor(QRhiResourceUpdateBatch* batch, int index, float floorSpacing,
                        quint64 revealClock, FloorMove move = FloorMove::Ramped);

    // Whether @a node's fade-in, floor ramp, or fade-out still has frames to
    // draw after the frame the shader reads at @a revealClock
    bool nodeRevealing(const NodeRecord& node, quint64 revealClock) const;

    // The loads and uploads still to come: hasStreamingWork() without the
    // reveal ramp, which is what streamResources() can know before gather()
    bool hasLoadingWork() const;

    // Writes @a fade into @a index's constants slot, stamped at @a fadeFrame
    void writeFade(QRhiResourceUpdateBatch* batch, int index, NodeFade fade, quint64 fadeFrame);

    // Starts @a index fading @a fade (In or Out) on @a revealClock, carrying
    // on from the share of its weight its current fade drew last frame, so a
    // node that turns around mid-fade keeps its weight instead of jumping
    void startFade(QRhiResourceUpdateBatch* batch, int index, NodeFade fade, quint64 revealClock);

    // The share of its weight @a node's fade (apart from its arrival's)
    // draws on the frame the shader reads at @a revealClock
    double fadeShare(const NodeRecord& node, quint64 revealClock) const;

    // Under the reveal ramp: fades in every resident node of @a drawnNodes
    // that @a previous did not hold and that did not arrive this frame,
    // starts a fade-out for every resident node of @a previous this frame's
    // cut dropped, and ends the fade-outs evicted, back in the cut, or faded
    // out.
    void updateFades(QRhiResourceUpdateBatch* batch, const cw::octree::Selection& previous,
                     const QVector<int>& drawnNodes, quint64 frame, quint64 revealClock);

    // @a index, which the last frame drew, is gone: its nearest ancestor in
    // this @a frame's cut takes a rising floor at once rather than over the
    // ramp, since nothing else covers that node's cells any more
    void closeOverReleasedNode(QRhiResourceUpdateBatch* batch, int index, quint64 frame);

    // Ends @a index's fade-out and gives it back its own spacing as its floor
    void finishDeparture(QRhiResourceUpdateBatch* batch, int index, quint64 revealClock);

    // Ends every fade-out at once: the ramp was turned off or nothing draws
    // the cloud
    void endDepartures(QRhiResourceUpdateBatch* batch, quint64 revealClock);

    // The node box overlay (cwPointCloudShading::showNodeBoxes): a wire cube
    // per node of the last cut and per resident node outside it, drawn in the
    // Overlay pass so the boxes skip EDL. Loading nodes have no constants slot
    // yet, so the boxes read their own instance buffer, rebuilt each frame the
    // overlay is on from the cut and the node table.
    void writeNodeBoxes(QRhi* rhi, QRhiResourceUpdateBatch* batch);
    bool gatherNodeBoxes(const GatherContext& context, QVector<PipelineBatch>& batches);
    bool ensureNodeBoxPipeline(const RenderData& data);

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
    // Derived from m_source in resetNodes(); keys nodes without QFileInfo
    cw::octree::NodeKeyBase m_nodeKeyBase;

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

    // The last streamResources() held a ready payload back because the
    // eviction hold left the slot pool full.
    bool m_waitingForSlot = false;

    // The last live gather drew a node whose reveal ramp has frames left
    bool m_revealing = false;

    // The resident nodes fading out after leaving the live cut, which the
    // live gather draws beside the cut until they have faded to nothing
    QVector<int> m_departingNodes;

    // {nodeMin.xyz, nodeSize / kQuantMax}, {floorSpacing, arrival frame,
    // node index, measured spacing} and {previous floor, floor changed frame,
    // fade frame, fade} per resident node, one 48 byte slot each,
    // bound per instance at the drawing node's slot offset.
    QRhiBuffer* m_nodeConstants = nullptr;
    QVector<int> m_freeSlots;

    // Slots the constants buffer is built with, kMaxResidentNodes in the app.
    // A shallower pool is what makes the eviction fallback reachable, which is
    // how a test exercises it without a cloud of a hundred thousand nodes.
    int m_maxResidentNodes;

    cwLedgeredBytes m_gpuBytes {cwRenderMemoryLedger::Category::PointCloudGeometry,
                                cwRenderMemoryLedger::Residency::Gpu};
    cwLedgeredBytes m_mirrorBytes {cwRenderMemoryLedger::Category::PointCloudGeometry,
                                   cwRenderMemoryLedger::Residency::Cpu};

    // This frame's cut — the forest's slice for this cloud on a live frame, its
    // own selectCut() on an export frame: its nodes, and what drawing it would
    // cost in points and bytes.
    cw::octree::Selection m_selected;

    // The projected spacing the cut refines to — the view's screenSpaceErrorPx
    // times the forest's inflation — which is what PointCloud.vert turns back
    // into a world spacing at each vertex's own depth. Render-thread only.
    float m_sseThresholdPx = float(cw::budgets::kDefaultScreenSpaceErrorPx);

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

    // The node box overlay: the unit cube's 24 edge ends, built once, and one
    // instance per box, grown as the box count needs. m_nodeBoxCount is what
    // the last writeNodeBoxes() put in it, 0 while the overlay is off.
    QRhiBuffer* m_nodeBoxCube = nullptr;
    QRhiBuffer* m_nodeBoxInstances = nullptr;
    int m_nodeBoxCapacity = 0;
    int m_nodeBoxCount = 0;
    QRhiVertexInputLayout m_nodeBoxInputLayout;
    QRhiShaderResourceBindings* m_nodeBoxSrb = nullptr;
    cwRhiPipelineSet m_nodeBoxPipelines;
    cwRhiPipelineRecord* m_nodeBoxPipelineRecord = nullptr;
};

#endif // CWRHIPOINTCLOUD_H
