//Our includes
#include "cwWallsExporterCaveTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwStationReference.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

//Std includes
#include <math.h>

const char* cwWallsExporterCaveTask::WallsNewLine = "\r\n";
const char* cwWallsExporterCaveTask::WallsComment = ";";

cwWallsExporterCaveTask::cwWallsExporterCaveTask(QObject *parent) :
    cwCaveExporterTask(parent){}

/** Writes all the trips to the data stream */
void cwWallsExporterCaveTask::writeCave(QTextStream& stream, cwCave* cave) {
    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
        stream << WallsNewLine;
    }
}

/** Writes a single trip to the stream */
void cwWallsExporterCaveTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    //fix this here - Walls trips should be split up into individual files
    writeHeader(stream, trip);

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        writeChunk(stream, chunk);
    }
}

/** Writes the Walls file header to a file */
void cwWallsExporterCaveTask::writeHeader(QTextStream& stream, cwTrip* trip) {
    cwCave* cave = trip->parentCave();

    Q_ASSERT(cave != NULL);

    QDate tDate = trip->date();
    /* Write all commented data to header */
    stream << WallsComment << "Walls Files Exported by CaveWare on "
           << QDate::currentDate().toString("ddMMMyyyy")
           << WallsNewLine;

    stream << WallsComment << "Cave Name: " << cave->name() << WallsNewLine;
    stream << WallsComment << "Survey Name: " << trip->name() << WallsNewLine;
    stream << WallsComment << "Date: " << tDate.toString("dddd MMMM, d yyyy") << WallsNewLine;
    stream << WallsComment << "Survey Team: " << surveyTeam(trip) << WallsNewLine;

    cwTripCalibration* calibrations = trip->calibrations();
    //Walls automatically calculates declinations - this var is not used
    //If necessary, declination can be added to the #Units directive
    float declination = calibrations->declination();
    float CompassCorrections = calibrations->frontCompassCalibration();
    float clinoCorrections = calibrations->frontClinoCalibration();
    float tapeCorrections = cwUnits::convert(calibrations->tapeCalibration(),
                                             cwUnits::DecimalFeet, cwUnits::Meters);

    /* Write all directives to header */
    //Write Date Field
    stream << "#DATE " << tDate.toString("yyyy-dd-MM") << WallsNewLine;
    //Write Units directive + check forCorrections
    //Write more complex code here for change in data format, units - later!
    stream << "#UNITS " << "Order=DAV " << "Units=feet ";
    if(tapeCorrections)     stream << "INCD=" << QString("%1").arg(tapeCorrections,0,'f',2) << " ";
    if(CompassCorrections)  stream << "INCA=" << QString("%1").arg(CompassCorrections,0,'f',2) << " ";
    if(clinoCorrections)    stream << "INCV=" << QString("%1").arg(clinoCorrections,0,'f',2) << " ";
    //Write code here for prefix or leave out?

    stream << WallsNewLine;
}

/**
  Writes data to the stream, with a fieldLength.  FieldName is only used for error reporteding.
  If data is longer then fieldLength, then data is truncated and an error is reported.
  */
