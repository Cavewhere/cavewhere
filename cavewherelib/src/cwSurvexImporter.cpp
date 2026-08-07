/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexImporter.h"
#include "cwSurvexCS.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTreeImportDataNode.h"
#include "cwSurvexGlobalData.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurvexLRUDChunk.h"
#include "cwSurvexNodeData.h"

//Qt includes
#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

//Std include
#include "math.h"

namespace {
// Survex uses "-" as a "no measurement" sentinel in data fields. Treat it as
// empty so the reading lands in State::Empty rather than State::Invalid.
QString stripSurvexSentinel(const QString& value) {
    return value.trimmed() == QLatin1String("-") ? QString() : value;
}

// Survex's anonymous stations. Cavern resolves a leg to one of these as a wall
// point instead of a named station, which is what makes the leg a splay. "-" is
// the spelling `*alias station - ..` declares; CaveWhere reads it as anonymous
// whether or not the alias was declared, because older files rely on that.
bool isAnonymousStation(const QString& name) {
    return name == QLatin1String(".")
           || name == QLatin1String("..")
           || name == QLatin1String("...")
           || name == QLatin1String("-");
}
}

cwSurvexImporter::cwSurvexImporter(QObject* parent) :
    cwTreeDataImporter(parent),
    RootBlock(new cwTreeImportDataNode()),
    CurrentBlock(nullptr),
    GlobalData(nullptr),
    PreviousGlobalData(new cwSurvexGlobalData())
{
    RootBlock->moveToThread(nullptr);

    connect(this, &cwTask::finished, this, [&]() {
        //Move GlobalData to PreviousGlobalData
        PreviousGlobalData->deleteLater();
        PreviousGlobalData = GlobalData;
        PreviousGlobalData->moveToThread(thread());
        PreviousGlobalData->setParent(this);
    });
}

cwSurvexImporter::~cwSurvexImporter()
{
    Q_ASSERT(isReady());
    delete RootBlock;
    if(GlobalData != nullptr) {
        delete GlobalData;
    }
}

void cwSurvexImporter::runTask() {
    RootBlock->moveToThread(QThread::currentThread());
    if(GlobalData != nullptr) {
        GlobalData->deleteLater();
    }
    GlobalData = new cwSurvexGlobalData();

    if (!RootFilenames.isEmpty()) {
        importSurvex(RootFilenames[0]);
    }

    RootBlock->moveToThread(nullptr);
    GlobalData->moveToThread(nullptr);

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

    //Sets the current block to the root block
    CurrentBlock = RootBlock;

    loadFile(filename);

    //A file that never opens a *begin keeps its data on the root block, and the
    //root block never sees an *end
    finishBlock();

    //Add the rootBlocks to GlobalData
    GlobalData->setNodes(RootBlock->childNodes());

    saveLastImport(filename);

}

/**
  Constructor for teh begin and end state object

  This creates a default dataformat and datatype for the begin and end state
  */
