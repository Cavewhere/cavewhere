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

//Qt includes
#include <QAbstractItemModel>
#include <QDateTime>
#include <QHash>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>

/**
 * The survey tree as an item model: one row per cwSurveyNode and one row per
 * cwTrip, in seven columns. A node's rows are its child nodes first, then its
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

public:
    //! The columns of the Data page's tree table. Actions holds the row's ⋯ menu.
    enum Column {
        Name,
        Kind,
        Trips,
        Length,
        Depth,
        LastSurvey,
        Actions
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
        IsReadOnlyRole,
        IsSourceRootRole,
        TripCountRole,
        LengthRole,
        DepthValueRole,
        LastSurveyRole,
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

signals:
    void regionChanged();

private:
    QPointer<cwCavingRegion> m_region;

    //! Every node and trip this model holds connections to, so a second wiring
    //! of the same object is refused and a deleted one is forgotten.
    QSet<QObject*> m_connected;

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
    void disconnectObject(QObject* object);

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

    //! The trip count and last survey of \a node and of every node above it
    //! moved, because those two fold over a whole subtree.
    void emitAggregateChanged(cwSurveyNode* node);

    //! The number of trips over \a node's subtree, its own trips included.
    static int subtreeTripCount(const cwSurveyNode* node);

    //! The latest date over \a node's subtree, invalid when it holds no trip.
    static QDateTime lastSurvey(const cwSurveyNode* node);
};

#endif // CWSURVEYTREEMODEL_H
