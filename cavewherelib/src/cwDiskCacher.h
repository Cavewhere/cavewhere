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
    void insert(const Key& key, const QByteArray& data);

    QDir dir() const;
    void setDir(const QDir& directory);

private:
    // Helper utilities
    QDir relativeDir(const Key& key) const;
    QString filePath(const Key& key) const;
    std::shared_ptr<QMutex> fileMutexForPath(const QString& path) const;

    QDir m_dir;
    mutable QMutex m_mutex;
    mutable QMutex m_hashMutex;
    mutable QHash<QString, std::shared_ptr<QMutex>> m_fileMutexes;
};
