#ifndef CWSURVEXEXPORTERTASK_H
#define CWSURVEXEXPORTERTASK_H

//Our includes
#include "cwTask.h"

//Qt includes
#include <QStringList>
#include <QFile>
#include <QTextStream>

class cwSurvexExporterTask : public cwTask
{
Q_OBJECT

public:
    explicit cwSurvexExporterTask(QObject* object);

    void setParentSurvexExporter(cwSurvexExporterTask* parent);

    bool parentIsRunning();

    void setOutputFile(QString outputFile);

    QStringList errors();

protected:
    QStringList Errors;
    QTextStream OutputStream;

    bool openOutputFile();
    void closeOutputFile();

private:
    cwSurvexExporterTask* ParentExportTask;

    QString OutputFileName;
    QFile OutputFile;
};

#endif // CWSURVEXEXPORTERTASK_H
