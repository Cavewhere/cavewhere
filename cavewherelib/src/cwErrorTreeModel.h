/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWERRORTREEMODEL_H
#define CWERRORTREEMODEL_H

//Qt includes
#include <QHash>
#include <QPointer>
#include <QQmlEngine>
#include <QSortFilterProxyModel>

//Our includes
#include "cwGlobals.h"
#include "cwErrorListModel.h"
class cwErrorModel;
class cwErrorTreeListModel;

/**
 * Every error under root (trip -> chunks -> cells) as one flat list, live as
 * the chunk creates and deletes cell models. Backs the trip page's ignored
 * warnings section (#174).
 *
 * Rows are the source cwErrorListModels' own rows, so setData(SuppressedRole)
 * here is the same operation as ticking the cell popup's checkbox.
 *
 * Child models that join arrive together in one queued pass; child models that
 * leave take their rows with them at once. count and kinds follow in a queued
 * pass.
 */
class CAVEWHERE_LIB_EXPORT cwErrorTreeModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ErrorTreeModel)

    Q_PROPERTY(cwErrorModel* root READ root WRITE setRoot NOTIFY rootChanged)
    Q_PROPERTY(bool ignoredOnly READ ignoredOnly WRITE setIgnoredOnly NOTIFY ignoredOnlyChanged)
    Q_PROPERTY(QString kindFilter READ kindFilter WRITE setKindFilter NOTIFY kindFilterChanged)
    Q_PROPERTY(QStringList kinds READ kinds NOTIFY kindsChanged)
    Q_PROPERTY(QStringList allKinds READ allKinds CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum class Roles : int {
        //Family label of the row's cwErrorTypeId, for grouping
        KindRole = static_cast<int>(cwErrorListModel::ErrorRoles::ErrorTypeRole) + 1,
        //The leaf cwErrorModel that owns the row, for jump-to-cell
        ErrorModelRole
    };
    Q_ENUM(Roles)

    explicit cwErrorTreeModel(QObject* parent = nullptr);

    cwErrorModel* root() const { return m_root; }
    void setRoot(cwErrorModel* root);

    bool ignoredOnly() const { return m_ignoredOnly; }
    void setIgnoredOnly(bool ignoredOnly);

    QString kindFilter() const { return m_kindFilter; }
    void setKindFilter(const QString& kindFilter);

    //! Kinds of the rows that pass ignoredOnly, rare and serious kinds first
    QStringList kinds() const { return m_kinds; }
    int count() const { return m_count; }

    //! Every kind label in display order, whether or not any row has it
    static QStringList allKinds();

    // The cwErrorTypeId values grouped under one kind label
    Q_INVOKABLE static QList<int> typeIdsForKind(const QString& kind);
    static QString kindForTypeId(int errorTypeId);

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void rootChanged();
    void ignoredOnlyChanged();
    void kindFilterChanged();
    void kindsChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    cwErrorTreeListModel* m_flat;
    QPointer<cwErrorModel> m_root;
    bool m_ignoredOnly = false;
    QString m_kindFilter;
    QStringList m_kinds;
    int m_count = 0;

    //Each tracked node and the list model it contributes to m_flat
    QHash<cwErrorModel*, QPointer<cwErrorListModel>> m_tracked;
    //Tracked nodes whose list models join m_flat in the next pass
    QList<cwErrorModel*> m_pendingAdds;
    bool m_passScheduled = false;
    bool m_summaryDirty = false;

    bool passesSuppressedFilter(const cwError& error) const;
    void track(cwErrorModel* node, QList<cwErrorModel*>* trackedNodes);
    void addChild(cwErrorModel* child);
    void untrack(cwErrorModel* node);
    void untrackSubtree(cwErrorModel* node, QList<cwErrorListModel*>* lists);
    void untrackAll();
    void forgetDestroyedNode(QObject* node);
    void clearForDestroyedRoot();
    void addPendingSources();
    void onFlatDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles);
    void markSummaryDirty();
    void schedulePass();
    void runPass();
    void refresh();
};

#endif // CWERRORTREEMODEL_H
