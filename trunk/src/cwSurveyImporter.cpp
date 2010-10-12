//Our includes
#include "cwSurveyImporter.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

//Qt includes
#include <QFile>
#include <QDebug>
#include <QSettings>

cwSurveyImporter::cwSurveyImporter()
{
}

void cwSurveyImporter::importSurvex(QString filename) {
    clear();
    useDefaultDataFormat();

    CurrentState = FirstBegin;

    loadFile(filename);

    //if(!hasErrors()) {
    saveLastImport(filename);
    //}

    emit finishedImporting();

}

/**
  \brief Returns true if errors have accured.
  \returns The list of errors
  */
bool cwSurveyImporter::hasErrors() {
    return !Errors.isEmpty();
}

/**
  \brief Gets the errors of the importer
  \return Returns the errors if any.  Will be empty if HasErrors() returns false
  */
QStringList cwSurveyImporter::errors() {
    return Errors;
}

/**
  \brief Gets all the survey chunks from the file
  */
QList<cwSurveyChunk*> cwSurveyImporter::chunks() {
    return Chunks;
}

/**
  \brief Clears all the current data in the object
  */
void cwSurveyImporter::clear() {
    Chunks.clear();
    Errors.clear();
}

/**
  \brief Loads the file
  */
void cwSurveyImporter::loadFile(QString filename) {

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Errors.append(QString("Error: Couldn't open ") + filename);
        return;
    }

    //QString line;
    CurrentLine = 0;
    while(!file.atEnd()) {
        QString line = file.readLine();
        CurrentLine++;
        parseLine(line);
        //qDebug() << "Is open" << file.isOpen();
    }

    foreach(QString error, Errors) {
        qDebug() << error;
    }

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
void cwSurveyImporter::parseLine(QString line) {
    //Remove comments
    line = removeComment(line);

    //Skip empty lines
    if(line.isEmpty()) { return; }

    QRegExp exp("^\\s*\\*begin\\s*(\\w*)");
    if(line.contains(exp)) {
       // qDebug() << "Has begin" << line;
        CurrentState = InsideBegin;
        BeginNames.append(exp.cap(1));
        return;
    }

    if(InsideBegin) {

        //Parse command
        exp = QRegExp("^\\s*\\*(\\w+)");
        if(line.contains(exp)) {
            QString command = exp.cap(1);
            if(command == "end") {
                BeginNames.removeLast();
            } else if(command == "data") {
                importDataFormat(line);
            }  else {
                addWarning(QString("Unknown survex keyword:") + command);
            }

            if(BeginNames.isEmpty()) {
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
void cwSurveyImporter::saveLastImport(QString filename) {
    QSettings settings;
    settings.setValue("surveyImporter", filename);
    settings.sync();
}

/**
  \brief Get's the last survex import
  */
QString cwSurveyImporter::lastImport() {
    QSettings settings;
    QVariant filename;
    settings.value("surveyImporter", filename);
    return filename.toString();
}

/**
  \brief Remove comments from the line
  */
QString cwSurveyImporter::removeComment(QString& line) {
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
void cwSurveyImporter::importDataFormat(QString line) {
    //Remove *data from the line
    QRegExp regExp("^\\s*\\*data\\s*(.*)");
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
    if(dataFormatList.front() != "normal") {
        addWarning("Normal data is only currently excepted, using default format");
        useDefaultDataFormat();
        return;
    }

    DataFormat.clear();

    for(int i = 1; i < dataFormatList.size(); i++) {
        QString format = dataFormatList.at(i);

        int index = i - 1;
        //qDebug() << format << index;

        if(format == "to") {
            DataFormat[To] = index;
        } else if(format == "from") {
            DataFormat[From] = index;
        } else if(format == "tape") {
            DataFormat[Distance] = index;
        } else if(format == "compass") {
            DataFormat[Compass] = index;
        } else if(format == "backcompass") {
            DataFormat[BackCompass] = index;
        } else if(format == "clino") {
            DataFormat[Clino] = index;
        } else if(format == "backclino") {
            DataFormat[BackClino] = index;
        } else {
            addError(QString("Unknown *data keyword: ") + format + " Using default format");
            useDefaultDataFormat();
            return;
        }
    }
}

void cwSurveyImporter::addError(QString error) {
    Errors.append(QString("Error: Line ") + QString("%1 ").arg(CurrentLine) + error);
}

void cwSurveyImporter::addWarning(QString error) {
    Errors.append(QString("Warning: Line ") + QString("%1 ").arg(CurrentLine) + error);
}

void cwSurveyImporter::useDefaultDataFormat() {
    DataFormat.clear();
    DataFormat[To] = 0;
    DataFormat[From] = 1;
    DataFormat[Distance] = 2;
    DataFormat[Compass] = 3;
    DataFormat[Clino] = 4;
}

/**
  \brief Imports a line of survey data
  */
void cwSurveyImporter::importData(QString line) {
    QStringList data = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
    data.removeAll(""); //Remove all empty ones

    if(DataFormat.size() != data.size()) {
        addError("Can't extract shot data. To many or few data columns, skipping data");
        //qDebug() << data;
        //qDebug() << DataFormat;
        return;
    }

    QString fromStationName = extractData(data, From);
    QString toStationName= extractData(data, To);

    //Make sure the to and from stations exist
    if(fromStationName.isEmpty() || toStationName.isEmpty()) {
        addError("Can't extract shot data. No toStation or from station");
    }

    //Create the from and to stations
    cwStation* fromStation = createOrLookupStation(fromStationName);
    cwStation* toStation = createOrLookupStation(toStationName);

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
QString cwSurveyImporter::extractData(const QStringList data, DataFormatType type) {
    if(DataFormat.contains(type)) {
        int index = DataFormat[type];
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
cwStation* cwSurveyImporter::createOrLookupStation(QString stationName) {
    if(StationLookup.contains(stationName)) {
        return StationLookup[stationName];
    }
    cwStation* station = new cwStation(stationName);
    StationLookup[stationName] = station;
    return station;
}

/**
  \brief Will add the shot to a survey chunk

  This will either add the shot to the current survey chunk, if the fromStation
  is the last station in the chunk or will create a new survey chunk.  If a
  new survey chunk is created, it'll be added to the end of the list of chunks.
  */
void cwSurveyImporter::addShotToCurrentChunk(cwStation* fromStation, cwStation* toStation, cwShot* shot) {
    cwSurveyChunk* chunk;
    if(Chunks.empty()) {
        //Create a new chunk
        chunk = new cwSurveyChunk();
        Chunks.append(chunk);
    } else {
        chunk = Chunks.last();
    }

    if(chunk->CanAddShot(fromStation, toStation, shot)) {
        chunk->AddShot(fromStation, toStation, shot);
    } else {
        if(!chunk->IsValid()) {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation << toStation << shot;
            return;
        }

        chunk = new cwSurveyChunk();
        Chunks.append(chunk);
        if(chunk->CanAddShot(fromStation, toStation, shot)) {
            chunk->AddShot(fromStation, toStation, shot);
        } else {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation << toStation << shot;
            return;
        }
    }
}

