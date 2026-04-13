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
#include "CaveWhereLibExport.h"

//Qt includes
#include <QReadWriteLock>
#include <QMutex>

class CAVEWHERE_LIB_EXPORT cwCavernTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwCavernTask(QObject *parent = 0);

    void setSurvexFile(QString inputFile);
    QString output3dFileName() const;

protected:
    void runTask();

private:
    //The filename of the survex file that'll be used by cavern
    QString SurvexFileName;
    QReadWriteLock SurvexFileNameLocker;

    QString survexFileName() const;

    static QString survex3dExtension() { return QLatin1String(".3d"); };

private slots:
    void privateSetSurvexFile(QString suvexFile);
};

#endif // CWCAVERNTASK_H
