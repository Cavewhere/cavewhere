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
// last gathered frame culled and what the last streaming frame held resident.
// Render threads and offscreen render jobs both publish here, so one QMutex
// guards both halves. Each publish replaces a whole struct and bumps the one
// revision, so a reader sees a coherent half and polls a single counter. Views
// publish into the same slot and the last writer wins — enough for a HUD that
// only needs a recent frame.
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

    static cwRenderFrameStats* instance();

    void publishCulling(const Culling& culling);
    void publishStreaming(const Streaming& streaming);

    Culling culling() const;
    Streaming streaming() const;
    quint64 revision() const;

private:
    cwRenderFrameStats() = default;

    mutable QMutex m_mutex;
    Culling m_culling;
    Streaming m_streaming;
    quint64 m_revision = 0;
};

#endif // CWRENDERFRAMESTATS_H
