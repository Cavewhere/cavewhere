/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWATTACHEDCENTERLINESMODEL_H
#define CWATTACHEDCENTERLINESMODEL_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/**
 * One row per cave or trip with an attached external centerline, owned
 * and populated by cwLinePlotManager (rebuilt on every watch-set
 * recompute, i.e. on attach / detach / attachment-dir changes).
 * CavernOutputPage's attached-centerlines section binds to this.
 *
 * Rows arrive pre-sorted by (cave display name, then trip display name);
 * setRows() diffs against the current rows by ownerId so pure removals
 * emit rowsRemoved and pure insertions emit rowsInserted (reordering
 * falls back to a model reset). lastSolved survives rebuilds — it is
 * carried over by ownerId and only advanced by markSolved(), which the
 * manager calls when a solve completes successfully.
 */
class CAVEWHERE_LIB_EXPORT cwAttachedCenterlinesModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AttachedCenterlinesModel)
    QML_UNCREATABLE("Owned by LinePlotManager")

public:
    enum Roles {
        OwnerNameRole = Qt::UserRole + 1,
        OwnerKindRole,
        EntryFileRole,
        DepCountRole,
        WarningCountRole,
        LastSolvedRole
    };
    Q_ENUM(Roles)

    struct Row {
        QUuid ownerId;
        QString caveName;   // sort key only, not exposed as a role
        QString ownerName;  // cave name for cave owners, trip name for trips
        QString ownerKind;  // "Cave" or "Trip"
        QString entryFile;
        int depCount = 0;
        int warningCount = 0;
        QDateTime lastSolved;
    };

    explicit cwAttachedCenterlinesModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces the row set with `rows` (already sorted by the manager).
    // lastSolved is preserved for owners that persist across the rebuild.
    void setRows(QVector<Row> rows);

    // Stamps lastSolved on every current row. Rows attached after this
    // call start with a null lastSolved until the next solve completes.
    void markSolved(const QDateTime& when);

private:
    QVector<Row> m_rows;

    static bool rowDataEqual(const Row& left, const Row& right);

    // Copies changed row content from `rows` into m_rows and emits
    // dataChanged per changed row. Precondition: same owners, same order.
    void syncRowData(const QVector<Row>& rows);
};

#endif // CWATTACHEDCENTERLINESMODEL_H
