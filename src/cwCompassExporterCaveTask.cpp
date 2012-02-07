//Our includes
#include "cwCompassExporterCaveTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

//Std includes
#include <math.h>

const char* cwCompassExportCaveTask::CompassNewLine = "\r\n";

cwCompassExportCaveTask::cwCompassExportCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
}

/**
  Writes all the trips to the data stream
  */
bool cwCompassExportCaveTask::writeCave(QTextStream& stream, cwCave* cave) {
    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
        stream << CompassNewLine;
    }

    return true;
}

/**
  Writes a signle trip to the stream
  */
void cwCompassExportCaveTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    writeHeader(stream, trip);

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        writeChunk(stream, chunk);
    }

    stream << (char)(0x0C); //0C is the ending char for the compass file
}

/**
  Writes the compass file header to a file
  */
void cwCompassExportCaveTask::writeHeader(QTextStream& stream, cwTrip* trip) {
    cwCave* cave = trip->parentCave();

    Q_ASSERT(cave != NULL);

    writeData(stream, "Cave Name", -80, cave->name());
    stream << CompassNewLine;

    stream << "SURVEY NAME: ";
    writeData(stream, "Survey Name", -12, trip->name());
    stream << CompassNewLine;

    stream << "SURVEY DATE: ";
    writeData(stream, "Survey Date", -12, QString("%1 %2 %3")
              .arg(trip->date().month())
              .arg(trip->date().day())
              .arg(trip->date().year()));
    stream << " COMMENT:" << CompassNewLine;

    stream << "SURVEY TEAM: " << CompassNewLine;
    writeData(stream, "Survey Team", -100, surveyTeam(trip));
    stream << CompassNewLine;

    cwTripCalibration* calibrations = trip->calibrations();
    float declination = calibrations->declination();
    float compassCorrections = calibrations->frontCompassCalibration();
    float clinoCorrections = calibrations->frontClinoCalibration();

    float tapeCorrections = cwUnits::convert(calibrations->tapeCalibration(),
                     calibrations->distanceUnit(),
                     cwUnits::Feet);

    stream << "DECLINATION: " << QString("%1 ").arg(declination, 0, 'f', 2);
    stream << "FORMAT: DMMDLRUDLAD";

    if(calibrations->hasBackSights()) {
        stream << "B ";
    } else {
        stream << "N ";
    }

    stream << "CORRECTIONS: " << QString("%1 %2 %3")
              .arg(compassCorrections, 0, 'f', 2)
              .arg(clinoCorrections, 0, 'f', 2)
              .arg(tapeCorrections, 0, 'f', 2);
    stream << CompassNewLine;

    stream << CompassNewLine;
    stream << "FROM TO   LEN  BEAR   INC LEFT RIGHT UP DOWN AZM2 INC2 FLAGS COMMENTS" << CompassNewLine;
    stream << CompassNewLine;
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
void cwCompassExportCaveTask::writeChunk(QTextStream& stream, cwSurveyChunk* chunk) {
    cwTrip* trip = chunk->parentTrip();
    cwUnits::LengthUnit distanceUnit = trip->calibrations()->distanceUnit();

    for(int i = 0; i < chunk->shotCount(); i++) {
        cwShot shot = chunk->shot(i);
        cwStation from = chunk->station(i);
        cwStation to = chunk->station(i + 1);

        float shotLength = cwUnits::convert(shot.distance(),
                                            distanceUnit,
                                            cwUnits::Feet);

        writeData(stream, "From", 12, from.name());
        stream << " ";
        writeData(stream, "To", 12, to.name());
        stream << " ";
        stream << formatFloat(shotLength) << " ";
        stream << formatFloat(convertField(trip, shot, Compass)) << " ";
        stream << formatFloat(convertField(trip, shot, Clino)) << " ";
        stream << formatFloat(convertField(from, Left, trip->calibrations()->distanceUnit())) << " ";
        stream << formatFloat(convertField(from, Up, distanceUnit)) << " ";
        stream << formatFloat(convertField(from, Right, distanceUnit)) << " ";
        stream << formatFloat(convertField(from, Down, distanceUnit)) << " ";

        stream << formatFloat(convertField(trip, shot, BackCompass)) << " ";
        stream << formatFloat(convertField(trip, shot, BackClino)) << " ";
        stream << CompassNewLine;
    }

    cwStation lastStation = chunk->station(chunk->stationCount() - 1);
    writeData(stream, "From", 12, lastStation.name());
    stream << " ";
    writeData(stream, "To", 12, lastStation.name() + "lrud");
    stream << " ";
    stream << formatFloat(0.0) << " ";
    stream << formatFloat(0.0) << " ";
    stream << formatFloat(0.0) << " ";
    stream << formatFloat(convertField(lastStation, Left, distanceUnit)) << " ";
    stream << formatFloat(convertField(lastStation, Up, distanceUnit)) << " ";
    stream << formatFloat(convertField(lastStation, Right, distanceUnit)) << " ";
    stream << formatFloat(convertField(lastStation, Down, distanceUnit)) << " ";
    stream << formatFloat(180.0) << " ";
    stream << formatFloat(0.0) << " ";
    stream << CompassNewLine;
}

/**
  Extracts the data from the station's LRUD field

  This will convert the value into decimal feet
  */
float cwCompassExportCaveTask::convertField(cwStation station,
                                       StationLRUDField field,
                                       cwUnits::LengthUnit unit) {

    QString value;
    switch(field) {
    case Left:
        value = station.left();
        break;
    case Right:
        value = station.right();
        break;
    case Up:
        value = station.up();
        break;
    case Down:
        value = station.down();
        break;
    }

    bool okay;
    double numValue = value.toDouble(&okay);
    if(!okay) {
        return 0.0;
    }

    return cwUnits::convert(numValue, unit, cwUnits::Feet);
}

/**
  \brief Extracts the compass and clino from the shot

  This will convert the shot's compass and clino data such that it works correctly in compass
  */
float cwCompassExportCaveTask::convertField(cwTrip* trip, cwShot shot, ShotField field) {

    QString frontSite;
    QString backSite;

    switch(field) {
    case Compass:
    case BackCompass:
        frontSite = shot.compass();
        backSite = shot.backCompass();
        break;
    case Clino:
    case BackClino:
        frontSite = shot.clino();
        backSite = shot.backClino();
    }

    float value = 0.0f;

    if(field == Clino || field == BackClino) {
        frontSite = convertFromDownUp(frontSite);
        backSite = convertFromDownUp(backSite);
    }

    bool okay;
    switch(field) {
    case Compass:
        value = frontSite.toFloat(&okay);
        if(!okay) {
            value = backSite.toFloat();
            value = fmod((value + 180.0), 360.0);
        }
        break;
    case BackCompass:
        value = backSite.toFloat(&okay);
        if(!okay) {
            value = frontSite.toFloat();
        }
        if(!okay || trip->calibrations()->hasCorrectedCompassBacksight()) {
            value = fmod((value + 180.0), 360.0);
        }
        break;
    case Clino:
        value = frontSite.toFloat(&okay);
        if(!okay) {
            value = -backSite.toFloat();
        }
        break;
    case BackClino:
        value = backSite.toFloat(&okay);
        if(!okay) {
            value = frontSite.toFloat();
        }
        if(!okay || trip->calibrations()->hasCorrectedClinoBacksight()) {
            value = -value;
        }
        break;
    }

    return value;
}

/**
  Heleper to extract shot data
  */
QString cwCompassExportCaveTask::convertFromDownUp(QString clinoReading) {
    if(clinoReading.compare("down", Qt::CaseInsensitive) == 0) {
        clinoReading = "-90.0";
    } else if(clinoReading.compare("up", Qt::CaseInsensitive) == 0) {
        clinoReading = "90.0";
    }
    return clinoReading;
}

QString cwCompassExportCaveTask::formatFloat(float value) {
    return QString("%1").arg(value, 7, 'f', 2);
}

///**
//  Heleper to extract shot data
//  */
//float cwCompassExportCaveTask::fixCompass(cwTrip* trip, QString compass1, QString compass2) {
//    float value;
//    if(compass1.isEmpty()) {
//        if(!compass2.isEmpty()) {
//            value = fmod(compass2.toDouble() + 180.0, 360.0);
//        } else {
//            return 0.0;
//        }
//    } else {
//        value = compass1.toDouble();
//    }
//    return value;
//}

///**
//  Heleper to extract shot data
//  */
//float cwCompassExportCaveTask::fixClino(cwTrip* trip, QString forward, QString backward) {
//    float value;
//    if(forward.isEmpty()) {
//        if(!backward.isEmpty()) {
//            value = backward.toDouble() * -1;
//        } else {
//            return 0.0;
//        }
//    } else {
//        value = forward.toDouble();
//    }
//    return value;
//}

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
