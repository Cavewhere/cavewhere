/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCompassExporterCaveTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunkTrimmer.h"

//Std includes
#include "cwMath.h"

cwCompassExportCaveTask::cwCompassExportCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
}

/**
  Writes all the trips to the data stream
  */
bool cwCompassExportCaveTask::writeCave(QTextStream& stream, const cwCaveData &cave) {
    //Haven't done anything
    TotalProgress = 0;

    auto cavePtr = std::make_unique<cwCave>();
    cavePtr->setData(cave);

    //Go throug all the trips and save them
    for(int i = 0; i < cavePtr->tripCount(); i++) {
        cwTrip* trip = cavePtr->trip(i);
        writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
        stream << compassNewLine();
    }

    return true;
}

/**
  Writes a signle trip to the stream
  */
void cwCompassExportCaveTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    writeHeader(stream, trip);

    //Make sure the trip has data
    if(trip->chunkCount() <= 1) {
        if(trip->chunkCount() == 1) {
            cwSurveyChunk* chunk = trip->chunk(0);

            //Trim the invalid stations off
            cwSurveyChunkTrimmer::trim(chunk);

            if(!chunk->isValid()) {
                //Invalid chunk
                writeInvalidTripData(stream, trip);
            }
        } else {
            //No chunks
            writeInvalidTripData(stream, trip);
        }
    }

    //Write all chunks
    cwTripCalibration* calibration = trip->calibrations();
    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        writeChunk(stream, chunk, calibration);

        if(chunk->lastCalibration() != nullptr) {
            calibration = chunk->lastCalibration();
        }
    }

    stream << (char)(0x0C); //0C is the ending char for the compass file
}

/**
  Writes the compass file header to a file
  */
void cwCompassExportCaveTask::writeHeader(QTextStream& stream, cwTrip* trip) {
    cwCave* cave = trip->parentCave();

    Q_ASSERT(cave != nullptr);

    writeData(stream, "Cave Name", -80, cave->name());
    stream << compassNewLine();

    stream << "SURVEY NAME: ";
    writeData(stream, "Survey Name", -12, trip->name().remove(" "));
    stream << compassNewLine();

    stream << "SURVEY DATE: ";
    writeData(stream, "Survey Date", -12, QString("%1 %2 %3")
              .arg(trip->date().date().month())
              .arg(trip->date().date().day())
              .arg(trip->date().date().year()));
    stream << " COMMENT:" << compassNewLine();

    stream << "SURVEY TEAM: " << compassNewLine();
    writeData(stream, "Survey Team", -100, surveyTeam(trip));
    stream << compassNewLine();

    writeDeclination(stream, trip->calibrations());
    writeDataFormat(stream, trip->calibrations());
    writeCorrections(stream, trip->calibrations());


    stream << compassNewLine();
    stream << "FROM TO   LEN  BEAR   INC LEFT UP DOWN RIGHT AZM2 INC2 FLAGS COMMENTS" << compassNewLine();
    stream << compassNewLine();
}

void cwCompassExportCaveTask::writeDataFormat(QTextStream &stream, cwTripCalibration *calibrations) {

    stream << "FORMAT: D";

    //Write if the distance units
    switch(calibrations->distanceUnit()) {
    case cwUnits::Feet:
        stream << "DD";
        break;
    case cwUnits::Meters:
        stream << "MM";
        break;
    default:
        stream << "MM";
    }

    //Write all the other options
    stream << "DLRUDLAD";

    //Write the if the trip has backsights
    if(calibrations->hasBackSights()) {
        stream << "B ";
    } else {
        stream << "N ";
    }
}

/**
  \brief Writes the declination to the header for a trip
  */
void cwCompassExportCaveTask::writeDeclination(QTextStream &stream, cwTripCalibration *calibrations) {
    stream << "DECLINATION: " << QString("%1 ").arg(calibrations->declination(), 0, 'f', 2);
}

/**
  Writes data to the stream, with a fieldLength.  FieldName is only used for error reporteding.
  If data is longer then fieldLength, then data is truncated and an error is reported.
  */
void cwCompassExportCaveTask::writeData(QTextStream& stream,
                                        QString fieldName,
                                        int fieldLength,
                                        QString data) {
    QString paddedString;
    if(fieldLength < 0) {
        paddedString = QString("%1").arg(data, fieldLength);
    } else {
        //This is evil, need to resize from the left, not from the right
        if(data.size() > fieldLength) {
            data.resize(fieldLength);
        }
        paddedString = QString("%1").arg(data, fieldLength);
    }

    if(data.size() > fieldLength) {
        Errors.append(QString("Warning: Truncating %1 field from \"%2\" to \"%3\"").arg(fieldName,
                                                                                        data,
                                                                                        paddedString));
    }

    stream << paddedString;
}

/**
  Writes a chunk to the stream

  THe chunk has all the real survey data
  */
