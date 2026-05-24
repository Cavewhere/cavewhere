/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTERREGION_H
#define CWSURVEXEXPORTERREGION_H

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCavingRegionData.h"

#include <QString>

/**
 * \brief Writes a survex (.svx) file for the entire caving region.
 *
 * Iterates the region's caves and emits a *begin/*end block per cave using
 * cwSurvexExporterCaveTask::writeCave under the hood. Pure compute on the
 * provided value-snapshot \a region; safe to call from any thread.
 */
class CAVEWHERE_LIB_EXPORT cwSurvexExporterRegion
{
public:
    cwSurvexExporterRegion() = delete;

    static Monad::ResultBase exportRegion(const cwCavingRegionData& region,
                                          const QString& outputPath);
};

#endif // CWSURVEXEXPORTERREGION_H
