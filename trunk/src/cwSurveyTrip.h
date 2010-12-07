#ifndef CWSURVEYTRIP_H
#define CWSURVEYTRIP_H


//Qt includes
#include <QObject>

//Our includes
class cwSurveyChunk;

class cwSurveyTrip : public QObject
{
    Q_OBJECT
public:
    explicit cwSurveyTrip(QObject *parent = 0);

    int chunkCount() const;
    int chunk(int i) const;
    void setChunk(int i, cwSurveyChunk* chunk);
    void addChunk(cwSurveyChunk* chunk);
    void insertChunk(int i, cwSurveyChunk* chunk);
    void removeChunk(int i, cwSurveyChunk* chunk);
    void clearChunks();

signals:
    void chunkChanged(int i);
    void chunksInserted(int begin, int end);
    void chunksRemoved(int begin, int end);

public slots:

private:
    QList<cwSurveyChunk*> Chunks;

};

#endif // CWSURVEYTRIP_H
