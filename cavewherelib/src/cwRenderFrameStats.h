/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERFRAMESTATS_H
#define CWRENDERFRAMESTATS_H

//Qt includes
#include <QMutex>
#include <QtGlobal>

//Our includes
#include "cwGlobals.h"

// Process-wide per-frame render counts feeding the debug render HUD: what the
// last gathered frame culled, what the last streaming frame held resident, and
// what the last point cloud frame selected. Render threads and offscreen render
// jobs both publish here, so one QMutex guards every part. Each publish
// replaces a whole struct and bumps the one revision, so a reader sees a
// coherent part and polls a single counter. Views publish into the same slot
// and the last writer wins — enough for a HUD that only needs a recent frame.
class CAVEWHERE_LIB_EXPORT cwRenderFrameStats
{
public:
    struct Culling {
        int objectsTotal = 0;
        int objectsCulled = 0;
        int itemsTotal = 0;
        int itemsCulled = 0;

        bool operator==(const Culling& other) const = default;
    };

    struct Streaming {
        //! Items drawing from a streamed source rather than a whole-texture image
        int streamedItems = 0;
        //! Level loads queued, running, or waiting to be drained
        int loadsInFlight = 0;
        //! Streamed items holding a level coarser than the camera asked for
        int itemsBelowDesired = 0;
        //! Payload bytes those loads hold alive on the CPU
        qint64 readyCpuBytes = 0;
        //! Items giving detail back to bring the scene under the GPU budget
        int demotionsInFlight = 0;

        bool operator==(const Streaming& other) const = default;
    };

    struct PointCloud {
        //! Octree nodes holding a GPU buffer
        int residentNodes = 0;
        //! Octree nodes the last cut asked to draw
        int selectedNodes = 0;
        //! Node loads queued, running, or waiting to be uploaded
        int nodeLoadsInFlight = 0;
        //! Screen-space-error multiplier the view raised to fit the GPU budget
        double sseInflation = 1.0;

        bool operator==(const PointCloud& other) const = default;
    };

    static cwRenderFrameStats* instance();

    void publishCulling(const Culling& culling);
    void publishStreaming(const Streaming& streaming);
    void publishPointCloud(const PointCloud& pointCloud);

    Culling culling() const;
    Streaming streaming() const;
    PointCloud pointCloud() const;
    quint64 revision() const;

private:
    cwRenderFrameStats() = default;

    mutable QMutex m_mutex;
    Culling m_culling;
    Streaming m_streaming;
    PointCloud m_pointCloud;
    quint64 m_revision = 0;
};

#endif // CWRENDERFRAMESTATS_H
