/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
    cwPlotSauceTask(QObject* parent = nullptr);

    void setSurvex3DFile(QString inputFile);
    QString outputXMLFile() const;

protected:
    void runTask();

private:
    //The filename of the survex 3d file
    static const QString PlotSauceExtension;
    QString Survex3DFileName;

    QString survex3DFilename() const;

};

#endif // CWPLOTSAUCETASK_H
