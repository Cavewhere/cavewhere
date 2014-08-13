/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexImporter.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwSurvexBlockData.h"
#include "cwSurvexGlobalData.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurvexLRUDChunk.h"

//Qt includes
#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QLinkedList>
#include <QFileInfo>
#include <QDir>

//Std include
#include "math.h"

cwSurvexImporter::cwSurvexImporter(QObject* parent) :
    cwTask(parent),
    RootBlock(new cwSurvexBlockData(this)),
    CurrentBlock(nullptr),
    GlobalData(new cwSurvexGlobalData(this)),
    CurrentState(FirstBegin)
{
}

void cwSurvexImporter::runTask() {
    importSurvex(RootFilename);
    done();
}

void cwSurvexImporter::importSurvex(QString filename) {
    clear();

    //Initilizing the importer
    runStats(filename);
    setNumberOfSteps(TotalNumberOfLines);

    //Clear again because RunStats messes things up
    clear();

    //Setup inital state data
    BeginEndState rootBlock; //Should never be poped off
    BeginEndStateStack.append(rootBlock);

    //The block state
    CurrentState = FirstBegin;

    //Sets the current block to the root block
    CurrentBlock = RootBlock;

    loadFile(filename);

    //Add the rootBlocks to GlobalData
    GlobalData->setBlocks(RootBlock->childBlocks());

    saveLastImport(filename);

}

/**
  Constructor for teh begin and end state object

  This creates a default dataformat and datatype for the begin and end state
  */
cwSurvexImporter::BeginEndState::BeginEndState() {
    DataFormat = defaultDataFormat();
    DataType = Normal;
}

/**
  \brief Creates the default map for the data format
  */
QMap<cwSurvexImporter::DataFormatType, int> cwSurvexImporter::BeginEndState::defaultDataFormat() {
    QMap<cwSurvexImporter::DataFormatType, int> dataFormat;
    dataFormat[From] = 0;
    dataFormat[To] = 1;
    dataFormat[Distance] = 2;
    dataFormat[Compass] = 3;
    dataFormat[Clino] = 4;
    return dataFormat;
}

/**
  \brief Returns true if errors have accured.
  \returns The list of errors
  */
bool cwSurvexImporter::hasErrors() {
    return !Errors.isEmpty();
}

/**
  \brief Gets the errors of the importer
  \return Returns the errors if any.  Will be empty if HasErrors() returns false
  */
QStringList cwSurvexImporter::errors() {
    return Errors;
}


/**
  \brief Clears all the current data in the object
  */
void cwSurvexImporter::clear() {
    RootBlock->clear(); //Chunks.clear();
    Errors.clear();
    IncludeStack.clear();
    IncludeFiles.clear();
    BeginEndStateStack.clear();
    TotalNumberOfLines = 0;
    CurrentTotalNumberOfLines = 0;
}

/**
  \brief Loads the file
  */
void cwSurvexImporter::loadFile(QString filename) {
    //Open the survex file
    QFile file;
    bool fileIsOpen = openFile(file, filename);
    if(!fileIsOpen) {
        return;
    }

    //Add the file to the include stack
    IncludeStack.append(Include(file.fileName()));

    //Update the status
    emit statusMessage("Importing " + file.fileName() );

    while(!file.atEnd() && isRunning()) {
        QString line = file.readLine();

        //The last includ file
        increamentLineNumber();

        //Get the line's data
        parseLine(line);


    }
    //Emit the current line number
    emit progressed(CurrentTotalNumberOfLines);

    IncludeStack.removeLast();
}

/**
  \brief This does the checking to open a survex file

  It will also fix up the filename if need to try to load it

  Adds an error to the error stack if it can open a file
  */
