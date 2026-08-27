/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTEXTURESTREAMINGSTATS_H
#define CWTEXTURESTREAMINGSTATS_H

//Qt includes
#include <QMutex>
#include <QtGlobal>

//Our includes
#include "cwGlobals.h"

// Process-wide streamed-texture residency counts for the last frame that
// streamed, feeding the debug render HUD. Streaming runs on the render thread
// and each view streams its own frame, so this has no QObject thread affinity
// and a QMutex guards the value: any thread may publish into it.
//
// Every frame publishes the whole struct at once, so a reader always sees one
// coherent frame. Views publish into the same slot and the last writer wins —
// acceptable for a debug HUD, which only needs a recent frame.
class CAVEWHERE_LIB_EXPORT cwTextureStreamingStats
{
public:
    struct Counts {
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
    };

    static cwTextureStreamingStats* instance();

    void publish(const Counts& counts);
    Counts counts() const;
    quint64 revision() const;

private:
    cwTextureStreamingStats() = default;

    mutable QMutex m_mutex;
    Counts m_counts;
    quint64 m_revision = 0;
};

#endif // CWTEXTURESTREAMINGSTATS_H
