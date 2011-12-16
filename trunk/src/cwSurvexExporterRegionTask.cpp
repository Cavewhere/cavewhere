//Our includes
#include "cwSurvexExporterRegionTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"

//Qt includes
#include <QTime>
#include <QDebug>

cwSurvexExporterRegionTask::cwSurvexExporterRegionTask(QObject* parent) :
    cwExporterTask(parent)
{
    CaveExporter = new cwSurvexExporterCaveTask(this);
    CaveExporter->setParentSurvexExporter(this);
    connect(CaveExporter, SIGNAL(progressed(int)), SLOT(UpdateProgress(int)));
    Region = new cwCavingRegion(this);
}

/**
  \brief Sets the data for the task
  */
void cwSurvexExporterRegionTask::setData(const cwCavingRegion& region) {
    if(isRunning()) {
        qWarning() << "Can't set data for survexExporterRegionTask because it's already running";
        return;
    }
    *Region = region;
}

/**
  \brief Outputs region to the stream
  */
void cwSurvexExporterRegionTask::writeRegion(QTextStream& stream, cwCavingRegion* region) {
    TotalProgress = 0;

    for(int i = 0; i < region->caveCount(); i++) {
        cwCave* cave = region->cave(i);
        CaveExporter->writeCave(stream, cave);
        stream << endl;
    }

}

/**
  \brief Runs the survex exporter task
  */
void cwSurvexExporterRegionTask::runTask() {
    openOutputFile();
    writeRegion(OutputStream, Region);
    closeOutputFile();
    done();
}

/**
  \brief Updates the current progress of the task
  */
void cwSurvexExporterRegionTask::UpdateProgress(int /*tripProgress*/) {

}