bool cwSurvexImporter::openFile(QFile& file, QString filename) {
    QFileInfo fileInfo(filename);
    if(!fileInfo.exists() && !IncludeStack.isEmpty()) {
        //This maybe a relative path to the rootFile
        QFileInfo rootFileInfo(currentFile());
        QDir rootFileDir = rootFileInfo.absoluteDir().path();

        rootFileDir.setNameFilters(QStringList("*.svx"));
        QStringList entries = rootFileDir.entryList();

        //Find the filename, this is needed because, filename case insensitive
        foreach(QString entry, entries) {
            QRegExp regExp(".+/" + entry + "(\\.svx)?$");
            regExp.setCaseSensitivity(Qt::CaseInsensitive);
            if(regExp.exactMatch(filename)) {
                filename = entry;
                break;
            }
        }

        QString fixedUpFile = rootFileDir.path() + "/" + filename;
        if(!QFileInfo(fixedUpFile).exists()) {
            //Try to add the .svx extension
            fixedUpFile += ".svx";
        }
        filename = fixedUpFile;
    }

    file.setFileName(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Errors.append(QString("Error: Couldn't open ") + filename);
        return false;
    }

    //Make sure we don't reopen the same file twice
    if(qFind(IncludeFiles, filename) != IncludeFiles.end() && !IncludeFiles.empty()) {
        //File has already been included... Do nothing
        return false;
    }

    QStringList::iterator iter = qLowerBound(IncludeFiles.begin(), IncludeFiles.end(), filename);
    IncludeFiles.insert(iter, filename);
    return true;
}

/**
  \brief Parses a survex line
  */
void cwSurvexImporter::parseLine(QString line) {
    //Remove comments
    line = removeComment(line);

    //Skip empty lines
    if(line.isEmpty()) { return; }

    QRegExp exp("^\\s*\\*begin\\s*((?:\\w|-|_)*)");
    exp.setCaseSensitivity(Qt::CaseInsensitive);
    if(line.contains(exp)) {
       // qDebug() << "Has begin" << line;
        CurrentState = InsideBegin;
        //BeginNames.append(exp.cap(1));

        //Create a new block
        cwSurvexBlockData* newBlock = new cwSurvexBlockData();
        QString blockName = exp.cap(1).trimmed();
        newBlock->setName(blockName);

        //Add the block to the structure
        CurrentBlock->addChildBlock(newBlock);

        //Copy the calibrations
        *(newBlock->calibration()) = *(CurrentBlock->calibration());

        //Make the newBlock the current block
        CurrentBlock = newBlock;

        //Copy the last state variables
        BeginEndState lastState;
        if(!BeginEndStateStack.isEmpty()) {
            lastState = BeginEndStateStack.last();
        }

        BeginEndState currentState;
        currentState.Filename = currentFile();
        if(lastState.Filename == currentFile()) {
            currentState = lastState;
        }
        BeginEndStateStack.append(currentState);

        return;
    }

    if(InsideBegin) {

        //Parse command
        exp = QRegExp("^\\s*\\*(\\w+)\\s*(.*)");
        if(line.contains(exp)) {
            QString command = exp.cap(1);
            QString arg = exp.cap(2).trimmed();

            if(compare(command, "end")) {

                //Update the LRUD before getting out of this block
                updateLRUDForCurrentBlock();

                cwSurvexBlockData* parentBlock = CurrentBlock->parentBlock();
                if(parentBlock != nullptr) {
                    CurrentBlock = parentBlock;
                }

                //Remove the current state valiable
                if(BeginEndStateStack.size() > 1) {
                    BeginEndStateStack.removeLast();
                } else {
                    addError("Too many *end");
                }

            } else if(compare(command, "data")) {
                parseDataFormat(line);
            } else if(compare(command, "include")) {
                loadFile(arg);
            } else if(compare(command, "date")) {
                parseDate(arg);
            } else if(compare(command, "team")) {
                parseTeamMember(arg);
            } else if(compare(command, "calibrate")) {
                parseCalibrate(arg);
            } else if(compare(command, "units")) {
                parseUnits(arg);
            } else if(compare(command, "export")) {
                parseExport(arg);
            } else if(compare(command, "equate")) {
                parseEquate(arg);
            } else if(compare(command, "flags")) {
                parseFlags(arg);
            } else {
                addWarning(QString("Unknown survex keyword:") + command);
            }

            if(CurrentBlock == RootBlock) {
                CurrentState = FirstBegin;
            }

            return;
        }

        //Parse normal survey data
        switch(currentDataEntryType()) {
        case Normal:
            parseNormalData(line);
            break;
        case Passage:
            parsePassageData(line);
            break;
        case NoSurvey:
            //Just ignore!
            break;
        }
    }


   // qDebug() << "Line: " << CurrentLine << " :" << line;
}

