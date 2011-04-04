#ifndef CWCOMPASSEXPORTCAVETASK_H
#define CWCOMPASSEXPORTCAVETASK_H

//Our includes
#include "cwCaveExporterTask.h"
class cwCave;
class cwTrip;
class cwSurveyChunk;

class cwCompassExportCaveTask : public cwCaveExporterTask
{
    Q_OBJECT
public:
    explicit cwCompassExportCaveTask(QObject *parent = 0);

    void writeCave(QTextStream& stream, cwCave* cave);

signals:

public slots:

private:
    void writeTrip(QTextStream& stream, cwTrip* trip);
    void writeHeader(QTextStream& stream, cwTrip* trip);
    void writeData(QTextStream& stream, QString fieldName, int fieldLength, QString data);
    void writeChunk(QTextStream& stream, cwSurveyChunk* chunk);
};

#endif // CWCOMPASSEXPORTCAVETASK_H
