/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMEMORYUSAGE_H
#define CWMEMORYUSAGE_H

//Qt includes
#include <QtGlobal>

//Our includes
#include "cwGlobals.h"

namespace cwMemoryUsage {

/**
 * The process's physical footprint in bytes — dirty plus compressed memory,
 * which is the number macOS's memory watchdog terminates on ("ran out of
 * application memory"). Clean file-backed pages (memory-mapped files) do NOT
 * count. Returns -1 where unsupported.
 */
CAVEWHERE_LIB_EXPORT qint64 physFootprint();

/**
 * physFootprint() in whole MB, for log lines (-1 when unavailable).
 */
CAVEWHERE_LIB_EXPORT qint64 physFootprintMB();

}

#endif // CWMEMORYUSAGE_H
