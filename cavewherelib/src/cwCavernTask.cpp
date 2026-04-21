/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCavernTask.h"
#include "cwDebug.h"

//Survex library
#include "cavern_lib.h"

//Qt includes
#include <QReadLocker>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMutexLocker>

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
    if (info.exists()) {
        return info.absoluteFilePath();
    } else {
        return QString();
    }
}

/**
  \brief Runs survex's cavern as a library call
  */
void cwCavernTask::runTask() {
    if (!isRunning()) {
        done();
        return;
    }

    // cavern_run() uses global state, serialize all calls
    static QMutex cavernMutex;
    QMutexLocker locker(&cavernMutex);

    QString inputFile = survexFileName();
    QString outputFile = inputFile + survex3dExtension();

    char arg0[] = "cavern";
    char quietArg[] = "--quiet";
    QByteArray outputArg = QStringLiteral("--output=%1").arg(outputFile).toUtf8();
    QByteArray inputArg = inputFile.toUtf8();

    char* argv[] = {
        arg0,
        quietArg,
        quietArg,
        outputArg.data(),
        inputArg.data(),
        nullptr
    };

    int rc = cavern_run(5, argv);
    if (rc != 0) {
        qDebug() << "cavern_run failed with exit code" << rc << "for" << inputFile;
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
