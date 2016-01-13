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
class cwCave;

//Qt includes
#include <QTextStream>

class CAVEWHERE_LIB_EXPORT cwCaveExporterTask : public cwExporterTask
{
public:
    cwCaveExporterTask(QObject* parent = 0);

    void setData(const cwCave& cave);
    virtual bool writeCave(QTextStream& stream, cwCave* cave) = 0;

protected:
    cwCave* Cave;
    int TotalProgress;

    virtual void runTask();

    bool checkData();
    bool checkData(cwCave* cave);

protected slots:
    void UpdateProgress(int tripProgress);


};

#endif // CWCAVEEXPORTERTASK_H
