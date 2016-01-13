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
    ~cwExporterTask() { qDebug() << "Deleted:" << this; }

    void setParentSurvexExporter(cwExporterTask* parent);

    bool parentIsRunning();

    void setOutputFile(QString outputFile);

    QStringList errors();

protected:
    QStringList Errors;
    QScopedPointer<QTextStream> OutputStream;

    bool openOutputFile();
    void closeOutputFile();

private:
    cwExporterTask* ParentExportTask;

    QString OutputFileName;
    QScopedPointer<QFile> OutputFile;
};

#endif // CWSURVEXEXPORTERTASK_H
