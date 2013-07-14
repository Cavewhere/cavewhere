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

//Qt includes
#include <QStringList>
#include <QFile>
#include <QTextStream>

class cwExporterTask : public cwTask
{
Q_OBJECT

public:
    explicit cwExporterTask(QObject* object);

    void setParentSurvexExporter(cwExporterTask* parent);

    bool parentIsRunning();

    void setOutputFile(QString outputFile);

    QStringList errors();

protected:
    QStringList Errors;
    QTextStream OutputStream;

    bool openOutputFile();
    void closeOutputFile();

private:
    cwExporterTask* ParentExportTask;

    QString OutputFileName;
    QFile OutputFile;
};

#endif // CWSURVEXEXPORTERTASK_H
