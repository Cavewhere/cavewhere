/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYNODECHILDMODEL_H
#define CWSURVEYNODECHILDMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
class cwSurveyNode;

/**
 * A flat list model whose rows are the direct child nodes of one survey node,
 * in the node's own order. cwSurveyNode's own rows are its trips, so a QML
 * view or Instantiator that walks a node's children reads them through this.
 *
 * The rows follow the node's insert and remove signals, so a move between
 * parents shows up as a removal here and an insertion under the new parent.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyNodeChildModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNodeChildModel)

    Q_PROPERTY(cwSurveyNode* node READ node WRITE setNode NOTIFY nodeChanged)

public:
    enum Roles {
        NodeObjectRole = Qt::UserRole + 1
    };
    Q_ENUM(Roles)

    explicit cwSurveyNodeChildModel(QObject* parent = nullptr);

    cwSurveyNode* node() const { return m_node; }
    void setNode(cwSurveyNode* node);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void nodeChanged();

private:
    QPointer<cwSurveyNode> m_node;
    QList<QMetaObject::Connection> m_connections;

    void disconnectNode();
};

#endif // CWSURVEYNODECHILDMODEL_H
