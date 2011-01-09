#ifndef CWLINEPLOTTASK_H
#define CWLINEPLOTTASK_H

//Our includes
#include "cwTask.h"
#include "cwCavingRegion.h"
class cwSurvexExporterRegionTask;
class cwCavernTask;
class cwPlotSauceTask;

//Qt includes
#include <QTemporaryFile>

class cwLinePlotTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwLinePlotTask(QObject *parent = 0);

signals:

protected:
    virtual void runTask();

public slots:
    void setData(cwCavingRegion region);

private slots:
    void exportData();
    void runCavern();
    void convertToXML();
    void readXML();
    void complete();

private:
    //The region data
    cwCavingRegion* Region;

    //The temparary survex file
    QTemporaryFile* SurvexFile;
    cwSurvexExporterRegionTask* SurvexExporter;

    cwCavernTask* CavernTask;
    cwPlotSauceTask* PlotSauceTask;

};

#endif // CWLINEPLOTTASK_H
