//Our includes
#include "cwCompassImporter.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QFileInfo>

cwCompassImporter::cwCompassImporter(QObject *parent) :
    cwTask(parent),
    SurveyNameRegExp("^SURVEY NAME:\\s*"),
    DateRegExp("SURVEY DATE:\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)"),
    CalibrationRegExp("DECLINATION:\\s*(\\S+)\\s+FORMAT:\\s*(\\w+)(?:\\s+CORRECTIONS:\\s*(\\S+)\\s+(\\S+)\\s+(\\S+)\\s*)?")
{

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
        CurrentTrip = NULL;
        LineCount = 0;

        //Make sure file is good
        verifyCompassDataFileExists();

        //Parse the current file
        parseFile();

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

    parseCaveName(file);
    parseTripName(file);
    parseTripDate(file);
    parseSurveyTeam(file);
    parseSurveyFormatAndCalibration(file);
    parseSurveyData(file);

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

    if(tripName.size() > 12) {
        emit statusMessage(QString("I found the trip name to be longer than 12 characters. I'm trimming it to 12 characters, in %1 on line %2")
                      .arg(CurrentFilename)
                      .arg(LineCount));
        tripName.resize(12);
    }

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
    surveyTeam = surveyTeamLabel.trimmed();

    LineCount++;
    if(!isFileGood(file, "survey team")) { return; }

    if(surveyTeam.size() > 100) {
        emit statusMessage(QString("I found the team to be longer than 100 characters. I'm trimming it to 100 characters, in %1 on line %2")
                      .arg(CurrentFilename)
                      .arg(LineCount));
        surveyTeam.resize(100);
    }

    QStringList teamList = surveyTeam.split(" ", QString::SkipEmptyParts);
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


        bool okay;
        double declination = declinationString.toDouble(&okay);
        if(okay) {
            CurrentTrip->calibrations()->setDeclination(declination);
        } else {
            emit statusMessage(QString("I couldn't read the declination because it's not a number in %1 on line %2")
                               .arg(CurrentFilename)
                               .arg(LineCount));
        }

        if(!(compassCorrectionString.isEmpty() ||
             clinoCorrectionString.isEmpty() ||
             lengthCorrectionString.isEmpty()))
        {

            double compassCorrection = compassCorrectionString.toDouble(&okay);
            if(okay) {
                CurrentTrip->calibrations()->setFrontCompassCalibration(compassCorrection);
                CurrentTrip->calibrations()->setBackCompassCalibration(compassCorrection);
            } else {
                emit statusMessage(QString("I couldn't read compass correction because it's not a number in %1 on line %2")
                                   .arg(CurrentFilename)
                                   .arg(LineCount));
            }

            double clinoCorrection = clinoCorrectionString.toDouble(&okay);
            if(okay) {
                CurrentTrip->calibrations()->setFrontClinoCalibration(clinoCorrection);
                CurrentTrip->calibrations()->setBackClinoCalibration(clinoCorrection);
            } else {
                emit statusMessage(QString("I couldn't read clino correction because it's not a number in %1 on line %2")
                                   .arg(CurrentFilename)
                                   .arg(LineCount));
            }

            double lengthCorrection = lengthCorrectionString.toDouble(&okay);
            if(okay) {
                CurrentTrip->calibrations()->setTapeCalibration(lengthCorrection);

            } else {
                emit statusMessage(QString("I couldn't read length correction because it's not a number in %1 on line %2")
                                                               .arg(CurrentFilename)
                                                               .arg(LineCount));
            }
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

}
