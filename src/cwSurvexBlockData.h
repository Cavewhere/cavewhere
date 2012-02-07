#ifndef CWSURVEXBLOCKDATA_H
#define CWSURVEXBLOCKDATA_H

//Our includes
class cwSurveyChunk;
class cwShot;
class cwTeam;
class cwTripCalibration;
#include "cwStation.h"

//Qt includes
#include <QList>
#include <QObject>
#include <QDate>


class cwSurvexBlockData : public QObject
{
    Q_OBJECT

    friend class cwSurvexImporter;
    friend class cwSurvexGlobalData;

public:
    enum ImportType {
        NoImport, //!< Don't import this block
        Cave, //!< This block is a cave
        Trip, //!< This block is a trip
        Structure //!< This is neither a Cave nor a Trip, but a imported survex block
    };

    cwSurvexBlockData(QObject* parent = 0);

    int childBlockCount();
    cwSurvexBlockData* childBlock(int index);

    int chunkCount();
    cwSurveyChunk* chunk(int index);

    cwSurvexBlockData* parentBlock() const;

    void setName(QString name);
    QString name() const;

    void setDate(QDate date);
    QDate date() const;

    cwTeam* team() const;
    cwTripCalibration* calibration() const;

    void setImportType(ImportType type);
    ImportType importType() const;
    static QString importTypeToString(ImportType type);

    QList<cwSurveyChunk*> chunks();
    QList<cwSurvexBlockData*> childBlocks();

    int stationCount() const;
    cwStation station(int index) const;

    int shotCount() const;
//    cwShot shot(int index) const;
    cwSurveyChunk* parentChunkOfShot(int shotIndex) const;
    int chunkShotIndex(int shotIndex) const;

signals:
    void nameChanged();
    void importTypeChanged();

private:
    QList<cwSurveyChunk*> Chunks;
    QList<cwSurvexBlockData*> ChildBlocks;
    cwSurvexBlockData* ParentBlock;

    //Mutible elements
    QString Name;
    ImportType Type;

    QDate Date;
    cwTeam* Team;
    cwTripCalibration* Calibration;

    void addChildBlock(cwSurvexBlockData* blockData);
    void addChunk(cwSurveyChunk* chunk);

    void setParentBlock(cwSurvexBlockData* parentBlock);

    void clear();

    bool isTrip() const;
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

inline cwSurvexBlockData::ImportType cwSurvexBlockData::importType() const {
    return Type;
}

/**
  Sets the date for the block
  */
inline void cwSurvexBlockData::setDate(QDate date) {
    Date = date;
}

/**
  Gets the date for the block
  */
inline QDate cwSurvexBlockData::date() const {
    return Date;
}

/**
  Gets the survey team for the block
  */
inline cwTeam* cwSurvexBlockData::team() const {
    return Team;
}

/**
  Gets the survey's calibration
  */
inline cwTripCalibration* cwSurvexBlockData::calibration() const {
    return Calibration;
}


#endif // CWSURVEXBLOCKDATA_H
