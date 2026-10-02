/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERCULLINGSTATS_H
#define CWRENDERCULLINGSTATS_H

//Qt includes
#include <QMutex>
#include <QtGlobal>

//Our includes
#include "cwGlobals.h"

// Process-wide culled-versus-total draw counts for the last gathered frame,
// feeding the debug render HUD. Scene gathering happens on the render thread and
// offscreen render jobs gather their own frames, so this has no QObject thread
// affinity and a QMutex guards the value: any thread may publish into it.
//
// Every gather publishes the whole struct at once, so a reader always sees one
// coherent frame. Offscreen jobs publish alongside the live frame and the last
// writer wins — acceptable for a debug HUD, which only needs a recent frame.
class CAVEWHERE_LIB_EXPORT cwRenderCullingStats
{
public:
    struct Counts {
        int objectsTotal = 0;
        int objectsCulled = 0;
        int itemsTotal = 0;
        int itemsCulled = 0;
    };

    static cwRenderCullingStats* instance();

    void publish(const Counts& counts);
    Counts counts() const;
    quint64 revision() const;

private:
    cwRenderCullingStats() = default;

    mutable QMutex m_mutex;
    Counts m_counts;
    quint64 m_revision = 0;
};

#endif // CWRENDERCULLINGSTATS_H
