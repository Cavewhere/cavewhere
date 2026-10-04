/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWNODEPAGEMODEL_H
#define CWNODEPAGEMODEL_H

//Qt includes
#include <QDateTime>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
class cwSurveyNode;

/**
 * The figures a node page shows that the node does not carry itself: the
 * label its Kind chip reads, how many trips sit at or below the node, and the
 * date of the most recent one. Length, depth and the fix-station count are
 * the node's own properties; the page's rows (child nodes, then trips) are the
 * survey tree rooted at the node.
 *
 * The trip count and the date follow the whole subtree: a trip or node added
 * or removed at any depth, and any trip's date edit, refresh them.
 */
class CAVEWHERE_LIB_EXPORT cwNodePageModel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NodePageModel)

    Q_PROPERTY(cwSurveyNode* node READ node WRITE setNode NOTIFY nodeChanged)
    Q_PROPERTY(QString kindLabel READ kindLabel NOTIFY kindLabelChanged)
    Q_PROPERTY(int tripCount READ tripCount NOTIFY tripCountChanged)
    Q_PROPERTY(QDateTime lastSurvey READ lastSurvey NOTIFY lastSurveyChanged)

public:
    explicit cwNodePageModel(QObject* parent = nullptr);

    cwSurveyNode* node() const { return m_node; }
    void setNode(cwSurveyNode* node);

    //! The node's kind as its tree row's chip names it; empty without a node.
    QString kindLabel() const;

    int tripCount() const { return m_tripCount; }

    //! The latest valid trip date at or below the node; invalid when no trip
    //! there has a date.
    QDateTime lastSurvey() const { return m_lastSurvey; }

signals:
    void nodeChanged();
    void kindLabelChanged();
    void tripCountChanged();
    void lastSurveyChanged();

private:
    QPointer<cwSurveyNode> m_node;
    QMetaObject::Connection m_subtreeConnection;
    QMetaObject::Connection m_kindConnection;
    QMetaObject::Connection m_destroyedConnection;
    QList<QMetaObject::Connection> m_tripConnections;

    int m_tripCount = 0;
    QDateTime m_lastSurvey;

    void rewireTrips();
    void updateLastSurvey();
    void disconnectTrips();
    void setTripCount(int count);
    void setLastSurvey(const QDateTime& date);
};

#endif // CWNODEPAGEMODEL_H
