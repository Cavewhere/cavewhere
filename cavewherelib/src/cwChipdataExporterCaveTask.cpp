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

//Qt includes
#include <QRegularExpression>


cwChipdataExportCaveTask::cwChipdataExportCaveTask(QObject *parent) :
    cwCaveExporterTask(parent)
{
}

/**
  Writes all the trips to the data stream
  */
bool cwChipdataExportCaveTask::writeCave(QTextStream& stream, const cwCaveData &cave) {
    //Haven't done anything
    TotalProgress = 0;

    auto cavePtr = std::make_unique<cwCave>();
    cavePtr->setData(cave);

    //Go throug all the trips and save them
    for(int i = 0; i < cavePtr->tripCount(); i++) {
        cwTrip* trip = cavePtr->trip(i);
        writeTrip(stream, trip, i == 0 ? cavePtr->name() : QString());
        TotalProgress += trip->numberOfStations();
    }

    return true;
}

/**
  Writes a signle trip to the stream
  */
void cwChipdataExportCaveTask::writeTrip(QTextStream& stream, cwTrip* trip, QString caveName) {
    writeHeader(stream, trip, caveName);

    bool feetAndInches = isFeetAndInches(trip);

    //Write all chunks
    foreach(cwSurveyChunk* chunk, trip->chunks()) {
        //Trim the invalid stations off
        cwSurveyChunkTrimmer::trim(chunk);

        if (chunk->isValid()) {
            writeChunk(stream, chunk, feetAndInches);
        }
    }
}

/**
  Writes the compass file header to a file
  */
void cwChipdataExportCaveTask::writeHeader(QTextStream& stream, cwTrip* trip, QString caveName) {
    cwCave* cave = trip->parentCave();
    Q_ASSERT(cave != nullptr);

    if (!caveName.isNull()) {
        stream << caveName << chipdataNewLine();
    } else {
        stream << " *" << chipdataNewLine();
    }
    stream << trip->name() << chipdataNewLine();
    QList<cwTeamMember> teamMembers = trip->team()->teamMembers();
    for (int i = 0; i < teamMembers.size(); i++) {
        if (i > 0) {
            stream << ", ";
        }
        stream << teamMembers[i].name();
    }
    if (trip->date().isValid()) {
        stream << " - " << trip->date().toString("M/d/yy") << chipdataNewLine();
    }

    stream << " *" << chipdataNewLine();

    writeDataFormat(stream, trip);
}

void cwChipdataExportCaveTask::writeDataFormat(QTextStream &stream, cwTrip *trip) {
    cwTripCalibration* calibrations = trip->calibrations();

    if (isFeetAndInches(trip)) {
        stream << "FI ";
    } else {
        switch (calibrations->distanceUnit()) {
        case cwUnits::Feet: 	stream << "FT "; break;
        case cwUnits::Inches:	stream << "FI "; break;
        default:			stream << "M  "; break;
        }
    }

    stream << (calibrations->hasCorrectedCompassBacksight() ? "C" : "B");
    stream << (calibrations->hasCorrectedClinoBacksight() ? "C" : "B");
    stream << " DD" << chipdataNewLine();
}

/**
  Writes a chunk to the stream
  THe chunk has all the real survey data
  */
