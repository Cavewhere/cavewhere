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

//Qt includes
#include <QAbstractItemModel>
#include <QHash>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>

/**
 * The Fix Stations page's tree: one row for rootNode, under it a row per
 * descendant node in tree order, and under each node row that node's
 * fixStationTable() rows (its fix rows) ahead of its child node rows.
 *
 * Fix rows forward the underlying table's roles by name, and setData() on one
 * reaches the model that owns the row. Node rows answer the Roles below.
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
        FixRow = 1
    };
    Q_ENUM(RowKind)

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
        //! comes from.
        NodeObjectRole,
        //! The words a node row's kind is drawn as (cwSurveyTreeModel::kindLabel).
        NodeKindLabelRole,
        //! The fixes with no coordinate system the node's own attachment holds
        //! anywhere under it; zero for a node that attaches nothing.
        AttachmentBareFixCountRole,
        //! The nearest attachment owner above a node row, null for none.
        InheritedFromNodeRole,
        //! A fix row's page row; -1 on a node row.
        FixRowRole,
        //! The attached fixes under a node row that the solve reads in no
        //! coordinate system (cwAttachedFix::readInSystem is empty), the ones
        //! warning 603 is about.
        SubtreeUnsetFixCountRole
    };
    Q_ENUM(Roles)

    explicit cwFixStationTreeModel(QObject* parent = nullptr);

    cwSurveyNode* rootNode() const { return m_rootNode; }
    void setRootNode(cwSurveyNode* rootNode);

    //! The number of fix rows in the whole tree.
    int fixRowCount() const;

    //! The fix row at page row \a fixRow, invalid when there is none.
    Q_INVOKABLE QModelIndex indexOfFixRow(int fixRow) const;

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

    //! The nodes this model holds connections to.
    QSet<cwSurveyNode*> m_connected;

    //! Each node's first page row, and the page row count, rebuilt on demand
    //! after the rows move.
    mutable QHash<const cwSurveyNode*, int> m_firstFixRow;
    mutable int m_fixRowCount = 0;
    mutable bool m_fixRowsDirty = true;

    //! The fix rows on their way out, between a table's rowsAboutToBeRemoved
    //! and rowsRemoved, or every fix row of the nodes leaving the tree. They
    //! have no page row, so a view row still drawing one gives up its page
    //! row before it goes, the way a ListView row's index turns -1.
    struct LeavingRows {
        const cwSurveyNode* node = nullptr;
        int first = -1;
        int last = -1;
    };
    LeavingRows m_leavingRows;
    QSet<const cwSurveyNode*> m_leavingNodes;

    bool isLeaving(const cwSurveyNode* holder, int row) const;

    //! Announces FixRowRole on \a node's fix rows \a first through \a last.
    void emitFixRowChanged(const cwSurveyNode* node, int first, int last);

    //! The node whose rows hold \a index: its parent node row's node, nullptr
    //! for the top-level row.
    static cwSurveyNode* holderOf(const QModelIndex& index);

    //! The node a node row stands for, nullptr for a fix row or an invalid index.
    cwSurveyNode* nodeAt(const QModelIndex& index) const;

    //! True when \a index is one of a node's fix rows.
    static bool isFixRow(const QModelIndex& index);

    //! The row \a node owns here, invalid when it hangs outside rootNode.
    QModelIndex indexOfNode(const cwSurveyNode* node) const;

    //! \a node's fixStationTable() row \a row as this model's index.
    QModelIndex fixIndex(const cwSurveyNode* node, int row) const;

    //! True when \a node is rootNode or hangs under it.
    bool isInTree(const cwSurveyNode* node) const;

    void connectSubtree(cwSurveyNode* node);
    void connectNode(cwSurveyNode* node);
    void disconnectSubtree(cwSurveyNode* node);
    void disconnectAll();
    void disconnectNode(cwSurveyNode* node);

    void rebuildFixRows() const;

    //! The rows moved: page rows and the count are recounted, and every fix
    //! row's FixRowRole is announced.
    void fixRowsMoved();

    //! \a node's fix counts moved, and so did every node's above it.
    void emitCountsChanged(const cwSurveyNode* node);

    //! The attachment roles of \a node and every node under it moved.
    void emitAttachmentChanged(const cwSurveyNode* node);

    void emitNodeRowChanged(const cwSurveyNode* node, const QList<int>& roles);

    static int subtreeFixCount(const cwSurveyNode* node);
    static int subtreeBareFixCount(const cwSurveyNode* node);
    static int subtreeUnsetFixCount(const cwSurveyNode* node);
    static int attachmentBareFixCount(const cwSurveyNode* owner);

    //! The nearest node above \a node with an attachment of its own.
    static cwSurveyNode* attachmentOwnerAbove(const cwSurveyNode* node);
};

#endif // CWFIXSTATIONTREEMODEL_H
