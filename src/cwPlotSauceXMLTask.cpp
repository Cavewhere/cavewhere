/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwPlotSauceXMLTask.h"
#include "cwGunZipReader.h"

//ZLib includes
#include <zlib.h>

//Qt includes
#include <QFileInfo>
#include <QDomDocument>
#include <QDebug>

cwPlotSauceXMLTask::cwPlotSauceXMLTask(QObject *parent) :
    cwTask(parent)
{

}

/**
  \brief Sets the input file for the task to parse
  */
void cwPlotSauceXMLTask::setPlotSauceXMLFile(QString inputFile) {
    privateSetPlotSauceXMLFile(inputFile);
}



/**
  \brief Run's the parser on the input file

  This will emit the stationPositonChanged for all the station's it finds
  in the plot sauce xml file.  This will completely ignore the line
  higharchy in the xml file.  This will also try to decompress the file, because
  plot sauce file are usually compressed.
  */
void cwPlotSauceXMLTask::runTask() {

    StationPositions.clearStations();

    QScopedPointer<cwGunZipReader> gunZipReader(new cwGunZipReader());
    gunZipReader->setFilename(XMLFileName);
    gunZipReader->setUsingThreadPool(false);
    gunZipReader->start();

    QByteArray xmlData = gunZipReader->data();
    ParseXML(xmlData);
    done();
}

/**
  \brief Helper to the setPlotSauceXMLFile
  */
void cwPlotSauceXMLTask::privateSetPlotSauceXMLFile(QString inputFile) {
    XMLFileName = inputFile;
}

/**
  \brief Parse the xml out of the QByteArray

  This will emit stationPosition() when a station's position is found
  */
void cwPlotSauceXMLTask::ParseXML(QByteArray xmlData) {
    if(!isRunning()) { return; }

    QDomDocument document;

    QString errorMessage;
    document.setContent(xmlData, &errorMessage);

    if(!errorMessage.isEmpty()) {
        //There was an error
        qWarning() << "Plot Sauce XML parse error: " << errorMessage;
        qWarning() << QString::fromLocal8Bit(xmlData);
        return;
    }

    QDomNodeList stationList = document.elementsByTagName("Station");
    for(int i = 0; i < stationList.count() && isRunning(); i++) {
        QDomNode station = stationList.at(i);
        ParseStationXML(station);
    }
}

/**
  \brief Extracts the information for on station
  */
void cwPlotSauceXMLTask::ParseStationXML(QDomNode station) {

    QString stationName;
    QString xString;
    QString yString;
    QString zString;

    QDomElement nameElement = station.firstChildElement("Name");
    stationName = extractString(nameElement);

    QDomElement positionElement = station.firstChildElement("Position");
    if(!positionElement.isNull()) {
        QDomElement XElement = positionElement.firstChildElement("X");
        QDomElement YElement = positionElement.firstChildElement("Y");
        QDomElement ZElement = positionElement.firstChildElement("Z");

        xString = extractString(XElement);
        yString = extractString(YElement);
        zString = extractString(ZElement);
    }

    if(stationName.isEmpty()) {
        return;
    }

    if(xString.isEmpty() || yString.isEmpty() || zString.isEmpty()) {
        qWarning() << "Can't extract data for " << stationName << "at" << xString << yString << zString;
        return;
    }

    bool okay;
    double x = xString.toDouble(&okay);

    if(!okay) {
        qWarning() << "xString isn't a double, skip station:" << stationName;
        return;
    }

    double y = yString.toDouble(&okay);
    if(!okay) {
        qWarning() << "yString isn't a double, skip station:" << stationName;
        return;
    }

    double z = zString.toDouble(&okay);
    if(!okay) {
        qWarning() << "zString isn't a double, skip station:" << stationName;
        return;
    }

    QVector3D position(x, y, z);
    StationPositions.setPosition(stationName, position);
}

/**
  \brief Gets the string out of element, if it's a text node

  If it's not a text node, then this returns an empty string
  */
QString cwPlotSauceXMLTask::extractString(QDomElement element) {
    if(!element.isNull()) {
        return element.text();
    }

    return QString();
}
