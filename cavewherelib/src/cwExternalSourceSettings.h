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
#include <QStringList>
#include <QUrl>
#include <QUuid>

class QSettings;

/**
 * Per-user, per-machine breadcrumbs recording the file an external
 * centerline was last attached or replaced from, keyed by owner UUID
 * (cwCave::id() / cwTrip::id()). A breadcrumb is a convenience for the
 * Replace file dialog - it decides which folder that dialog opens in,
 * and nothing else. The in-project copy is the attachment
 * (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html section 5.2), so a
 * breadcrumb naming a file that has moved or been deleted is an
 * ordinary state: the dialog opens at the default location instead.
 *
 * One store serves every project on the machine - owner UUIDs are
 * globally unique, so there is no per-project namespace and entries
 * survive bundled .cw open/close cycles (the extraction dir is a fresh
 * temp path every session). Copies of a project (Git clone, Save As,
 * restored backup) preserve owner UUIDs and therefore share
 * breadcrumbs on this machine. Entries whose owner no longer exists in
 * any project are harmless (a lookup that never matches) and are not
 * GC'd.
 *
 * Backed by QSettings - the same store as the rest of CaveWhere's
 * per-machine state - with one key per owner under the
 * "externalCenterlineSources" group. That group name is the on-disk
 * format and stays as it is, so settings files written by older
 * versions keep resolving. QSettings serializes concurrent writers
 * with a lock file and merges per-key on sync, so two CaveWhere
 * instances upserting different owners both persist; a hand-rolled
 * settings file would be whole-file last-writer-wins. Every read and
 * write goes straight to QSettings (no in-memory copy), so all
 * instances in a process agree. The change signal fires only on the
 * instance that performed the mutation.
 *
 * Each entry also carries the fingerprint of the source as it was when
 * it was copied - one size + last-modified + SHA-256 record per file in
 * the source's *include dependency set - so a later check can tell
 * whether the source has moved on since (plans/
 * EXTERNAL_SOURCE_CHANGE_NOTIFY.html section 2). Entries written before
 * fingerprints existed simply record none, and read back as an empty
 * fingerprint.
 *
 * Owned by cwRootData; the attach orchestrator writes breadcrumbs and
 * the Replace dialog reads them.
 */
class CAVEWHERE_LIB_EXPORT cwExternalSourceSettings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExternalSourceSettings)
    QML_UNCREATABLE("ExternalSourceSettings is owned by RootData and can't be created directly")

public:
    /**
     * One owner's breadcrumb. ownerId is the cwCave or cwTrip QUuid
     * (matches cwCave::id() / cwTrip::id()). An empty path is an entry
     * that names no file, which the Replace dialog treats the same way
     * as no entry at all.
     */
    struct Breadcrumb {
        QUuid ownerId;
        QString path;
    };

    /**
     * The state one source file was in when it was last copied into
     * the project: its absolute path, byte size, last-modified time
     * (milliseconds since the epoch, UTC), and the SHA-256 of its
     * contents as lowercase hex. size is -1 and sha256 is empty for a
     * file that could not be read when the fingerprint was taken.
     */
    struct SourceFileFingerprint {
        QString path;
        qint64 size = -1;
        qint64 lastModified = -1;
        QString sha256;

        bool operator==(const SourceFileFingerprint& other) const = default;
    };

    /**
     * One owner's source fingerprint: one record per file in the
     * source's *include dependency set, entry file first, in the
     * scanner's order. An empty fingerprint means no fingerprint was
     * recorded for that owner - the state of every entry written
     * before fingerprints existed, and the state a caller must read as
     * "nothing to compare against" rather than as a change.
     */
    struct SourceFingerprint {
        QList<SourceFileFingerprint> files;

        bool isEmpty() const { return files.isEmpty(); }
        bool operator==(const SourceFingerprint& other) const = default;
    };

    explicit cwExternalSourceSettings(QObject* parent = nullptr);

    /**
     * Returns the path recorded for ownerId, or an empty string when
     * no entry exists. The path is returned as-is even when it no
     * longer names a file on disk.
     */
    Q_INVOKABLE QString breadcrumbPath(const QUuid& ownerId) const;

    /** True if any entry (with or without a path) is recorded for ownerId. */
    Q_INVOKABLE bool hasBreadcrumb(const QUuid& ownerId) const;

    /**
     * The folder a file dialog should open in for ownerId: the
     * directory holding the breadcrumb's file. Empty when there is no
     * breadcrumb, when it records no path, or when the directory it
     * names is gone - the caller falls back to its own default folder.
     */
    Q_INVOKABLE QUrl breadcrumbFolder(const QUuid& ownerId) const;

    /** Every recorded breadcrumb. */
    QList<Breadcrumb> breadcrumbs() const;

    /**
     * The fingerprint recorded for ownerId, empty when the entry
     * records none - either because it predates fingerprints or
     * because it was written by setBreadcrumbPath.
     */
    SourceFingerprint fingerprint(const QUuid& ownerId) const;

    /**
     * Upserts the breadcrumb for ownerId, recording no fingerprint:
     * any fingerprint previously stored for ownerId is dropped, since
     * it described a copy this path may no longer name. To remove the
     * entry entirely use clearBreadcrumb.
     */
    void setBreadcrumbPath(const QUuid& ownerId, const QString& path);

    /**
     * Upserts the breadcrumb for ownerId together with the fingerprint
     * of the source that was just copied into the project. Every copy
     * path (Attach, Replace, Reload) stamps through here.
     */
    void setBreadcrumb(const QUuid& ownerId,
                       const QString& path,
                       const SourceFingerprint& fingerprint);

    /**
     * Reads `files` and returns their fingerprint, in the order given.
     * Sources are small text files, so this reads every one of them;
     * callers on the quiet path compare size and last-modified first
     * and only come here to settle a stat-level difference.
     */
    static SourceFingerprint computeFingerprint(const QStringList& files);

    /** Removes any breadcrumb recorded for ownerId. */
    Q_INVOKABLE void clearBreadcrumb(const QUuid& ownerId);

signals:
    /**
     * Emitted after this instance changes the store (upsert with a
     * new value, or removing an existing entry). No-op writes don't
     * emit. Mutations from other instances or processes don't emit
     * here - re-read on your own triggers if that matters.
     */
    void breadcrumbsChanged();

private:
    static QString breadcrumbKey(const QUuid& ownerId);
    static QString fingerprintKey(const QUuid& ownerId);
    static SourceFingerprint readFingerprint(QSettings& settings, const QUuid& ownerId);
    static void writeFingerprint(QSettings& settings,
                                 const QUuid& ownerId,
                                 const SourceFingerprint& fingerprint);
};

#endif // CWEXTERNALSOURCESETTINGS_H
