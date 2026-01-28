/**************************************************************************
**
**    Copyright (C) 2007-2026 Cavewhere contributors
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CAVEWHERE_CWFILEUTILS_H
#define CAVEWHERE_CWFILEUTILS_H

// Qt includes
#include <QString>

namespace cwFileUtils {

bool waitForFileReady(const QString& filePath,
                      int existenceTimeoutMs = 10000,
                      int stabilityMs = 200,
                      int stabilityTimeoutMs = 10000);

}

#endif // CAVEWHERE_CWFILEUTILS_H
