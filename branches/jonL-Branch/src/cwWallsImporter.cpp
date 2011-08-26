//Our includes
#include "cwWallsImporter.h"
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwWallsBlockData.h"
#include "cwWallsGlobalData.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QLinkedList>
#include <QFileInfo>
#include <QDir>

cwWallsImporter::cwWallsImporter(QObject* parent) :
    cwTask(parent),
    RootBlock(new cwWallsBlockData(this)),
    CurrentBlock(NULL),
    GlobalData(new cwWallsGlobalData(this)),
    CurrentState(FirstBegin)
{
}

void cwWallsImporter::runTask() {
    importWalls(RootFilename);
    done();
}

void cwWallsImporter::importWalls(QString filename) {
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

    loadProjectFile(filename);

    //Add the rootBlocks to GlobalData
    GlobalData->setBlocks(RootBlock->childBlocks());

    saveLastImport(filename);

}

/**
  Constructor for the begin and end state object

  This creates a default dataformat and datatype for the begin and end state
  */
cwWallsImporter::BeginEndState::BeginEndState() {
    DataFormat = defaultDataFormat();
    DataType = Normal;
}

/**
  \brief Creates the default map for the data format
  */
QMap<cwWallsImporter::DataFormatType, int> cwWallsImporter::BeginEndState::defaultDataFormat() {
    QMap<cwWallsImporter::DataFormatType, int> dataFormat;
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
bool cwWallsImporter::hasErrors() {
    return !Errors.isEmpty();
}

/**
  \brief Gets the errors of the importer
  \return Returns the errors if any.  Will be empty if HasErrors() returns false
  */
QStringList cwWallsImporter::errors() {
    return Errors;
}


/**
  \brief Clears all the current data in the object
  */
void cwWallsImporter::clear() {
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
void cwWallsImporter::loadProjectFile(QString filename) {
    //Open the Walls Project file
    QString word, rest;

    //Update the status
    bool firstBook = TRUE;
    emit statusMessage("Importing " + filename );

    QFile PRJfile;
    PRJfile.setFileName(filename);
    if (!PRJfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while(!PRJfile.atEnd() && isRunning()) {
            QString line = PRJfile.readLine();
            QRegExp regExp("\\.{1}(\\w+)\\W(.+)");
            regExp.setCaseSensitivity(Qt::CaseInsensitive);
            if(line.contains(regExp)) {
                word = regExp.cap(1);
                rest = regExp.cap(2);
                if(!word.compare("BOOK")){
                    //assign cwRegion type to first book, then child of cwRegion to each after
                    //assign name in "rest" to region/child
                    if(firstBook){
                        firstBook=FALSE;
                        CurrentBlock->Name = rest;
                        //RootBlock->name = rest;
                    }
                    else {

                    }
                }
                if(!word.compare("ENDBOOK")){
                    //close book
                }
                if(!word.compare("NAME")){
                    //assign filename in "rest"
                }
                if(!word.compare("STATUS")){
                    //figure out what each of different statuses mean
                    //8
                    //9
                    //24 - compiled?
                }
                if(!word.compare("REF")){
                    QRegExp georef("(\\d+.\\d+)\\W(\\d+.\\d+)\\W(\\d+)\\W(\\d+.\\d+)\\W(\\d+)\\W(\\d+)\\W(\\d+)\\W(\\d+)\\W(\\d+.\\d+)\\W(\\d+)\\W(\\d+)\\W(\\d+.\\d+)\\W(\\d+)\\W(.+)");
                    //long I know, but it works!
                    georef.setCaseSensitivity(Qt::CaseInsensitive);
                    if(rest.contains(regExp)) {
                        //georef.cap(1) is Northing
                        //georef.cap(2) is Easting
                        //georef.cap(3) is UTM Grid
                        //georef.cap(4) is Grid Convergence
                        //georef.cap(5) is elevation in meters
                        //georef.cap(6) is ??? (6)
                        //georef.cap(7) is Lat-degrees
                        //georef.cap(8) is Lat-min
                        //georef.cap(9) is Lat-decimal sec
                        //georef.cap(10) is Lon-degrees
                        //georef.cap(11) is Lon-min
                        //georef.cap(12) is Lon-decimal sec
                        //georef.cap(13) is ???
                        //georef.cap(14) is Datum (text with quotes)
                    }
                    else {
                        //big error!
                        addError("I don't recognize your geo-ref format!!!");
                    }
                }
                if(!word.compare("SURVEY")){
                    //assign name in "rest" to survey
                }
                if(!word.compare("PATH")){
                    //add path to filename
                }
            }
        }
    }
}

/**
  \brief This does the checking to open a Walls file

  It will also fix up the filename if need to try to load it

  Adds an error to the error stack if it can open a file
  */
bool cwWallsImporter::openWallsSRV(QFile& file, QString filename) {
    QFileInfo fileInfo(filename);
    if(!fileInfo.exists() && !IncludeStack.isEmpty()) {
        //This maybe a relative path to the rootFile
        QFileInfo rootFileInfo(currentFile());
        QDir rootFileDir = rootFileInfo.absoluteDir().path();

        rootFileDir.setNameFilters(QStringList("*.prj"));
        QStringList entries = rootFileDir.entryList();

        //Find the filename, this is needed because, filename case insensitive
        foreach(QString entry, entries) {
            if(entry.contains(filename, Qt::CaseInsensitive)) {
                filename = entry;
                break;
            }
        }

        QString fixedUpFile = rootFileDir.path() + "/" + filename;
        if(!QFileInfo(fixedUpFile).exists()) {
            //Try to add the .srv extension
            fixedUpFile += ".srv";
        }
        filename = fixedUpFile;
    }

    file.setFileName(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Errors.append(QString("Error: Couldn't open ") + filename);
        return false;
    }

    //Make sure we don't reopen the same file twice
    if(qFind(IncludeFiles, filename) != IncludeFiles.end()) {
        //File has already been included... Do nothing
        return false;
    }

    QStringList::iterator iter = qLowerBound(IncludeFiles.begin(), IncludeFiles.end(), filename);
    IncludeFiles.insert(iter, filename);
    return true;
}

/**
  \brief Parses a Walls line
  */
void cwWallsImporter::parseLine(QString line, qint8 projectfile=0) {
    //Remove comments & skip empty lines
    line = removeComment(line);
    if(line.isEmpty()) { return; }

    QRegExp regExp("(#{1})(.+)");
    regExp.setCaseSensitivity(Qt::CaseInsensitive);
    if(line.contains(regExp)) {
        line = regExp.cap(2);

    QRegExp exp("^\\s*\\*begin\\s*((?:\\w|-|_)*)");
    exp.setCaseSensitivity(Qt::CaseInsensitive);
    if(line.contains(exp)) {

        //Create a new block
        cwWallsBlockData* newBlock = new cwWallsBlockData();
        QString blockName = exp.cap(1).trimmed();
        newBlock->setName(blockName);

        //Add the block to the structure
        CurrentBlock->addChildBlock(newBlock);
        CurrentBlock = newBlock;

        //Copy the last state variables
        BeginEndState defaultState;
        BeginEndStateStack.append(defaultState);

        return;
    }

    if(InsideBegin) {

        //Parse command
        exp = QRegExp("^\\s*\\*(\\w+)\\s*(.*)");
        if(line.contains(exp)) {
            QString command = exp.cap(1);
            QString args = exp.cap(2).trimmed();

            if(compare(command, "end")) {
                cwWallsBlockData* parentBlock = CurrentBlock->parentBlock();
                if(parentBlock != NULL) {
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
                //loadFile(args);
            } else if(compare(command, "date")) {
                parseDate(args);
            } else if(compare(command, "team")) {
                parseTeamMember(args);
            } else if(compare(command, "calibrate")) {
                parseCalibrate(args);
            }  else {
                addWarning(QString("Unknown Walls keyword:") + command);
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
        case Dive:
            break;
        }
    }


   // qDebug() << "Line: " << CurrentLine << " :" << line;
    }
}

/**
  \brief Saves the last import to the qsettings
  */
void cwWallsImporter::saveLastImport(QString filename) {
    QSettings settings;
    settings.setValue("surveyImporter", filename);
    settings.sync();
}

/**
  \brief Get's the last Walls import
  */
QString cwWallsImporter::lastImport() {
    QSettings settings;
    QVariant filename;
    settings.value("surveyImporter", filename);
    return filename.toString();
}

/**
  \brief Remove comments from the line
  */
QString cwWallsImporter::removeComment(QString& line) {
    QString removeCommentLine;
    removeCommentLine = line.trimmed();

    QRegExp regExp("(.+)(;.+)");
    regExp.setCaseSensitivity(Qt::CaseInsensitive);
    if(line.contains(regExp)) {
        line = regExp.cap(1);
    }
    int index = removeCommentLine.indexOf(';');
    if(index >= 0) {
        removeCommentLine = removeCommentLine.left(index);
    }
    return removeCommentLine;
}

/**
  \brief Tries to load the data formate
  */
void cwWallsImporter::parseDataFormat(QString line) {
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
    } else {
        addError("Normal and passage data are supported, using default format");
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

void cwWallsImporter::addError(QString error) {
    addToErrors("Error", error);
}

void cwWallsImporter::addWarning(QString error) {
    addToErrors("Warning", error);
}

void cwWallsImporter::addToErrors(QString prefix, QString errorMessage) {
    QString currentFilename;
    if(!IncludeStack.isEmpty()) {
        currentFilename = currentFile();
    }
    //Errors.append(QString("%1: %2::Line %3::%4").arg(prefix).arg(currentFilename).arg(currentLineNumber()).arg(errorMessage));
}

/**
Helper to parseNormalData and parsePassageData

This makes the line has enough elements for the current data format

It will return all the data in the line.  If there's an error, the error is added to the error
list and this returns empty data

  */
QStringList cwWallsImporter::parseData(QString line) {
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
void cwWallsImporter::parseNormalData(QString line) {
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
    cwStationReference fromStation = createOrLookupStation(fromStationName);
    cwStationReference toStation = createOrLookupStation(toStationName);

    cwShot* shot = new cwShot();
    shot->setDistance(extractData(data, Distance));
    shot->setCompass(extractData(data, Compass));
    shot->setBackCompass(extractData(data, BackCompass));
    shot->setClino(extractData(data, Clino));
    shot->setbackClino(extractData(data, BackClino));

    addShotToCurrentChunk(fromStation, toStation, shot);
}

/**
  \brief Extracts the data from data with type
  \param data - The line data
  \param type - The which piece of the line data that needs to be extracted
  */
QString cwWallsImporter::extractData(const QStringList data, DataFormatType type) {
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
  \brief Creates or lookup station based on the station name
  \param stationName - The name of the station
  \returns A new station of a station that's already exists
  */
cwStationReference cwWallsImporter::createOrLookupStation(QString stationName) {
    if(stationName.isEmpty()) { return cwStationReference(); }

    //Create the Walls prefix
    QString fullName = fullStationName(stationName);

    //See if the station already exist
    if(StationLookup.contains(fullName)) {
        return StationLookup[fullName];
    }

    //Create the stations
    cwStationReference station(stationName);
    StationLookup[fullName] = station;
    return station;
}

/**
  \brief Will add the shot to a survey chunk

  This will either add the shot to the current survey chunk, if the fromStation
  is the last station in the chunk or will create a new survey chunk.  If a
  new survey chunk is created, it'll be added to the end of the list of chunks.
  */
void cwWallsImporter::addShotToCurrentChunk(cwStationReference fromStation,
                                             cwStationReference toStation,
                                             cwShot* shot) {
    cwSurveyChunk* chunk;
    if(CurrentBlock->chunkCount() == 0) {
        //Create a new chunk
        chunk = new cwSurveyChunk();
        CurrentBlock->addChunk(chunk);
    } else {
        chunk = CurrentBlock->chunks().last(); //Chunks.last();
    }

    if(chunk->canAddShot(fromStation, toStation, shot)) {
        chunk->AppendShot(fromStation, toStation, shot);
    } else {
        if(!chunk->isValid()) {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation.name() << toStation .name() << shot;
            return;
        }

        chunk = new cwSurveyChunk();
        CurrentBlock->addChunk(chunk);
        //Chunks.append(chunk);
        if(chunk->canAddShot(fromStation, toStation, shot)) {
            chunk->AppendShot(fromStation, toStation, shot);
        } else {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation.name() << toStation.name() << shot;
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
void cwWallsImporter::parsePassageData(QString line) {
    QStringList data = parseData(line);
    if(data.isEmpty()) { return; } //Error, check the error messages

    QString stationName = extractData(data, Station);

    //Make sure the station exists
    if(stationName.isEmpty()) {
        addError("Can't extract passage data. No station");
        return;
    }

    //Create or find a station from the name
    cwStationReference station = createOrLookupStation(stationName);
    station.setLeft(extractData(data, Left));
    station.setRight(extractData(data, Right));
    station.setUp(extractData(data, Up));
    station.setDown(extractData(data, Down));
}

/**
  \brief Get's the full station name

  \param The short name of the station

  This will return all the Walls block name prefixes to create a full name
  */
QString cwWallsImporter::fullStationName(QString name) {
    if(name.isEmpty()) { return QString(); }

    cwWallsBlockData* current = CurrentBlock;
    QLinkedList<QString> fullNameList;

    //While not the root element of the importer
    while(current->parentBlock() != NULL) {
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
QMap<cwWallsImporter::DataFormatType, int> cwWallsImporter::currentDataFormat() const {
    if(BeginEndStateStack.isEmpty()) { return QMap<DataFormatType, int>(); }
    return BeginEndStateStack.last().DataFormat;
}

/**
  \brief Gets the current data entry type

  *data normal <-- This is the normal entry type
  *data passage <-- This is the passage entry type
  *data somethingElse

  */
cwWallsImporter::DataEntryType cwWallsImporter::currentDataEntryType() const {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    return BeginEndStateStack.last().DataType;
}


void cwWallsImporter::setCurrentDataFormat(QMap<DataFormatType, int> format) {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    BeginEndStateStack.last().DataFormat = format;
}

void cwWallsImporter::setCurrentDataEntryType(DataEntryType type) {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    BeginEndStateStack.last().DataType = type;
}


QString cwWallsImporter::currentFile() const {
    if(IncludeStack.isEmpty()) { return QString(); }
    return IncludeStack.last().File;
}

int cwWallsImporter::currentLineNumber() const {
    if(IncludeStack.isEmpty()) { return -1; }
    return IncludeStack.last().CurrentLine;
}
void cwWallsImporter::incrementLineNumber() {
    Q_ASSERT(!IncludeStack.isEmpty());
    IncludeStack.last().CurrentLine++;
    CurrentTotalNumberOfLines++; //All the lines combine
}

/**
  \brief Tries to extract the date from the date line

  If it can't this function return a date of 2000 01 01
  */
void cwWallsImporter::parseDate(QString dateString) {
    QDate date = QDate::fromString(dateString, "yyyy.MM.dd");

    if(!date.isValid()) {
        date = QDate(2000, 01, 01);
    }

    CurrentBlock->setDate(date);
}

/**
  \brief Extracts the team member from the survey line
  */
void cwWallsImporter::parseTeamMember(QString line) {
    QRegExp reg("\"(.+)\"\\s*(?:(\\S+\\s*)*)");

    if(reg.exactMatch(line)) {
        QString name = reg.cap(1);
        QString rolesString = reg.cap(2);
        QStringList roles = rolesString.split(' ', QString::SkipEmptyParts);

        cwTeam* currentTeam = CurrentBlock->team();
        currentTeam->addTeamMember(cwTeamMember(name, roles));
    } else {
        addError("couldn't read team member");
    }
}

/**
  \brief This extracts the calibration data from the Walls file

  This will set the TripCalibration object's data
  */
void cwWallsImporter::parseCalibrate(QString line) {

    QRegExp reg("(TAPE|COMPASS|BACKCOMPASS|BACKCLINO|CLINO|COUNTER|DEPTH|DECLINATION|X|Y|Z)\\s+(\\S+)(?:\\s*(\\S+)\\s*)?");
    reg.setCaseSensitivity(Qt::CaseInsensitive);

    if(reg.exactMatch(line)) {
        QString type = reg.cap(1).toLower();
        QString calibrationString = reg.cap(2);
        QString scaling = reg.cap(3);

        //Parse the calibration value
        bool okay;
        float calibrationValue = calibrationString.toFloat(&okay);
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

        //Flip the calibration value because Walls is written by strange british people
        calibrationValue = -calibrationValue;

        //Get the current calibration
        cwTripCalibration* calibration = CurrentBlock->calibration();

        if(type == "tape") {
            calibration->setTapeCalibration(calibrationValue);

        } else if (type == "compass") {
            calibration->setFrontCompassCalibration(calibrationValue);

        } else if (type == "backcompass") {
            //Check to see if this is a correct compasss calibration
            const float correctCalibrationThreshold = 20.0;
            if(calibrationValue - 180.0 < correctCalibrationThreshold) {
                //This is probably a correct back compass
                calibrationValue = calibrationValue - 180.0;
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
  \brief Runs the stats on the Walls files

  This will go through all the Walls file and
  */
void cwWallsImporter::runStats(QString filename) {
    emit statusMessage("Gathering sauce for " + filename);

    QFile file;
    bool canOpenFile = openWallsSRV(file, filename);

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
