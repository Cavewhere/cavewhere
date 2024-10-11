/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexportCSVTask.h"

//Qt includes
#include <QFileInfo>
#include <QDomDocument>
#include <QDebug>

cwSurvexportCSVTask::cwSurvexportCSVTask(QObject *parent) :
    cwTask(parent)
{

}

/**
  \brief Sets the input file for the task to parse
  */
void cwSurvexportCSVTask::setSurvexportCSVFile(const QString &inputFile) {
    CSVFileName = inputFile;
}

/**
  \brief Run's the parser on the input file

  This will parse the survexport csv file and produce cwStationPositionLookup
  */
void cwSurvexportCSVTask::runTask() {

    StationPositions.clearStations();

    QFile csvFile(CSVFileName);
    bool okay = csvFile.open(QFile::ReadOnly);
    if(okay) {
        //Skip header
        csvFile.readLine();
        for(int lineNumber = 1; !csvFile.atEnd(); lineNumber++) {
            try {
                QString line = csvFile.readLine();
                parseCSVLine(line);
            } catch (std::runtime_error error) {
                qWarning() << "Issue while parsing survexport csv file:" << CSVFileName << " line:" << lineNumber << error.what();
            }
        }
    }

    done();
}

/**
  \brief Extracts the information for on station
  */
void cwSurvexportCSVTask::parseCSVLine(const QString& line) {

    enum Column {
        Easting,
        Northing,
        Altitude,
        StationName,
        NumberOfColumns
    };

    QStringList columns = line.split(",");

    if(columns.size() != NumberOfColumns) {
        throw std::runtime_error(QString("Columns miss match, looking for %1 but found %2").arg(NumberOfColumns).arg(columns.size()).toStdString());
    }

    QString stationName = columns.at(StationName).trimmed();
    QString xString = columns.at(Easting).trimmed();
    QString yString = columns.at(Northing).trimmed();
    QString zString = columns.at(Altitude).trimmed();

    if(stationName.isEmpty()) {
        throw std::runtime_error("Station name is empty");
    }

    if(xString.isEmpty() || yString.isEmpty() || zString.isEmpty()) {
        throw std::runtime_error(QString("Can't extract data for %1 at (%2,%3,%4)").arg(stationName, xString, yString, zString).toStdString());
    }

    bool okay;
    double x = xString.toDouble(&okay);

    if(!okay) {
        throw std::runtime_error(QString("xString isn't a double, skip station:%1").arg(stationName).toStdString());
    }

    double y = yString.toDouble(&okay);
    if(!okay) {
        throw std::runtime_error(QString("yString isn't a double, skip station:%1").arg(stationName).toStdString());
    }

    double z = zString.toDouble(&okay);
    if(!okay) {
        throw std::runtime_error(QString("zString isn't a double, skip station:%1").arg(stationName).toStdString());
    }

    QVector3D position(x, y, z);
    StationPositions.setPosition(stationName, position);
}

