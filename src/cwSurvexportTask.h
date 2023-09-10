/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXPORTTASK_H
#define CWSURVEXPORTTASK_H

//Our includes
#include "cwTask.h"
#include "cwGlobals.h"

//Qt includes
#include <QProcess>

class CAVEWHERE_LIB_EXPORT cwSurvexportTask : public cwTask
{
    Q_OBJECT

public:
    cwSurvexportTask(QObject* parent = nullptr);

    void setSurvex3DFile(QString inputFile);
    QString outputFilename() const;

public slots:
    void printErrors() const;

protected:
    void runTask();

private:
    //The filename of the survex 3d file
    static const QString Extension;
    QString Survex3DFileName;

    QProcess* SurvexportProcess;

    QString survex3DFilename() const;

};

#endif // CWSURVEXPORTTASK_H
