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
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QDebug>

const QString cwPlotSauceTask::PlotSauceExtension = ".xml.gz";

cwPlotSauceTask::cwPlotSauceTask(QObject* parent) :
    cwTask(parent)
{

}

/**
  \brief This sets the survex 3d file for the task

  This function is thread safe
*/
void cwPlotSauceTask::setSurvex3DFile(QString inputFile) {
    Survex3DFileName = inputFile;
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

    auto plotSauceProcess = std::make_unique<QProcess>();
    auto plotSaucePtr = plotSauceProcess.get();
    connect(plotSaucePtr, &QProcess::errorOccurred,
            plotSaucePtr,
            [plotSaucePtr]()
    {
         qDebug() << "PlotSauce errors: " << plotSaucePtr->errorString();
    });

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

    plotSauceProcess->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    plotSauceProcess->start(plotSaucePath, arguments);
    plotSauceProcess->waitForFinished();

    done();
}
