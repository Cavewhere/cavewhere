#ifndef CWSURVEXEXPORTER_H
#define CWSURVEXEXPORTER_H

//Qt includes
#include <QObject>
class QTextStream;

//Our includes
class cwSurveyChunkGroup;
class cwSurveyChunk;

class cwSurvexExporter : public QObject
{
    Q_OBJECT
public:
    explicit cwSurvexExporter(QObject *parent = 0);

    void setChunks(cwSurveyChunkGroup* Chunks);

signals:

public slots:
    void exportSurvex(QString filename);

private:
    cwSurveyChunkGroup* ChunkGroup;

    void writeChunk(QTextStream& stream, cwSurveyChunk* chunk);

};

#endif // CWSURVEXEXPORTER_H
