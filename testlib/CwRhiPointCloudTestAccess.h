/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRHIPOINTCLOUDTESTACCESS_H
#define CWRHIPOINTCLOUDTESTACCESS_H

//Our includes
#include "cwRHIPointCloud.h"

//Std includes
#include <optional>

// Friend accessor (declared `friend struct CwRhiPointCloudTestAccess` in
// cwRHIPointCloud.h) for the node table the render thread streams into.
// Nothing in production reads it from outside the class, so it stays private
// rather than growing accessors the app would never call. Mirrors the shape of
// CwRhiTexturedItemsTestAccess.
struct CwRhiPointCloudTestAccess {
    using NodeState = cwRHIPointCloud::NodeState;
    using PerCloudUniform = cwRHIPointCloud::PerCloudUniform;

    static NodeState nodeState(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return NodeState::Absent;
        }
        return cloud.m_nodes.at(index).state;
    }

    static int nodeCount(const cwRHIPointCloud& cloud) {
        return int(cloud.m_nodes.size());
    }

    // The count the render thread keeps as residency moves, rather than a walk
    // of the table, which is what makes it worth checking against one.
    static int residentCount(const cwRHIPointCloud& cloud) {
        return cloud.m_residentCount;
    }

    // The resident list itself, so a test can hold it against the node states.
    static QVector<int> residentIndices(const cwRHIPointCloud& cloud) {
        return cloud.m_residentIndices;
    }

    // Builds the constants pool with @a slotCount rather than the app's 32 768.
    // Call it before the first frame: initializeResources reads it once. A pool
    // shallower than the cut is what puts an upload on the eviction fallback,
    // which is otherwise out of reach on anything but a hundred-thousand-node
    // cloud.
    static void setMaxResidentNodes(cwRHIPointCloud& cloud, int slotCount) {
        cloud.m_maxResidentNodes = slotCount;
    }

    static int maxResidentNodes(const cwRHIPointCloud& cloud) {
        return cloud.m_maxResidentNodes;
    }

    static int freeSlotCount(const cwRHIPointCloud& cloud) {
        return int(cloud.m_freeSlots.size());
    }

    // Entries in the cold queue an upload draws its constants slot from, so a
    // test can pin that the stale ones are swept rather than piling up.
    static int coldNodeCount(const cwRHIPointCloud& cloud) {
        return int(cloud.m_coldNodes.size());
    }

    // The governor is the frame's, shared by every cloud of the view, so this
    // reaches it through the cloud the test already has in hand.
    static double sseInflation(const cwRHIPointCloud& cloud) {
        return cloud.forest().sseInflation();
    }

    // What the last cut would cost to draw, which is what the governor reads.
    static qint64 selectedPoints(const cwRHIPointCloud& cloud) {
        return cloud.m_selected.points;
    }

    static qint64 selectedBytes(const cwRHIPointCloud& cloud) {
        return cloud.m_selected.bytes;
    }

    static bool pointCapped(const cwRHIPointCloud& cloud) {
        return cloud.m_selected.pointCapped;
    }

    // The block PointCloud.vert reads for the live appearance: the coverage and
    // the projected spacing the cut refines to. Together with each node's floor
    // it is what every sprite of the cloud is sized from, so a test can pin the
    // CPU side of the sizing rule without a readback.
    static PerCloudUniform liveAppearanceUniform(const cwRHIPointCloud& cloud) {
        const auto& live = cloud.m_renderState.value();
        return cloud.appearanceUniform(live.spacingCoverage);
    }

    // Writes the live appearance block into override @a slot with its debug
    // view swapped for @a debugView, every other field as the live slot holds
    // it this frame (reveal ramps included, which an export job's slot drops).
    // Drawing the frame's gathered drawables again at this slot redraws the
    // very same sprites colored by the view, which is how the [PointCloudEval]
    // bench reads the sprite sides the GPU drew without a second frame.
    static void writeDebugViewSlot(const cwRHIPointCloud& cloud, QRhiResourceUpdateBatch* batch,
                                   int slot, cw::pointcloud::DebugView debugView) {
        PerCloudUniform uniform = cloud.appearanceUniform(cloud.m_renderState.value().spacingCoverage);
        uniform.debugView = qint32(debugView);
        batch->updateDynamicBuffer(cloud.m_perCloudUBO, appearanceSlotOffset(cloud, slot),
                                   sizeof(PerCloudUniform), &uniform);
    }

    // The byte offset of appearance @a slot in the cloud's appearance block,
    // the offset its drawables bind for that slot
    static quint32 appearanceSlotOffset(const cwRHIPointCloud& cloud, int slot) {
        return quint32(slot) * cloud.m_perCloudStride;
    }

    // The bindings the cloud's point drawables carry, which tells them apart
    // from another object's in a gathered pass
    static const QRhiShaderResourceBindings* pointBindings(const cwRHIPointCloud& cloud) {
        return cloud.m_srb;
    }

    // The world spacing @a index's constants slot held when the last frame
    // drew: the floor that frame's live gather worked out, or the node's own
    // spacing while no live gather has drawn it. 0 for a node that is not
    // resident. This is the CPU mirror of the write: that the write reaches the
    // GPU ahead of the same frame's draws rests on cwRhiFrameRenderer gathering
    // into the batch drawScene() submits first, and the [PointCloudEval] bench
    // is what judges the drawn result.
    static float drawnNodeFloorSpacing(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return 0.0f;
        }
        return cloud.m_nodes.at(index).floorSpacing;
    }

    // The frame @a index became resident, which its reveal ramp counts from,
    // as its constants slot holds it. 0 for a node that is not resident.
    static quint64 arrivalFrame(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()
            || cloud.m_nodes.at(index).state != cwRHIPointCloud::NodeState::Resident) {
            return 0;
        }
        return cloud.m_nodes.at(index).arrivalFrame;
    }

    // What @a index's floor ramp moves between, as its constants slot holds
    // it: the floor before the last move, the floor the move aims for (the
    // value drawnNodeFloorSpacing() reads) and the frame the move happened.
    // cw::clod::revealedFloor() turns the three into the floor a frame draws.
    struct FloorRamp {
        float previous = 0.0f;
        float target = 0.0f;
        quint64 changedFrame = 0;
    };

    static FloorRamp nodeFloorRamp(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return FloorRamp();
        }
        const auto& node = cloud.m_nodes.at(index);
        return FloorRamp{node.previousFloorSpacing, node.floorSpacing, node.floorChangedFrame};
    }

    // The resident nodes the last live gather drew fading out beside its cut
    // (cwPointCloudShading::revealFrames), in no order.
    static QVector<int> departingNodes(const cwRHIPointCloud& cloud) {
        return cloud.m_departingNodes;
    }

    // The frame @a index's fade-out counts from, as its constants slot holds
    // it, while it is departing; std::nullopt for a node that is not.
    static std::optional<quint64> departureFrame(const cwRHIPointCloud& cloud, int index) {
        return fadeFrame(cloud, index, cwRHIPointCloud::NodeFade::Out);
    }

    // The frame @a index's fade back into the cut counts from, as its
    // constants slot holds it, while it has one; std::nullopt otherwise.
    static std::optional<quint64> reentryFrame(const cwRHIPointCloud& cloud, int index) {
        return fadeFrame(cloud, index, cwRHIPointCloud::NodeFade::In);
    }

    static std::optional<quint64> fadeFrame(const cwRHIPointCloud& cloud, int index,
                                            cwRHIPointCloud::NodeFade fade) {
        if (index < 0 || index >= cloud.m_nodes.size() || cloud.m_nodes.at(index).fade != fade) {
            return std::nullopt;
        }
        return cloud.m_nodes.at(index).fadeFrame;
    }

    // The levels the last cut asked for, so a test can say what the camera
    // wants — which is the finer of the two while children are still streaming.
    static QVector<int> selectedLevels(const cwRHIPointCloud& cloud) {
        QVector<int> levels;
        if (!cloud.m_source.manifest) {
            return levels;
        }

        const QVector<cwPointOctreeNode>& nodes = cloud.m_source.manifest->nodes;
        for (const cw::octree::SelectedNode& selected : cloud.m_selected.nodes) {
            if (selected.node >= 0 && selected.node < nodes.size()) {
                levels.append(nodes.at(selected.node).level);
            }
        }
        return levels;
    }

    // The nodes the cloud has resident out of the last cut — what a frame
    // actually drew, which is the set each node's floor is worked out over.
    static QVector<int> drawnNodes(const cwRHIPointCloud& cloud) {
        QVector<int> drawn;
        if (!cloud.m_source.manifest) {
            return drawn;
        }

        const QVector<cwPointOctreeNode>& nodes = cloud.m_source.manifest->nodes;
        for (const cw::octree::SelectedNode& selected : cloud.m_selected.nodes) {
            if (selected.node >= 0 && selected.node < nodes.size()
                && cloud.m_nodes.at(selected.node).state == NodeState::Resident) {
                drawn.append(selected.node);
            }
        }
        return drawn;
    }

    // The nodes of the last cut the cloud does not have resident yet — what
    // the frame drew a coarser ancestor in place of.
    static QVector<int> missingNodes(const cwRHIPointCloud& cloud) {
        QVector<int> missing;
        if (!cloud.m_source.manifest) {
            return missing;
        }

        const QVector<cwPointOctreeNode>& nodes = cloud.m_source.manifest->nodes;
        for (const cw::octree::SelectedNode& selected : cloud.m_selected.nodes) {
            if (selected.node >= 0 && selected.node < nodes.size()
                && cloud.m_nodes.at(selected.node).state != NodeState::Resident) {
                missing.append(selected.node);
            }
        }
        return missing;
    }

    // The levels of the nodes the cloud has resident out of the last cut — what
    // a frame actually drew.
    static QVector<int> drawnLevels(const cwRHIPointCloud& cloud) {
        QVector<int> levels;
        if (!cloud.m_source.manifest) {
            return levels;
        }

        const QVector<cwPointOctreeNode>& nodes = cloud.m_source.manifest->nodes;
        for (const cw::octree::SelectedNode& selected : cloud.m_selected.nodes) {
            if (selected.node >= 0 && selected.node < nodes.size()
                && cloud.m_nodes.at(selected.node).state == NodeState::Resident) {
                levels.append(nodes.at(selected.node).level);
            }
        }
        return levels;
    }

    // The relax probe's state, so a test can pin how often it runs and that an
    // export job leaves it where the live frame put it.
    static int relaxProbeFrame(const cwRHIPointCloud& cloud) {
        return cloud.forest().relaxProbeFrame();
    }

    static qint64 desiredBytesRelaxed(const cwRHIPointCloud& cloud) {
        return cloud.forest().desiredBytesRelaxed();
    }

    // Pretends the node was last wanted in @a frame, so a test can order the
    // eviction planner's candidates without running frames.
    static void setLastDesiredFrame(cwRHIPointCloud& cloud, int index, quint64 frame) {
        if (index >= 0 && index < cloud.m_nodes.size()) {
            cloud.m_nodes[index].lastDesiredFrame = frame;
        }
    }

    static quint64 lastDesiredFrame(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return 0;
        }
        return cloud.m_nodes.at(index).lastDesiredFrame;
    }

    // Payloads the upload budget has not taken yet. They are off the streamer's
    // CPU cap while they wait here, so a test can check they stay bounded.
    static qint64 readyQueueBytes(const cwRHIPointCloud& cloud) {
        qint64 bytes = 0;
        for (const auto& result : cloud.m_readyQueue) {
            bytes += result.payload.bytes.size();
        }
        return bytes;
    }

    static int readyQueueCount(const cwRHIPointCloud& cloud) {
        return int(cloud.m_readyQueue.size());
    }

    // The nodes whose payloads wait in the ready queue
    static QVector<int> readyQueueNodes(const cwRHIPointCloud& cloud) {
        QVector<int> nodes;
        for (const auto& result : cloud.m_readyQueue) {
            nodes.append(int(result.itemId));
        }
        return nodes;
    }

    static bool streamerHasWork(const cwRHIPointCloud& cloud) {
        return cloud.m_streamer.hasWork();
    }

    static bool exportRequested(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return false;
        }
        return cloud.m_nodes.at(index).exportRequested;
    }

    // Set when residency or the cut moved, and cleared by the publish that
    // follows in the next frame's streamResources, so a test can pin that a
    // frame which changed nothing republishes nothing.
    static bool residencyChanged(const cwRHIPointCloud& cloud) {
        return cloud.m_residencyChanged;
    }

    // The raw GPU-resource identity, so a lifetime test can watch a node's
    // buffer appear and go away.
    static const void* bufferPointer(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return nullptr;
        }
        return cloud.m_nodes.at(index).buffer;
    }

    // The pick mirror itself, so a test can dequantize the very points a pick
    // can reach and reason about the pick radius against them.
    static QByteArray nodeBytes(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return QByteArray();
        }
        return cloud.m_nodes.at(index).bytes;
    }

    // The pick mirror the node kept of what it uploaded (Q4 reads it for
    // picking) plus the pick index built over it, so a test can check the CPU
    // ledger against what is actually held.
    static qint64 mirrorBytes(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return 0;
        }
        const auto& node = cloud.m_nodes.at(index);
        return node.bytes.size() + node.index.byteSize();
    }
};

#endif // CWRHIPOINTCLOUDTESTACCESS_H
