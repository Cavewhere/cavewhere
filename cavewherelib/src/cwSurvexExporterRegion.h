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

#include <QHash>
#include <QString>
#include <QUuid>

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
    /**
     * Per-call options for the driver exporter. The default-constructed
     * value reproduces the user-facing exporter's contract (no
     * \c *include emission); the line-plot worker populates the
     * attachment-dir maps so caves and trips with an
     * \c externalCenterline produce \c *include blocks instead of native
     * shot data.
     *
     * \c caveAttachmentDirs maps \c cwCave::id() to the absolute
     * filesystem path where reconcile placed that cave's dependency
     * closure. The cave-level \c externalCenterline.entryFile() is
     * project-relative to that directory; the exporter joins them and
     * emits an absolute forward-slash quoted \c *include path.
     *
     * \c tripAttachmentDirs is the parallel map for
     * \c cwTrip::id(). A cave attachment shadows any trip attachments
     * inside it (the trip loop is skipped entirely when the cave is
     * attached).
     *
     * Maps may be empty even when the snapshot carries
     * \c externalCenterline values — in that case the exporter falls
     * back to a native-emission path that logs an error for the cave
     * (the user-facing exporter never runs in this state; commit 10's
     * \c canExport gate stops it before it gets here).
     *
     * \c tripInjectedDeclinations maps \c cwTrip::id() to the
     * CaveWhere-resolved declination (degrees, east-positive) for
     * external trips whose file carries no declination of its own. The
     * exporter emits it as \c *calibrate \c DECLINATION inside the
     * \c trip_<uuid> block before \c *include, where cavern applies it
     * to the included legs. An absent key means the file owns
     * declination (or ownership is unknown) and nothing is emitted —
     * the file's own directive would override an injected value anyway
     * (cavern-verified, master plan §8.8 q7).
     */
    struct CAVEWHERE_LIB_EXPORT Options {
        QHash<QUuid, QString> caveAttachmentDirs;
        QHash<QUuid, QString> tripAttachmentDirs;
        QHash<QUuid, double> tripInjectedDeclinations;
    };

    cwSurvexExporterRegion() = delete;

    static Monad::ResultBase exportRegion(const cwCavingRegionData& region,
                                          const QString& outputPath,
                                          const Options& options = {});
};

#endif // CWSURVEXEXPORTERREGION_H
