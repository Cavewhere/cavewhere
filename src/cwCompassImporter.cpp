/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCompassImporter.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLength.h"

//Qt includes
#include <QFileInfo>

cwCompassImporter::cwCompassImporter(QObject *parent) :
    cwTask(parent),
    SurveyNameRegExp("^SURVEY NAME:\\s*"),
    DateRegExp("SURVEY DATE:\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)"),
    CalibrationRegExp("DECLINATION:\\s*(\\S+)(?:\\s+FORMAT:\\s*)?(\\w+)(?:\\s+CORRECTIONS:\\s*(\\S+)\\s+(\\S+)\\s+(\\S+)\\s*)?(?:\\s+CORRECTIONS2:\\s*(\\S+)\\s+(\\S+))?.*")

{
//    CalibrationRegExp.setMinimal(true);
}

/**
 * @brief cwCompassImporter::runTask
 *
 * This begins the parser
 */
void cwCompassImporter::runTask()
{
    Caves.clear();

    for(int i = 0; i < CompassDataFiles.size() && isRunning(); i++) {
        CurrentFilename = CompassDataFiles.at(i);
        Caves.append(cwCave());
        CurrentCave = &Caves.last();
        CurrentFileGood = true;
        CurrentTrip = nullptr;
        LineCount = 0;
        StationRenamer.setCave(CurrentCave);

        //Make sure file is good
        verifyCompassDataFileExists();

        //Parse the current file
        parseFile();

        //Fix invalid station names
//        StationRenamer.renameInvalidStations();

        if(!CurrentFileGood) {
            //Parsing error in the last cave, remove it from list
            emit statusMessage(QString("Couldn't parse cave found in %1").
                               arg(CurrentFilename));
            Caves.removeLast();
        }
    }

    done();
}

/**
 * @brief cwCompassImporter::verifyCompassDataFileExists
 */
void cwCompassImporter::verifyCompassDataFileExists()
{
    if(!CurrentFileGood) { return; }

    QFileInfo fileInfo(CurrentFilename);
    if(!fileInfo.exists()) {
        //TODO: Fix error message
        emit statusMessage(QString("I can't parse %1 because it does not exist!").arg(CurrentFilename));
        CurrentFileGood = false;
    }

    if(!fileInfo.isReadable()) {
        //TODO: Fix error message
        emit statusMessage(QString("I can't parse %1 because it's not readable, change the permissions?").arg(CurrentFilename));
        stop();
    }

}

/**
 * @brief cwCompassImporter::parseFile
 *
 * Tries to parse the compass import file
 */
void cwCompassImporter::parseFile()
{
    if(!CurrentFileGood) { return; }

    //Open the file
    QFile file(CurrentFilename);
    bool okay = file.open(QFile::ReadOnly);

    if(!okay) {
        //TODO: Fix error message
        emit statusMessage(QString("I couldn't open %1").arg(CurrentFilename));
        stop();
    }

    while (!file.atEnd()) {
        parseSurvey(&file);
    }
}

/**
 * @brief cwCompassImporter::parseSurvey
 * @param file
 *
 * This tries to parse the survey out of the file
 */
void cwCompassImporter::parseSurvey(QFile *file)
{
    if(!CurrentFileGood) { return; }

    CurrentTrip = new cwTrip();
    CurrentCave->addTrip(CurrentTrip);

    parseCaveName(file);
    parseTripName(file);
    parseTripDate(file);
    parseSurveyTeam(file);
    parseSurveyFormatAndCalibration(file);
    parseSurveyData(file);

    qint64 position = file->pos();
    QByteArray lastLine = file->readLine();
    if(!lastLine.contains(0x1A)) {
        file->seek(position); //Go back
    }
}

/**
 * @brief cwCompassImporter::isFileGood
 * @param file
 * @param infoHelp
 * @return True if the file is still good, and false if it has errored
 */
bool cwCompassImporter::isFileGood(QFile *file, QString infoHelp)
{
    if(file->error() != QFile::NoError) {
        //TODO: Fix error message
        emit statusMessage(QString("Hmm, while trying to parse %1, the Compass data file (%1) has error: %2. In %3 on line %4")
                           .arg(file->errorString())
                           .arg(infoHelp)
                           .arg(CurrentFilename)
                           .arg(LineCount));
        CurrentFileGood = false;
        return false;
    }
    return true;
}

/**
 * @brief cwCompassImporter::parseCaveName
 * @param file
 */
void cwCompassImporter::parseCaveName(QFile *file)
{
    if(!CurrentFileGood) { return; }
    QString caveName = file->readLine();

    caveName = caveName.trimmed();

    LineCount++;

    if(caveName.size() > 80) {
        emit statusMessage(QString("I found the cave name to be longer than 80 characters. I'm trimming it to 80 characters, in %1 on line %2")
                           .arg(CurrentFilename)
                           .arg(LineCount));
        caveName.resize(80);
    }


    if(!isFileGood(file, "cave name")) { return; }
    CurrentCave->setName(caveName);
}

