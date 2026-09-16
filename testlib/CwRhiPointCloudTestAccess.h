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

    static double sseInflation(const cwRHIPointCloud& cloud) {
        return cloud.m_sseInflation;
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

    // The block PointCloud.vert reads for the live appearance: the tuned world
    // radius, the coverage, the projected spacing the cut refines to, and the
    // finest spacing the last live frame drew. It is
    // what every sprite of the cloud is sized from, so a test can pin the CPU
    // side of the sizing rule without a readback.
    static PerCloudUniform liveAppearanceUniform(const cwRHIPointCloud& cloud) {
        const auto& live = cloud.m_renderState.value();
        return cloud.appearanceUniform(live.worldRadius, live.spacingCoverage);
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
        return cloud.m_relaxProbeFrame;
    }

    static qint64 desiredBytesRelaxed(const cwRHIPointCloud& cloud) {
        return cloud.m_desiredBytesRelaxed;
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

    static bool hasStreamingWork(const cwRHIPointCloud& cloud) {
        return !cloud.m_readyQueue.isEmpty() || cloud.m_streamer.hasWork();
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
