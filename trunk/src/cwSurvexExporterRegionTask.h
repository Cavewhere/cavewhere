#ifndef CWSURVEXEXPORTERREGIONTASK_H
#define CWSURVEXEXPORTERREGIONTASK_H

//Our includes
#include "cwSurvexExporterCaveTask.h"
class cwCavingRegion;

class cwSurvexExporterRegionTask : public cwExporterTask {
    Q_OBJECT

public:
    cwSurvexExporterRegionTask(QObject* parent = NULL);

    void setData(const cwCavingRegion& region);

    void writeRegion(QTextStream& stream, cwCavingRegion* region);

protected:
    virtual void runTask();

private slots:
    void UpdateProgress(int tripProgress);

private:
    cwSurvexExporterCaveTask* CaveExporter;
    cwCavingRegion* Region;
    int TotalProgress;

};

#endif // CWSURVEXEXPORTERREGIONTASK_H