/**
 * @brief cwCompassImporter::parseTripName
 * @param file
 */
void cwCompassImporter::parseTripName(QFile *file)
{
    if(!CurrentFileGood) { return; }
    QString tripName = file->readLine();
    tripName.remove(SurveyNameRegExp);
    tripName = tripName.trimmed();

    LineCount++;

    if(!isFileGood(file, "survey name")) { return; }

    CurrentTrip->setName(tripName);
}

/**
 * @brief cwCompassImporter::parseTripDate
 * @param file
 *
 * This parses the trip's date from the input file
 */
void cwCompassImporter::parseTripDate(QFile *file)
{
    if(!CurrentFileGood) { return; }

    QString dateString = file->readLine();

    LineCount++;

    if(!isFileGood(file, "survey date")) { return; }

    if(DateRegExp.indexIn(dateString) == -1)  {
        //Couldn't parse the date
        //TODO: Add warning that we couldn't parse the date
        emit statusMessage(QString("I couldn't parse the date in %1 on line %2")
                           .arg(CurrentFilename)
                           .arg(LineCount));
    } else {
        QString monthString = DateRegExp.cap(1);
        QString dayString = DateRegExp.cap(2);
        QString yearString = DateRegExp.cap(3);

        bool okay;
        int month = monthString.toInt(&okay);
        if(!okay) {
            //TODO: Add warning that we couldn't parse the date
            emit statusMessage(QString("I couldn't understand the month.  I found \"%1\". It needs to be a number. Line %2")
                               .arg(monthString)
                               .arg(LineCount));
            return;
        }

        if(month < 1 || month > 12) {
            //Bad month
            emit statusMessage(QString("I found the month to be \"%1\" it needs to be between 1 and 12. Line %2")
                               .arg(month)
                               .arg(LineCount));

            return;
        }

        int day = dayString.toInt(&okay);
        if(!okay) {
            //TODO: Add warning that we couldn't parse the date
            emit statusMessage(QString("I couldn't understand the day.  I found \"%1\". It needs to be a number. Line %2")
                               .arg(dayString)
                               .arg(LineCount));
            return;
        }

        if(day < 1 || day > 31) {
            emit statusMessage(QString("I found an wrong day of the month, %1 on line %2. It should be between 1 and 31.")
                               .arg(day)
                               .arg(LineCount));
            return;
        }

        int year = yearString.toInt(&okay);
        if(!okay) {
            emit statusMessage(QString("I found the year isn't a number, on line %1").arg(LineCount));
            return;
        }

        if(year < 0) {
            emit statusMessage(QString("I found that the is negitive, on line %1").arg(LineCount));
            return;
        }

        if(year < 1900) {
            int newYear = 1900 + year;
            emit statusMessage(QString("I assuming year that %1 is really %2 on line %3")
                               .arg(year)
                               .arg(newYear)
                               .arg(LineCount));
            year = newYear;
        }

        QDate tripDate(year, month, day);
        CurrentTrip->setDate(tripDate);
    }
}

/**
 * @brief cwCompassImporter::parseSurveyTeam
 * @param file
 */
void cwCompassImporter::parseSurveyTeam(QFile *file)
{
    if(!CurrentFileGood) { return; }
    QString surveyTeamLabel = file->readLine();
    surveyTeamLabel = surveyTeamLabel.trimmed();

    LineCount++;
    if(!isFileGood(file, "survey team")) { return; }

    if(surveyTeamLabel.compare("SURVEY TEAM:") != 0) {
        emit statusMessage(QString("I was expecting to find \"SURVEY TEAM:\" but instead found \"%1\", in %2 on line %3")
                           .arg(surveyTeamLabel)
                           .arg(CurrentFilename)
                           .arg(LineCount));
    }

    QString surveyTeam = file->readLine();
    surveyTeam = surveyTeam.trimmed();

    LineCount++;
    if(!isFileGood(file, "survey team")) { return; }

    if(surveyTeam.size() > 100) {
        emit statusMessage(QString("I found the team to be longer than 100 characters. I'm trimming it to 100 characters, in %1 on line %2")
                           .arg(CurrentFilename)
                           .arg(LineCount));
        surveyTeam.resize(100);
    }

    QRegExp delimiter;
    if (surveyTeam.contains(';')) {
        delimiter = QRegExp("\\s*;\\s*");
    } else {
        delimiter = QRegExp("\\s\\s+|\\s*,\\s*");
    }

    QStringList teamList = surveyTeam.split(delimiter, QString::SkipEmptyParts);
    if(!teamList.isEmpty()) {
        cwTeam* team = new cwTeam();

        foreach(QString name, teamList) {
            cwTeamMember member(name, QStringList());
            team->addTeamMember(member);
        }

        CurrentTrip->setTeam(team);
    }
}

