/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTREEIMPORTDATANODE_H
#define CWTREEIMPORTDATANODE_H

//Our includes
class cwSurveyChunk;
class cwShot;
class cwTeam;
class cwTripCalibration;
#include "cwStation.h"
#include "cwSurvexLRUDChunk.h"
#include "cwGlobals.h"

//Qt includes
#include <QList>
#include <QStringList>
#include <QString>
#include <QObject>
#include <QDate>


class CAVEWHERE_LIB_EXPORT cwTreeImportDataNode : public QObject
{
    Q_OBJECT

    friend class cwSurvexImporter;
    friend class cwSurvexGlobalData;
    friend class cwWallsImporter;
    friend class cwTreeImportData;

public:
    enum ImportType {
        NoImport, //!< Don't import this block
        Cave, //!< This block is a cave
        Trip, //!< This block is a trip
        Structure //!< This is neither a Cave nor a Trip, but a imported survex block
    };

    cwTreeImportDataNode(QObject* parent = 0);

    void clear();

    int childNodeCount();
    cwTreeImportDataNode* childNode(int index);

    int chunkCount();
    cwSurveyChunk* chunk(int index);

    cwTreeImportDataNode* parentNode() const;

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
    QList<cwTreeImportDataNode*> childNodes();

    int stationCount() const;
    cwStation station(int index) const;

    int shotCount() const;
//    cwShot shot(int index) const;
    cwSurveyChunk* parentChunkOfShot(int shotIndex) const;
    int chunkShotIndex(int shotIndex) const;

    void setIncludeDistance(bool includeLength);
    bool isDistanceInclude() const;

signals:
    void nameChanged();
    void importTypeChanged();

private:
    QList<cwSurveyChunk*> Chunks;
    QList<cwTreeImportDataNode*> ChildNodes;
    cwTreeImportDataNode* ParentNode;

    //Mutible elements
    QString Name;
    ImportType Type;

    QDate Date;
    cwTeam* Team;
    cwTripCalibration* Calibration;

    bool IncludeDistance;

    void addChildNode(cwTreeImportDataNode* blockData);
    void addChunk(cwSurveyChunk* chunk);

    void setParentNode(cwTreeImportDataNode* parentNode);

    bool isTrip() const;
};

/**
  \brief Gets the number of child blocks
  */
inline int cwTreeImportDataNode::childNodeCount() {
    return ChildNodes.size();
}

/**
  \brief Get's the number of chunks
  */
inline int cwTreeImportDataNode::chunkCount() {
    return Chunks.size();
}

/**
  \brief Get's the name of the block
  */
inline QString cwTreeImportDataNode::name() const {
    return Name;
}

/**
  \brief Get's all the chunks held by the block
  */
inline QList<cwSurveyChunk*> cwTreeImportDataNode::chunks() {
    return Chunks;
}

/**
  \brief Get's all the child blocks held by the this block
  */
inline QList<cwTreeImportDataNode*> cwTreeImportDataNode::childNodes() {
    return ChildNodes;
}

/**
  \brief Get's the parent block
  */
inline cwTreeImportDataNode* cwTreeImportDataNode::parentNode() const {
    return ParentNode;
}

/**
  \brief Set's the parent block
  */
inline void cwTreeImportDataNode::setParentNode(cwTreeImportDataNode* parentBlock) {
    ParentNode = parentBlock;
}

inline cwTreeImportDataNode::ImportType cwTreeImportDataNode::importType() const {
    return Type;
}

/**
  Sets the date for the block
  */
inline void cwTreeImportDataNode::setDate(QDate date) {
    Date = date;
}

/**
  Gets the date for the block
  */
inline QDate cwTreeImportDataNode::date() const {
    return Date;
}

/**
  Gets the survey team for the block
  */
inline cwTeam* cwTreeImportDataNode::team() const {
    return Team;
}

/**
  Gets the survey's calibration
  */
inline cwTripCalibration* cwTreeImportDataNode::calibration() const {
    return Calibration;
}

/**
 * @brief cwSurvexBlockData::setIncludeDistance
 * @param includeLength
 *
 * If false the distance shot length will be excluded
 */
inline void cwTreeImportDataNode::setIncludeDistance(bool includeLength)
{
    IncludeDistance = includeLength;
}

/**
 * @brief cwSurvexBlockData::isDistanceInclude
 * @return True if the distance is include, and false, if it's not
 */
inline bool cwTreeImportDataNode::isDistanceInclude() const
{
    return IncludeDistance;
}


#endif // CWTREEIMPORTDATANODE_H
