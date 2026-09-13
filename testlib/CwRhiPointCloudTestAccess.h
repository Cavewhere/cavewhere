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

    static NodeState nodeState(const cwRHIPointCloud& cloud, int index) {
        if (index < 0 || index >= cloud.m_nodes.size()) {
            return NodeState::Absent;
        }
        return cloud.m_nodes.at(index).state;
    }

    static int nodeCount(const cwRHIPointCloud& cloud) {
        return int(cloud.m_nodes.size());
    }

    static int residentCount(const cwRHIPointCloud& cloud) {
        int count = 0;
        for (const auto& node : cloud.m_nodes) {
            if (node.state == NodeState::Resident) {
                count++;
            }
        }
        return count;
    }

    static double sseInflation(const cwRHIPointCloud& cloud) {
        return cloud.m_sseInflation;
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
