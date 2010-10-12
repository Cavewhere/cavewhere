//Our includes
#include "cwSurveryChunkGroup.h"
#include "cwSurveyChunk.h"

cwSurveyChunkGroup::cwSurveyChunkGroup(QObject *parent) :
    QObject(parent)
{

}

/**
  \brief Get's the number of chunks in this group
  */
int cwSurveyChunkGroup::chunkCount() const {
    return Chunks.size();
}

/**
  \brief Get's a chunk at an index
  */
cwSurveyChunk* cwSurveyChunkGroup::chunk(int index) const {
    if(index < 0 || index >= chunkCount()) { return NULL; }
    return Chunks.at(index);
}


/**
  \brief Set chunks for the group

  This will reparent all the chunks to this object
  */
void cwSurveyChunkGroup::setChucks(QList<cwSurveyChunk*> chunks) {
    Chunks = chunks;

    foreach(cwSurveyChunk* chunk, Chunks) {
        chunk->setParent(this);
        connect(chunk, SIGNAL(StationAdded()), SIGNAL(dataChanged()));
    }
}
