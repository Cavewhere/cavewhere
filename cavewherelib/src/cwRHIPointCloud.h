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
#include "cwPointOctreeSelection.h"
#include "cwPointOctreeSource.h"
#include "cwRHIObject.h"
#include "cwRenderMemoryLedger.h"
#include "cwRenderPointCloud.h"
#include "cwRhiFrameRenderer.h"
#include "cwTileStreamer.h"

// Qt includes
#include <QByteArray>
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
    cwRHIPointCloud();
    ~cwRHIPointCloud() override;

    void initialize(const ResourceUpdateData& data) override;
    void synchronize(const SynchronizeData& data) override;
    void updateResources(const ResourceUpdateData& data) override;
    bool streamResources(ResourceUpdateData& data, qint64& remainingUploadBytes) override;
    bool residencyReady(const RenderData& jobRenderData) override;
    void releaseStreamedResources() override;
    bool gather(const GatherContext& context, QVector<PipelineBatch>& batches) override;
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

        int constantSlot = -1;
        quint64 lastDesiredFrame = 0;

        // An offscreen job asked for this node through residencyReady(). The
        // live cut cancels the requests it no longer wants, and an export whose
        // camera differs from the live one would lose every load it started;
        // this flag exempts it until the node lands, fails, or is released.
        bool exportRequested = false;
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

    using NodeStreamer = cwTileStreamer<cwPointOctreeNodeSource, QByteArray>;

    // The streamer's loader, on a cwConcurrent worker. Static and capturing
    // nothing: everything it touches travels on the source, and cwDiskCacher
    // is thread-safe.
    static Monad::Result<QByteArray> loadNode(const cwPointOctreeNodeSource& source, int level);

    void initializeResources(const ResourceUpdateData& data);
    bool ensurePipeline(const RenderData& data);
    bool ensureShaderResources(QRhi* rhi, cwRhiItemRenderer* renderer);
    cwRhiPipelineKey buildPipelineKey(QRhiRenderPassDescriptor* renderPassDescriptor,
                                      int sampleCount) const;

    // Throws away the whole node table and everything it holds, for a source
    // that no longer describes the same octree.
    void resetNodes();

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
                    const QByteArray& bytes);

    // A free constants slot, evicting resident nodes worth @a incomingBytes to
    // make one when the stack is empty. -1 when nothing could be freed.
    int takeConstantSlot(qint64 incomingBytes);

    QVector<cw::octree::NodeResidency> residencyStats() const;
    void enforceGpuBudget(const cwRenderBudgets& budgets);

    // cwAppearanceSlotted: grow m_perCloudUBO to @a slotCount slots, deferring
    // deletion of the prior buffer + SRB (m_retiredBuffers/m_retiredSrbs, flushed
    // next updateResources) so draws already recorded this frame keep valid
    // pointers. Re-writes the live slot 0 onto @a batch.
    void resizeAppearanceSlots(QRhi* rhi, QRhiResourceUpdateBatch* batch,
                               int slotCount) override;

    // Write one PerCloudUniform (just @a worldRadius today) into @a slot.
    void writeAppearanceSlot(QRhiResourceUpdateBatch* batch, int slot, float worldRadius);

    // std140 rounds a uniform block to a multiple of 16 bytes; pad three
    // floats so the C++ struct matches the shader-side block size. Mirrors
    // the PerCloudBlock declaration in PointCloud.vert.
    struct PerCloudUniform {
        float worldRadius = 0.0f;
        float pad[3] = {0.0f, 0.0f, 0.0f};
    };

    bool m_resourcesInitialized = false;

    // One layout for the whole class: binding 0 is a node's quantized points,
    // binding 1 the shared per-instance constants. Built once in initialize().
    QRhiVertexInputLayout m_inputLayout;

    // Per-cloud uniform block (binding 1): world-space sprite radius in meters,
    // one aligned slot per appearance slot, bound with a dynamic offset so an
    // offscreen job can render the cloud at an overridden radius without disturbing
    // the live view (slot 0). Steady state is ONE slot (the live radius); the pool
    // (cwAppearanceSlotted) grows it on demand to the concurrent-override high-water
    // mark, so an interactive session pays one slot per cloud, not kAppearanceSlotCount.
    // m_perCloudStride is the aligned byte size of one slot, also the dynamic-offset
    // granularity.
    QRhiBuffer* m_perCloudUBO = nullptr;
    quint32 m_perCloudStride = 0;
    QRhiShaderResourceBindings* m_srb = nullptr;

    cwPointOctreeSource m_source;

    // Parallel to m_source.manifest->nodes.
    QVector<NodeRecord> m_nodes;

    // Indices of the nodes in NodeState::Requested, so cancelling the ones the
    // cut dropped is a pass over a handful of entries, not the whole table.
    QVector<int> m_requested;

    NodeStreamer m_streamer;

    // Loads the budget refused this frame. The streamer already handed them
    // over, so they wait here rather than being asked for again.
    QVector<NodeStreamer::Result> m_readyQueue;

    // {nodeMin.xyz, nodeSize / kQuantMax} per resident node, one 16 byte slot
    // each, bound per instance at the drawing node's slot offset.
    QRhiBuffer* m_nodeConstants = nullptr;
    QVector<int> m_freeSlots;

    // This view's screen-space-error multiplier: raised while the cut it wants
    // outruns the GPU budget, lowered once the ledger has room to spare.
    double m_sseInflation = 1.0;

    cwLedgeredBytes m_gpuBytes {cwRenderMemoryLedger::Category::PointCloudGeometry,
                                cwRenderMemoryLedger::Residency::Gpu};
    cwLedgeredBytes m_mirrorBytes {cwRenderMemoryLedger::Category::PointCloudGeometry,
                                   cwRenderMemoryLedger::Residency::Cpu};

    // This frame's cut, from gather()'s selectNodes().
    QVector<cw::octree::SelectedNode> m_selected;

    cwTracked<cwRenderPointCloud::RenderState> m_renderState;
};

#endif // CWRHIPOINTCLOUD_H
