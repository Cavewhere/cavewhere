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

// Qt includes
#include <QTextStream>

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
