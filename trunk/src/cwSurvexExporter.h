#ifndef CWSURVEXEXPORTER_H
#define CWSURVEXEXPORTER_H

//Qt includes
#include <QObject>
class QTextStream;

//Our includes
class cwSurveyTrip;
class cwSurveyChunk;

class cwSurvexExporter : public QObject
{
    Q_OBJECT
public:
    explicit cwSurvexExporter(QObject *parent = 0);

    void setChunks(cwSurveyTrip* Chunks);

signals:

public slots:
    void exportSurvex(QString filename);

private:
    cwSurveyTrip* ChunkGroup;

    void writeChunk(QTextStream& stream, cwSurveyChunk* chunk);

};

#endif // CWSURVEXEXPORTER_H
