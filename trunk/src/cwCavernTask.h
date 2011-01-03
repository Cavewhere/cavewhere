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
    QString SurvexFileName;
    QReadWriteLock SurvexFileNameLocker;

    QProcess* CavernProcess;

    QString survexFileName() const;

private slots:
    void privateSetSurvexFile(QString suvexFile);

    void cavernFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/);

};

#endif // CWCAVERNTASK_H
