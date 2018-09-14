/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAVERNTASK_H
#define CWCAVERNTASK_H

//Our includes
#include "cwTask.h"

//Qt includes
#include <QReadWriteLock>
#include <QProcess>

class cwCavernTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwCavernTask(QObject *parent = 0);

    void setSurvexFile(QString inputFile);
    QString output3dFileName() const;

signals:

public slots:



protected:
    void runTask();

private:
    //The filename of the survex file that'll be used by cavern
    static const QString Survex3dExtension;
    QString SurvexFileName;
    QReadWriteLock SurvexFileNameLocker;

    QProcess* CavernProcess;

    QString survexFileName() const;

private slots:
    void privateSetSurvexFile(QString suvexFile);

//    void cavernFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);
};

#endif // CWCAVERNTASK_H
