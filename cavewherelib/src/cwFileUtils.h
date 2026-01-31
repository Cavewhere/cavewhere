/**************************************************************************
**
**    Copyright (C) 2007-2026 Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CAVEWHERE_CWFILEUTILS_H
#define CAVEWHERE_CWFILEUTILS_H

// Qt includes
#include <QString>

// Our includes
#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwFileUtils {
public:

    cwFileUtils() = delete;

    // Waits for a file to exist and stop changing size before returning.
    // Returns true once the file exists and its size is stable for stabilityMs,
    // or false if either the existence or stability timeouts are reached.
    static bool waitForFileReady(const QString& filePath,
                          int existenceTimeoutMs = 10000,
                          int stabilityMs = 200,
                          int stabilityTimeoutMs = 10000);

};

#endif // CAVEWHERE_CWFILEUTILS_H
