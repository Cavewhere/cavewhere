//Our includes
#include "cwCavernTask.h"

//Qt includes
#include <QReadLocker>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

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
                              Qt::AutoConnection, QGenericArgument("QString", &inputFile));
}

/**
  \brief Gets the 3d file's output file
  */
QString cwCavernTask::output3dFileName() const {
    QFileInfo info(survexFileName().append(".3d"));
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
     QString inputFile = survexFileName();
     QString outputFile = inputFile + ".3d";
     QString cavernPath = "/usr/local/bin/cavern";

     QStringList arguments;
     arguments.append(QString("--output=%1").arg(outputFile));
     arguments.append(inputFile);

     CavernProcess->start(cavernPath, arguments);
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
    qDebug() << "Cavern has finish outputfile:" << output3dFileName();
    qDebug() << CavernProcess->readAllStandardOutput();
    done();
}