/**
  \brief Saves the last import to the qsettings
  */
void cwSurvexImporter::saveLastImport(QString filename) {
    QSettings settings;
    settings.setValue("surveyImporter", filename);
    settings.sync();
}

/**
  \brief Get's the last survex import
  */
QString cwSurvexImporter::lastImport() {
    QSettings settings;
    QVariant filename;
    settings.value("surveyImporter", filename);
    return filename.toString();
}

/**
  \brief Remove comments from the line
  */
QString cwSurvexImporter::removeComment(QString& line) {
    QString removeCommentLine;
    removeCommentLine = line.trimmed();

    int index = removeCommentLine.indexOf(';');
    if(index >= 0) {
        removeCommentLine = removeCommentLine.left(index);
    }
    return removeCommentLine;
}

/**
  \brief Tries to load the data formate
  */
void cwSurvexImporter::parseDataFormat(QString line) {
    //Remove *data from the line
    QRegExp regExp("^\\s*\\*data\\s*(.*)");
    regExp.setCaseSensitivity(Qt::CaseInsensitive);
    if(line.contains(regExp)) {
        line = regExp.cap(1);
    }

    QStringList dataFormatList = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);

    //Remove all empty data
    dataFormatList.removeAll("");

    if(dataFormatList.empty()) {
        addWarning("Data format is empty, using default format");
    }

    QString dataFormatType = dataFormatList.front();
    if(compare(dataFormatType, "normal")) {
        setCurrentDataEntryType(Normal);
    } else if(compare(dataFormatType, "passage")) {
        setCurrentDataEntryType(Passage);
        CurrentBlock->addLRUDChunk();
    } else if(compare(dataFormatType, "nosurvey")) {
        setCurrentDataEntryType(NoSurvey);
    } else {
        addError("Normal, passage data, nosurvey are supported, using default format");
        setCurrentDataEntryType(Normal);
        setCurrentDataFormat(BeginEndState::defaultDataFormat());
    }

    QMap<DataFormatType, int> dataFormat;
    dataFormat.clear();

    for(int i = 1; i < dataFormatList.size(); i++) {
        QString format = dataFormatList.at(i);

        int index = i - 1;
        //qDebug() << format << index;

        if(compare(format, "to")) {
            dataFormat[To] = index;
        } else if(compare(format, "from")) {
            dataFormat[From] = index;
        } else if(compare(format, "tape") || compare(format, "length")) {
            dataFormat[Distance] = index;
        } else if(compare(format, "compass") || compare(format, "bearing")) {
            dataFormat[Compass] = index;
        } else if(compare(format, "backcompass")) {
            dataFormat[BackCompass] = index;
        } else if(compare(format, "clino") || compare(format, "gradient")) {
            dataFormat[Clino] = index;
        } else if(compare(format, "backclino")) {
            dataFormat[BackClino] = index;
        } else if(compare(format, "ignore")) {
            dataFormat[Ignore] = index;
        } else if(compare(format, "ignoreall")) {
            dataFormat[IgnoreAll] = index;
        } else if(compare(format, "station")) {
            dataFormat[Station] = index;
        } else if(compare(format, "left")) {
            dataFormat[Left] = index;
        } else if(compare(format, "right")) {
            dataFormat[Right] = index;
        } else if(compare(format, "up")) {
            dataFormat[Up] = index;
        } else if(compare(format, "down")) {
            dataFormat[Down] = index;
        } else {
            addError(QString("Unknown *data keyword: ") + format + " Using default format");
            dataFormat = BeginEndState::defaultDataFormat();
            break;
        }
    }

    setCurrentDataFormat(dataFormat);
}

