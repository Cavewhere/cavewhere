/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExporterTask.h"

cwExporterTask::cwExporterTask(QObject* object) :
cwTask(object)
{
    ParentExportTask = nullptr;
}

/**
  \brief Sets the output file

  Does nothing if the exporter is still running
  */
void cwExporterTask::setOutputFile(QString outputFile) {
    if(!isRunning()) {
        OutputFileName = outputFile;
    }
}

/**
  \brief Set's the parent survex exporter

  This is useful for reusing survex exporter's to do sub exporting
  */
void cwExporterTask::setParentSurvexExporter(cwExporterTask* parent) {
    if(isRunning()) {
        qWarning("Can't set the survexExportTask's parent when it's running");
        return;
    }

    ParentExportTask = parent;
    setParentTask(parent);
}

/**
  \brief See's if any of the parent's are running

  \returns true if the parents are running
  */
bool cwExporterTask::parentIsRunning() {
    cwExporterTask* parentTask = ParentExportTask;
    while(parentTask != nullptr) {
        if(parentTask->isRunning()) {
            return true;
        }

        //Get the next parent
        parentTask = parentTask->ParentExportTask;
    }
    return false;
}


/**
  \brief Gets all the errors when from the exporter

  Does nothing if the exporter is still running
  */
QStringList cwExporterTask::errors() {
    if(isRunning()) {
        return QStringList();
    }
    return Errors;
}

/**
  \brief Opens the survex output file for writting
  */
bool cwExporterTask::openOutputFile() {
    OutputFile.setFileName(OutputFileName);
    bool canWrite = OutputFile.open(QIODevice::WriteOnly);
    if(!canWrite) {
        //File is bad
        Errors.append(QString("Open file %1").arg(OutputFileName));
        stop();
    } else {
        //File is good
        OutputStream.setDevice(&OutputFile);
    }

    return canWrite;
}
/**
  \brief Closes the survex output file
  */
void cwExporterTask::closeOutputFile() {
    if(OutputFile.isOpen()) {
        OutputStream.flush();
        OutputFile.close();
    }
}
