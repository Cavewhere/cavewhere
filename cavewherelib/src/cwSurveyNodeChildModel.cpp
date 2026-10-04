/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyNodeChildModel.h"
#include "cwSurveyNode.h"

cwSurveyNodeChildModel::cwSurveyNodeChildModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

void cwSurveyNodeChildModel::setNode(cwSurveyNode* node)
{
    if(m_node == node) {
        return;
    }

    beginResetModel();
    disconnectNode();
    m_node = node;

    if(m_node != nullptr) {
        m_connections = {
            connect(m_node, &cwSurveyNode::beginInsertNodes, this, [this](int begin, int end) {
                beginInsertRows(QModelIndex(), begin, end);
            }),
            connect(m_node, &cwSurveyNode::insertedNodes, this, [this]() {
                endInsertRows();
            }),
            connect(m_node, &cwSurveyNode::beginRemoveNodes, this, [this](int begin, int end) {
                beginRemoveRows(QModelIndex(), begin, end);
            }),
            connect(m_node, &cwSurveyNode::removedNodes, this, [this]() {
                endRemoveRows();
            }),
            //The QPointer is already null here, so rowCount() reads zero by
            //the time the reset ends.
            connect(m_node, &QObject::destroyed, this, [this]() {
                beginResetModel();
                disconnectNode();
                endResetModel();
                emit nodeChanged();
            })
        };
    }

    endResetModel();
    emit nodeChanged();
}

int cwSurveyNodeChildModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid() || m_node == nullptr) {
        return 0;
    }
    return m_node->childNodeCount();
}

QVariant cwSurveyNodeChildModel::data(const QModelIndex& index, int role) const
{
    if(index.isValid() && m_node != nullptr && role == NodeObjectRole) {
        return QVariant::fromValue(m_node->childNode(index.row()));
    }
    return QVariant();
}

QHash<int, QByteArray> cwSurveyNodeChildModel::roleNames() const
{
    return {
        {NodeObjectRole, QByteArrayLiteral("nodeObjectRole")}
    };
}

void cwSurveyNodeChildModel::disconnectNode()
{
    for(const auto& connection : std::as_const(m_connections)) {
        disconnect(connection);
    }
    m_connections.clear();
}