void cwSurvexImporter::addError(QString error) {
    addToErrors("Error", error);
}

void cwSurvexImporter::addWarning(QString error) {
    addToErrors("Warning", error);
}

void cwSurvexImporter::addToErrors(QString prefix, QString errorMessage) {
    QString currentFilename;
    if(!IncludeStack.isEmpty()) {
        currentFilename = currentFile();
    }
    Errors.append(QString("%1: %2::Line %3::%4").arg(prefix).arg(currentFilename).arg(currentLineNumber()).arg(errorMessage));
}

/**
Helper to parseNormalData and parsePassageData

This makes the line has enough elements for the current data format

It will return all the data in the line.  If there's an error, the error is added to the error
list and this returns empty data

  */
QStringList cwSurvexImporter::parseData(QString line) {
    QStringList data = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
    data.removeAll(""); //Remove all empty ones

    QMap<DataFormatType, int> dataFormat = currentDataFormat();

    //Make sure the there's the same number of columns as needed
    if(dataFormat.size() != data.size() && !dataFormat.contains(IgnoreAll)) {
        addError("Can't extract data. To many or not enough data columns, skipping data");
        return QStringList();
    }

    //Make sure there's enough columns
    if(dataFormat.contains(IgnoreAll) && dataFormat[IgnoreAll] > data.size()) {
        addError("Can't extract data. Not enough data columns, skipping data");
        return QStringList();
    }

    return data;
}

/**
  \brief Imports a line of survey data
  */
void cwSurvexImporter::parseNormalData(QString line) {
    QStringList data = parseData(line);
    if(data.isEmpty()) { return; } //Error, check the error messages

    QString fromStationName = extractData(data, From);
    QString toStationName= extractData(data, To);

    //Make sure the to and from stations exist
    if(fromStationName.isEmpty() || toStationName.isEmpty()) {
        addError("Can't extract shot data. No toStation or from station");
        return;
    }

    //Create the from and to stations
    cwStation fromStation(fromStationName);
    cwStation toStation(toStationName);

    cwShot shot;
    shot.setDistance(extractData(data, Distance));
    shot.setCompass(extractData(data, Compass));
    shot.setBackCompass(extractData(data, BackCompass));
    shot.setClino(extractData(data, Clino));
    shot.setBackClino(extractData(data, BackClino));
    shot.setDistanceIncluded(CurrentBlock->isDistanceInclude());

    addShotToCurrentChunk(fromStation, toStation, shot);
}

/**
  \brief Extracts the data from data with type
  \param data - The line data
  \param type - The which piece of the line data that needs to be extracted
  */
QString cwSurvexImporter::extractData(const QStringList data, DataFormatType type) {
    QMap<DataFormatType, int> dataFormat = currentDataFormat();
    if(dataFormat.contains(type)) {
        int index = dataFormat[type];
        if(index >= 0 && index < data.size()) {
            return data[index];
        }
    }
    return QString();
}

/**
  \brief Will add the shot to a survey chunk

  This will either add the shot to the current survey chunk, if the fromStation
  is the last station in the chunk or will create a new survey chunk.  If a
  new survey chunk is created, it'll be added to the end of the list of chunks.
  */
void cwSurvexImporter::addShotToCurrentChunk(cwStation fromStation,
                                             cwStation toStation,
                                             cwShot shot) {
    cwSurveyChunk* chunk;
    if(CurrentBlock->chunkCount() == 0) {
        //Create a new chunk
        chunk = new cwSurveyChunk();
        CurrentBlock->addChunk(chunk);
    } else {
        chunk = CurrentBlock->chunks().last(); //Chunks.last();
    }

    if(chunk->canAddShot(fromStation, toStation)) {
        chunk->appendShot(fromStation, toStation, shot);
    } else {
        if(!chunk->isValid()) {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation.name() << toStation.name();
            return;
        }

        chunk = new cwSurveyChunk();
        CurrentBlock->addChunk(chunk);
        //Chunks.append(chunk);
        if(chunk->canAddShot(fromStation, toStation)) {
            chunk->appendShot(fromStation, toStation, shot);
        } else {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation.name() << toStation.name();
            return;
        }
    }
}

