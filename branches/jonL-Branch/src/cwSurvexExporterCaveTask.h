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

    void writeCave(QTextStream& stream, cwCave* cave);

private:
    cwSurvexExporterTripTask* TripExporter;
};

#endif // CWSURVEXEXPORTERCAVETASK_H
