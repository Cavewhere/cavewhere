//Our includes
#include "cwSurvexExporterRegionTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwDebug.h"

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
    if(!isReady()) {
        qWarning() << "Can't set data for survexExporterRegionTask because it's already running" << LOCATION;
        return;
    }
    *Region = region;
}

/**
  \brief Outputs region to the stream
  */
bool cwSurvexExporterRegionTask::writeRegion(QTextStream& stream, cwCavingRegion* region) {
    if(!checkData()) {
        return false;
    }

    TotalProgress = 0;

    for(int i = 0; i < region->caveCount(); i++) {
        cwCave* cave = region->cave(i);
        bool good = CaveExporter->writeCave(stream, cave);
        stream << endl;

        if(!good) {
            return false;
        }
    }

    return true;
}

/**
  \brief Runs the survex exporter task
  */
void cwSurvexExporterRegionTask::runTask() {
    if(openOutputFile()) {
        bool good = writeRegion(OutputStream, Region);
        closeOutputFile();

        if(!good) {
            stop();
        }
    } else {
        stop();
    }

    done();
}

/**
  \brief Updates the current progress of the task
  */
void cwSurvexExporterRegionTask::UpdateProgress(int /*tripProgress*/) {

}

/**
    This makes sure that the data is good
  */
bool cwSurvexExporterRegionTask::checkData() {
    if(!Region->hasCaves()) {
        //No caves to process
        Errors.append("No caves to do loop closure");
        return false;
    }
    return true;
}