/**
  \brief Parses the passage data
  Something like this
  *data passage station left right up down
  a1 2.0 .3 2.1 4
  */
void cwSurvexImporter::parsePassageData(QString line) {
    QStringList data = parseData(line);
    if(data.isEmpty()) { return; } //Error, check the error messages

    QString stationName = extractData(data, Station);

    //Make sure the station exists
    if(stationName.isEmpty()) {
        addError("Can't extract passage data. No station");
        return;
    }

    //Create or find a station from the name
    cwStation station(stationName);
    station.setLeft(extractData(data, Left));
    station.setRight(extractData(data, Right));
    station.setUp(extractData(data, Up));
    station.setDown(extractData(data, Down));

    //Add the station to the current LRUD chunk
    CurrentBlock->LRUDChunks.last().Stations.append(station);
}

/**
  \brief Get's the full station name

  \param The short name of the station

  This will return all the survex block name prefixes to create a full name
  */
QString cwSurvexImporter::fullStationName(QString name) {
    if(name.isEmpty()) { return QString(); }

    cwSurvexBlockData* current = CurrentBlock;
    QLinkedList<QString> fullNameList;

    //While not the root element of the importer
    while(current->parentBlock() != nullptr) {
        QString blockName = current->name();
        if(!blockName.isEmpty()) {
            fullNameList.prepend(blockName);
        }
        current = current->parentBlock();
    }

    QString fullName;
    foreach(QString blockName, fullNameList) {
        fullName.append(blockName).append(".");
    }
    fullName.append(name);

    return fullName;
}

/**
  \brief Get's the current data format for the importer

  This uses the current state in the BeginEndStateStack
  */
QMap<cwSurvexImporter::DataFormatType, int> cwSurvexImporter::currentDataFormat() const {
    if(BeginEndStateStack.isEmpty()) { return QMap<DataFormatType, int>(); }
    return BeginEndStateStack.last().DataFormat;
}

/**
  \brief Gets the current data entry type

  *data normal <-- This is the normal entry type
  *data passage <-- This is the passage entry type
  *data somethingElse

  */
cwSurvexImporter::DataEntryType cwSurvexImporter::currentDataEntryType() const {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    return BeginEndStateStack.last().DataType;
}


void cwSurvexImporter::setCurrentDataFormat(QMap<DataFormatType, int> format) {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    BeginEndStateStack.last().DataFormat = format;
}

void cwSurvexImporter::setCurrentDataEntryType(DataEntryType type) {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    BeginEndStateStack.last().DataType = type;
}


QString cwSurvexImporter::currentFile() const {
    if(IncludeStack.isEmpty()) { return QString(); }
    return IncludeStack.last().File;
}

int cwSurvexImporter::currentLineNumber() const {
    if(IncludeStack.isEmpty()) { return -1; }
    return IncludeStack.last().CurrentLine;
}
void cwSurvexImporter::increamentLineNumber() {
    Q_ASSERT(!IncludeStack.isEmpty());
    IncludeStack.last().CurrentLine++;
    CurrentTotalNumberOfLines++; //All the lines combine
}

/**
  \brief Tries to extract the date from the date line

  If it can't this function return a date of 2000 01 01
  */
void cwSurvexImporter::parseDate(QString dateString) {
    QDate date = QDate::fromString(dateString, "yyyy.MM.dd");

    if(!date.isValid()) {
        date = QDate(2000, 01, 01);
    }

    CurrentBlock->setDate(date);
}

/**
  \brief Extracts the team member from the survey line
  */
