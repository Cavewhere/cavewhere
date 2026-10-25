/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALSOURCERECORD_H
#define CWEXTERNALSOURCERECORD_H

//Our includes
#include "CaveWhereLibExport.h"

//Qt includes
#include <QDir>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

//Monad
#include <Monad/Result.h>

/**
 * The value types and file functions of the external-source record store:
 * one binary protobuf (CavewhereProto::ExternalSourceRecord) per owner at
 * <project root>/.cw_local/external-sources/<owner uuid>.pb. cwSaveLoad
 * reads and writes the files on its worker; cwExternalSourceSettings holds
 * the open project's records in memory.
 */
namespace cwExternalSourceRecord {

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
 * recorded for that owner, which a caller must read as "nothing to
 * compare against" rather than as a change.
 */
struct SourceFingerprint {
    QList<SourceFileFingerprint> files;

    bool isEmpty() const { return files.isEmpty(); }
    bool operator==(const SourceFingerprint& other) const = default;
};

/**
 * One owner's whole record: the breadcrumb path and the fingerprint of
 * the source it names.
 */
struct Record {
    QString path;
    SourceFingerprint fingerprint;

    bool operator==(const Record& other) const = default;
};

//! Every owner's record in one project.
using Records = QHash<QUuid, Record>;

/**
 * Reads `files` and returns their fingerprint, in the order given.
 * Sources are small text files, so this reads every one of them;
 * callers on the quiet path compare size and last-modified first
 * and only come here to settle a stat-level difference.
 */
CAVEWHERE_LIB_EXPORT SourceFingerprint computeFingerprint(const QStringList& files);

//! The directory under a project's .cw_local holding the records.
CAVEWHERE_LIB_EXPORT QString recordsDirName();

//! The file name of ownerId's record in the records directory.
CAVEWHERE_LIB_EXPORT QString recordFileName(const QUuid& ownerId);

/**
 * Reads every record in recordsDir, skipping files whose name is no
 * owner id and files that do not parse. Safe on any thread.
 */
CAVEWHERE_LIB_EXPORT Records readRecords(const QDir& recordsDir);

/**
 * Serializes record and writes it to filePath atomically, creating the
 * directory as needed. Safe on any thread.
 */
CAVEWHERE_LIB_EXPORT Monad::ResultBase writeRecord(const QString& filePath, const Record& record);

} // namespace cwExternalSourceRecord

#endif // CWEXTERNALSOURCERECORD_H
