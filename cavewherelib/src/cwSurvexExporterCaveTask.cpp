/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterUtils.h"
#include "cwTrip.h"

//Qt includes
#include <QSet>

cwSurvexExporterCaveTask::cwSurvexExporterCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
    TripExporter = new cwSurvexExporterTripTask(this);
    TripExporter->setParentSurvexExporter(this);

//    connect(TripExporter, SIGNAL(progressed(int)), SIGNAL(progressed(int)));
}

/**
  \brief Writes the cave data to the stream
  */
bool cwSurvexExporterCaveTask::writeCave(QTextStream& stream, const cwCaveData& cave, const QString& globalCS) {

    if(!checkData(cave)) {
        if(isRunning()) {
            stop();
        }
        return false;
    }

    QString caveName = QString(cave.name).remove(" ");

    stream << "*begin " << caveName << " ;" << cave.name << Qt::endl << Qt::endl;

    writeFixStations(stream, cave, globalCS);

    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave.trips.size(); i++) {
        const cwTripData& tripData = cave.trips.at(i);
        auto trip = std::make_unique<cwTrip>();
        trip->setData(tripData);
        TripExporter->writeTrip(stream, trip.get());
        TotalProgress += trip->numberOfStations();
        stream << Qt::endl;
    }

    stream << "*end " << caveName << " ; End of " << cave.name << Qt::endl;

    return true;
}

/**
 * Emit the *cs / *fix block for the cave. Validates the snapshot's
 * fixStations against the actual station names; rejected fixes are dropped
 * silently here (the user-facing exporter — cwSurvexExporterRule — runs
 * the same validation at snapshot time and surfaces errors on the cave).
 * Falls back to `*fix <firstStation> 0 0 0` when no valid fix exists so
 * un-fixed caves still resolve in cavern.
 */
void cwSurvexExporterCaveTask::writeFixStations(QTextStream &stream, const cwCaveData &cave, const QString& globalCS)
{
    QSet<QString> stationNamesLower;
    QString firstValidStation;
    for (const cwTripData& trip : cave.trips) {
        for (const cwSurveyChunkData& chunk : trip.chunks) {
            for (const cwStation& station : chunk.stations) {
                if (station.isValid()) {
                    stationNamesLower.insert(cwStation::canonicalKey(station.name()));
                    if (firstValidStation.isEmpty()) {
                        firstValidStation = station.name();
                    }
                }
            }
        }
    }

    QStringList errors;
    const QList<cwFixStation> validFixes = cwSurvexExporterUtils::validateFixStations(
        cave.fixStations, stationNamesLower, errors);
    for (const QString& message : std::as_const(errors)) {
        Errors.append(message);
    }

    cwSurvexExporterUtils::writeFixStations(stream, validFixes, firstValidStation, globalCS);
}


