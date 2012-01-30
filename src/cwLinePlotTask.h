#ifndef CWLINEPLOTTASK_H
#define CWLINEPLOTTASK_H

//Our includes
#include "cwTask.h"
#include "cwCavingRegion.h"
#include "cwLinePlotGeometryTask.h"
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

    QVector<QVector3D> stationPositions() const;
    QVector<unsigned int> linePlotIndexData() const;

signals:

protected:
    virtual void runTask();

public slots:
    void setData(const cwCavingRegion &region);

private slots:
    void exportData();
    void runCavern();
    void convertToXML();
    void readXML();
    void generateCenterlineGeometry();
    void linePlotTaskComplete();

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
    cwLinePlotGeometryTask* CenterlineGeometryTask;

    //For performance testing
    QTime Time;
};

/**
  This functions aren't thread safe!!! You should only call these if the task isn't running

  This returns all the positions of the points.  This is used strictly for rendering.
  */
inline QVector<QVector3D> cwLinePlotTask::stationPositions() const {
    return CenterlineGeometryTask->pointData();
}

/**
  This functions aren't thread safe!! You should only call these if the task isn't running

  Returns all the plot line data indexes.  This is to construct a line array
  */
inline QVector<unsigned int> cwLinePlotTask::linePlotIndexData() const {
    return CenterlineGeometryTask->indexData();
}

#endif // CWLINEPLOTTASK_H
