/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporter.h"
#include "cwSurvexExporterUtils.h"

//Qt includes
#include <QTextStream>

cwSurvexExporterTripTask::cwSurvexExporterTripTask(QObject *parent) :
    cwExporterTask(parent)
{
}

/**
  \brief Sets the trip data
  */
void cwSurvexExporterTripTask::setData(const cwTripData &trip) {
    if(!isRunning()) {
        Trip = trip;
    } else {
        qDebug() << QStringLiteral("Can't set trip data will the trip exporter is running");
    }
}

void cwSurvexExporterTripTask::setCaveFixStations(const QList<cwFixStation> &fixStations) {
    if(!isRunning()) {
        CaveFixStations = fixStations;
    } else {
        qDebug() << QStringLiteral("Can't set the cave's fix stations while the trip exporter is running");
    }
}

/**
  \brief Saves the trip to a survex file

  Both setData and setOutputFile should be set before calling cwTask::start
  */
void cwSurvexExporterTripTask::runTask() {
    setNumberOfSteps(cwSurvexExporter::stationCount(Trip));

    openOutputFile();
    QTextStream& stream = *OutputStream.data();

    const bool autoDeclinationInScope = cwSurvexExporterUtils::writeStandaloneTripHeader(
        stream, sidecars(), CaveFixStations, Trip.calibrations.autoDeclination());

    writeTrip(stream, Trip, autoDeclinationInScope);
    closeOutputFile();

    done();
}

/**
  \brief Writes a trip to a stream
  */
void cwSurvexExporterTripTask::writeTrip(QTextStream& stream,
                                         const cwTripData& trip,
                                         bool autoDeclinationInScope,
                                         double gridConvergence) {
    cwSurvexExporter::writeTrip(stream, trip, Errors, autoDeclinationInScope, gridConvergence);
}
