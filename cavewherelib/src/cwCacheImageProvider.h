#pragma once

#include <QQuickImageProvider>
#include <QDir>
#include <QMutex>

#include "cwDiskCacher.h"

class cwCacheImageProvider : public QQuickImageProvider
{
public:
    cwCacheImageProvider();

    static QString name();

    void setProjectDirectory(const QDir& dir);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    static QString encodeKey(const cwDiskCacher::Key& key);
    static bool decodeKey(const QString& encodedKey, cwDiskCacher::Key* key);

private:
    QDir cacheBaseDir() const;

    mutable QMutex m_mutex;
    QDir m_projectDir;
};
