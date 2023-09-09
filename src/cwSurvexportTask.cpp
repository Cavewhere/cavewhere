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

cwSurvexportTask::cwSurvexportTask(QObject* parent) :
    cwTask(parent)
{

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
QString cwSurvexportTask::outputXMLFile() const {
    QString inputFile = survex3DFilename();
    auto inputInfo = QFileInfo(inputFile);
    QString outputFile = inputInfo.absoluteDir().path() + "/" + inputInfo.completeBaseName() + extension();

    QFileInfo info(outputFile);
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

    auto process = std::make_unique<QProcess>();
    auto surexportPtr = process.get();
    connect(surexportPtr, &QProcess::errorOccurred,
            surexportPtr,
            [surexportPtr]()
    {
         qDebug() << "survexport errors: " << surexportPtr->errorString();
    });

    QString inputFile = survex3DFilename();

    QStringList plotSauceAppNames;
    plotSauceAppNames.append("survexport");
    plotSauceAppNames.append("survexport.exe");

    QString path = cwGlobals::findExecutable(plotSauceAppNames);

    //Found plot sauce executable?
    if(path.isEmpty()) {
        qDebug() << "Can't find plotsauce executable!  This means cavewhere can't do loop closure or line ploting!!! Oh the horror!";
        done();
        return;
    }

    QStringList arguments = {
        "--csv",
        inputFile
    };

    process->setWorkingDirectory(QFileInfo(inputFile).absoluteDir().path());
    process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    process->start(path, arguments);
    process->waitForFinished();

    done();
}
