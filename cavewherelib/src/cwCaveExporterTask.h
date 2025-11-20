/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAVEEXPORTERTASK_H
#define CWCAVEEXPORTERTASK_H

//Our includes
#include "cwExporterTask.h"
#include "cwGlobals.h"
#include "cwCaveData.h"
class cwCave;


//Qt includes
#include <QTextStream>

class CAVEWHERE_LIB_EXPORT cwCaveExporterTask : public cwExporterTask
{
public:
    cwCaveExporterTask(QObject* parent = 0);
    ~cwCaveExporterTask() = default;

    void setData(const cwCaveData& cave);
    virtual bool writeCave(QTextStream& stream, const cwCaveData& cave) = 0;

protected:
    cwCaveData Cave;
    int TotalProgress;

    virtual void runTask();

    bool checkData();
    bool checkData(const cwCaveData& cave);

protected slots:
    void UpdateProgress(int tripProgress);


};

#endif // CWCAVEEXPORTERTASK_H
