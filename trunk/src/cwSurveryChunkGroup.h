#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

//Our includes
class cwSurveyChunk;

//Qt include
#include <QObject>
#include <QList>

class cwSurveyChunkGroup : public QObject
{
    Q_OBJECT
public:
    explicit cwSurveyChunkGroup(QObject *parent = 0);

signals:
    void dataChanged();

public slots:
    int chunkCount() const;
    cwSurveyChunk* chunk(int index) const;
    void setChucks(QList<cwSurveyChunk*> chunks);


protected:
    QList<cwSurveyChunk*> Chunks;

};

#endif // CWSURVERYCHUNKGROUP_H
