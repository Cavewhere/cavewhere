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
#include "cwSurvexExporterUtils.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>

/**
 * \brief Writes a survex (.svx) file for the entire caving region.
 *
 * Walks the region's node tree and emits a nested *begin / *end block per
 * native node (cwSurvexExporterCaveTask::writeNode), then every region equate
 * fully qualified at region scope. Labels come from one cwScopeLabels built
 * from \a region. Pure compute on the provided value-snapshot \a region; safe
 * to call from any thread.
 */
class CAVEWHERE_LIB_EXPORT cwSurvexExporterRegion
{
public:
    //! Which coordinate system *cs out should name — see cwSurvexExporterUtils
    //! for what the two answers mean and why they differ.
    using OutputCSPolicy = cwSurvexExporterUtils::OutputCSPolicy;

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
     * \c *begin block before \c *include, where cavern applies it
     * to the included legs. An absent key means the file owns
     * declination (or ownership is unknown) and nothing is emitted —
     * the file's own directive would override an injected value anyway
     * (cavern-verified, master plan §8.8 q7).
     *
     * \c excludedExternalOwners lists caves and trips that carry an
     * \c externalCenterline the solve cannot read: dependencies reaching
     * outside the project's data root, so the attachment cannot travel with
     * the project, or an in-project copy that is gone from disk, which
     * cavern would fatal on. The exporter emits no \c *include for them and
     * no \c *begin wrapper around it, and keeps writing the rest of the
     * region, so one broken attachment costs its own survey and leaves every
     * other survey solvable. When it was the region's only data the driver
     * comes out empty and cavern still fails the run with "No survey data" —
     * the same way an all-empty region already fails today.
     *
     * A trip owner is told why through the file-error banner (containment)
     * or the missing-copy banner; a cave owner reads the reason from its
     * attached-centerlines row.
     *
     * \c externalFixedStations maps a cave or trip owner to the stations its
     * file fixes itself, in the file's namespace. A node fix naming one of
     * them is dropped with an error rather than written: cavern rejects a
     * second fix at different coordinates (error 46) and fails the whole run.
     */
    struct CAVEWHERE_LIB_EXPORT Options {
        QHash<QUuid, QString> caveAttachmentDirs;
        QHash<QUuid, QString> tripAttachmentDirs;
        QHash<QUuid, double> tripInjectedDeclinations;
        QSet<QUuid> excludedExternalOwners;
        QHash<QUuid, QStringList> externalFixedStations;
        OutputCSPolicy outputCSPolicy = OutputCSPolicy::Shareable;
    };

    cwSurvexExporterRegion() = delete;

    //! \a options has no default: `= {}` here makes clang error with "default
    //! member initializer for 'outputCSPolicy' needed within definition of
    //! enclosing class ... outside of member functions". Callers wanting the
    //! defaults pass {}.
    static Monad::ResultBase exportRegion(const cwCavingRegionData& region,
                                          const QString& outputPath,
                                          const Options& options);
};

#endif // CWSURVEXEXPORTERREGION_H
