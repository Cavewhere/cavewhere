/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYCHUNKTRIMMER_H
#define CWSURVEYCHUNKTRIMMER_H

//Qt includes
#include <QObject>

//Our includes
#include "cwSurveyChunk.h"

/**
  This class expands the survey chunk to allow editting.  When
  a survey chunk is selected in the cwSurveyChunkView, the chunk should
  be set using setSurveyChunk.  Once the chunk is set, a new empty chunk
  shot and station is appended.  This class will keep a new shot and station
  at all times.  It will remove empty station to keep the chunk trimmed.

  Once the survey chunk is unset, the survey chunk will be trimmed.  All empty
  station and shot are removed.
  */
class cwSurveyChunkTrimmer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwSurveyChunk* chunk READ chunk WRITE setChunk NOTIFY chunkChanged)

public:
    explicit cwSurveyChunkTrimmer(QObject *parent = 0);
    
    cwSurveyChunk* chunk() const;
    void setChunk(cwSurveyChunk* chunk);

    static void trim(cwSurveyChunk* chunk);

signals:
    void chunkChanged();
    
public slots:

private:
    enum TrimType {
        FullTrim, //Removes all empty stations and shots
        PreserveLastEmptyOne //Removes all empty shot expend for the last empty shot
    };

    cwSurveyChunk* Chunk;

    void trim(TrimType trimType);
    static void trim(cwSurveyChunk *chunk, TrimType trimType);

    static bool isStationShotEmpty(cwSurveyChunk* chunk, int stationIndex);

private slots:
    void addLastEmptyStation();
    void chunkDestroyed();

};

Q_DECLARE_METATYPE(cwSurveyChunkTrimmer*)

inline cwSurveyChunk* cwSurveyChunkTrimmer::chunk() const {
    return Chunk;
}

#endif // CWSURVEYCHUNKTRIMMER_H