/**
 * @brief cwCompassImporter::parseSurveyFormatAndCalibration
 * @param file
 */
void cwCompassImporter::parseSurveyFormatAndCalibration(QFile *file)
{
    if(!CurrentFileGood) { return; }
    QString calibrationLine = file->readLine();
    calibrationLine = calibrationLine.trimmed();

    LineCount++;
    if(!isFileGood(file, "calibration")) { return; }

    if(CalibrationRegExp.exactMatch(calibrationLine)) {
        QString declinationString = CalibrationRegExp.cap(1);
        QString fileFormatString = CalibrationRegExp.cap(2);
        QString compassCorrectionString = CalibrationRegExp.cap(3);
        QString clinoCorrectionString = CalibrationRegExp.cap(4);
        QString lengthCorrectionString = CalibrationRegExp.cap(5);


        double declination;
        if(convertNumber(declinationString, "declination", &declination)) {
            CurrentTrip->calibrations()->setDeclination(declination);
        }

        if(!(compassCorrectionString.isEmpty() ||
             clinoCorrectionString.isEmpty() ||
             lengthCorrectionString.isEmpty()))
        {

            double compassCorrection;
            double clinoCorrection;
            double lengthCorrection;

            if(convertNumber(compassCorrectionString, "compass correction", &compassCorrection)) {
                CurrentTrip->calibrations()->setFrontCompassCalibration(compassCorrection);
                CurrentTrip->calibrations()->setBackCompassCalibration(compassCorrection);
            }

            if(convertNumber(clinoCorrectionString, "clino correction", &clinoCorrection)) {
                CurrentTrip->calibrations()->setFrontClinoCalibration(clinoCorrection);
                CurrentTrip->calibrations()->setBackClinoCalibration(clinoCorrection);
            }

            if(convertNumber(lengthCorrectionString, "length correcton", &lengthCorrection)) {
                CurrentTrip->calibrations()->setTapeCalibration(lengthCorrection);
            }
        }

        if(fileFormatString.size() == 11 || fileFormatString.size() == 12 || fileFormatString.size() == 13) {
            if(fileFormatString.at(0) != 'D') {
                emit statusMessage(QString("I can only understand Degrees for the Bearing Units. Converting all Bearing units to Degrees. In %1 on line %2")
                                   .arg(CurrentFilename)
                                   .arg(LineCount));
            }

            if(fileFormatString.at(1) == 'D') {
                CurrentTrip->calibrations()->setDistanceUnit(cwUnits::Feet);
            } else if(fileFormatString.at(1) == 'M') {
                CurrentTrip->calibrations()->setDistanceUnit(cwUnits::Meters);
            } else {
                CurrentTrip->calibrations()->setDistanceUnit(cwUnits::Feet);
                emit statusMessage(QString("I can't use Feet and Inches.  Converting all length measurements to decimal feet. In %1 on line %2")
                                   .arg(CurrentFilename)
                                   .arg(LineCount));
            }

            if(fileFormatString.at(3) != 'D') {
                emit statusMessage(QString("I can only understand Degrees for the Inclination Units. Converting all Inclination units to Degrees. In %1 on line %2")
                                   .arg(CurrentFilename)
                                   .arg(LineCount));
            }

            if(fileFormatString.size() >= 12) {
                if(fileFormatString.at(11) == 'B') {
                    CurrentTrip->calibrations()->setBackSights(true);
                } else {
                    CurrentTrip->calibrations()->setBackSights(false);
                }
            }

        } else if(fileFormatString.size() == 1) {
            //No format, so use some defaults
            CurrentTrip->calibrations()->setDistanceUnit(cwUnits::Feet);
            CurrentTrip->calibrations()->setBackSights(false);
        } else {
            emit statusMessage(QString("I found that the file format to be %1. It must be 11 or 12 characters long. In file %2, on line %3")
                               .arg(fileFormatString.size())
                               .arg(CurrentFilename)
                               .arg(LineCount)
                               );
        }

    } else {
        emit statusMessage(QString("I couldn't understand the calibration line found in %1 on line %2")
                           .arg(CurrentFilename)
                           .arg(LineCount));
        CurrentFileGood = false;
    }
}

/**
 * @brief cwCompassImporter::parseSurveyData
 * @param file
 */
