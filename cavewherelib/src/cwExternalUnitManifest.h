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

} // namespace cwExternalUnitManifest

#endif // CWEXTERNALUNITMANIFEST_H
