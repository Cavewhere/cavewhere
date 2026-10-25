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
#include "cwExternalSourceRecord.h"

//Qt includes
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QUuid>

class cwSaveLoad;

/**
 * The open project's record of where each external centerline came from on
 * this machine, keyed by owner UUID (cwCave::id() / cwTrip::id()): the file
 * the owner was last attached or replaced from (its breadcrumb), and the
 * fingerprint of that source as it was copied in. A breadcrumb decides which
 * folder the Replace file dialog opens in; a breadcrumb naming a file that
 * has moved or been deleted is an ordinary state, and the dialog opens at
 * the default location instead. The in-project copy is the attachment
 * (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html section 5.2).
 *
 * The records belong to the project: one binary protobuf
 * (CavewhereProto::ExternalSourceRecord) per owner at
 * <project root>/.cw_local/external-sources/<owner uuid>.pb. Git ignores
 * .cw_local, since the paths are machine-specific, and the bundled .cw
 * carries it, so the records move, copy and delete with the project. Each
 * record stores the fingerprint's paths relative to the closure's common
 * ancestor directory, the same base the attach mirrors the copy from;
 * readers here see the absolute paths the scanner produced.
 *
 * This object holds the open project's records in memory. cwSaveLoad reads
 * them on its load worker and hands them over before the region is set;
 * every change is written through as a job on cwSaveLoad's file-system queue,
 * which serializes and writes on its worker, so a save drain, Save As and
 * bundling all see the record on disk. Without a cwSaveLoad the records live
 * in memory only.
 *
 * Each record carries the fingerprint of the source as it was when it was
 * copied - one size + last-modified + SHA-256 record per file in the
 * source's *include dependency set - so a later check can tell whether the
 * source has moved on since (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html
 * section 2).
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
    explicit cwExternalSourceSettings(QObject* parent = nullptr);

    /**
     * Follows \a saveLoad's project: drops the records held for the previous
     * one, takes each project's records as \a saveLoad loads it, and writes
     * every later change into \a saveLoad's project root.
     */
    void setSaveLoad(cwSaveLoad* saveLoad);

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
    QList<cwExternalSourceRecord::Breadcrumb> breadcrumbs() const;

    /**
     * The fingerprint recorded for ownerId, empty when there is no
     * entry or the entry was written by setBreadcrumbPath.
     */
    cwExternalSourceRecord::SourceFingerprint fingerprint(const QUuid& ownerId) const;

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
                       const cwExternalSourceRecord::SourceFingerprint& fingerprint);

    /**
     * Rewrites the size and last-modified fields of ownerId's stored
     * fingerprint from `fingerprint`, which must describe the same files
     * with the same contents (a checker that read them and found the
     * hashes unchanged). It records that the files are still what they
     * were, so the size+mtime fast path recognizes them without reading
     * them again - bookkeeping about files nobody changed, so it emits
     * nothing, leaves the breadcrumb path alone, and does nothing at all
     * for an owner with no entry.
     */
    void refreshFingerprintStats(const QUuid& ownerId,
                                 const cwExternalSourceRecord::SourceFingerprint& fingerprint);

    /** Removes any breadcrumb recorded for ownerId. */
    Q_INVOKABLE void clearBreadcrumb(const QUuid& ownerId);

    /** Removes the breadcrumbs of every owner in ownerIds, with one change signal. */
    void clearBreadcrumbs(const QList<QUuid>& ownerIds);

signals:
    /**
     * Emitted after the records change: an upsert with a new value, the
     * removal of an existing entry, or a project's records replacing the
     * previous ones. No-op writes don't emit.
     */
    void breadcrumbsChanged();

private:
    void installRecords(const cwExternalSourceRecord::Records& records);
    void writeThrough(const QUuid& ownerId, const cwExternalSourceRecord::Record& record);
    void removeThrough(const QUuid& ownerId);

    cwExternalSourceRecord::Records m_records;
    QPointer<cwSaveLoad> m_saveLoad;
};

#endif // CWEXTERNALSOURCESETTINGS_H
