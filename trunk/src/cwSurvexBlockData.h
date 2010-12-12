#ifndef CWSURVEXBLOCKDATA_H
#define CWSURVEXBLOCKDATA_H

//Our includes
class cwSurveyChunk;
class cwStation;
class cwShot;

//Qt includes
#include <QList>
#include <QObject>


class cwSurvexBlockData : public QObject
{
    Q_OBJECT

    friend class cwSurvexImporter;
    friend class cwSurvexGlobalData;

public:
    cwSurvexBlockData(QObject* parent = 0);

    int childBlockCount();
    cwSurvexBlockData* childBlock(int index);

    int chunkCount();
    cwSurveyChunk* chunk(int index);

    cwSurvexBlockData* parentBlock() const;

    QString name() const;

    QList<cwSurveyChunk*> chunks();
    QList<cwSurvexBlockData*> childBlocks();

    int stationCount();
    cwStation* station(int index);

    int shotCount();
    cwShot* shot(int index);

private:
    QList<cwSurveyChunk*> Chunks;
    QList<cwSurvexBlockData*> ChildBlocks;
    cwSurvexBlockData* ParentBlock;
    QString Name;

    void setBlockName(QString name);
    void addChildBlock(cwSurvexBlockData* blockData);
    void addChunk(cwSurveyChunk* chunk);

    void setParentBlock(cwSurvexBlockData* parentBlock);

    void clear();



};

/**
  \brief Gets the number of child blocks
  */
inline int cwSurvexBlockData::childBlockCount() {
    return ChildBlocks.size();
}

/**
  \brief Get's the number of chunks
  */
inline int cwSurvexBlockData::chunkCount() {
    return Chunks.size();
}

/**
  \brief Get's the name of the block
  */
inline QString cwSurvexBlockData::name() const {
    return Name;
}

/**
  \brief Get's all the chunks held by the block
  */
inline QList<cwSurveyChunk*> cwSurvexBlockData::chunks() {
    return Chunks;
}

/**
  \brief Get's all the child blocks held by the this block
  */
inline QList<cwSurvexBlockData*> cwSurvexBlockData::childBlocks() {
    return ChildBlocks;
}

/**
  \brief Get's the parent block
  */
inline cwSurvexBlockData* cwSurvexBlockData::parentBlock() const {
    return ParentBlock;
}

/**
  \brief Set's the parent block
  */
inline void cwSurvexBlockData::setParentBlock(cwSurvexBlockData* parentBlock) {
    ParentBlock = parentBlock;
}


#endif // CWSURVEXBLOCKDATA_H