void cwCompassExportCaveTask::writeChunk(QTextStream& stream,
                                         cwSurveyChunk* chunk,
                                         cwTripCalibration* calibration) {

    //Trim the invalid stations off
    cwSurveyChunkTrimmer::trim(chunk);

    //Go through all the shots
    for(int i = 0; i < chunk->shotCount(); i++) {
        //Change the calibration
        cwTripCalibration* overrideCalibration = chunk->calibrations().value(i, nullptr);
        calibration = overrideCalibration == nullptr ? calibration : overrideCalibration;

        cwShot shot = chunk->shot(i);
        cwStation from = chunk->station(i);
        cwStation to = chunk->station(i + 1);

        writeShot(stream, calibration, from, to, shot, false);
    }

    //Write the last stations LRUD
    cwStation lastStation = chunk->station(chunk->stationCount() - 1);
    cwShot emptyShot;
    writeShot(stream, calibration, lastStation, lastStation, emptyShot, true);
}

/**
  Extracts the data from the station's LRUD field

  This will convert the value into decimal feet
  */
double cwCompassExportCaveTask::convertField(cwStation station,
                                             StationLRUDField field,
                                             cwUnits::LengthUnit unit) {

    double value = -999.0;
    switch(field) {
    case Left:
        if(station.left().state() == cwDistanceReading::State::Valid) {
            value = station.left().toDouble();
            return cwUnits::convert(value, unit, cwUnits::Feet);
        }
        break;
    case Right:
        if(station.right().state() == cwDistanceReading::State::Valid) {
            value = station.right().toDouble();
            return cwUnits::convert(value, unit, cwUnits::Feet);
        }
        break;
    case Up:
        if(station.up().state() == cwDistanceReading::State::Valid) {
            value = station.up().toDouble();
            return cwUnits::convert(value, unit, cwUnits::Feet);
        }
        break;
    case Down:
        if(station.down().state() == cwDistanceReading::State::Valid) {
            value = station.down().toDouble();
            return cwUnits::convert(value, unit, cwUnits::Feet);
        }
        break;
    }
    return value;
}

/**
  \brief Extracts the compass and clino from the shot

  This will convert the shot's compass and clino data such that it works correctly in compass
  */
double cwCompassExportCaveTask::convertField(cwTripCalibration* calibrations, cwShot shot, ShotField field) {

    double frontSite;
    double backSite;

    switch(field) {
    case Compass:
    case BackCompass:
        frontSite = shot.compass().toDouble();
        backSite = shot.backCompass().toDouble();
        break;
    case Clino:
    case BackClino:
        frontSite = shot.clino().toDouble();
        backSite = shot.backClino().toDouble();
    }

    double value = -999.0;

    if(field == Clino || field == BackClino) {
        convertFromDownUp(shot.clino().state(), &frontSite);
        convertFromDownUp(shot.backClino().state(), &backSite);
    }

    switch(field) {
    case Compass:

        if(shot.compass().state() == cwCompassReading::State::Valid) {
            value = frontSite;

            if(calibrations->hasCorrectedClinoFrontsight()) {
                value = value + 180.0;
            }

            if(calibrations->frontCompassCalibration() != calibrations->backCompassCalibration()) {
                value += calibrations->frontCompassCalibration();
            }

            value = fmod(value, 360.0);
        }

        break;
    case BackCompass:

        if(shot.backCompass().state() == cwCompassReading::State::Valid) {
            //Backsite is valid
            value = backSite;
            if(calibrations->hasCorrectedCompassBacksight()) {
                value = value + 180.0;
            }

            if(calibrations->frontCompassCalibration() != calibrations->backCompassCalibration()) {
                value += calibrations->backClinoCalibration();
            }
            value = fmod(value, 360.0);
        }

        break;
    case Clino:
        if(shot.clino().state() == cwClinoReading::State::Valid
                || shot.clino().state() == cwClinoReading::State::Up
                || shot.clino().state() == cwClinoReading::State::Down) {
            if(calibrations->hasCorrectedCompassFrontsight()) {
                value = -frontSite;
            } else {
                value = frontSite;
            }

            if(calibrations->frontClinoCalibration() != calibrations->backClinoCalibration()) {
                value += calibrations->frontClinoCalibration();
            }

            value = qBound(-90.0, value, 90.0);
        }
        break;
    case BackClino:
        if(shot.backClino().state() == cwClinoReading::State::Valid
                || shot.backClino().state() == cwClinoReading::State::Up
                || shot.backClino().state() == cwClinoReading::State::Down) {
            if(calibrations->hasCorrectedClinoBacksight()) {
                value = -backSite;
            } else {
                value = backSite;
            }

            if(calibrations->frontClinoCalibration() != calibrations->backClinoCalibration()) {
                value += calibrations->backClinoCalibration();
            }

            value = qBound(-90.0, value, 90.0);
        }
        break;
    }

    return value;
}