cwSurvexImporter::BeginEndState::BeginEndState() {
    DataFormat = defaultDataFormat();
    DataType = Normal;
    ColumnCount = DataFormat.size();
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
bool cwSurvexImporter::hasParseErrors() {
    return !Errors.isEmpty();
}

/**
  \brief Gets the errors of the importer
  \return Returns the errors if any.  Will be empty if HasErrors() returns false
  */
QStringList cwSurvexImporter::parseErrors() {
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
    CapturedFixStations.clear();
    TotalNumberOfLines = 0;
    CurrentTotalNumberOfLines = 0;
}

/**
  \brief Loads the file
  */
void cwSurvexImporter::loadFile(QString filename) {
    filename.remove('"');

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
//    emit progressed(CurrentTotalNumberOfLines);

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
            QString pattern = "^.+/" + QRegularExpression::escape(entry) + "(\\.svx)?$";
            QRegularExpression regExp(pattern, QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch match = regExp.match(filename);
            if (match.hasMatch()) {
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
    if(std::find(IncludeFiles.begin(), IncludeFiles.end(), filename) != IncludeFiles.end() && !IncludeFiles.empty()) {
        //File has already been included... Do nothing
        return false;
    }

    QStringList::iterator iter = std::lower_bound(IncludeFiles.begin(), IncludeFiles.end(), filename);
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

    QRegularExpression beginExp("^\\s*\\*begin\\s*((?:\\w|-|_)*)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch beginMatch = beginExp.match(line);

    if (beginMatch.hasMatch()) {
        // Create a new block
        cwTreeImportDataNode* newBlock = new cwTreeImportDataNode();
        QString blockName = beginMatch.captured(1).trimmed();
        newBlock->setName(blockName);

        //Add the block to the structure
        CurrentBlock->addChildNode(newBlock);

        //Copy the calibrations
        newBlock->calibration()->setData(CurrentBlock->calibration()->data());

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

        //The coordinate system crosses an *include boundary where the format
        //state doesn't: cavern copies proj_str into the child settings whatever
        //file the *begin is in, and only *end puts the parent's back.
        currentState.CS = lastState.CS;

        BeginEndStateStack.append(currentState);

        return;
    }

    // Parse command
    QRegularExpression commandExp("^\\s*\\*(\\w+)\\s*(.*)");
    QRegularExpressionMatch commandMatch = commandExp.match(line);

    if (commandMatch.hasMatch()) {
        QString command = commandMatch.captured(1);
        QString arg = commandMatch.captured(2).trimmed();

        if(compare(command, "end")) {

            //Drain the block's scratch onto its chunks before getting out of it
            finishBlock();

            cwTreeImportDataNode* parentBlock = CurrentBlock->parentNode();
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
        } else if(compare(command, "title")) {
            parseTitle(arg);
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
        } else if(compare(command, "alias")) {
            parseAlias(arg);
        } else if(compare(command, "cs")) {
            parseCS(arg);
        } else if(compare(command, "fix")) {
            parseFix(arg);
        } else {
            addWarning(QString("Unknown survex keyword:") + command);
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
    // Remove *data from the line
    QRegularExpression regExp("^\\s*\\*data\\s*(.*)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regExp.match(line);
    if (match.hasMatch()) {
        line = match.captured(1);
    }

    QStringList dataFormatList = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
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
        nodeData(CurrentBlock)->addLRUDChunk();
    } else if(compare(dataFormatType, "nosurvey")) {
        setCurrentDataEntryType(NoSurvey);
    } else {
        addError("Normal, passage data, nosurvey are supported, using default format");
        setCurrentDataEntryType(Normal);
        const auto defaultFormat = BeginEndState::defaultDataFormat();
        setCurrentDataFormat(defaultFormat);
        setCurrentColumnCount(defaultFormat.size());
        return;
    }

    QMap<DataFormatType, int> dataFormat;
    dataFormat.clear();

    const int columnCount = dataFormatList.size() - 1; //Number of columns after normal/passage/nosurvey

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
        } else if(compare(format, "backcompass") || compare(format, "backbearing")) {
            dataFormat[BackCompass] = index;
        } else if(compare(format, "clino") || compare(format, "gradient")) {
            dataFormat[Clino] = index;
        } else if(compare(format, "backclino") || compare(format, "backgradient")) {
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
            addWarning(QString("Unknown *data keyword '%1', treating as ignore").arg(format));
            dataFormat[Ignore] = index;
        }
    }

    setCurrentDataFormat(dataFormat);
    setCurrentColumnCount(columnCount);
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
    QStringList data = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    data.removeAll(""); //Remove all empty ones

    QMap<DataFormatType, int> dataFormat = currentDataFormat();
    int expectedColumnCount = currentColumnCount();

    //Make sure the there's the same number of columns as needed
    if(expectedColumnCount != data.size() && !dataFormat.contains(IgnoreAll)) {
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

  A leg with one anonymous end is a splay: it gets buffered on the named end
  rather than becoming a shot, since it has no destination station to chain to.
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

    const bool fromIsAnonymous = isAnonymousStation(fromStationName);
    const bool toIsAnonymous = isAnonymousStation(toStationName);

    if(fromIsAnonymous && toIsAnonymous) {
        addWarning(QString("Skipping the leg between %1 and %2 because both ends are anonymous, "
                           "so it has nothing to hang on")
                       .arg(fromStationName, toStationName));
        return;
    }

    const auto isUsableEnd = [](const QString& name) {
        return isAnonymousStation(name) || cwStation::nameIsValid(name);
    };

    if(!isUsableEnd(fromStationName) || !isUsableEnd(toStationName)) {
        addWarning(QString("Skipping the leg between \"%1\" and \"%2\" because those aren't "
                           "station names CaveWhere can use")
                       .arg(fromStationName, toStationName));
        return;
    }

    const QString distance = stripSurvexSentinel(extractData(data, Distance));
    const QString compass = stripSurvexSentinel(extractData(data, Compass));
    const QString clino = stripSurvexSentinel(extractData(data, Clino));

    if(fromIsAnonymous || toIsAnonymous) {
        //Survex anchors the splay at the named end and records the instrument
        //reading as written, whichever column the anonymous station lands in
        nodeData(CurrentBlock)->addSplay(fromIsAnonymous ? toStationName : fromStationName,
                                         cwShotMeasurement(cwDistanceReading(distance),
                                                           cwCompassReading(compass),
                                                           cwClinoReading(clino)));
        return;
    }

    if(CurrentBlock->isSplay()) {
        //Both ends are named, so keep the leg and its names. Anchoring it as a
        //splay would throw the to-station's name away
        addWarning(QString("Keeping the splay between %1 and %2 as a shot, since both ends are "
                           "named. Its length is excluded from the cave")
                       .arg(fromStationName, toStationName));
    }

    //Create the from and to stations
    cwStation fromStation(fromStationName);
    cwStation toStation(toStationName);

    cwShot shot;
    shot.setDistance(distance);
    shot.setCompass(compass);
    shot.setBackCompass(stripSurvexSentinel(extractData(data, BackCompass)));
    shot.setClino(clino);
    shot.setBackClino(stripSurvexSentinel(extractData(data, BackClino)));

    //A splay is a wall shot, so it never adds to the cave's length whatever the
    //duplicate and surface flags say
    shot.setDistanceIncluded(CurrentBlock->isDistanceInclude() && !CurrentBlock->isSplay());

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
 * @brief cwSurvexImporter::addCalibrationToCurrentChunk
 * @param calibration
 *
 * Applies the calibration to CurrentBlock's trip-level calibration and deletes the
 * temporary object. Per-shot chunk calibrations have been removed.
 */
void cwSurvexImporter::addCalibrationToCurrentChunk(cwTripCalibration *calibration)
{
    CurrentBlock->calibration()->setData(calibration->data());
    delete calibration;
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
    station.setLeft(stripSurvexSentinel(extractData(data, Left)));
    station.setRight(stripSurvexSentinel(extractData(data, Right)));
    station.setUp(stripSurvexSentinel(extractData(data, Up)));
    station.setDown(stripSurvexSentinel(extractData(data, Down)));

    //Add the station to the current LRUD chunk
    nodeData(CurrentBlock)->LRUDChunks.last().Stations.append(station);
}

/**
  \brief Get's the full station name

  \param The short name of the station

  This will return all the survex block name prefixes to create a full name
  */
QString cwSurvexImporter::fullStationName(QString name) {
    if(name.isEmpty()) { return QString(); }

    cwTreeImportDataNode* current = CurrentBlock;
    QStringList fullNameList;

    //While not the root element of the importer
    while(current->parentNode() != nullptr) {
        QString blockName = current->name();
        if(!blockName.isEmpty()) {
            fullNameList.prepend(blockName);
        }
        current = current->parentNode();
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

/**
  \brief The coordinate system the innermost *begin block is under

  Survex scopes *cs to its block: cmd_begin copies the whole settings struct,
  including proj_str, and *end restores the parent's, so a system named inside
  a block is the parent's again at the matching *end (survex/src/commands.c,
  cmd_begin and pop_settings).
  */
cwSurvexCS::Parsed cwSurvexImporter::currentCS() const {
    if(BeginEndStateStack.isEmpty()) { return cwSurvexCS::Parsed(); }
    return BeginEndStateStack.last().CS;
}

void cwSurvexImporter::setCurrentCS(const cwSurvexCS::Parsed& cs) {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    BeginEndStateStack.last().CS = cs;
}

int cwSurvexImporter::currentColumnCount() const {
    if(BeginEndStateStack.isEmpty()) { return 0; }
    return BeginEndStateStack.last().ColumnCount;
}

void cwSurvexImporter::setCurrentColumnCount(int count) {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    BeginEndStateStack.last().ColumnCount = count;
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
  \brief Extracts the title from a `*title` line and stores it on the current block.

  Survex titles are typically quoted strings (e.g. `*title "Big Room Survey"`),
  but bare unquoted forms are also accepted. The title is purely a display label;
  it does NOT participate in station-name prefixing — that role is reserved for
  the `*begin <name>` block name.
  */
void cwSurvexImporter::parseTitle(QString line) {
    QString title = line.trimmed();

    if(title.size() >= 2 && title.startsWith('"') && title.endsWith('"')) {
        title = title.mid(1, title.size() - 2);
    }

    CurrentBlock->setTitle(title);
}

/**
  \brief Extracts the team member from the survey line
  */
void cwSurvexImporter::parseTeamMember(QString line) {

    QStringList jobAndNameList;

    QRegularExpression splitReg("\"(\\w+\\s*)+\"|\\w+");
    QRegularExpressionMatchIterator it = splitReg.globalMatch(line);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString nameOrJob = match.captured();
        nameOrJob = nameOrJob.remove('"');
        nameOrJob = nameOrJob.trimmed();
        jobAndNameList.append(nameOrJob);
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

    QRegularExpression reg("^(TAPE|COMPASS|BACKCOMPASS|BACKCLINO|CLINO|COUNTER|DEPTH|DECLINATION|X|Y|Z)\\s+(\\S+)(?:\\s*(\\S+))?$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = reg.match(line);

    if (match.hasMatch()) {
        QString type = match.captured(1).toLower();
        QString calibrationString = match.captured(2);
        QString scaling = match.captured(3);

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

        //Create a new calibration
        cwTripCalibration* calibration = new cwTripCalibration();

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
            calibration->setImportedDeclination(calibrationValue);

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

        addCalibrationToCurrentChunk(calibration);

    } else {
        addError("Couldn't read calibration");
    }
}

/**
  This parses the units out of the survex importer
  */
void cwSurvexImporter::parseUnits(QString line) {
    QRegularExpression reg("(TAPE|LENGTH|COMPASS|BEARING|CLINO|GRADIENT|COUNTER|DEPTH|DECLINATION|X|Y|Z)\\s+(\\S+)\\s*",
                           QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = reg.match(line);

    if (match.hasMatch()) {
        QString type = match.captured(1).toLower();
        QString unitString = match.captured(2);

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
        } else if(type == "compass" || type == "bearing"
                  || type == "clino" || type == "gradient"
                  || type == "declination") {
            //CaveWhere reads every angle in degrees, so a *units line saying so
            //restates the default — TopoDroid writes one for every export
            const bool inDegrees = compare(unitString, "degrees")
                                   || compare(unitString, "degs")
                                   || compare(unitString, "deg");
            if(!inDegrees) {
                addWarning(QString("cavewhere reads '%1' in degrees, ignoring %2")
                               .arg(type, unitString));
            }
        } else if(type == "counter") {
            addWarning("cavewhere cannot handle 'counter' units");
        } else if(type == "depth") {
            addWarning("cavewhere cannot handle 'depth' units");
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
    QRegularExpression splitRegex("\\s+");
    QStringList equalStations = line.split(splitRegex);

    if(equalStations.size() <= 1) {
        Errors.append(QString("Error: *equate on %1 has only one station").arg(currentLineNumber()));
        return;
    }

    nodeData(CurrentBlock)->addToEquated(equalStations);
}

/**
 * @brief cwSurvexImporter::parseExport
 * @param line
 *
 * This parses the export stations
 */
void cwSurvexImporter::parseExport(QString line)
{
    QStringList stations = line.split(QRegularExpression("\\s+"));
    nodeData(CurrentBlock)->addExportStations(stations);
}

/**
 * @brief cwSurvexImporter::parseFlags
 * @param line
 *
 * This parses the flags
 *
 * `not` applies to the flag that follows it and to nothing else, so each flag
 * is tracked on its own and `*flags not splay` leaves an active `duplicate`
 * alone.
 *
 * Currently, surface isn't support.
 */
void cwSurvexImporter::parseFlags(QString line)
{
    const QStringList flags = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    bool flagIsOn = true;
    for(const QString& flag : flags) {
        if(compare(flag, "not")) {
            flagIsOn = false;
            continue;
        }

        if(compare(flag, "splay")) {
            CurrentBlock->setSplay(flagIsOn);
        } else if(compare(flag, "duplicate")) {
            CurrentBlock->setIncludeDistance(!flagIsOn);
        } else if(compare(flag, "surface")) {
            if(flagIsOn) {
                addWarning("*flags surface isn't support at this time, excluding shot lengths");
            }
            CurrentBlock->setIncludeDistance(!flagIsOn);
        }

        flagIsOn = true;
    }
}

/**
 * @brief cwSurvexImporter::parseAlias
 * @param line
 *
 * Survex defines exactly one alias, `*alias station - ..`, which makes `-`
 * another spelling of the anonymous station `..`; `*alias station -` drops it
 * again. CaveWhere reads `-` as anonymous either way, so this only has to take
 * the directive quietly and say something about spellings it doesn't know.
 */
void cwSurvexImporter::parseAlias(QString line)
{
    const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    const QStringList arguments = parts.mid(1);

    const bool stationAlias = !parts.isEmpty()
                              && compare(parts.at(0), "station")
                              && (arguments == QStringList{QStringLiteral("-")}
                                  || arguments == QStringList{QStringLiteral("-"), QStringLiteral("..")});

    if(!stationAlias) {
        addWarning(QString("Ignoring \"*alias %1\", the only alias survex defines is "
                           "\"*alias station - ..\"").arg(line));
    }
}

/**
 * Put the `*cs` on the block it appears in, so following `*fix` directives
 * carry its CS as inputCS until the matching `*end` — see currentCS() for why
 * the block, and not the file, is the scope. Survex's own spellings — UTM16N,
 * S-MERC, OSGB:SD, CUSTOM
 * "+proj=…" — are translated into the PROJ string each stands for, since that
 * is the one form every reader of inputCS understands: the picker, the export,
 * and the local projection the scene is drawn in. A spelling survex doesn't
 * define leaves the following fixes with no coordinate system at all, which
 * says so on the fix rather than storing text nothing can read.
 *
 * `*cs out ...` (output CS) is ignored on import; that's a region-level concern
 * handled by globalCS.
 */
void cwSurvexImporter::parseCS(QString line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QStringLiteral("out"), Qt::CaseInsensitive)) {
        return;
    }

    const std::optional<cwSurvexCS::Parsed> parsed = cwSurvexCS::fromSurvexCS(trimmed);
    if (!parsed.has_value()) {
        addWarning(QString("CaveWhere doesn't know the coordinate system \"%1\"; the "
                           "*fix stations under it carry none until you set one on them")
                       .arg(trimmed));
    }

    setCurrentCS(parsed.value_or(cwSurvexCS::Parsed()));
}

/**
 * Parse a `*fix <station> [<reference>] <x> <y> <z> [<varH> <varV>]` line.
 * Survex accepts both `*fix A 100 200 300` and the reference form
 * `*fix A reference 100 200 300`; we accept both shapes by counting
 * trailing numerics.
 */
void cwSurvexImporter::parseFix(QString line)
{
    //A local, not a shared static: importers run on the task thread pool, and
    //QRegularExpression is only documented reentrant, so two imports would be
    //calling match() on the same instance.
    const QRegularExpression whitespaceExp(QStringLiteral("\\s+"));
    const QStringList parts = line.split(whitespaceExp, Qt::SkipEmptyParts);
    if (parts.size() < 4) {
        addWarning(QStringLiteral("*fix needs at least station + 3 coordinates"));
        return;
    }

    auto isNumber = [](const QString& s) {
        bool ok = false;
        s.toDouble(&ok);
        return ok;
    };

    int coordStart = -1;
    for (int i = parts.size() - 3; i >= 1; --i) {
        if (isNumber(parts.at(i)) && isNumber(parts.at(i + 1)) && isNumber(parts.at(i + 2))) {
            coordStart = i;
            break;
        }
    }
    if (coordStart < 0) {
        addWarning(QStringLiteral("*fix coordinates couldn't be parsed: %1").arg(line));
        return;
    }

    //Under LAT-LONG the file leads with latitude, and a coordinate always
    //reaches cwFixStation easting first.
    const double first = parts.at(coordStart).toDouble();
    const double second = parts.at(coordStart + 1).toDouble();
    const cwSurvexCS::Parsed cs = currentCS();
    const double easting = cs.latitudeFirst ? second : first;
    const double northing = cs.latitudeFirst ? first : second;

    cwFixStation fix;
    fix.setStationName(fullStationName(parts.first()));
    fix.setInputCS(cs.projCS);
    //All three at once: a *fix with no *cs before it is the ordinary local-grid
    //case, and a fix with no CS written a component at a time would keep only
    //the last (see cwFixStation::setCoordinate).
    fix.setCoordinate(easting, northing, parts.at(coordStart + 2).toDouble());
    if (coordStart + 4 < parts.size() && isNumber(parts.at(coordStart + 3)) && isNumber(parts.at(coordStart + 4))) {
        fix.setHorizontalVariance(parts.at(coordStart + 3).toDouble());
        fix.setVerticalVariance(parts.at(coordStart + 4).toDouble());
    }
    CapturedFixStations.append(fix);
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

    QRegularExpression exp;
    while (!file.atEnd() && isRunning()) {
        QString line = file.readLine();

        // Find the include files
        exp = QRegularExpression("^\\s*\\*(\\w+)\\s*(.*)");
        QRegularExpressionMatch match = exp.match(line);

        if (match.hasMatch()) {
            QString command = match.captured(1);
            QString args = match.captured(2).trimmed();
            args.remove('"');

            if (compare(command, "include")) {
                runStats(args);
            }
        }

        // Add all the lines up
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

    foreach(cwSurvexLRUDChunk lrudChunk, nodeData(CurrentBlock)->LRUDChunks) {
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
 * @brief cwSurvexImporter::finishBlock
 *
 * Drains everything the block buffered while it parsed onto its chunks. Both
 * *end and the end of the file go through here, so a file that never opens a
 * *begin gets the same treatment on the root block.
 */
void cwSurvexImporter::finishBlock()
{
    updateLRUDForCurrentBlock();
    updateSplaysForCurrentBlock();
}

/**
 * @brief cwSurvexImporter::updateSplaysForCurrentBlock
 *
 * Hangs the splays buffered while the block parsed on the stations they were
 * shot from.
 *
 * This waits until the block's chunks are complete because a station's splays
 * can show up before the leg that introduces it — TopoDroid writes all of a3's
 * wall shots ahead of the `a0 a4` centerline leg. A station that occurs more
 * than once gets its splays on the first occurrence, which is the one the
 * editor shows.
 */
void cwSurvexImporter::updateSplaysForCurrentBlock()
{
    cwSurvexNodeData* blockData = nodeData(CurrentBlock);

    for(const cwStation& station : std::as_const(blockData->Splays)) {
        const auto hangOnFirstOccurrence = [&]() {
            for(cwSurveyChunk* chunk : std::as_const(CurrentBlock->Chunks)) {
                const QList<int> indices = chunk->indicesOfStation(station.name());
                if(!indices.isEmpty()) {
                    chunk->setStationSplays(indices.first(), station.splays());
                    return true;
                }
            }
            return false;
        };

        if(!hangOnFirstOccurrence()) {
            addWarning(QString("Skipping %1 splay(s) at %2 because no shot in this block reaches "
                               "that station")
                           .arg(station.splayCount())
                           .arg(station.name()));
        }
    }

    blockData->Splays.clear();
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
    Q_UNUSED(before);

    //Copies just the four readings onto the chunk's own station, so everything
    //else it already carries — its splays — survives the *data passage pass
    auto withLRUD = [](cwStation existing, const cwStation& source) {
        existing.setLeft(source.left());
        existing.setRight(source.right());
        existing.setUp(source.up());
        existing.setDown(source.down());
        return existing;
    };

    //Update's the chunk's LRUD from station and after
    auto setLRUD = [withLRUD](
            const cwStation& station,
            const cwStation& after,
            cwSurveyChunk* chunk,
            int stationIndex)
            ->bool
    {
        QString afterStationName = cwStation::canonicalKey(chunk->station(stationIndex + 1).name());
        QString currentStationName = cwStation::canonicalKey(chunk->station(stationIndex).name());
        if(cwStation::canonicalKey(station.name()) == currentStationName
                && cwStation::canonicalKey(after.name()) == afterStationName)
        {
            chunk->setStation(withLRUD(chunk->station(stationIndex), station), stationIndex);
            chunk->setStation(withLRUD(chunk->station(stationIndex + 1), after), stationIndex + 1);
            return true;
        }
        return false;
    };

    //Swaps the left distance with the right distance
    auto swapLeftRight = [](cwStation& station) {
        auto left = station.left();
        auto right = station.right();
        station.setLeft(right);
        station.setRight(left);
    };

    for(int i = 0; i < CurrentBlock->Chunks.size(); i++) {
        cwSurveyChunk* chunk = CurrentBlock->Chunks.at(i);
        QList<int> stationIndices = chunk->indicesOfStation(station.name());

        foreach(int stationIndex, stationIndices) {
            Q_ASSERT(chunk->station(stationIndex) == station.name());

            cwStation stationTmp = station;
            cwStation afterTmp = after;

            bool success = setLRUD(stationTmp, afterTmp, chunk, stationIndex);
            if(!success) {
                //Swap the stations
                std::swap(stationTmp, afterTmp);

                //Swap the station's left and right direction
                swapLeftRight(stationTmp);
                swapLeftRight(afterTmp);

                success = setLRUD(stationTmp, afterTmp, chunk, stationIndex-1);
                if(success) {
                    return;
                }
            }
        }
    }
}