void cwCompassImporter::parseSurveyData(QFile *file)
{
    //Skip 3 lines
    file->readLine();
    LineCount++;
    if(!isFileGood(file, "")) { return; }

    file->readLine();
    LineCount++;
    if(!isFileGood(file, "")) { return; }

    file->readLine();
    LineCount++;
    if(!isFileGood(file, "")) { return; }

    QString dataLine;
    while(!file->atEnd()) {
        dataLine = file->readLine();
        LineCount++;
        if(!isFileGood(file, "data line")) { return; }

        //Make sure not at the end of the survey section
        if(!dataLine.isEmpty() && dataLine.at(0) == 0x0C) { break; }

        QStringList dataStrings = dataLine.split(QRegExp("\\s+"), QString::SkipEmptyParts);

        if(dataStrings.size() >= 9) {
            QString fromStationName = dataStrings.at(0);
            QString toStationName = dataStrings.at(1);

            if(fromStationName == toStationName) {
                continue;
            }

            QString lengthString = dataStrings.at(2);
            QString bearingString = dataStrings.at(3);
            QString inclinationString = dataStrings.at(4);
            QString leftString = dataStrings.at(5);
            QString rightString = dataStrings.at(6);
            QString upString = dataStrings.at(7);
            QString downString = dataStrings.at(8);
            QString flags = dataStrings.size() >= 10 ? dataStrings.at(9) : QString();

            cwStation fromStation = StationRenamer.createStation(fromStationName);
            cwStation toStation = StationRenamer.createStation(toStationName);
            cwShot shot;

            double length;
            double bearing;
            double inclination;
            double left;
            double right;
            double up;
            double down;

            if(!convertNumber(lengthString, "length", &length)) { CurrentFileGood = false; return; }
            if(!convertNumber(bearingString, "bearing", &bearing)) { CurrentFileGood = false; return; }
            if(!convertNumber(inclinationString, "inclination", &inclination)) { CurrentFileGood = false; return; }
            if(!convertNumber(leftString, "left", &left)) { CurrentFileGood = false; return; }
            if(!convertNumber(rightString, "right", &right)) { CurrentFileGood = false; return; }
            if(!convertNumber(upString, "up", &up)) { CurrentFileGood = false; return; }
            if(!convertNumber(downString, "down", &down)) { CurrentFileGood = false; return; }

            cwUnits::LengthUnit distanceUnits = CurrentTrip->calibrations()->distanceUnit();

            shot.setDistance(cwUnits::convert(length, cwUnits::Feet, distanceUnits));
            shot.setCompass(bearing);
            shot.setClino(inclination);

            fromStation.setLeft(cwUnits::convert(left, cwUnits::Feet, distanceUnits));
            fromStation.setRight(cwUnits::convert(right, cwUnits::Feet, distanceUnits));
            fromStation.setUp(cwUnits::convert(up, cwUnits::Feet, distanceUnits));
            fromStation.setDown(cwUnits::convert(down, cwUnits::Feet, distanceUnits));

            if(CurrentTrip->calibrations()->hasBackSights() && dataStrings.size() >= 11) {
                QString backCompassString = dataStrings.at(9);
                QString backClinoString = dataStrings.at(10);

                double backCompass;
                double backClino;

                if(!convertNumber(backCompassString, "back compass", &backCompass)) { CurrentFileGood = false; return; }
                if(!convertNumber(backClinoString, "back clino", &backClino)) { CurrentFileGood = false; return; }

                shot.setBackCompass(backCompass);
                shot.setBackClino(backClino);
            }

            //Exclude length from calculation
            if(flags.contains("L")) {
                shot.setDistanceIncluded(false);
            }

            CurrentTrip->addShotToLastChunk(fromStation, toStation, shot);
            if(!CurrentTrip->chunks().isEmpty()) {
                cwSurveyChunk* lastChunk = CurrentTrip->chunks().last();
                int secondToLastStation = lastChunk->stationCount() - 2;
                lastChunk->setStation(fromStation, secondToLastStation);
            }
        } else {
            emit statusMessage(QString("Data string doesn't have enough fields. I need at least 9 but found only %1 in %2 on line %3")
                               .arg(dataStrings.size())
                               .arg(CurrentFilename)
                               .arg(LineCount));
        }
    }
}

/**
 * @brief cwCompassImporter::covertNumber
 * @param name
 * @param field
 * @param value
 * @return
 */
bool cwCompassImporter::convertNumber(QString numberString, QString field, double *value)
{
    bool okay;
    *value = numberString.toDouble(&okay);
    if(!okay) {
        emit statusMessage(QString("I couldn't read %1 because it's not a number (I found \"%4\" instead) in %2 on line %3")
                           .arg(field)
                           .arg(CurrentFilename)
                           .arg(LineCount)
                           .arg(numberString));
        return false;
    }
    return true;
}
