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
#include "cwGlobals.h"

//Qt includes
#include <QString>
#include <QVector3D>

class CAVEWHERE_LIB_EXPORT cwSurvexportCSVTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwSurvexportCSVTask(QObject *parent = 0);

    void setSurvexportCSVFile(const QString& inputFile);

    cwStationPositionLookup stationPositions() const;
    void clearStationPositions();

public slots:

protected:
    void runTask();

private:
    //Input file
    QString CSVFileName;

    //Output
    cwStationPositionLookup StationPositions;

    void parseCSVLine(const QString& station);
};

/**
  Get's the station position that were parsed from the xml

  This should only be called when the task has finished
  */
inline cwStationPositionLookup cwSurvexportCSVTask::stationPositions() const {
    return StationPositions;
}

/**
  \brief Clears all the stations from memory
  */
inline void cwSurvexportCSVTask::clearStationPositions() {
    StationPositions.clearStations();
}

#endif // CWPLOTSAUCEXMLTASK_H
