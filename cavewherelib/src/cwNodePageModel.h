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
#include <QStringList>

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
 *
 * A node of a sourced subtree also gets what its source card shows: how many
 * derived trips (units of the survey file) sit at or below it, how many
 * stations it holds (cwSurveyTreeModel::sourcedStationCount(), the count its
 * tree row shows), and, on a source root, the names of the derived
 * nodes and trips the user hid from the project.
 */
class CAVEWHERE_LIB_EXPORT cwNodePageModel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NodePageModel)

    Q_PROPERTY(cwSurveyNode* node READ node WRITE setNode NOTIFY nodeChanged)
    Q_PROPERTY(QString kindLabel READ kindLabel NOTIFY kindLabelChanged)
    Q_PROPERTY(int tripCount READ tripCount NOTIFY tripCountChanged)
    Q_PROPERTY(QDateTime lastSurvey READ lastSurvey NOTIFY lastSurveyChanged)
    Q_PROPERTY(int stationCount READ stationCount NOTIFY stationCountChanged)
    Q_PROPERTY(QStringList hiddenNames READ hiddenNames NOTIFY hiddenNamesChanged)

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

    //! The solved stations in a sourced node's scopedPositions(), or its
    //! trips' harvested stations before a solve; 0 for a native node.
    int stationCount() const { return m_stationCount; }

    //! On a source root, one name per path its source excludes: the last
    //! segment of the hidden node's or trip's source path, its node's segment
    //! for a node's own unit. Empty for every other node.
    QStringList hiddenNames() const { return m_hiddenNames; }

signals:
    void nodeChanged();
    void kindLabelChanged();
    void tripCountChanged();
    void lastSurveyChanged();
    void stationCountChanged();
    void hiddenNamesChanged();

private:
    QPointer<cwSurveyNode> m_node;
    QMetaObject::Connection m_subtreeConnection;
    QMetaObject::Connection m_kindConnection;
    QMetaObject::Connection m_sectionConnection;
    QMetaObject::Connection m_destroyedConnection;
    QList<QMetaObject::Connection> m_sourceConnections;
    QList<QMetaObject::Connection> m_tripConnections;

    int m_tripCount = 0;
    QDateTime m_lastSurvey;
    int m_stationCount = 0;
    QStringList m_hiddenNames;

    void rewireTrips();
    void updateLastSurvey();
    void disconnectTrips();
    void setTripCount(int count);
    void setLastSurvey(const QDateTime& date);
    void rewireSource();
    void disconnectSource();
    void updateStationCount();
    void updateHiddenNames();
};

#endif // CWNODEPAGEMODEL_H
