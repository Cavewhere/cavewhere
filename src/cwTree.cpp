/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyTrip.h"
#include "cwSurveyChunk.h"

cwTrip::cwTrip(QObject *parent) :
    QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles.reserve(1);
    roles[ChunkRole] = "chunk";
    setRoleNames(roles);
}

/**
  \brief Set's the name of the survey trip
  */
void cwTrip::setName(QString name) {
    if(name != Name) {
        Name = name;
        emit nameChanged(Name);
    }
}


int cwTrip::rowCount(const QModelIndex &/*parent*/) const {
    return Chunks.size();
}

QVariant cwTrip::data(const QModelIndex &index, int role) const {
    if(!index.isValid()) { return QVariant(); }
    if(role == ChunkRole) {
        return QVariant::fromValue(qobject_cast<QObject*>(Chunks[index.row()]));
    }
    return QVariant();
}

/**
  \brief Insert rows to a chunk group

  This is reimplimentation of the model in the model view frame work

  Rows must always be added to the root item
  */
bool cwTrip::insertRows ( int row, int count, const QModelIndex & parent) {
    if(parent != QModelIndex()) { return false; }
    if(row < 0 || row > rowCount()) { return false; }

    beginInsertRows(parent, row, row + count - 1);
    for(int i = 0; i < count; i++) {
        cwSurveyChunk* newChunk = new cwSurveyChunk(this);
        newChunk->AppendNewShot();

        Chunks.insert(row, newChunk);
    }
    endInsertRows();
    return true;
}

/**
  \brief Insert the chunk at row

  The chunk must be valid
  */
void cwTrip::insertRow(int row, cwSurveyChunk* chunk) {
    if(chunk == NULL) { return; }
    if(row < 0) { row = 0; }
    if(row > rowCount()) { row = rowCount(); }

    //Make this own the chunk
    chunk->setParent(this);

    beginInsertRows(QModelIndex(), row, row);
    Chunks.insert(row, chunk);
    endInsertRows();
}

/**
  \brief Adds the chunk to the trip
  */
 void cwTrip::addChunk(cwSurveyChunk* chunk) {
     insertRow(rowCount(), chunk);
 }

/**
  \brief Set chunks for the group

  This will reparent all the chunks to this object
  */
void cwTrip::setChucks(QList<cwSurveyChunk*> chunks) {
    beginResetModel();

    Chunks = chunks;

    foreach(cwSurveyChunk* chunk, Chunks) {
        chunk->setParent(this);
    }

    endResetModel();
}

/**
  \brief Gets all the chunks
  */
QList<cwSurveyChunk*> cwTrip::chunks() const {
    return Chunks;
}