void cwSurvexImporter::parseTeamMember(QString line) {

    QStringList jobAndNameList;

    QRegExp splitReg("\"(\\w+\\s*)+\"|\\w+");
    int pos = 0;
    while ((pos = splitReg.indexIn(line, pos)) != -1) {
        QString nameOrJob = line.mid(pos, splitReg.matchedLength());
        nameOrJob = nameOrJob.remove('"');
        nameOrJob = nameOrJob.trimmed();
        jobAndNameList.append(nameOrJob);
        pos += splitReg.matchedLength();
    }

    if(!jobAndNameList.isEmpty()) {
        cwTeam* currentTeam = CurrentBlock->team();
        QString name = jobAndNameList.first();

        jobAndNameList.removeFirst();
        QStringList jobs = jobAndNameList;

        currentTeam->addTeamMember(cwTeamMember(name, jobs));
    } else {
       addError("couldn't read team member");
    }
}

/**
  \brief This extracts the calibration data from the survex file

  This will set the TripCalibration object's data
  */
void cwSurvexImporter::parseCalibrate(QString line) {

    QRegExp reg("(TAPE|COMPASS|BACKCOMPASS|BACKCLINO|CLINO|COUNTER|DEPTH|DECLINATION|X|Y|Z)\\s+(\\S+)(?:\\s*(\\S+)\\s*)?");
    reg.setCaseSensitivity(Qt::CaseInsensitive);

    if(reg.exactMatch(line)) {
        QString type = reg.cap(1).toLower();
        QString calibrationString = reg.cap(2);
        QString scaling = reg.cap(3);

        //Parse the calibration value
        bool okay;
        double calibrationValue = calibrationString.toDouble(&okay);
        if(!okay) {
            addError("Calibration value isn't a number");
            return;
        }

        float scaleValue = -1;
        if(!scaling.isEmpty()) {
            scaleValue = scaling.toFloat(&okay);
            if(!okay) {
                addError("Scaling value isn't a number");
                return;
            }
        }

        //Flip the calibration value because survex is written by strange british people
        calibrationValue = -calibrationValue;

        //Get the current calibration
        cwTripCalibration* calibration = CurrentBlock->calibration();

        if(type == "tape") {
            calibration->setTapeCalibration(calibrationValue);

        } else if (type == "compass") {
            calibration->setFrontCompassCalibration(calibrationValue);

        } else if (type == "backcompass") {
            //Check to see if this is a correct compasss calibration
            const float correctCalibrationThreshold = 45.0;
            if(fmod(calibrationValue + 180.0, 360.0) < correctCalibrationThreshold) {
                //This is probably a correct back compass
                calibrationValue = fmod(calibrationValue + 180.0, 360.0);
                calibration->setCorrectedCompassBacksight(true);
            }
            calibration->setBackCompassCalibration(calibrationValue);
        } else if (type == "clino") {
            calibration->setFrontClinoCalibration(calibrationValue);

        } else if (type == "backclino") {
            if(scaleValue == -1.0f) {
                calibration->setCorrectedClinoBacksight(true);
            }
            calibration->setBackClinoCalibration(calibrationValue);

        } else if (type == "declination") {
            calibration->setDeclination(calibrationValue);

        } else if (type == "counter") {
            addWarning("cavewhere cannot handle 'COUNTER' calibration");

        } else if (type == "depth") {
            addWarning("cavewhere cannot handle 'DEPTH' calibration");

        } else if (type == "x") {
            addWarning("cavewhere cannot handle 'X' calibration");

        } else if (type == "y") {
            addWarning("cavewhere cannot handle 'Y' calibration");

        } else if (type == "z") {
            addWarning("cavewhere cannot handle 'Z' calibration");
        }

    } else {
        addError("Couldn't read calibration");
    }
}

/**
  This parses the units out of the survex importer
  */
