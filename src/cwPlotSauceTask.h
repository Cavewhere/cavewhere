#ifndef CWPLOTSAUCETASK_H
#define CWPLOTSAUCETASK_H

//Our includes
#include "cwTask.h"

//Qt includes
#include <QProcess>
#include <QReadWriteLock>

class cwPlotSauceTask : public cwTask
{
    Q_OBJECT

public:
    cwPlotSauceTask(QObject* parent = NULL);

    void setSurvex3DFile(QString inputFile);
    QString outputXMLFile() const;

protected:
    void runTask();

private:
    //The filename of the survex 3d file
    static const QString PlotSauceExtension;
    QString Survex3DFileName;
    QReadWriteLock Survex3DFileNameLocker;

    QProcess* PlotSauceProcess;

    QString survex3DFilename() const;

    Q_INVOKABLE void privateSetSurvex3DFile(QString survex3dFilename);

private slots:
    void plotSauceFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/);
    void printErrors();

};

#endif // CWPLOTSAUCETASK_H
