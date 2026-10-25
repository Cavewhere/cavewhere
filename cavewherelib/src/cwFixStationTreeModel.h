/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONTREEMODEL_H
#define CWFIXSTATIONTREEMODEL_H

//Our includes
#include "cwGlobals.h"
#include "cwSurveyNode.h"
class cwTrip;

//Qt includes
#include <QAbstractItemModel>
#include <QHash>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>

/**
 * The Fix Stations page's tree: one row for rootNode, under it a row per
 * descendant node in tree order, and under each node row that node's
 * fixStationTable() rows (its fix rows), then a trip row per derived trip that
 * holds attached fixes (a leaf Survex block's, cwTrip::attachedFixes()), in
 * the node's trip order and each over its own fix rows, then its child node
 * rows.
 *
 * Fix rows forward the underlying table's roles by name, and setData() on one
 * reaches the model that owns the row. Node and trip rows answer the Roles
 * below; a trip row is read-only and shows the system its node inherits.
 *
 * Every fix row also has a page row (FixRowRole): its place among all the fix
 * rows in depth-first order. rootNode's own rows come first, so a page row
 * below rootNode's fixStationCount() is the same row of rootNode's table.
 */
class CAVEWHERE_LIB_EXPORT cwFixStationTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FixStationTreeModel)

    Q_PROPERTY(cwSurveyNode* rootNode READ rootNode WRITE setRootNode NOTIFY rootNodeChanged)
    Q_PROPERTY(int fixRowCount READ fixRowCount NOTIFY fixRowCountChanged)

public:
    //! What a row stands for.
    enum RowKind {
        NodeRow = 0,
        FixRow = 1,
        TripRow = 2
    };
    Q_ENUM(RowKind)

    //! The delegate that draws a row (DelegateKindRole).
    enum DelegateKind {
        NodeDelegate = 0,
        TripDelegate = 1,
        EditableFix = 2,
        ReadOnlyFix = 3
    };
    Q_ENUM(DelegateKind)

    //! Numbered clear of the fix table's role blocks (cwFixStationModel's
    //! Qt::UserRole + 1…, cwFixStationDiagnosticsModel's + 100…,
    //! cwAttachedFixModel's + 200…), which fix rows forward.
    enum Roles {
        RowKindRole = Qt::UserRole + 500,
        NodeNameRole,
        NodeIdRole,
        NodeKindRole,
        OwnFixCountRole,
        SubtreeFixCountRole,
        SubtreeBareFixCountRole,
        IsAttachmentOwnerRole,
        //! The owner's cwExternalCenterline::fixCoordinateSystem; writable on
        //! an attachment owner's row.
        FixCoordinateSystemRole,
        InheritedFixCoordinateSystemRole,
        InheritedFromNodeIdRole,
        //! The cwSurveyNode a node row stands for, or whose table a fix row
        //! comes from; a trip row's node for the trip row and its fix rows.
        NodeObjectRole,
        //! The words a node row's kind is drawn as (cwSurveyTreeModel::kindLabel).
        NodeKindLabelRole,
        //! The fixes with no coordinate system the node's own attachment holds
        //! anywhere under it; zero for a node that attaches nothing.
        AttachmentBareFixCountRole,
        //! The nearest attachment owner above a node row, null for none; for a
        //! trip row, its node when that owns one, else the nearest above.
        InheritedFromNodeRole,
        //! A fix row's page row; -1 on a node or trip row.
        FixRowRole,
        //! The attached fixes under a node row that the solve reads in no
        //! coordinate system (cwAttachedFix::readInSystem is empty), the ones
        //! warning 603 is about.
        SubtreeUnsetFixCountRole,
        //! The cwTrip a trip row stands for, or whose fixes a fix row lists;
        //! null on every other row.
        TripObjectRole,
        //! On a node row, the node's trips, in trip order, whose own
        //! attachment lists a fix that names no coordinate system; each
        //! chooses the system those fixes read in. Empty on every other row.
        BareFixOwnerTripsRole,
        //! The delegate that draws the row: a node row, a trip row, one of a
        //! node's own editable fixes, or a fix an attached file holds.
        DelegateKindRole,
        //! True when the user may add a fix to the row's node
        //! (cwSurveyNode::canHoldFixes): a node row's own node, a fix row's
        //! owning node; false on a trip row.
        CanHoldFixesRole
    };
    Q_ENUM(Roles)

    explicit cwFixStationTreeModel(QObject* parent = nullptr);

    cwSurveyNode* rootNode() const { return m_rootNode; }
    void setRootNode(cwSurveyNode* rootNode);

    //! The number of fix rows in the whole tree.
    int fixRowCount() const;

    //! The fix row at page row \a fixRow, invalid when there is none.
    Q_INVOKABLE QModelIndex indexOfFixRow(int fixRow) const;

    //! The row \a node owns here, invalid when it hangs outside rootNode.
    Q_INVOKABLE QModelIndex nodeRowIndex(cwSurveyNode* node) const;

    //! The node Add Fix puts a new fix on when \a index is the current row: a
    //! node row's own node, a fix or trip row's node, rootNode for no row.
    Q_INVOKABLE cwSurveyNode* addTargetAt(const QModelIndex& index) const;

    //! Adds a fix to addTargetAt(\a index) and returns its page row, or -1
    //! with nothing added when that node cannot hold fixes.
    Q_INVOKABLE int addFixAt(const QModelIndex& index);

    //! Announces every role of every fix row. A TreeView that reuses its
    //! delegates applies a dataChanged that arrives between a row insert or
    //! removal and its next layout to the delegate the row left, so the view
    //! calls this once it has laid the new rows out.
    Q_INVOKABLE void announceFixRows();

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void rootNodeChanged();
    void fixRowCountChanged();

