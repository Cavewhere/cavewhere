//Our includes
#include "cwCompassExporterCaveTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwStationReference.h"

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

        writeData(stream, "From", 12, from->name());
        stream << " ";
        writeData(stream, "To", 12, to->name());
        stream << " ";
        stream << shot->GetDistance() << " ";
        stream << shot->GetCompass() << " ";
        stream << shot->GetClino() << " ";
        if(!from->left().isEmpty()) { stream << from->left() << " "; } else { stream << 0.0 << " "; }
        if(!from->up().isEmpty()) { stream << from->right() << " ";
        stream << from->up() << " ";
        stream << from->down() << " ";

        bool okay;
        shot->GetBackCompass().toDouble(&okay);
        if(okay) {
            stream << shot->GetBackCompass() << " ";
        } else {
            stream << shot->GetCompass() << " ";
        }

        if(okay) {
            stream << shot->GetBackClino() << " ";
        } else {
            stream << shot->GetCompass() << " ";
        }

        stream << shot->GetBackClino() << " ";
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
    stream << lastStation->left() << " ";
    stream << lastStation->right() << " ";
    stream << lastStation->up() << " ";
    stream << lastStation->down() << " ";
    stream << 0.0 << " ";
    stream << 0.0 << " ";
    stream << endl;
}