void cwSurvexImporter::parseUnits(QString line) {
    QRegExp reg("(TAPE|LENGTH|COMPASS|BEARING|CLINO|GRADIENT|COUNTER|DEPTH|DECLINATION|X|Y|Z)\\s+(\\S+)\\s*");
    reg.setCaseSensitivity(Qt::CaseInsensitive);

    if(reg.exactMatch(line)) {
        QString type = reg.cap(1).toLower();
        QString unitString = reg.cap(2);

        //Get the current calibration
        cwTripCalibration* calibration = CurrentBlock->calibration();

        if(type == "tape" || type == "length") {
            cwUnits::LengthUnit unit = cwUnits::toLengthUnit(unitString);

            //Make sure the units are good
            if(unit == cwUnits::LengthUnitless) {
                addError(QString("Bad unit %1, good units are YARDS, FEET, METRIC, METRES, or METERS. Using meters instead.").arg(unitString));
                calibration->setDistanceUnit(cwUnits::Meters);
                return;
            }

            calibration->setDistanceUnit(unit);
        } else if(type == "compass") {
           addWarning("cavewhere cannot handle 'compass' units");
        } else if(type == "bearing") {
           addWarning("cavewhere cannot handle 'bearing' units");
        } else if(type == "clino") {
           addWarning("cavewhere cannot handle 'clino' units");
        } else if(type == "gradient") {
            addWarning("cavewhere cannot handle 'gradient' units");
        } else if(type == "counter") {
            addWarning("cavewhere cannot handle 'counter' units");
        } else if(type == "depth") {
            addWarning("cavewhere cannot handle 'depth' units");
        } else if(type == "declination") {
            addWarning("cavewhere cannot handle 'declination' units");
        } else if(type == "x") {
            addWarning("cavewhere cannot handle 'x' units");
        } else if(type == "y") {
            addWarning("cavewhere cannot handle 'y' units");
        } else if(type == "z") {
            addWarning("cavewhere cannot handle 'z' units");
        }


    } else {
        addError("Couldn't read units");
    }

}

/**
 * @brief cwSurvexImporter::parseEquate
 * @param line - The line of all the station's that are equal
 */
void cwSurvexImporter::parseEquate(QString line)
{
    QRegExp splitRegex("\\s+");
    QStringList equalStations = line.split(splitRegex);

    if(equalStations.size() <= 1) {
        Errors.append(QString("Error: *equate on %1 has only one station").arg(currentLineNumber()));
        return;
    }

    CurrentBlock->addToEquated(equalStations);
}

/**
 * @brief cwSurvexImporter::parseExport
 * @param line
 *
 * This parses the export stations
 */
void cwSurvexImporter::parseExport(QString line)
{
    QStringList stations = line.split(QRegExp("\\s+"));
    CurrentBlock->addExportStations(stations);
}

/**
 * @brief cwSurvexImporter::parseFlags
 * @param line
 *
 * This parses the flags
 *
 * Currently, surface isn't support.
 */
void cwSurvexImporter::parseFlags(QString line)
{
    QStringList flags = line.split(QRegExp("\\s+"));

    bool flagOperator = true;
    bool excludeLength = false;
    foreach(QString flag, flags) {
        if(flag.compare("duplicate", Qt::CaseInsensitive) == 0) {
            excludeLength = flagOperator;
            flagOperator = true;

            //Flip, becuause we're including
            CurrentBlock->setIncludeDistance(!excludeLength);
        } else if(flag.compare("splay", Qt::CaseInsensitive) == 0) {
            excludeLength = flagOperator;
            flagOperator = true;

            //Flip, becuause we're including
            CurrentBlock->setIncludeDistance(!excludeLength);
        } else if(flag.compare("surface", Qt::CaseInsensitive) == 0) {
            Errors.append(QString("Warning: *flags surface isn't support at this time, excluding shot lengths"));
            excludeLength = flagOperator;
            flagOperator = true;

            //Flip, becuause we're including
            CurrentBlock->setIncludeDistance(!excludeLength);
        } else if(flag.compare("not", Qt::CaseInsensitive) == 0) {
            flagOperator = false;
        }
    }
}

/**
  \brief Runs the stats on the survex files

  This will go through all the survex file and
  */