/**
  Heleper to extract shot data
  */
bool cwCompassExportCaveTask::convertFromDownUp(cwClinoReading::State clinoReading, double* value) {
    switch(clinoReading) {
    case cwClinoReading::State::Up:
        *value = 90.0;
        return false;
    case cwClinoReading::State::Down:
        *value = -90.0;
        return true;
    default:
        return false;
    }
}

QString cwCompassExportCaveTask::formatDouble(double value) {
    return QString("%1").arg(value, 7, 'f', 2);
}

/**
  Creates the team line for a trip

  All team member are join together using a comma
  */
QString cwCompassExportCaveTask::surveyTeam(cwTrip* trip) {
    cwTeam* team = trip->team();

    QStringList teamMemberNames;
    for(int i = 0; i < team->rowCount(); i++) {
        QModelIndex index = team->index(i);
        QString memberName = index.data(cwTeam::NameRole).toString();
        teamMemberNames.append(memberName);
    }

    return teamMemberNames.join(", ");
}

/**
  This writes an invalid trip data, so compass can open the trip

  If we don't do this, compass will fail to open the trip
  */
void cwCompassExportCaveTask::writeInvalidTripData(QTextStream &stream, cwTrip* trip)
{
    cwStation fromStation;
    fromStation.setName("New1");
    cwStation toStation;
    toStation.setName("New2");
    cwShot emptyShot;

    writeShot(stream, trip->calibrations(), fromStation, toStation, emptyShot, false);
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
void cwCompassExportCaveTask::writeShot(QTextStream &stream,
                                        cwTripCalibration* calibrations,
                                        const cwStation &fromStation,
                                        const cwStation &toStation,
                                        cwShot shot,
                                        bool LRUDShotOnly)
{
    writeData(stream, "From", 12, fromStation.name().toLower());
    stream << " ";
    if(LRUDShotOnly) {
        writeData(stream, "To", 12, fromStation.name().toLower());
        stream << " ";
        stream << formatDouble(0.0) << " ";
        stream << formatDouble(-999) << " ";
        stream << formatDouble(-999) << " ";
    } else {
        double shotLength = cwUnits::convert(shot.distance().toDouble(),
                                             calibrations->distanceUnit(),
                                             cwUnits::Feet);

        if(!calibrations->hasFrontSights()) {
            shot.setCompass(cwCompassReading(fmod(convertField(calibrations, shot, BackCompass) + 180.0, 360.0)));
            shot.setClino(cwClinoReading(-convertField(calibrations, shot, BackClino)));
        }

        writeData(stream, "To", 12, toStation.name().toLower());
        stream << " ";
        stream << formatDouble(shotLength) << " ";
        stream << formatDouble(convertField(calibrations, shot, Compass)) << " ";
        stream << formatDouble(convertField(calibrations, shot, Clino)) << " ";
    }

    stream << formatDouble(convertField(fromStation, Left, calibrations->distanceUnit())) << " ";
    stream << formatDouble(convertField(fromStation, Up, calibrations->distanceUnit())) << " ";
    stream << formatDouble(convertField(fromStation, Down, calibrations->distanceUnit())) << " ";
    stream << formatDouble(convertField(fromStation, Right, calibrations->distanceUnit())) << " ";

    //Write out backsight
    if(calibrations->hasBackSights()) {
        if(LRUDShotOnly) {
            stream << formatDouble(-999.0) << " ";
            stream << formatDouble(-999.0) << " ";
        } else {
            stream << formatDouble(convertField(calibrations, shot, BackCompass)) << " ";
            stream << formatDouble(convertField(calibrations, shot, BackClino)) << " ";
        }
    }

    //Write out if the distance is included
    if(!shot.isDistanceIncluded()) {
        stream << "#|L# ";
    }

    stream << compassNewLine();
}

/**
  \brief Writes the correction information to the compass file
  */
void cwCompassExportCaveTask::writeCorrections(QTextStream &stream, cwTripCalibration *calibrations)
{
    double compassCorrections = 0.0;
    if(calibrations->frontCompassCalibration() == calibrations->backCompassCalibration()) {
        compassCorrections = calibrations->frontCompassCalibration();
    }

    double clinoCorrections = 0.0;
    if(calibrations->frontClinoCalibration() == calibrations->backClinoCalibration()) {
        clinoCorrections = calibrations->frontClinoCalibration();
    }

    double tapeCorrections = cwUnits::convert(calibrations->tapeCalibration(),
                                              calibrations->distanceUnit(),
                                              cwUnits::Feet);
    stream << "CORRECTIONS: " << QString("%1 %2 %3")
              .arg(compassCorrections, 0, 'f', 2)
              .arg(clinoCorrections, 0, 'f', 2)
              .arg(tapeCorrections, 0, 'f', 2);
    stream << compassNewLine();
}
