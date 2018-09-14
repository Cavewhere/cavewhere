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
class cwGunZipReader;

//Qt includes
#include <QString>
#include <QVector3D>
#include <QDomNode>
#include <QDomElement>

class cwPlotSauceXMLTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwPlotSauceXMLTask(QObject *parent = 0);

    void setPlotSauceXMLFile(QString inputFile);

    cwStationPositionLookup stationPositions() const;
    void clearStationPositions();

public slots:

protected:
    void runTask();

private:
    //Input file
    QString XMLFileName;

    //Output
    cwStationPositionLookup StationPositions;

    void ParseXML(QByteArray xml);
    void ParseStationXML(QDomNode station);

    QString extractString(QDomElement element);
};

/**
  Get's the station position that were parsed from the xml

  This should only be called when the task has finished
  */
inline cwStationPositionLookup cwPlotSauceXMLTask::stationPositions() const {
    return StationPositions;
}

/**
  \brief Clears all the stations from memory
  */
inline void cwPlotSauceXMLTask::clearStationPositions() {
    StationPositions.clearStations();
}

#endif // CWPLOTSAUCEXMLTASK_H