void cwSurvexImporter::runStats(QString filename) {
    emit statusMessage("Gathering sauce for " + filename);

    QFile file;
    bool canOpenFile = openFile(file, filename);

    if(!canOpenFile) { return; } //Can't open the file

    //Add the file to the include stack
    IncludeStack.append(Include(file.fileName()));

    QRegExp exp;
    while(!file.atEnd() && isRunning()) {
        QString line = file.readLine();

        //Find the include files
        exp = QRegExp("^\\s*\\*(\\w+)\\s*(.*)");
        if(line.contains(exp)) {
            QString command = exp.cap(1);
            QString args = exp.cap(2).trimmed();

            if(compare(command, "include")) {
                runStats(args);
            }
        }

        //Add all the lines up
        TotalNumberOfLines++;
    }

    IncludeStack.removeLast();
}

/**
 * @brief cwSurvexImporter::updateLRUDForCurrentBlock
 *
 * This will try to update the LRUD for the stations in the chunks
 *
 */
void cwSurvexImporter::updateLRUDForCurrentBlock() {

    foreach(cwSurvexLRUDChunk lrudChunk, CurrentBlock->LRUDChunks) {
        QList<cwStation> stations = lrudChunk.Stations;
        for(int i = 0; i < stations.size(); i++) {
            int before = i - 1;
            int after = i + 1;

            cwStation beforeStation = before >= 0 ? stations.at(before) : cwStation();
            cwStation station = stations.at(i);
            cwStation afterStation = after < stations.size() ? stations.at(after) : cwStation();

            updateStationLRUD(beforeStation, station, afterStation);
        }
    }
}

/**
 * @brief cwSurvexImporter::updateStationLRUD
 * @param before - The station before station that occures in *data passage
 * @param station - The station with LRUDs
 * @param after - The station after station that occures in *data passage
 *
 * This will attempt to update chunk station with station's LRUD data.  This function
 * uses shot inforamation to varify we're updating the correct station in the chunk
 */
void cwSurvexImporter::updateStationLRUD(cwStation before, cwStation station, cwStation after)
{
    for(int i = 0; i < CurrentBlock->Chunks.size(); i++) {
        cwSurveyChunk* chunk = CurrentBlock->Chunks.at(i);
        QList<int> stationIndices = chunk->indicesOfStation(station.name());
        if(!stationIndices.isEmpty()) {
            foreach(int index, stationIndices) {
                //NOTE: This won't work this case sensitive survex files
                QString beforeStationName = chunk->station(index - 1).name();
                bool beforeGood = !before.isValid() || beforeStationName.compare(before.name(), Qt::CaseInsensitive) == 0;

                QString afterStationName = chunk->station(index + 1).name();
                bool afterGood = !after.isValid() || afterStationName.compare(after.name(), Qt::CaseInsensitive) == 0;

                if(beforeGood && afterGood) {
                    chunk->setStation(station, index);
                    break;
                }

                //Try altrating shot orintation. A good test case is the Erwong Dong survex file
                if(index == 0) {

                    //Get the previous chunk
                    if(i > 1) {
                        cwSurveyChunk* previousChunk = CurrentBlock->Chunks.at(i - 1);

                        //Get the last station in the previous chunk
                        beforeStationName = previousChunk->stations().last().name();
                        beforeGood = !before.isValid() || beforeStationName.compare(before.name(), Qt::CaseInsensitive) == 0;
                    }
                } else if(index == chunk->stationCount() - 1) {
                    //Last index in the chunk
                    if(i < CurrentBlock->Chunks.size() - 1) {
                        cwSurveyChunk* nextChunk = CurrentBlock->Chunks.at(i + 1);

                        //Get the first station in the next chunk
                        afterStationName = nextChunk->stations().first().name();
                        afterGood = !after.isValid() || afterStationName.compare(after.name(), Qt::CaseInsensitive) == 0;
                    }
                }

                //After station should be good.
                if(beforeGood && afterGood) {
                    chunk->setStation(station, index);
                    break;
                }
            }
        }
    }
}
