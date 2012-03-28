//Our includes
#include "cwCavernTask.h"

//Qt includes
#include <QReadLocker>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QApplication>

const QString cwCavernTask::Survex3dExtension = ".3d";

cwCavernTask::cwCavernTask(QObject *parent) :
    cwTask(parent)
{
    CavernProcess = new QProcess(this);
    connect(CavernProcess, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(cavernFinished(int,QProcess::ExitStatus)));
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
    QFileInfo info(survexFileName().append(Survex3dExtension));
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

     QString inputFile = survexFileName();
     QString outputFile = inputFile + Survex3dExtension;

     QStringList cavernAppNames;
     cavernAppNames.append("cavern");
     cavernAppNames.append("cavern.exe");

     //The absolute pathe for the cavern executable
     QString cavernPathName;

     //Try to get the correct cavern executable
     foreach(QString cavernAppName, cavernAppNames) {
         QDir cavernDir(QApplication::applicationDirPath());
         QString currentCavernPathName = cavernDir.absoluteFilePath(cavernAppName);

         QFileInfo cavernFileInfo(currentCavernPathName);
         if(cavernFileInfo.exists() && cavernFileInfo.isExecutable()) {
              cavernPathName = currentCavernPathName;
              break;
         }
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

     CavernProcess->start(cavernPathName, arguments);
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

    //Set the process's working directory
    QFileInfo survexFileInfo(SurvexFileName);
    QString workingDirectory = survexFileInfo.absoluteDir().absolutePath();
    CavernProcess->setWorkingDirectory(workingDirectory);
}

/**
  \brief Called when cavern has finished
  */
void cwCavernTask::cavernFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    //qDebug() << "Cavern has finish, outputfile:" << output3dFileName();
    //qDebug() << CavernProcess->readAllStandardOutput();
    if(exitStatus == QProcess::CrashExit || exitCode > 0) {
        //Errors in cavern
        stop();
    }
    done();
}
