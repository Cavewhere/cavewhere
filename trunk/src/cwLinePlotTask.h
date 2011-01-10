#ifndef CWLINEPLOTTASK_H
#define CWLINEPLOTTASK_H

//Our includes
#include "cwTask.h"
#include "cwCavingRegion.h"
class cwSurvexExporterRegionTask;
class cwCavernTask;
class cwPlotSauceTask;
class cwPlotSauceXMLTask;

//Qt includes
#include <QTemporaryFile>
#include <QVector3D>
#include <QTime>

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

    //For testing
    void setStationPosition(QString name, QVector3D position);

private:
    //The region data
    cwCavingRegion* Region;

    //The temparary survex file
    QTemporaryFile* SurvexFile;
    cwSurvexExporterRegionTask* SurvexExporter;

    cwCavernTask* CavernTask;
    cwPlotSauceTask* PlotSauceTask;
    cwPlotSauceXMLTask* PlotSauceParseTask;

    //For performance testing
    QTime Time;
};

#endif // CWLINEPLOTTASK_H
