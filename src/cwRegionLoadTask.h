#ifndef CWREGIONLOADTASK_H
#define CWREGIONLOADTASK_H

//Our includes
#include "cwRegionIOTask.h"
class cwCavingRegion;

class cwRegionLoadTask : public cwRegionIOTask
{
    Q_OBJECT
public:
    explicit cwRegionLoadTask(QObject *parent = 0);

signals:
    void finishedLoading(cwCavingRegion* region);

public slots:

protected:
    void runTask();

private:
    QString readXMLFromDatabase();
    QString readProtoBufferFromDatabase();

    void insureVacuuming();


};

#endif // CWREGIONLOADTASK_H
