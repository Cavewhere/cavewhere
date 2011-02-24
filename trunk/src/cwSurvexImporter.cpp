//Our includes
#include "cwSurvexImporter.h"
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"
#include "cwSurvexBlockData.h"
#include "cwSurvexGlobalData.h"

//Qt includes
#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QLinkedList>
#include <QFileInfo>
#include <QDir>

cwSurvexImporter::cwSurvexImporter(QObject* parent) :
    QObject(parent),
    RootBlock(new cwSurvexBlockData(this)),
    GlobalData(new cwSurvexGlobalData(this)),
    CurrentState(FirstBegin),
    CurrentBlock(NULL)
{
}

void cwSurvexImporter::importSurvex(QString filename) {
    clear();

    //Setup inital state data
    QMap<DataFormatType, int> defaultFormat = useDefaultDataFormat();
    BeginEndState rootBlock; //Should never be poped off
    rootBlock.DataFormat = defaultFormat;
    BeginEndStateStack.append(rootBlock);

    //The block state
    CurrentState = FirstBegin;

    //
    CurrentBlock = RootBlock;

    loadFile(filename);

    //Add the rootBlocks to GlobalData
    GlobalData->setBlocks(RootBlock->childBlocks());

    //if(!hasErrors()) {
    saveLastImport(filename);
    //}

//    foreach(QString error, Errors) {
//        qDebug() << error;
//    }

    emit finishedImporting();

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

///**
//  \brief Gets all the survey chunks from the file
//  */
//QList<cwSurveyChunk*> cwSurvexImporter::chunks() {
//    return Chunks;
//}

/**
  \brief Clears all the current data in the object
  */
void cwSurvexImporter::clear() {
    RootBlock->clear(); //Chunks.clear();
    Errors.clear();
    IncludeStack.clear();
    BeginEndStateStack.clear();
}

/**
  \brief Loads the file
  */
void cwSurvexImporter::loadFile(QString filename) {

    QFileInfo fileInfo(filename);
    if(!fileInfo.exists() && !IncludeStack.isEmpty()) {
        //This maybe a relative path to the rootFile
        QFileInfo rootFileInfo(currentFile());
        QDir rootFileDir = rootFileInfo.absoluteDir().path();

        rootFileDir.setNameFilters(QStringList("*.svx"));
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
            //Try to add the .svx extension
            fixedUpFile += ".svx";
        }
        filename = fixedUpFile;
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Errors.append(QString("Error: Couldn't open ") + filename);
        return;
    }


    IncludeStack.append(Include(filename));

    //QString line;
    //CurrentLine = 0;
    while(!file.atEnd()) {
        QString line = file.readLine();

        //The last includ file
        increamentLineNumber();

        parseLine(line);
        //qDebug() << "Is open" << file.isOpen();
    }



    IncludeStack.removeLast();

    //Chunks
//    foreach(cwSurveyChunk* chunk, Chunks) {
//        //qDebug() << "------------------" << chunk << "------------------";
//        for(int i = 0; i < chunk->StationCount() - 1; i++) {
//            cwStation* station = chunk->Station(i);
//            cwShot* shot = chunk->Shot(i);

//            //qDebug() << station->GetName();
//            //qDebug() << "\t" << shot->GetDistance().toString() << shot->GetCompass().toString() << "/" << shot->GetBackCompass().toString() << shot->GetClino().toString() << "/" << shot->GetBackClino().toString();
//        }
//        qDebug() << chunk->Station(chunk->StationCount() - 1)->GetName();
//    }

    //qDebug() << "Done";
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
        CurrentBlock = newBlock;

        //Copy the last state variables
        BeginEndState state = BeginEndStateStack.last();
        BeginEndStateStack.append(state);

        return;
    }

    if(InsideBegin) {

        //Parse command
        exp = QRegExp("^\\s*\\*(\\w+)\\s*(.*)");
        if(line.contains(exp)) {
            QString command = exp.cap(1);
            QString args = exp.cap(2).trimmed();

            if(compare(command, "end")) {
                cwSurvexBlockData* parentBlock = CurrentBlock->parentBlock();
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
                importDataFormat(line);
            } else if(compare(command, "include")) {
                loadFile(args);
            }  else {
                addWarning(QString("Unknown survex keyword:") + command);
            }

            if(CurrentBlock == RootBlock) {
                CurrentState = FirstBegin;
            }

            return;
        }

        //Parse survey data
        importData(line);



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
void cwSurvexImporter::importDataFormat(QString line) {
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

    //Check for normal data
    if(!compare(dataFormatList.front(), "normal")) {
        addError("Normal data is only currently excepted, using default format");
        setCurrentDataFormat(useDefaultDataFormat());
        return;
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
        } else {
            addError(QString("Unknown *data keyword: ") + format + " Using default format");
            dataFormat = useDefaultDataFormat();
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

QMap<cwSurvexImporter::DataFormatType, int> cwSurvexImporter::useDefaultDataFormat() {
    QMap<DataFormatType, int> dataFormat;
    dataFormat[To] = 0;
    dataFormat[From] = 1;
    dataFormat[Distance] = 2;
    dataFormat[Compass] = 3;
    dataFormat[Clino] = 4;
    return dataFormat;
}

/**
  \brief Imports a line of survey data
  */
void cwSurvexImporter::importData(QString line) {
    QStringList data = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
    data.removeAll(""); //Remove all empty ones

    QMap<DataFormatType, int> dataFormat = currentDataFormat();

    //Make sure the there's the same number of columns as needed
    if(dataFormat.size() != data.size() && !dataFormat.contains(IgnoreAll)) {
        addError("Can't extract shot data. To many or not enough data columns, skipping data");
        return;
    }

    //Make sure there's enough columns
    if(dataFormat.contains(IgnoreAll) && dataFormat[IgnoreAll] < data.size()) {
        addError("Can't extract shot data. Not enough data columns, skipping data");
    }

    QString fromStationName = extractData(data, From);
    QString toStationName= extractData(data, To);

    //Make sure the to and from stations exist
    if(fromStationName.isEmpty() || toStationName.isEmpty()) {
        addError("Can't extract shot data. No toStation or from station");
    }

    //Create the from and to stations
    cwStationReference* fromStation = createOrLookupStation(fromStationName);
    cwStationReference* toStation = createOrLookupStation(toStationName);

    cwShot* shot = new cwShot();
    shot->SetDistance(extractData(data, Distance));
    shot->SetCompass(extractData(data, Compass));
    shot->SetBackCompass(extractData(data, BackCompass));
    shot->SetClino(extractData(data, Clino));
    shot->SetBackClino(extractData(data, BackClino));

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
  \brief Creates or lookup station based on the station name
  \param stationName - The name of the station
  \returns A new station of a station that's already exists
  */
cwStationReference* cwSurvexImporter::createOrLookupStation(QString stationName) {
    if(stationName.isEmpty()) { return NULL; }

    //Create the survex prefix
    QString fullName = fullStationName(stationName);

    //See if the station already exist
    if(StationLookup.contains(fullName)) {
        return StationLookup[fullName];
    }

    //Create the stations
    cwStationReference* station = new cwStationReference(stationName);
    StationLookup[fullName] = station;
    return station;
}

/**
  \brief Will add the shot to a survey chunk

  This will either add the shot to the current survey chunk, if the fromStation
  is the last station in the chunk or will create a new survey chunk.  If a
  new survey chunk is created, it'll be added to the end of the list of chunks.
  */
void cwSurvexImporter::addShotToCurrentChunk(cwStationReference* fromStation,
                                             cwStationReference* toStation,
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
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation << toStation << shot;
            return;
        }

        chunk = new cwSurveyChunk();
        CurrentBlock->addChunk(chunk);
        //Chunks.append(chunk);
        if(chunk->canAddShot(fromStation, toStation, shot)) {
            chunk->AppendShot(fromStation, toStation, shot);
        } else {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation << toStation << shot;
            return;
        }
    }
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
QMap<cwSurvexImporter::DataFormatType, int> cwSurvexImporter::currentDataFormat() const {
    if(BeginEndStateStack.isEmpty()) { return QMap<DataFormatType, int>(); }
    return BeginEndStateStack.last().DataFormat;
}

void cwSurvexImporter::setCurrentDataFormat(QMap<DataFormatType, int> format) {
    Q_ASSERT(!BeginEndStateStack.isEmpty());
    BeginEndStateStack.last().DataFormat = format;
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
}
