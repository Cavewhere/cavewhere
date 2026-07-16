/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALSOURCESETTINGS_H
#define CWEXTERNALSOURCESETTINGS_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUuid>

/**
 * Per-user, per-machine external-centerline source paths for
 * live-link attachments, keyed by owner UUID (cwCave::id() /
 * cwTrip::id()). One store serves every project on the machine -
 * owner UUIDs are globally unique, so there is no per-project
 * namespace and entries survive bundled .cw open/close cycles (the
 * extraction dir is a fresh temp path every session). Copies of a
 * project (Git clone, Save As, restored backup) preserve owner UUIDs
 * and therefore share live-link entries on this machine - intended v1
 * behavior, see master plan section 5.3. Entries whose owner no longer
 * exists in any project are harmless (a lookup that never matches) and
 * are not GC'd.
 *
 * Backed by QSettings - the same store as the rest of CaveWhere's
 * per-machine state - with one key per owner under the
 * "externalCenterlineSources" group. QSettings serializes concurrent
 * writers with a lock file and merges per-key on sync, so two
 * CaveWhere instances upserting different owners both persist; a
 * hand-rolled settings file would be whole-file last-writer-wins.
 * Every read and write goes straight to QSettings (no in-memory
 * copy), so all instances in a process agree. The change signal fires
 * only on the instance that performed the mutation.
 *
 * Owned by cwRootData; consumers (cwLinePlotManager, the attach
 * orchestrator) receive the pointer and observe
 * externalCenterlineSourcesChanged.
 */
class CAVEWHERE_LIB_EXPORT cwExternalSourceSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExternalSourceSettings)
    QML_UNCREATABLE("ExternalSourceSettings is owned by RootData and can't be created directly")

public:
    /**
     * Per-machine source path for a single Attached / Scope
     * external-centerline. ownerId is the cwCave or cwTrip QUuid
     * (matches cwCave::id() / cwTrip::id()). Empty sourcePath means
     * "import mode" - the user attached without live-link.
     */
    struct ExternalCenterlineSource {
        QUuid ownerId;
        QString sourcePath;
    };

    explicit cwExternalSourceSettings(QObject* parent = nullptr);

    /**
     * Returns the source path stored for ownerId, or an empty string
     * when no entry exists (import mode). The path is returned as-is
     * even when it no longer exists on disk; surfacing the missing-
     * source banner is the consumer's responsibility.
     */
    Q_INVOKABLE QString sourcePathFor(const QUuid& ownerId) const;

    /** True if any entry (with or without sourcePath) is recorded for ownerId. */
    Q_INVOKABLE bool hasSource(const QUuid& ownerId) const;

    /**
     * True iff a non-empty sourcePath is recorded for ownerId - the
     * attachment is in live-link mode on this machine. An entry with
     * an empty path (import mode) or no entry at all returns false.
     */
    Q_INVOKABLE bool isLiveLink(const QUuid& ownerId) const;

    /** All ExternalCenterlineSource entries (live-link + import-mode). */
    QList<ExternalCenterlineSource> externalCenterlineSources() const;

    /**
     * Upserts the ExternalCenterlineSource for ownerId. Setting an
     * empty sourcePath records "import mode" - the entry still exists
     * but has no path. To remove the entry entirely use
     * clearSourcePath.
     */
    void setSourcePath(const QUuid& ownerId, const QString& sourcePath);

    /** Removes any ExternalCenterlineSource entry for ownerId. */
    void clearSourcePath(const QUuid& ownerId);

signals:
    /**
     * Emitted after this instance changes the store (upsert with a
     * new value, or removing an existing entry). No-op writes don't
     * emit. Mutations from other instances or processes don't emit
     * here - re-read on your own triggers if that matters.
     */
    void externalCenterlineSourcesChanged();

private:
    static QString sourceKey(const QUuid& ownerId);
};

#endif // CWEXTERNALSOURCESETTINGS_H
