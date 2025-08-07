// cwDiskCacher.h
#pragma once

#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QDir>
#include <QHash>
#include <memory>

class cwDiskCacher {
public:
    struct Key {
        QString id;
        QDir path;      // relative directory
        QString checksum;
    };

    explicit cwDiskCacher(const QDir& directory = QDir());
    ~cwDiskCacher();

    QByteArray entry(const Key& key) const;
    QByteArray entry(const QString& cacheFile, const QString &checksum = QString()) const;

    void insert(const Key& key, const QByteArray& data);

    QDir dir() const;
    void setDir(const QDir& directory);

    QString filePath(const Key& key) const;

private:

    // Helper utilities
    QDir relativeDir(const Key& key) const;
    std::shared_ptr<QMutex> fileMutexForPath(const QString& path) const;

    QDir m_dir;

    struct FileLockerSingleton {
        mutable QMutex hashMutex;
        mutable QHash<QString, std::shared_ptr<QMutex>> fileMutexes;
    };

    static FileLockerSingleton* singleton;
};