void cwWallsExporterCaveTask::writeData(QTextStream& stream,
                                        QString fieldName,
                                        int fieldLength,
                                        QString data) {
    QString paddedString;
    if(fieldLength < 0) {
        paddedString = QString("%1").arg(data, fieldLength);
    }
    else {
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
void cwWallsExporterCaveTask::writeChunk(QTextStream& stream, cwSurveyChunk* chunk) {
    cwTrip* trip = chunk->parentTrip();

    for(int i = 0; i < chunk->ShotCount(); i++) {
        cwShot* shot = chunk->Shot(i);
        cwStationReference from = shot->fromStation();
        cwStationReference to = shot->toStation();

        float shotLength = cwUnits::convert(shot->distance().toDouble(),
                                            trip->distanceUnit(),
                                            cwUnits::DecimalFeet);

        //FromSta/ToSta
        stream << from.name() << "\t" << to.name() << "\t";
        //Dist,Az,Incl - check for those pesky backsights!
        stream << formatFloat(shotLength) << "\t";
        if(shot->isForesightOnly()){
            stream << formatFloat(convertField(trip,shot,Compass)) << "\t";
            stream << formatFloat(convertField(trip,shot,Clino)) << "\t";
        }
        else{
            stream << formatFloat(convertField(trip, shot, Compass))   << "/"
                   << formatFloat(convertField(trip,shot,BackCompass)) << "\t";
            stream << formatFloat(convertField(trip, shot, Clino))     << "/"
                   << formatFloat(convertField(trip,shot,BackClino))   << "\t";
        }
        //LRUDs, do I need to check for existence of LRUDs?
        if(from.left().isNull() || from.right().isNull() || from.up().isNull() || from.down().isNull()){
            stream << "<" << formatFloat(convertField(from, Left, trip->distanceUnit())) << ",";
            stream << formatFloat(convertField(from, Up, trip->distanceUnit())) << ",";
            stream << formatFloat(convertField(from, Right, trip->distanceUnit())) << ",";
            stream << formatFloat(convertField(from, Down, trip->distanceUnit())) << ">";
        }

        stream << WallsNewLine;
    }

    cwStationReference ls = chunk->Station(chunk->StationCount() - 1);
    if(ls.left().isNull() || ls.right().isNull() || ls.up().isNull() || ls.down().isNull()){
        stream << ls.name() << "\t";
        stream << "<" << formatFloat(convertField(ls, Left, trip->distanceUnit())) << ",";
        stream << formatFloat(convertField(ls, Up, trip->distanceUnit())) << ",";
        stream << formatFloat(convertField(ls, Right, trip->distanceUnit())) << ",";
        stream << formatFloat(convertField(ls, Down, trip->distanceUnit())) << ">";
    }
    stream << WallsNewLine;
}

/**
  Extracts the data from the station's LRUD field

  This will convert the value into decimal feet
  */
float cwWallsExporterCaveTask::convertField(cwStationReference station,
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

    return cwUnits::convert(numValue, unit, cwUnits::DecimalFeet);
}

/**
  \brief Extracts the compass and clino from the shot

  This will convert the shot's compass and clino data such that it works correctly in compass
  */
float cwWallsExporterCaveTask::convertField(cwTrip* trip, cwShot* shot, ShotField field) {

    QString frontSite;
    QString backSite;

    switch(field) {
    case Compass:
    case BackCompass:
        frontSite = shot->compass();
        backSite = shot->backCompass();
        break;
    case Clino:
    case BackClino:
        frontSite = shot->clino();
        backSite = shot->backClino();
    }

    float value=0.0;

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

/** Helper to extract shot data */
QString cwWallsExporterCaveTask::convertFromDownUp(QString clinoReading) {
    if(clinoReading.compare("down", Qt::CaseInsensitive) == 0) {
        clinoReading = "-90.0";
    } else if(clinoReading.compare("up", Qt::CaseInsensitive) == 0) {
        clinoReading = "90.0";
    }
    return clinoReading;
}

QString cwWallsExporterCaveTask::formatFloat(float value) {
    return QString("%1").arg(value, 4, 'f', 2);
}

///** Helper to extract shot data */
//float cwWallsExporterCaveTask::fixCompass(cwTrip* trip, QString compass1, QString compass2) {
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

///** Helper to extract shot data */
//float cwWallsExporterCaveTask::fixClino(cwTrip* trip, QString forward, QString backward) {
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

/** Creates the team line for a trip - All team member are join together using a comma */
QString cwWallsExporterCaveTask::surveyTeam(cwTrip* trip) {
    cwTeam* team = trip->team();

    QStringList teamMemberNames;
    for(int i = 0; i < team->rowCount(); i++) {
        QModelIndex index = team->index(i);
        QString memberName = index.data(cwTeam::NameRole).toString();
        teamMemberNames.append(memberName);
    }

    return teamMemberNames.join(", ");
}
