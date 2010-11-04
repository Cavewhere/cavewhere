//Our includes
#include "cwSurveyChunkGroup.h"
#include "cwSurveyChunk.h"

cwSurveyChunkGroup::cwSurveyChunkGroup(QObject *parent) :
    QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[ChunkRole] = "chunk";
    setRoleNames(roles);
}

int cwSurveyChunkGroup::rowCount(const QModelIndex &/*parent*/) const {
    return Chunks.size();
}

QVariant cwSurveyChunkGroup::data(const QModelIndex &index, int role) const {
    if(!index.isValid()) { return QVariant(); }
    if(role == ChunkRole) {
        return QVariant::fromValue(qobject_cast<QObject*>(Chunks[index.row()]));
    }
    return QVariant();
}

/**
  \brief Set chunks for the group

  This will reparent all the chunks to this object
  */
void cwSurveyChunkGroup::setChucks(QList<cwSurveyChunk*> chunks) {
    beginResetModel();

    Chunks = chunks;

    foreach(cwSurveyChunk* chunk, Chunks) {
        chunk->setParent(this);
    }

    endResetModel();
}