void cwChipdataExportCaveTask::writeChunk(QTextStream& stream, cwSurveyChunk* chunk, bool feetAndInches) {
    cwTrip* trip = chunk->parentTrip();

    //Trim the invalid stations off
    cwSurveyChunkTrimmer::trim(chunk);

    //Go through all the shots
    for(int i = 0; i < chunk->shotCount(); i++) {
        cwShot shot = chunk->shot(i);
        cwStation from = chunk->station(i);
        cwStation to = chunk->station(i + 1);

        writeShot(stream, trip->calibrations(), feetAndInches, from, to, shot);
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
                                        bool feetAndInches,
                                        const cwStation &fromStation,
                                        const cwStation &toStation,
                                        cwShot shot)
{
    stream << toStation.name().rightJustified(5, ' ', true);
    stream << fromStation.name().rightJustified(5, ' ', true);

    if (shot.distance().state() == cwDistanceReading::State::Valid) {
        double distance = shot.distance().toDouble();
        if (feetAndInches) {
            stream << formatNumber(floor(distance), 0, 4);
            stream << formatNumber(fmod(distance, 1.0) * 12.0, 0, 3);
        } else {
            switch (calibrations->distanceUnit()) {
            case cwUnits::Feet:
                stream << formatNumber(distance, 2, 6) << ' ';
                break;
            case cwUnits::Inches:
                stream << formatNumber(distance / 12.0, 0, 4);
                stream << formatNumber(fmod(distance, 12.0), 0, 3);
                break;
            default:
                stream << formatNumber(cwUnits::convert(distance, calibrations->distanceUnit(), cwUnits::Meters), 2, 6) << ' ';
                break;
            }
        }
    } else {
        stream << "       ";
    }

    if (!shot.isDistanceIncluded()) {
        stream << "*";
    } else {
        stream << " ";
    }

    if (shot.compass().state() == cwCompassReading::State::Valid) {
        stream << formatNumber(shot.compass().value(), 2, 6);
    } else {
        stream << "      ";
    }
    if (shot.backCompass().state() == cwCompassReading::State::Valid) {
        stream << formatNumber(shot.backCompass().value(), 2, 6);
    } else {
        stream << "      ";
    }
    if (shot.clino().state() == cwClinoReading::State::Valid) {
        stream << formatNumber(shot.clino().value(), 1, 5);
    } else {
        stream << "     ";
    }
    if (shot.backClino().state() == cwClinoReading::State::Valid) {
        stream << formatNumber(shot.backClino().value(), 1, 5);
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


    writeLrudMeasurement(stream, toStation.left(),  calibrations->distanceUnit(), lrudUnit);
    writeLrudMeasurement(stream, toStation.right(), calibrations->distanceUnit(), lrudUnit);
    writeLrudMeasurement(stream, toStation.up(),    calibrations->distanceUnit(), lrudUnit);
    writeLrudMeasurement(stream, toStation.down(),  calibrations->distanceUnit(), lrudUnit);

    stream << chipdataNewLine();
}

void cwChipdataExportCaveTask::writeLrudMeasurement(QTextStream &stream, const cwDistanceReading& measurement, cwUnits::LengthUnit fromUnit, cwUnits::LengthUnit toUnit)
{
    if (measurement.state() == cwDistanceReading::State::Valid) {
        stream << formatNumber(cwUnits::convert(measurement.toDouble(), fromUnit, toUnit), 1, 3);
    } else {
        stream << "   ";
    }
}

QString cwChipdataExportCaveTask::formatNumber(QString formatted, int maxPrecision, int columnWidth)
{
    int decimalIndex = formatted.indexOf('.');

    if (decimalIndex >= 0) {
        static const QRegularExpression re(R"(\.?0+$)");
        QRegularExpressionMatch match = re.match(formatted);

        if (match.hasMatch()) {
            int trimIndex = match.capturedStart(0);
            formatted = formatted.left(trimIndex);
        }
    }

    return formatted.rightJustified(columnWidth, ' ', true);
}

QString cwChipdataExportCaveTask::formatNumber(double number, int maxPrecision, int columnWidth)
{
    QString formatted = QString::number(number, 'f', maxPrecision);
    return formatNumber(formatted, maxPrecision, columnWidth);
}

bool cwChipdataExportCaveTask::isFeetAndInches(cwTrip *trip)
{
    return trip->calibrations()->distanceUnit() == cwUnits::Feet && !containsShot(trip, [&](cwShot shot) {
        if (shot.distance().state() == cwDistanceReading::State::Valid) {
            double f = fmod(shot.distance().toDouble() * 12.0, 1.0);
            return f > 1e-6 && f < (1 - 1e-6);
        }
        return false;
    });
}

template<typename P>
bool cwChipdataExportCaveTask::containsShot(cwTrip* trip, P predicate)
{
    foreach (cwSurveyChunk* chunk, trip->chunks()) {
        foreach (cwShot shot, chunk->shots()) {
            if (predicate(shot)) {
                return true;
            }
        }
    }
    return false;
}
