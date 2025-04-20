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
#include <QFileSystemWatcher>
#include <QTimer>
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
    return m_outputFilename;
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
    m_outputFilename.clear();

    if(!isRunning()) {
        done();
        return;
    }

    SurvexportProcess = new QProcess();
    connect(SurvexportProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), SLOT(printErrors()));

    QString inputFile = survex3DFilename();
    QString outputFile = inputFile + Extension;

    QStringList survexportNames;
    survexportNames.append("survex/survexport");
    survexportNames.append("survex/survexport.exe");
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

    SurvexportProcess->deleteLater();

    //On windows survexport is slow to write files to disk, if the file
    //doesn't exist, we wait until it shows up.
    if(!QFileInfo::exists(outputFile)) {
        auto dirPath = QFileInfo(outputFile).absolutePath();
        QFileSystemWatcher watcher;
        watcher.addPath(dirPath);

        QEventLoop loop;
        QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged,
                         &loop, [&](const QString& /*changedDir*/){
                             if (QFileInfo::exists(outputFile)) {
                                 loop.quit();
                             }
                         });

        // also quit after 500 ms no matter what
        QTimer::singleShot(500, &loop, &QEventLoop::quit);

        // if the file already exists, skip waiting
        if (!QFileInfo::exists(outputFile)) {
            loop.exec();
        }
    }

    m_outputFilename = outputFile;

    done();
}

void cwSurvexportTask::printErrors() const {
    qDebug() << "PlotSauce errors: " << SurvexportProcess->errorString();
}
