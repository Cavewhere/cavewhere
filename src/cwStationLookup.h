/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTATIONLOOKUP_H
#define CWSTATIONLOOKUP_H

//Qt includes
#include <QObject>

//Our includes
class cwStationReference;

class cwStationLookup : public QObject
{
    Q_OBJECT
public:
    explicit cwStationLookup(QObject *parent = 0);

    cwStationReference* addStation(cwStation* station);
    cwStationReference* findStation();

signals:

public slots:

};

#endif // CWSTATIONLOOKUP_H
