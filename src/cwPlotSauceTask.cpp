/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPlotSauceTask.h"

//Qt includes
#include <QReadLocker>
#include <QWriteLocker>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QDebug>

const QString cwPlotSauceTask::PlotSauceExtension = ".xml.gz";

cwPlotSauceTask::cwPlotSauceTask(QObject* parent) :
    cwTask(parent)
{
    PlotSauceProcess = new QProcess(this);
    connect(PlotSauceProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(plotSauceFinished(int,QProcess::ExitStatus)));
    connect(PlotSauceProcess, SIGNAL(error(QProcess::ProcessError)), SLOT(printErrors()));
}

/**
  \brief This sets the survex 3d file for the task

  This function is thread safe
*/
void cwPlotSauceTask::setSurvex3DFile(QString inputFile) {
    //Thread safe way to set the data for the task
    QMetaObject::invokeMethod(this, "privateSetSurvex3DFile",
                              Q_ARG(QString, inputFile));
}

/**
  \brief Gets the path to the output file
  */
QString cwPlotSauceTask::outputXMLFile() const {
    QFileInfo info(survex3DFilename().append(PlotSauceExtension));
    if(info.exists()) {
        return info.absoluteFilePath();
    } else {
        return QString();
    }
}

/**
  \brief Gets the 3d filename for the task
  */
QString cwPlotSauceTask::survex3DFilename() const {
    QReadLocker locker(const_cast<QReadWriteLock*>(&Survex3DFileNameLocker));
    return Survex3DFileName;
}

/**
  Runs plotsauce task
  */
void cwPlotSauceTask::runTask() {
    if(!isRunning()) {
        done();
        return;
    }

    QString inputFile = survex3DFilename();
    QString outputFile = inputFile + PlotSauceExtension;

    QStringList plotSauceAppNames;
    plotSauceAppNames.append("plotsauce");
    plotSauceAppNames.append("plotsauce.exe");

    QString plotSaucePath = cwGlobals::findExecutable(plotSauceAppNames);

    //Found plot sauce executable?
    if(plotSaucePath.isEmpty()) {
        qDebug() << "Can't find plotsauce executable!  This means cavewhere can't do loop closure or line ploting!!! Oh the horror!";
        done();
        return;
    }

    QStringList arguments;
    arguments.append(inputFile);
    arguments.append(outputFile);

    PlotSauceProcess->start(plotSaucePath, arguments);
}

/**
  Set the survex 3d file
  */
void cwPlotSauceTask::privateSetSurvex3DFile(QString survex3dFilename) {
    QWriteLocker locker(&Survex3DFileNameLocker);
    Survex3DFileName = survex3dFilename;
}

void cwPlotSauceTask::plotSauceFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/) {
    done();
}

void cwPlotSauceTask::printErrors() {
    qDebug() << "PlotSauce errors: " << PlotSauceProcess->errorString();
}
