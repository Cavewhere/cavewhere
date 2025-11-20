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
#include "cwCavingRegionData.h"

class cwSurvexExporterRegionTask : public cwExporterTask {
    Q_OBJECT

public:
    cwSurvexExporterRegionTask(QObject* parent = nullptr);

    void setData(const cwCavingRegionData &region);

    bool writeRegion(QTextStream& stream, const cwCavingRegionData& region);

protected:
    virtual void runTask();

private slots:
    void UpdateProgress(int tripProgress);

private:
    cwSurvexExporterCaveTask* CaveExporter;
    cwCavingRegionData Region;
    int TotalProgress;

    //Makes sure the region has caves
    bool checkData();
};

#endif // CWSURVEXEXPORTERREGIONTASK_H
