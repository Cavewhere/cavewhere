// cwPointOctreePack.h
#pragma once

//Qt includes
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>

//Std includes
#include <memory>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPointOctreeManifest.h"

/**
 * One cache file holding a whole point octree: every node's payload, the
 * manifest, and an index from manifest node to payload. The reader maps the
 * file once and is immutable after open(), so any number of worker threads
 * read it without locking.
 *
 * Layout, little-endian: an 8 byte header (magic, version), the node payloads
 * in append order, the serialized manifest, the index (offset, size per
 * manifest node), and a fixed 56 byte footer that locates the rest and carries
 * a checksum over the manifest and index.
 */
class CAVEWHERE_LIB_EXPORT cwPointOctreePack
{
public:
    class Writer;

    //! Opens, maps, and validates the pack at packPath. nullptr when the file
    //! is missing, truncated, from another format version, or its index and
    //! manifest fail the checksum. The one QFile::open per cloud happens here.
    static std::shared_ptr<const cwPointOctreePack> open(const QString& packPath);

    cwPointOctreePack(const cwPointOctreePack&) = delete;
    cwPointOctreePack& operator=(const cwPointOctreePack&) = delete;

    const cwPointOctreeManifest& manifest() const;

    //! A copy of node index's payload bytes (manifest node order), empty when
    //! the index is out of range. The mapping stays owned by the pack, so
    //! callers never hold a raw pointer into it.
    QByteArray node(int index) const;

    int nodeCount() const;
    QString path() const;

private:
    struct IndexEntry {
        quint64 offset = 0;
        quint64 size = 0;
    };

    explicit cwPointOctreePack(const QString& packPath);

    bool load();

    //Keeps the mapping alive; the file itself is closed after open()
    QFile m_file;
    const uchar* m_mapped = nullptr;
    cwPointOctreeManifest m_manifest;
    QVector<IndexEntry> m_index;
};

/**
 * Writes a pack to a temp file next to packPath and renames it onto packPath
 * on finish(). An existing pack is first renamed aside to packPath + ".staleN"
 * so a reader can keep it mapped, and finish() removes every stale copy it
 * can afterward. A writer destroyed before a successful finish() removes the
 * temp file, so a failed or canceled build leaves no new pack behind.
 */
class CAVEWHERE_LIB_EXPORT cwPointOctreePack::Writer
{
public:
    //! Creates packPath's directory and opens packPath + ".tmp" for writing.
    //! isOpen() reports whether that worked.
    explicit Writer(const QString& packPath);
    //! Removes the temp file when finish() never succeeded.
    ~Writer();

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    bool isOpen() const;

    //! Appends one node's payload under its cell key (cw::octree::cellKey of
    //! the node's level, x, y, z). Thread-safe. Returns false when the write
    //! failed or the cell already has a payload. Records whatever size it is
    //! given; sizes are the reader's business.
    bool append(quint64 cellKey, const QByteArray& payload);

    //! Writes the manifest, the index in manifest node order, and the footer,
    //! flushes, closes, renames any existing pack to a unique packPath +
    //! ".staleN", renames the temp file onto packPath, and removes every stale
    //! copy it can; on Windows a copy a reader still maps waits for a later
    //! finish(). Fails, and leaves no new pack behind, when any manifest node
    //! has no appended payload, when a payload has no manifest node, when the
    //! same cell was appended twice, or when a write fails; a failed rename
    //! onto packPath puts the previous pack back. errorMessage() says which.
    bool finish(const cwPointOctreeManifest& manifest);
    QString errorMessage() const;

private:
    bool writeBytes(const QByteArray& bytes);
    bool fail(const QString& message);

    const QString m_packPath;
    mutable QMutex m_mutex;
    QFile m_file;
    QHash<quint64, IndexEntry> m_entries;
    qint64 m_end = 0;
    QString m_errorMessage;
    bool m_finished = false;
};
