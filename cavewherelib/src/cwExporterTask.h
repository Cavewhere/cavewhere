/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTERTASK_H
#define CWSURVEXEXPORTERTASK_H

//Our includes
#include "cwTask.h"
#include "cwDebug.h"
#include "cwGlobals.h"
#include "cwSurvexCS.h"

//Qt includes
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QScopedPointer>

class CAVEWHERE_LIB_EXPORT cwExporterTask : public cwTask
{
Q_OBJECT

public:
    explicit cwExporterTask(QObject* object);

    void setParentSurvexExporter(cwExporterTask* parent);

    bool parentIsRunning();

    void setOutputFile(QString outputFile);

    //! The sidecar writer for the file this task's `*cs` lines end up in.
    //!
    //! A task that opens its own output file already has one, rooted at
    //! setOutputFile()'s path. An exporter that writes the file itself and drives
    //! this task to fill part of it — cwSurvexExporterRegion — hands in its own
    //! instead. \a sidecars outlives the task, and its owner writes it.
    void setSidecarWriter(cwSurvexCS::SidecarWriter* sidecars);

    QStringList errors();

protected:
    QStringList Errors;
    QScopedPointer<QTextStream> OutputStream;

    bool openOutputFile();
    void closeOutputFile();

    cwSurvexCS::SidecarWriter& sidecars();

private:
    cwExporterTask* ParentExportTask;

    QString OutputFileName;
    QScopedPointer<QFile> OutputFile;
    cwSurvexCS::SidecarWriter OwnSidecars;
    cwSurvexCS::SidecarWriter* SharedSidecars = nullptr;
};

#endif // CWSURVEXEXPORTERTASK_H
