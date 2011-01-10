#ifndef CWPLOTSAUCEXMLTASK_H
#define CWPLOTSAUCEXMLTASK_H

//Our includes
#include "cwTask.h"

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

signals:
    void stationPosition(QString stationName, QVector3D position);

public slots:

protected:
    void runTask();

private:
    QString XMLFileName;

    Q_INVOKABLE void privateSetPlotSauceXMLFile(QString inputFile);

    QByteArray extractXMLData();
    void ParseXML(QByteArray xml);
    void ParseStationXML(QDomNode station);

    QString extractString(QDomElement element);



};

#endif // CWPLOTSAUCEXMLTASK_H
