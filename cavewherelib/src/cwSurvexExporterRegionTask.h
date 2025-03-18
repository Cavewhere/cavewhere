/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTERREGIONTASK_H
#define CWSURVEXEXPORTERREGIONTASK_H

//Our includes
#include "cwSurvexExporterCaveTask.h"
class cwCavingRegion;

class cwSurvexExporterRegionTask : public cwExporterTask {
    Q_OBJECT

public:
    cwSurvexExporterRegionTask(QObject* parent = nullptr);

    void setData(const cwCavingRegion& region);

    bool writeRegion(QTextStream& stream, cwCavingRegion* region);

protected:
    virtual void runTask();

private slots:
    void UpdateProgress(int tripProgress);

private:
    cwSurvexExporterCaveTask* CaveExporter;
    cwCavingRegion* Region;
    int TotalProgress;

    //Makes sure the region has caves
    bool checkData();
};

#endif // CWSURVEXEXPORTERREGIONTASK_H
