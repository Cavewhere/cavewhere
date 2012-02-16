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
public slots:

protected:
    void runTask();

private:
    //Input file
    QString XMLFileName;

    //Output
    cwStationPositionLookup StationPositions;

    //For extracting the gunzip data
    cwGunZipReader* GunZipReader;

    Q_INVOKABLE void privateSetPlotSauceXMLFile(QString inputFile);

//    QByteArray extractXMLData();
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

#endif // CWPLOTSAUCEXMLTASK_H
