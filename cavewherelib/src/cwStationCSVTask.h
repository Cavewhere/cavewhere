/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPLOTSAUCEXMLTASK_H
#define CWPLOTSAUCEXMLTASK_H

//Our includes
#include "cwTask.h"
#include "cwStationPositionLookup.h"

//Qt includes
#include <QString>
#include <QVector3D>

class cwStationCSVTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwStationCSVTask(QObject *parent = 0);

    void setSurvexportCSVFile(QString inputFile);

    cwStationPositionLookup stationPositions() const;
    void clearStationPositions();

public slots:

protected:
    void runTask();

private:
    //Input file
    QString FileName;

    //Output
    cwStationPositionLookup StationPositions;
};

/**
  Get's the station position that were parsed from the xml

  This should only be called when the task has finished
  */
inline cwStationPositionLookup cwStationCSVTask::stationPositions() const {
    return StationPositions;
}

/**
  \brief Clears all the stations from memory
  */
inline void cwStationCSVTask::clearStationPositions() {
    StationPositions.clearStations();
}

#endif // CWPLOTSAUCEXMLTASK_H
