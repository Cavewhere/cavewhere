/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCavernTask.h"
#include "cwDebug.h"

//Qt includes
#include <QReadLocker>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QApplication>

cwCavernTask::cwCavernTask(QObject *parent) :
    cwTask(parent)
{

}

/**
  \brief Set's the survex file name
  */
void cwCavernTask::setSurvexFile(QString inputFile) {
    //Thread safe way to set the data
    QMetaObject::invokeMethod(this, "privateSetSurvexFile",
                              Qt::AutoConnection,
                              Q_ARG(QString, inputFile));
}

/**
  \brief Gets the 3d file's output file
  */
QString cwCavernTask::output3dFileName() const {
    QFileInfo info(survexFileName().append(survex3dExtension()));
    if(info.exists()) {
        return info.absoluteFilePath();
    } else {
        return QString();
    }
}

/**
  \brief Runs survex's cavern
  */
 void cwCavernTask::runTask() {
     if(!isRunning()) {
         done();
         return;
     }

     auto cavernProcess = std::make_unique<QProcess>();

     //Set the process's working directory
     QFileInfo survexFileInfo(SurvexFileName);
     QString workingDirectory = survexFileInfo.absoluteDir().absolutePath();
     cavernProcess->setWorkingDirectory(workingDirectory);

     connect(cavernProcess.get(), SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));

     QString inputFile = survexFileName();
     QString outputFile = inputFile + survex3dExtension();

     QStringList cavernAppNames;
     cavernAppNames.append("survex/cavern");
     cavernAppNames.append("survex/cavern.exe");
     cavernAppNames.append("cavern");
     cavernAppNames.append("cavern.exe");

     //The absolute pathe for the cavern executable
     QString cavernPathName = cwGlobals::findExecutable(cavernAppNames, {cwGlobals::survexPath()});
     QFileInfo cavernFileInfo(cavernPathName);
     if(cavernFileInfo.absoluteDir().exists("en.msg")) {
         cavernProcess->setEnvironment({QStringLiteral("SURVEXLIB=") + cavernFileInfo.absolutePath()});
     }

     //Found cavern executable?
     if(cavernPathName.isEmpty()) {
         qDebug() << "Can't find cavern executable!  This means cavewhere can't do loop closure or line ploting!!! Oh the horror!";
         done();
         return;
     }

     QStringList arguments;
     arguments.append(QString("--output=%1").arg(outputFile));
     arguments.append(inputFile);

     cavernProcess->start(cavernPathName, arguments);
     cavernProcess->waitForFinished();

     if(cavernProcess->exitStatus() == QProcess::CrashExit
         || cavernProcess->exitCode() > 0)
     {
         qDebug() << "Cavern has crashed!" << cavernProcess->readAllStandardOutput();
         stop();
     }

     done();
 }


/**
  \brief Gets the survex file's
  */
QString cwCavernTask::survexFileName() const {
    QReadLocker locker(const_cast<QReadWriteLock*>(&SurvexFileNameLocker));
    return SurvexFileName;
}

/**
  \brief Helper function to the setSurvexFile
  */
void cwCavernTask::privateSetSurvexFile(QString survexFile) {
    QWriteLocker locker(&SurvexFileNameLocker);
    SurvexFileName = survexFile;
}


/**
* @brief cwCavernTask::processError
* @param error
*
* Called when cavern process has errored
*/
void cwCavernTask::processError(QProcess::ProcessError error)
{
   qDebug() << "Cavern has errored out with ProcessError code" << error << LOCATION;
   stop();
   done();
}
