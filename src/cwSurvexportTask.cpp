/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexportTask.h"

//Qt includes
#include <QReadLocker>
#include <QWriteLocker>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QDebug>

const QString cwSurvexportTask::Extension = ".csv";

cwSurvexportTask::cwSurvexportTask(QObject* parent) :
    cwTask(parent)
{

//    connect(PlotSauceProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(plotSauceFinished(int,QProcess::ExitStatus)));

}

/**
  \brief This sets the survex 3d file for the task

  This function is thread safe
*/
void cwSurvexportTask::setSurvex3DFile(QString inputFile) {
    Survex3DFileName = inputFile;
}

/**
  \brief Gets the path to the output file
  */
QString cwSurvexportTask::outputFilename() const {
    QFileInfo info(survex3DFilename().append(Extension));
    if(info.exists()) {
        return info.absoluteFilePath();
    } else {
        return QString();
    }
}

/**
  \brief Gets the 3d filename for the task
  */
QString cwSurvexportTask::survex3DFilename() const {
    return Survex3DFileName;
}

/**
  Runs plotsauce task
  */
void cwSurvexportTask::runTask() {
    if(!isRunning()) {
        done();
        return;
    }

    SurvexportProcess = new QProcess();
    connect(SurvexportProcess, SIGNAL(error(QProcess::ProcessError)), SLOT(printErrors()));

    QString inputFile = survex3DFilename();
    QString outputFile = inputFile + Extension;

    QStringList survexportNames;
    survexportNames.append("survexport");
    survexportNames.append("survexport.exe");

    QString survexportPath = cwGlobals::findExecutable(survexportNames, {cwGlobals::survexPath()});

    //Found plot sauce executable?
    if(survexportPath.isEmpty()) {
        qDebug() << "Can't find survexport executable!  This means cavewhere can't do loop closure or line ploting!!! Oh the horror!";
        done();
        return;
    }

    QStringList arguments;
    arguments.append("--station-names");
    arguments.append("--csv");
    arguments.append(inputFile);
    arguments.append(outputFile);

    SurvexportProcess->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    SurvexportProcess->start(survexportPath, arguments);
    SurvexportProcess->waitForFinished();

    delete SurvexportProcess;

    done();
}

void cwSurvexportTask::printErrors() {
    qDebug() << "PlotSauce errors: " << SurvexportProcess->errorString();
}
