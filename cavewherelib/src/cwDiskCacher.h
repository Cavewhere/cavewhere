// cwDiskCacher.h
#pragma once

#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QDir>
#include <QHash>
#include <memory>

#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwDiskCacher {
public:
    struct Key {
        QString id;
        // Directory the entry lives under, relative to dir() or absolute; the
        // cacher normalizes it the way QDir(path) would. An absolute path
        // outside dir() is mirrored under .cw_cache minus its root.
        QString path;
        QString checksum;
    };

    explicit cwDiskCacher(const QDir& directory = QDir());
    //! Takes the root as a path so hot paths stay on string arithmetic; a QDir
    //! consults the platform file engine (realpath on iOS) on construction
    explicit cwDiskCacher(const QString& directoryPath);

    QByteArray entry(const Key& key) const;
    QByteArray entry(const QString& cacheFile, const QString &checksum = QString()) const;

    bool hasEntry(const Key& key) const;

    //! Returns true once the whole entry is on disk; a short write removes the file
    bool insert(const Key& key, const QByteArray& data);

    QDir dir() const;
    void setDir(const QDir& directory);

    QString filePath(const Key& key) const;

private:

    QString relativeDirPath(const Key& key) const;
    std::shared_ptr<QMutex> fileMutexForPath(const QString& path) const;

    QString m_dirPath;

    struct FileLockerSingleton {
        mutable QMutex hashMutex;
        mutable QHash<QString, std::shared_ptr<QMutex>> fileMutexes;
    };

    static FileLockerSingleton* singleton;
};
