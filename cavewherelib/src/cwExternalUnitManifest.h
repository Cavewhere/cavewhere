/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALUNITMANIFEST_H
#define CWEXTERNALUNITMANIFEST_H

//Our includes
#include "cwExternalCenterlineScanner.h"
#include "cwGlobals.h"
#include <Monad/Result.h>

//Qt includes
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

/**
 * units.json: the scan tree of an attached source, saved beside the copied
 * files so the solve can split the source root's stations among derived nodes
 * and trips without re-scanning. It holds every cwScanRow in pre-order - the
 * node fields (sourcePath, kind, display name, cavernLabel, hasSurveyContent,
 * missing, date) and, per unit, its cavern-relative station names, each
 * canonicalized with cwStation::canonicalKey as cwExternalStationHarvest
 * spells them, de-duplicated and sorted.
 */
namespace cwExternalUnitManifest {

//! "units.json"
CAVEWHERE_LIB_EXPORT QString fileName();

CAVEWHERE_LIB_EXPORT QByteArray toJson(const QList<cwScanRow>& rows);
CAVEWHERE_LIB_EXPORT Monad::Result<QList<cwScanRow>> fromJson(const QByteArray& json);

//! Writes \a rows to \a path, replacing the file in one step.
CAVEWHERE_LIB_EXPORT Monad::ResultBase write(const QString& path, const QList<cwScanRow>& rows);
CAVEWHERE_LIB_EXPORT Monad::Result<QList<cwScanRow>> read(const QString& path);

/**
 * What the solve reads out of \a rows for one source root: each derived
 * node's cavernLabel, keyed by the node's sourcePath, and each unit's station
 * set keyed by the source path its derived trip carries
 * (cwExternalCenterlineAttach::unitSourcePath). A unit's stations are named
 * below the cavern path of the node that holds its trip ("A5" for a Compass
 * survey in ALPHA.DAT, whose names are "alpha.A5"), sorted.
 *
 * \a preserveCase keeps each name as the rows spell it, which is how cavern
 * reads a Compass or Walls file; otherwise names are canonical, as cavern
 * reads a Survex file under its default *case lower. Either way they are the
 * spelling an *equate operand needs to name the station.
 */
struct SolveView {
    QHash<QString, QString> cavernLabels;
    QHash<QString, QStringList> unitStations;

    bool operator==(const SolveView& other) const = default;
};
CAVEWHERE_LIB_EXPORT SolveView solveView(const QList<cwScanRow>& rows, bool preserveCase);

} // namespace cwExternalUnitManifest

#endif // CWEXTERNALUNITMANIFEST_H
