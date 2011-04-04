//Our includes
#include "cwCompassExporterCaveTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwStationReference.h"

//Std includes
#include <math.h>

cwCompassExportCaveTask::cwCompassExportCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
}

/**
  Writes all the trips to the data stream
  */
void cwCompassExportCaveTask::writeCave(QTextStream& stream, cwCave* cave) {
    //Haven't done anything
    TotalProgress = 0;

    //Go throug all the trips and save them
    for(int i = 0; i < cave->tripCount(); i++) {
        cwTrip* trip = cave->trip(i);
        writeTrip(stream, trip);
        TotalProgress += trip->numberOfStations();
        stream << endl;
    }
}

/**
  Writes a signle trip to the stream
  */
void cwCompassExportCaveTask::writeTrip(QTextStream& stream, cwTrip* trip) {
    writeHeader(stream, trip);

    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        writeChunk(stream, chunk);
    }

    stream << (char)(0x0C);
}

/**
  Writes the compass file header to a file
  */
void cwCompassExportCaveTask::writeHeader(QTextStream& stream, cwTrip* trip) {
    cwCave* cave = trip->parentCave();

    Q_ASSERT(cave != NULL);

    writeData(stream, "Cave Name", 80, cave->name());
    stream << endl;

    stream << "SURVEY NAME: ";
    writeData(stream, "Survey Name", 12, trip->name());
    stream << endl;

    stream << "SURVEY DATE: ";
    writeData(stream, "Survey Date", 12, QString("%1 %2 %3")
              .arg(trip->date().month())
              .arg(trip->date().day())
              .arg(trip->date().year()));
    stream << " COMMENT:" << endl;

    stream << "SURVEY TEAM: ";
    writeData(stream, "Survey Team", 100, QString());
    stream << endl;
    stream << endl;

    stream << "DECLINATION: 0.00 ";
    stream << "FORMAT: DMMDLRUDLADN ";
    stream << "CORRECTIONS: 0.00 0.00 0.00 ";
    stream << endl;

    stream << endl;
    stream << "FROM TO   LEN  BEAR   INC LEFT RIGHT UP DOWN AZM2 INC2 FLAGS COMMENTS" << endl;
    stream << endl;
}

/**
  Writes data to the stream, with a fieldLength.  FieldName is only used for error reporteding.
  If data is longer then fieldLength, then data is truncated and an error is reported.
  */
void cwCompassExportCaveTask::writeData(QTextStream& stream,
                                        QString fieldName,
                                        int fieldLength,
                                        QString data) {
    QString paddedString = QString("%1").arg(data, fieldLength);

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

    for(int i = 0; i < chunk->ShotCount(); i++) {
        cwShot* shot = chunk->Shot(i);
        cwStationReference* from = shot->fromStation();
        cwStationReference* to = shot->toStation();

        float shotLength = cwUnits::convert(shot->GetDistance().toDouble(),
                                            trip->distanceUnit(),
                                            cwUnits::DecimalFeet);

        writeData(stream, "From", 12, from->name());
        stream << " ";
        writeData(stream, "To", 12, to->name());
        stream << " ";
        stream << shotLength << " ";
        stream << convertField(shot, Compass) << " ";
        stream << convertField(shot, Clino) << " ";
        stream << convertField(from, Left, cwUnits::DecimalFeet) << " ";
        stream << convertField(from, Right, cwUnits::DecimalFeet) << " ";
        stream << convertField(from, Up, cwUnits::DecimalFeet) << " ";
        stream << convertField(from, Down, cwUnits::DecimalFeet) << " ";
        stream << convertField(shot, BackCompass) << " ";
        stream << convertField(shot, Clino) << " ";
        stream << endl;
    }

    cwStationReference* lastStation = chunk->Station(chunk->StationCount() - 1);
    writeData(stream, "From", 12, lastStation->name());
    stream << " ";
    writeData(stream, "To", 12, lastStation->name() + "lrud");
    stream << " ";
    stream << 0.0 << " ";
    stream << 0.0 << " ";
    stream << 0.0 << " ";
    stream << convertField(lastStation, Left, cwUnits::DecimalFeet) << " ";
    stream << convertField(lastStation, Right, cwUnits::DecimalFeet) << " ";
    stream << convertField(lastStation, Up, cwUnits::DecimalFeet) << " ";
    stream << convertField(lastStation, Down, cwUnits::DecimalFeet) << " ";
    stream << 0.0 << " ";
    stream << 0.0 << " ";
    stream << endl;
}

/**
  Extracts the data from the station's LRUD field

  This will convert the value into decimal feet
  */
float cwCompassExportCaveTask::convertField(cwStationReference* station,
                                       StationLRUDField field,
                                       cwUnits::LengthUnit unit) {

    QString value;
    switch(field) {
    case Left:
        value = station->left();
        break;
    case Right:
        value = station->right();
        break;
    case Up:
        value = station->up();
        break;
    case Down:
        value = station->down();
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
float cwCompassExportCaveTask::convertField(cwShot* shot, ShotField field) {

    QString frontSite;
    QString backSite;

    switch(field) {
    case Compass:
    case BackCompass:
        frontSite = shot->GetCompass();
        backSite = shot->GetBackCompass();
        break;
    case Clino:
    case BackClino:
        frontSite = shot->GetClino();
        backSite = shot->GetBackClino();
    }

    float value;

    if(field == Clino || field == BackClino) {
        frontSite = convertFromDownUp(frontSite);
        backSite = convertFromDownUp(backSite);
    }

    switch(field) {
    case Compass:
        value = fixCompass(frontSite, backSite);
        break;
    case BackCompass:
        value = fixCompass(backSite, frontSite);
        break;
    case Clino:
        value = fixClino(frontSite, backSite);
        break;
    case BackClino:
        value = fixClino(backSite, frontSite);
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

/**
  Heleper to extract shot data
  */
float cwCompassExportCaveTask::fixCompass(QString compass1, QString compass2) {
    float value;
    if(compass1.isEmpty()) {
        if(!compass2.isEmpty()) {
            value = fmod(compass2.toDouble() + 180.0, 360.0);
        } else {
            return 0.0;
        }
    } else {
        value = compass1.toDouble();
    }
    return value;
}

/**
  Heleper to extract shot data
  */
float cwCompassExportCaveTask::fixClino(QString forward, QString backward) {
    float value;
    if(forward.isEmpty()) {
        if(!backward.isEmpty()) {
            value = backward.toDouble() * -1;
        } else {
            return 0.0;
        }
    } else {
        value = forward.toDouble();
    }
    return value;
}
