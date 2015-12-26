/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwChipdataExporterCaveTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunkTrimmer.h"

//Std includes
#include "cwMath.h"

const QString cwChipdataExportCaveTask::ChipdataNewLine("\n");

cwChipdataExportCaveTask::cwChipdataExportCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
}

/**
  Writes all the trips to the data stream
  */
bool cwChipdataExportCaveTask::writeCave(QTextStream& stream, cwCave* cave) {
    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
    }

    return true;
}

/**
  Writes a signle trip to the stream
  */
void cwChipdataExportCaveTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    writeHeader(stream, trip);

    //Write all chunks
    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        //Trim the invalid stations off
        cwSurveyChunkTrimmer::trim(chunk);

        if (chunk->isValid()) {
            writeChunk(stream, chunk);
        }
    }
}

/**
  Writes the compass file header to a file
  */
void cwChipdataExportCaveTask::writeHeader(QTextStream& stream, cwTrip* trip) {
    cwCave* cave = trip->parentCave();
    Q_ASSERT(cave != nullptr);

    stream << " *" << ChipdataNewLine;
    stream << trip->name() << ChipdataNewLine;
    QList<cwTeamMember> teamMembers = trip->team()->teamMembers();
    for (int i = 0; i < teamMembers.size(); i++) {
        if (i > 0) {
            stream << ", ";
        }
        stream << teamMembers[i].name();
    }
    if (trip->date().isValid()) {
        stream << " - " << trip->date().toString() << ChipdataNewLine;
    }

    stream << " *" << ChipdataNewLine;

    writeDataFormat(stream, trip->calibrations());
}

void cwChipdataExportCaveTask::writeDataFormat(QTextStream &stream, cwTripCalibration *calibrations) {
    switch (calibrations->distanceUnit()) {
    case cwUnits::Feet: 	stream << "FT "; break;
    case cwUnits::Inches:	stream << "FI "; break;
    default:			stream << "M  "; break;
    }

    stream << (calibrations->hasCorrectedCompassBacksight() ? "C" : "B");
    stream << (calibrations->hasCorrectedClinoBacksight() ? "C" : "B");
    stream << " DD" << ChipdataNewLine;
}

/**
  Writes a chunk to the stream
  THe chunk has all the real survey data
  */
void cwChipdataExportCaveTask::writeChunk(QTextStream& stream, cwSurveyChunk* chunk) {
    cwTrip* trip = chunk->parentTrip();

    //Trim the invalid stations off
    cwSurveyChunkTrimmer::trim(chunk);

    //Go through all the shots
    for(int i = 0; i < chunk->shotCount(); i++) {
        cwShot shot = chunk->shot(i);
        cwStation from = chunk->station(i);
        cwStation to = chunk->station(i + 1);

        writeShot(stream, trip->calibrations(), from, to, shot);
    }
}

/**
  This writes a shot to the strem

  calibrations - The calibrations for the trip
  fromStation - the from station of the shot
  toStation - the to station of the shot
  shot - The shot's data
  LRUDShotOnly - This shot is really just printing out the last station's LRUD data
  this hould has zero length, direction and clino readings.
  */
void cwChipdataExportCaveTask::writeShot(QTextStream &stream,
                                        cwTripCalibration* calibrations,
                                        const cwStation &fromStation,
                                        const cwStation &toStation,
                                        cwShot shot)
{
    stream << fromStation.name().rightJustified(5, ' ', true);
    stream << toStation.name().rightJustified(5, ' ', true);

    if (shot.distanceState() == cwDistanceStates::Valid) {
        switch (calibrations->distanceUnit()) {
        case cwUnits::Feet:
            stream << QString::number(shot.distance(), 'f', 2).rightJustified(6, ' ', true) << ' ';
            break;
        case cwUnits::Inches:
            stream << QString::number(shot.distance() / 12.0, 'f', 0).rightJustified(4, ' ', true);
            stream << QString::number(fmod(shot.distance(), 12.0), 'f', 0).rightJustified(3, ' ', true);
            break;
        default:
            stream << QString::number(cwUnits::convert(shot.distance(), calibrations->distanceUnit(), cwUnits::Meters), 'f', 2)
                      .rightJustified(6, ' ', true) << ' ';
            break;
        }
    }
    else {
        stream << "       ";
    }

    if (!shot.isDistanceIncluded()) {
        stream << "*";
    } else {
        stream << " ";
    }

    if (shot.compassState() == cwCompassStates::Valid) {
        stream << QString::number(shot.compass(), 'f', 2).rightJustified(6, ' ', true);
    } else {
        stream << "      ";
    }
    if (shot.backCompassState() == cwCompassStates::Valid) {
        stream << QString::number(shot.backCompass(), 'f', 2).rightJustified(6, ' ', true);
    } else {
        stream << "      ";
    }
    if (shot.clinoState() == cwClinoStates::Valid) {
        stream << QString::number(shot.clino(), 'f', 1).rightJustified(5, ' ', true);
    } else {
        stream << "     ";
    }
    if (shot.backClinoState() == cwClinoStates::Valid) {
        stream << QString::number(shot.backClino(), 'f', 1).rightJustified(5, ' ', true);
    } else {
        stream << "     ";
    }

    cwUnits::LengthUnit lrudUnit;
    if (calibrations->distanceUnit() == cwUnits::Feet ||
            calibrations->distanceUnit() == cwUnits::Inches) {
        lrudUnit = cwUnits::Feet;
    } else {
        lrudUnit = cwUnits::Meters;
    }


    writeLrudMeasurement(stream, toStation.leftInputState(),  toStation.left(),  calibrations->distanceUnit(), lrudUnit);
    writeLrudMeasurement(stream, toStation.rightInputState(), toStation.right(), calibrations->distanceUnit(), lrudUnit);
    writeLrudMeasurement(stream, toStation.upInputState(),    toStation.up(),    calibrations->distanceUnit(), lrudUnit);
    writeLrudMeasurement(stream, toStation.downInputState(),  toStation.down(),  calibrations->distanceUnit(), lrudUnit);

    stream << ChipdataNewLine;
}

void cwChipdataExportCaveTask::writeLrudMeasurement(QTextStream &stream, cwDistanceStates::State state, double measurement, cwUnits::LengthUnit fromUnit, cwUnits::LengthUnit toUnit)
{
    if (state == cwDistanceStates::Valid) {
        stream << QString::number(cwUnits::convert(measurement, fromUnit, toUnit), 'f', 1).rightJustified(3, ' ', true);
    } else {
        stream << "   ";
    }

}
