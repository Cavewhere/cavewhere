/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStationCSVTask.h"

//Qt includes
#include <QFileInfo>
#include <QDomDocument>
#include <QDebug>

cwStationCSVTask::cwStationCSVTask(QObject *parent) :
    cwTask(parent)
{

}

/**
  \brief Sets the input file for the task to parse
  */
void cwStationCSVTask::setSurvexportCSVFile(QString inputFile) {
    FileName = inputFile;
}



/**
  \brief Run's the parser on the input file
  */
void cwStationCSVTask::runTask() {

    StationPositions.clearStations();

    if(!QFileInfo::exists(FileName)) {
        done();
    };

    QFile file(FileName);
    file.open(QFile::ReadOnly);

    //Read header
    file.readLine();

    enum Column {
        Easting,
        Northing,
        Altitude,
        StationName,
        NumberOfColumns
    };

    while(!file.atEnd()) {
        auto line = file.readLine().trimmed().split(',');

        auto position = [line]() {
            return QVector3D(line.at(Easting).toFloat(),
                             line.at(Northing).toFloat(),
                             line.at(Altitude).toFloat());
        };

        if(line.size() == NumberOfColumns) {
            StationPositions.setPosition(line.at(StationName),
                                         position());
        }
    }

    done();
}

