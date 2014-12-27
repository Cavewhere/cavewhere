/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEXEXPORTERCAVETASK_H
#define CWSURVEXEXPORTERCAVETASK_H

//Our includes
#include "cwCaveExporterTask.h"
class cwSurvexExporterTripTask;
class cwCave;

// Qt includes
#include <QTextStream>

class cwSurvexExporterCaveTask : public cwCaveExporterTask
{
    Q_OBJECT
public:
    explicit cwSurvexExporterCaveTask(QObject *parent = 0);

    bool writeCave(QTextStream& stream, cwCave* cave);

private:
    cwSurvexExporterTripTask* TripExporter;

    void fixFirstStation(QTextStream& stream, cwCave* cave);
};

#endif // CWSURVEXEXPORTERCAVETASK_H
