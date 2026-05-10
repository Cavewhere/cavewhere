/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
//    connect(CaveExporter, SIGNAL(progressed(int)), SLOT(UpdateProgress(int)));
    // Region = new cwCavingRegion(this);
}

/**
  \brief Sets the data for the task
  */
void cwSurvexExporterRegionTask::setData(const cwCavingRegionData& region) {
    if(!isReady()) {
        qWarning() << "Can't set data for survexExporterRegionTask because it's already running" << LOCATION;
        return;
    }
    Region = region;
    // *Region = region;
}

/**
  \brief Outputs region to the stream
  */
bool cwSurvexExporterRegionTask::writeRegion(QTextStream& stream, const cwCavingRegionData &region) {
    if(!checkData()) {
        return false;
    }

    TotalProgress = 0;

    stream << "*begin  ;All the caves" << Qt::endl;

    // Mirror cwSurvexExporterRule: when globalCS is unset, fall back to the
    // first fix's inputCS so *cs out is always set whenever any *cs is
    // emitted — survex rejects *cs without a matching *cs out.
    QString outputCS = region.globalCS;
    if (outputCS.isEmpty()) {
        for (const cwCaveData& cave : region.caves) {
            for (const cwFixStation& fix : cave.fixStations) {
                if (!fix.inputCS().trimmed().isEmpty()) {
                    outputCS = fix.inputCS().trimmed();
                    break;
                }
            }
            if (!outputCS.isEmpty()) {
                break;
            }
        }
    }

    if (!outputCS.isEmpty()) {
        stream << "*cs out " << outputCS << Qt::endl;
    }

    for(int i = 0; i < region.caves.size(); i++) {
        const cwCaveData& cave = region.caves.at(i);
        bool good = CaveExporter->writeCave(stream, cave, outputCS);
        stream << Qt::endl;

        if(!good) {
            return false;
        }
    }

    stream << "*end" << Qt::endl;

    return true;
}

/**
  \brief Runs the survex exporter task
  */
void cwSurvexExporterRegionTask::runTask() {
    if(openOutputFile()) {
        bool good = writeRegion(*OutputStream.data(), Region);
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
    if(Region.caves.isEmpty()) {
        //No caves to process
        Errors.append("No caves to do loop closure");
        return false;
    }
    return true;
}
