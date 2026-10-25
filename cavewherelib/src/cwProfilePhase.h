/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROFILEPHASE_H
#define CWPROFILEPHASE_H

//Our includes
#include "CaveWhereLibExport.h"

//Qt includes
#include <QLoggingCategory>
#include <QString>

//Std includes
#include <memory>

//! One timed phase of work. While the phase is open it is an interval on the
//! task signpost track (built with CAVEWHERE_TASK_TRACE and a recorder attached);
//! when it ends it writes one "<kind> <name> us=<elapsed>[ key=value ...]" line
//! on its logging category (enabled at runtime, off by default).
//!
//! Both outputs are decided once, in the constructor. A phase nobody listens to
//! holds no state, starts no timer, and formats no text, so the cost of a phase
//! in a normal run is one branch per output.
//!
//! Copies share one phase, so a phase can be captured by value into a
//! continuation; it ends on finish() or when the last copy is destroyed.
class CAVEWHERE_LIB_EXPORT cwProfilePhase
{
public:
    cwProfilePhase() = default;
    cwProfilePhase(const QLoggingCategory& category, const QString& kind, const QString& name);

    //! True when the line will be written, so callers skip gathering fields nobody reads
    bool logging() const;

    //! Appends " key=value" to the line; a no-op when the line will not be written
    void addField(const QString& key, qint64 value);
    void addField(const QString& key, const QString& value);

    //! Ends the interval and writes the line; later calls do nothing
    void finish();

private:
    struct State;
    std::shared_ptr<State> m_state;
};

#endif // CWPROFILEPHASE_H
