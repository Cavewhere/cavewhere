/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYTREEMODEL_H
#define CWSURVEYTREEMODEL_H

//Our includes
#include "cwGlobals.h"
#include "cwSurveyNode.h"
class cwCavingRegion;
class cwLength;
class cwTrip;
class cwTripStatsWatcher;

//Qt includes
#include <QAbstractItemModel>
#include <QDateTime>
#include <QHash>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>

/**
 * The survey tree as an item model: one row per cwSurveyNode and one row per
 * cwTrip, in eight columns. A node's rows are its child nodes first, then its
 * own trips; a trip row is a leaf. The region's root node owns no row of its
 * own, so its children are the model's top-level rows.
 *
 * The model reads cwCavingRegion::rootNode() and the cwSurveyNode row signals
 * directly. A node arrives with its whole subtree in one
 * beginInsertNodes/insertedNodes pair, which is exactly the contract Qt's
 * views and proxies assume of a parent row, so the rows are announced once and
 * QAbstractItemModelTester is satisfied.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyTreeModel)

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)
    Q_PROPERTY(bool moveActive READ moveActive NOTIFY moveChanged)
    Q_PROPERTY(QObject* moveSubject READ moveSubject NOTIFY moveChanged)
    Q_PROPERTY(QString moveSubjectName READ moveSubjectName NOTIFY moveChanged)
    Q_PROPERTY(bool moveRootIsTarget READ moveRootIsTarget NOTIFY moveChanged)

public:
    //! The columns of the Data page's tree table. Stations, Length, Depth and
    //! Trips fold over a node's subtree; Date and Decl belong to a trip row.
    enum Column {
        Name,
        Kind,
        Trips,
        Stations,
        Length,
        Depth,
        Date,
        Decl
    };
    Q_ENUM(Column)

    //! What a row stands for.
    enum RowType {
        Node,
        Trip
    };
    Q_ENUM(RowType)

    //! Numbered from Qt::UserRole so they cannot be read as Qt::DisplayRole or
    //! Qt::DecorationRole, which a view asks every model for.
    enum RoleItem {
        ObjectRole = Qt::UserRole + 1, //!< The cwSurveyNode or cwTrip the row stands for
        RowTypeRole,
        KindRole,
        KindLabelRole,
        IsSourcedRole,
        IsSourceRootRole,
        TripCountRole,
        StationCountRole,
        UsedStationsRole,
        LengthRole,
        DepthValueRole,
        DateRole,
        DeclinationRole,
        AutoDeclinationRole,
        MutedRole,
        NameRole
    };
    Q_ENUM(RoleItem)

    explicit cwSurveyTreeModel(QObject* parent = nullptr);
    ~cwSurveyTreeModel();

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

    //! The text a Kind is drawn as. The only place a Kind becomes words.
    static QString kindLabel(cwSurveyNode::Kind kind);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    //! The row \a object owns, invalid when it hangs outside this model's region.
    Q_INVOKABLE QModelIndex indexOf(QObject* object) const;

    //! The cwSurveyNode or cwTrip \a index stands for, nullptr for an invalid index.
    Q_INVOKABLE QObject* objectFor(const QModelIndex& index) const;

    //! True when \a index is a node row that owns a copied survey file — the
    //! row the view opens one level on insert.
    Q_INVOKABLE bool isSourceRootIndex(const QModelIndex& index) const;

    //! True when the row \a index stands for may be moved: Move to… is
    //! offered on it.
    Q_INVOKABLE bool isMovableIndex(const QModelIndex& index) const;

    //! Arms "Move to…" for the row \a index stands for, canceling any move
    //! already armed: one armed move per tree. Every row then answers
    //! isMoveTarget() until commitMove() or cancelMove().
    Q_INVOKABLE void startMove(const QModelIndex& index);
    Q_INVOKABLE void cancelMove();

    //! Moves the armed subject under the node \a targetIndex stands for, the
    //! invalid index standing for the region's root, and disarms. Returns
    //! false and keeps the move armed when the row cannot take it.
    Q_INVOKABLE bool commitMove(const QModelIndex& targetIndex);

    //! True while a move is armed and \a index is a node row that takes it.
    Q_INVOKABLE bool isMoveTarget(const QModelIndex& index) const;

    //! True while a move is armed and \a index is the moving row or a row
    //! under it.
    Q_INVOKABLE bool isMoveSource(const QModelIndex& index) const;

    //! Why the row \a index stands for cannot take the armed move, empty for
    //! a target and while no move is armed.
    Q_INVOKABLE QString moveTargetReason(const QModelIndex& index) const;

    //! The question to ask before the armed move lands under \a targetIndex:
    //! the ties it makes, the names it joins, the fixes it carries. Empty when
    //! the move is the move and nothing more.
    Q_INVOKABLE QString moveConfirmation(const QModelIndex& targetIndex) const;

    bool moveActive() const { return !m_moveSubject.isNull(); }
    QObject* moveSubject() const { return m_moveSubject; }
    QString moveSubjectName() const;
    //! True when the region's root, which owns no row, takes the armed move.
    bool moveRootIsTarget() const { return isMoveTarget(QModelIndex()); }

signals:
    void regionChanged();
    void moveChanged();

private:
    QPointer<cwCavingRegion> m_region;
    //! The node or trip an armed Move to… carries, null while none is armed.
    QPointer<QObject> m_moveSubject;
    QMetaObject::Connection m_moveSubjectDestroyed;

    //! Every node and trip this model holds connections to, so a second wiring
    //! of the same object is refused and a deleted one is forgotten.
    QSet<QObject*> m_connected;

    //! The length and used stations of each trip this model shows, keyed by the
    //! trip, so a node row folds the counts its subtree's trips already hold.
    QHash<QObject*, cwTripStatsWatcher*> m_tripStats;

    //! The node an index stands for: the region's root for the invalid index,
    //! the node itself for a node row, nullptr for a trip row.
    cwSurveyNode* nodeForIndex(const QModelIndex& index) const;

    //! The row's node, nullptr when the row is a trip or the index is invalid.
    cwSurveyNode* nodeAt(const QModelIndex& index) const;

    //! True when \a node hangs off this model's region root, so it owns a row here.
    bool isInTree(const cwSurveyNode* node) const;

    //! The row \a node's first trip takes, its child-node rows coming ahead of it.
    int firstTripRow(const cwSurveyNode* node) const;

    QModelIndex indexOfNode(cwSurveyNode* node) const;
    QModelIndex indexOfTrip(cwTrip* trip) const;

    //! Takes \a object under this model's watch, returning false when it is
    //! already watched so a second wiring of the same object is refused.
    bool trackObject(QObject* object);

    void connectSubtree(cwSurveyNode* node);
    void connectNode(cwSurveyNode* node);
    void connectTrip(cwTrip* trip);
    void disconnectSubtree(cwSurveyNode* node);
    void disconnectTrip(cwTrip* trip);
    void disconnectObject(QObject* object);

    //! Drops the statistics watcher \a object owns, if it has one.
    void removeTripStats(QObject* object);

    void beginInsertNodeRows(cwSurveyNode* parentNode, int begin, int end);
    void insertedNodeRows(cwSurveyNode* parentNode, int begin, int end);
    void beginRemoveNodeRows(cwSurveyNode* parentNode, int begin, int end);
    void beginInsertTripRows(cwSurveyNode* parentNode, int begin, int end);
    void insertedTripRows(cwSurveyNode* parentNode, int begin, int end);
    void beginRemoveTripRows(cwSurveyNode* parentNode, int begin, int end);

    //! dataChanged over columns \a first through \a last of the row \a rowIndex
    //! names. \a rowIndex is a column-zero index.
    void emitRowDataChanged(const QModelIndex& rowIndex,
                            Column first,
                            Column last,
                            const QList<int>& roles);

    //! The trip and station counts of \a node and of every node above it moved,
    //! because those two fold over a whole subtree.
    void emitAggregateChanged(cwSurveyNode* node);

    //! The number of trips over \a node's subtree, its own trips included.
    static int subtreeTripCount(const cwSurveyNode* node);

    //! The number of stations over \a node's subtree, zero for a trip whose
    //! stations have yet to be counted.
    int subtreeStationCount(const cwSurveyNode* node) const;

    //! The number of stations \a trip names, zero until its watcher counts them.
    int tripStationCount(cwTrip* trip) const;

    //! \a trip's stations as one line of abbreviated ranges, empty until its
    //! watcher names them.
    QString tripUsedStations(cwTrip* trip) const;

    //! \a trip's surveyed length, zero until its watcher adds it up.
    double tripLength(cwTrip* trip) const;
};

#endif // CWSURVEYTREEMODEL_H
