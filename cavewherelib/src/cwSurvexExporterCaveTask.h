/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTERCAVETASK_H
#define CWSURVEXEXPORTERCAVETASK_H

//Our includes
#include "cwCaveExporterTask.h"
#include "cwSurvexExporterRegion.h"
class cwSurvexExporterTripTask;
class cwCave;
class cwStationHandle;

// Qt includes
#include <QStringList>
#include <QTextStream>

// Std includes
#include <functional>

class cwSurvexExporterCaveTask : public cwCaveExporterTask
{
    Q_OBJECT
public:
    explicit cwSurvexExporterCaveTask(QObject *parent = 0);

    bool writeCave(QTextStream& stream, const cwCaveData &cave, const QString& globalCS = QString());

    // Per-call options forwarded from cwSurvexExporterRegion. The
    // attachment-dir maps drive *include emission for caves and trips
    // whose externalCenterline is set. Set once before the writeCave
    // loop; cleared by passing a default-constructed value.
    void setExportOptions(const cwSurvexExporterRegion::Options& options);

    // Render one equate handle as a station name relative to `cave`'s
    // "*begin" block: a NativeCave handle as its bare tail, a Trip handle as
    // "<scopePrefix><tail>". Returns an empty string when the handle names a
    // container that is not this cave (a Trip whose trip is absent, or a
    // NativeCave whose containerId is another cave), so the caller can drop
    // the malformed tie. Shared with the region exporter, which prepends
    // "cave_<hex>." to qualify the same operand across caves.
    static QString equateOperand(const cwStationHandle& handle, const cwCaveData& cave);

    // Emit one "*equate <a> <b> ..." line from pre-rendered operands. An empty
    // operand (an unresolvable handle) drops the whole tie; operands are
    // de-duplicated and the line is skipped unless at least two distinct
    // survive, since cavern rejects a self-equate.
    static void writeEquateLine(QTextStream& stream, const QStringList& operands);

    // Emit one "*equate" line per structurally-valid tie in `equates`, rendering
    // each handle with `renderOperand` — cave-relative (equateOperand) for
    // cave-scope emission, fully qualified for region-scope. Each line is handed
    // to writeEquateLine, so invalid ties and unrenderable handles drop
    // uniformly. Shared by the cave and region exporters, which differ only in
    // the renderer.
    static void writeEquates(QTextStream& stream,
                             const QList<cwEquate>& equates,
                             const std::function<QString(const cwStationHandle&)>& renderOperand);

private:
    cwSurvexExporterTripTask* TripExporter;
    cwSurvexExporterRegion::Options ExportOptions;

    void writeFixStations(QTextStream& stream, const cwCaveData& cave, const QString& globalCS);

    // Emits *include "<abs>" for the cave/trip's externalCenterline by
    // joining the owner's attachment dir with the project-relative
    // entry file. Returns false (and appends an error) when the owner's
    // attachment dir is missing from ExportOptions — that state means
    // reconcile has not run yet, so writing a stale *include would
    // surface as a cavern parse failure rather than a clear message.
    bool writeExternalInclude(QTextStream& stream,
                              const QUuid& ownerId,
                              const QHash<QUuid, QString>& attachmentDirs,
                              const QString& entryFile,
                              const QString& ownerLabel);
};

#endif // CWSURVEXEXPORTERCAVETASK_H
