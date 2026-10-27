/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALSOURCESTATUSMODEL_H
#define CWEXTERNALSOURCESTATUSMODEL_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QUuid>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/**
 * One row per external centerline attachment (cave or trip owner),
 * saying how the file that attachment was copied from compares with the
 * copy now in the project. This is the single status source
 * (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html section 3): the app banner,
 * the review list, and the trip panel's source line all read it, so they
 * can never disagree.
 *
 * Owned and populated by cwExternalCenterlineManager, which rebuilds the
 * rows wholesale whenever it re-reads the attachments from disk.
 *
 * The status describes the source on this machine only. A collaborator
 * who received the project through Git has no breadcrumb for the owner
 * and so reads NoBreadcrumb - there is no source there to compare
 * against.
 */
class CAVEWHERE_LIB_EXPORT cwExternalSourceStatusModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExternalSourceStatusModel)
    QML_UNCREATABLE("Owned by ExternalCenterlineManager")

    Q_PROPERTY(int changedCount READ changedCount NOTIFY statusesChanged FINAL)

public:
    /**
     * How an owner's source compares with the copy in the project.
     *
     * UpToDate      - the source is on disk and matches what was copied.
     * Changed       - the source is on disk and its contents have moved
     *                 on since the copy; the user can update the copy.
     * SourceMissing - this machine remembers a source that is no longer
     *                 there, so there is nothing to copy from.
     * NoBreadcrumb  - nothing to compare against: no remembered source,
     *                 or an entry recorded before fingerprints existed.
     *                 The quiet state - it never asks the user for
     *                 anything.
     */
    enum class Status {
        UpToDate,
        Changed,
        SourceMissing,
        NoBreadcrumb
    };
    Q_ENUM(Status)

    enum Roles {
        OwnerIdRole = Qt::UserRole + 1,
        SourcePathRole,
        StatusRole,
        SourceRevisionRole
    };
    Q_ENUM(Roles)

    struct Row {
        QUuid ownerId;
        QString sourcePath; // empty when the owner has no breadcrumb
        Status status = Status::NoBreadcrumb;
        // Opaque token for the contents the source has right now, carried
        // only by a Changed row (the one state where the source differs
        // from the copy and the contents were therefore read). It moves
        // whenever the source is edited again, which is what lets the app
        // banner tell a fresh edit from the one the user already dismissed
        // (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §4).
        QString sourceRevision;

        bool operator==(const Row& other) const = default;
    };

    explicit cwExternalSourceStatusModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces the row set. A rebuild that produces the same rows changes
    // nothing and emits nothing, so the surfaces bound to this model stay
    // quiet through the recomputes that find no news.
    void setRows(QVector<Row> rows);

    // This owner's status, NoBreadcrumb for an owner with no row (an
    // owner with no attachment, or one this model has not swept yet).
    // Re-read on statusesChanged.
    Q_INVOKABLE Status statusFor(const QUuid& ownerId) const;

    // The file this owner's copy came from, empty when this machine
    // remembers none.
    Q_INVOKABLE QString sourcePathFor(const QUuid& ownerId) const;

    // The token for the contents this owner's source has now, empty for
    // every status but Changed (see Row::sourceRevision).
    Q_INVOKABLE QString sourceRevisionFor(const QUuid& ownerId) const;

    // How many owners read Changed right now — what the app banner counts.
    // statusesChanged notifies.
    int changedCount() const;

    // Everything this model knows about one owner in a single read, a
    // default (NoBreadcrumb, empty) row for an owner it has none for. For
    // callers that want more than one field.
    Row rowFor(const QUuid& ownerId) const;

signals:
    // Emitted after setRows installs a row set that differs from the
    // previous one - the notification for the per-owner queries above,
    // which no model signal covers.
    void statusesChanged();

private:
    QVector<Row> m_rows;

    const Row* findRow(const QUuid& ownerId) const;
};

#endif // CWEXTERNALSOURCESTATUSMODEL_H
