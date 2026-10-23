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
class QConcatenateTablesProxyModel;

//Our includes
#include "cwGlobals.h"
#include "cwErrorListModel.h"
class cwErrorModel;

/**
 * Every error under root (trip -> chunks -> cells) as one flat list, live as
 * the chunk creates and deletes cell models. Backs the trip page's accepted
 * warnings section (#174).
 *
 * Rows are the source cwErrorListModels' own rows, so setData(SuppressedRole)
 * here is the same operation as ticking the cell popup's checkbox.
 */
class CAVEWHERE_LIB_EXPORT cwErrorTreeModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ErrorTreeModel)

    Q_PROPERTY(cwErrorModel* root READ root WRITE setRoot NOTIFY rootChanged)
    Q_PROPERTY(bool suppressedOnly READ suppressedOnly WRITE setSuppressedOnly NOTIFY suppressedOnlyChanged)
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

    bool suppressedOnly() const { return m_suppressedOnly; }
    void setSuppressedOnly(bool suppressedOnly);

    QString kindFilter() const { return m_kindFilter; }
    void setKindFilter(const QString& kindFilter);

    //! Kinds of the rows that pass suppressedOnly, rare and serious kinds first
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
    void suppressedOnlyChanged();
    void kindFilterChanged();
    void kindsChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QConcatenateTablesProxyModel* m_concat;
    QPointer<cwErrorModel> m_root;
    bool m_suppressedOnly = false;
    QString m_kindFilter;
    QStringList m_kinds;
    int m_count = 0;

    //Each tracked node and the list model it contributed to m_concat
    QHash<cwErrorModel*, QPointer<cwErrorListModel>> m_tracked;

    bool passesSuppressedFilter(const QModelIndex& concatIndex) const;
    void track(cwErrorModel* node);
    void untrack(cwErrorModel* node);
    void untrackAll();
    void forgetDestroyedNode(QObject* node);
    void removeSourceOf(cwErrorModel* node);
    void refresh();
};

#endif // CWERRORTREEMODEL_H
