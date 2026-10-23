/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWERRORTREEMODEL_H
#define CWERRORTREEMODEL_H

//Qt includes
#include <QSortFilterProxyModel>
#include <QPointer>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwErrorListModel.h"
class cwErrorModel;

/**
 * Every error under root (trip -> chunks -> cells) as one flat list, live as
 * the chunk creates and deletes cell models. Backs the trip page's accepted
 * warnings section (#174).
 */
class CAVEWHERE_LIB_EXPORT cwErrorTreeModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ErrorTreeModel)

    Q_PROPERTY(cwErrorModel* root READ root WRITE setRoot NOTIFY rootChanged)
    Q_PROPERTY(bool suppressedOnly READ suppressedOnly WRITE setSuppressedOnly NOTIFY suppressedOnlyChanged)
    Q_PROPERTY(QString kindFilter READ kindFilter WRITE setKindFilter NOTIFY kindFilterChanged)
    Q_PROPERTY(QStringList kinds READ kinds NOTIFY kindsChanged)
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

    QStringList kinds() const;
    int count() const;

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void rootChanged();
    void suppressedOnlyChanged();
    void kindFilterChanged();
    void kindsChanged();
    void countChanged();

private:
    QPointer<cwErrorModel> m_root;
    bool m_suppressedOnly = false;
    QString m_kindFilter;
};

#endif // CWERRORTREEMODEL_H