private:
    QPointer<cwSurveyNode> m_rootNode;

    //! The nodes and trips this model holds connections to.
    QSet<cwSurveyNode*> m_connected;
    QSet<cwTrip*> m_connectedTrips;

    //! Per node, the derived trips that have a row, in the node's trip order,
    //! and per such trip the fix rows its row holds. Kept here rather than read
    //! off the trips, so the rows change only between a begin/end pair.
    QHash<const cwSurveyNode*, QList<cwTrip*>> m_tripRows;
    QHash<const cwTrip*, int> m_tripFixRows;

    //! Each node's and trip row's first page row, and the page row count,
    //! rebuilt on demand after the rows move.
    mutable QHash<const QObject*, int> m_firstFixRow;
    mutable int m_fixRowCount = 0;
    mutable bool m_fixRowsDirty = true;

    //! The fix rows on their way out, between a table's rowsAboutToBeRemoved
    //! and rowsRemoved, or every fix row of the nodes and trip rows leaving the
    //! tree. They have no page row, so a view row still drawing one gives up
    //! its page row before it goes, the way a ListView row's index turns -1.
    struct LeavingRows {
        const QObject* holder = nullptr;
        int first = -1;
        int last = -1;
    };
    LeavingRows m_leavingRows;
    QSet<const QObject*> m_leavingHolders;

    //! Each connected node's BareFixOwnerTripsRole, kept so the role is
    //! announced only when the list changes.
    QHash<const cwSurveyNode*, QList<cwTrip*>> m_bareFixOwnerTrips;

    bool isLeaving(const QObject* holder, int row) const;

    //! Announces \a roles on \a holder's fix rows \a first through \a last.
    void emitFixRowChanged(const QObject* holder, int first, int last,
                           const QList<int>& roles = {FixRowRole});

    //! The node or trip whose rows hold \a index, nullptr for the top-level row.
    static QObject* holderOf(const QModelIndex& index);

    //! The node a node row stands for, nullptr for any other row.
    cwSurveyNode* nodeAt(const QModelIndex& index) const;

    //! The trip a trip row stands for, nullptr for any other row.
    cwTrip* tripAt(const QModelIndex& index) const;

    //! True when \a index is one of a node's or trip row's fix rows.
    bool isFixRow(const QModelIndex& index) const;

    //! How many fix rows \a holder, a node or a trip with a row, lists.
    int fixRowsOf(const QObject* holder) const;

    //! The number of trip rows under \a node.
    int tripRowCount(const cwSurveyNode* node) const;

    //! The row \a node owns here, invalid when it hangs outside rootNode.
    QModelIndex indexOfNode(const cwSurveyNode* node) const;

    //! The row \a trip owns here, invalid when it has none.
    QModelIndex indexOfTrip(const cwTrip* trip) const;

    //! \a holder's fix row \a row as this model's index.
    QModelIndex fixIndex(const QObject* holder, int row) const;

    //! True when \a node is rootNode or hangs under it.
    bool isInTree(const cwSurveyNode* node) const;

    void connectSubtree(cwSurveyNode* node);
    void connectNode(cwSurveyNode* node);
    void connectTrip(cwSurveyNode* node, cwTrip* trip);
    void disconnectSubtree(cwSurveyNode* node);
    void disconnectAll();
    void disconnectNode(cwSurveyNode* node);
    void disconnectTrip(cwTrip* trip);

    //! Gives \a trip a row under \a node, holding \a fixRows fix rows.
    void insertTripRow(cwSurveyNode* node, cwTrip* trip, int fixRows);
    //! Takes \a trip's row out from under \a node, when it has one.
    void removeTripRow(const cwSurveyNode* node, cwTrip* trip);
    //! \a trip's fixes are about to be replaced: its fix rows go.
    void tripFixesAboutToReset(cwTrip* trip);
    //! \a trip's fixes were replaced: its rows follow them.
    void tripFixesReset(cwSurveyNode* node, cwTrip* trip);
    //! Forgets the trip rows of the nodes that left the tree.
    void forgetLeavingNodes();

    void rebuildFixRows() const;

    //! The rows moved: page rows and the count are recounted, and every fix
    //! row's FixRowRole is announced.
    void fixRowsMoved();

    //! \a node's fix counts moved, and so did every node's above it.
    void emitCountsChanged(const cwSurveyNode* node);

    //! The attachment roles of \a node and every node and trip row under it moved.
    void emitAttachmentChanged(const cwSurveyNode* node);

    void emitNodeRowChanged(const cwSurveyNode* node, const QList<int>& roles);

    //! Recounts \a node's bare-fix owner trips, announcing them when they changed.
    void refreshBareFixOwnerTrips(const cwSurveyNode* node);
    //! Stores \a owners as \a node's bare-fix owner trips, announcing them
    //! when they differ from the ones stored.
    void setBareFixOwnerTrips(const cwSurveyNode* node, const QList<cwTrip*>& owners);
    static QList<cwTrip*> bareFixOwnerTrips(const cwSurveyNode* node);
    void emitTripRowChanged(const cwTrip* trip, const QList<int>& roles);

    static int subtreeBareFixCount(const cwSurveyNode* node);
    static int subtreeUnsetFixCount(const cwSurveyNode* node);
    static int attachmentBareFixCount(const cwSurveyNode* owner);

    //! The nearest node above \a node with an attachment of its own.
    static cwSurveyNode* attachmentOwnerAbove(const cwSurveyNode* node);
};

#endif // CWFIXSTATIONTREEMODEL_H
