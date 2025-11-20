#include "cwCacheImageProvider.h"

#include <QByteArray>
#include <QBuffer>
#include <QMutexLocker>
#include <QImage>
#include <QStringList>
#include <QUrl>

#include "cwDiskCacher.h"
#include "cwImageProvider.h"
#include <algorithm>

namespace {

QString encodeComponent(const QString& value)
{
    return QString::fromLatin1(value.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString decodeComponent(const QString& value)
{
    QByteArray data = value.toLatin1();
    const int remainder = data.size() % 4;
    if (remainder != 0) {
        data.append(QByteArray(4 - remainder, '='));
    }
    return QString::fromUtf8(QByteArray::fromBase64(data, QByteArray::Base64UrlEncoding));
}

}

cwCacheImageProvider::cwCacheImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QString cwCacheImageProvider::name()
{
    return QStringLiteral("cwcache");
}

void cwCacheImageProvider::setProjectDirectory(const QDir& dir)
{
    QMutexLocker locker(&m_mutex);
    m_projectDir = dir;
}

QDir cwCacheImageProvider::cacheBaseDir() const
{
    QMutexLocker locker(&m_mutex);
    return m_projectDir;
}

QString cwCacheImageProvider::encodeKey(const cwDiskCacher::Key& key)
{
    const QString encodedPath = encodeComponent(key.path.path());
    const QString encodedId = encodeComponent(key.id);
    const QString encodedChecksum = encodeComponent(key.checksum);
    return encodedPath + QStringLiteral("|") + encodedId + QStringLiteral("|") + encodedChecksum;
}

bool cwCacheImageProvider::decodeKey(const QString& encodedKey, cwDiskCacher::Key* key)
{
    if (key == nullptr) {
        return false;
    }

    const QByteArray decodedBytes = QByteArray::fromPercentEncoding(encodedKey.toUtf8());
    const QString decodedKey = QString::fromUtf8(decodedBytes);

    const QStringList parts = decodedKey.split(QStringLiteral("|"));
    if (parts.size() != 3) {
        return false;
    }

    key->path = QDir(decodeComponent(parts.at(0)));
    key->id = decodeComponent(parts.at(1));
    key->checksum = decodeComponent(parts.at(2));
    return true;
}

QImage cwCacheImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    cwDiskCacher::Key key;

    if (!decodeKey(id, &key)) {
        return {};
    }

    QDir baseDir = cacheBaseDir();
    if (!baseDir.exists()) {
        return {};
    }

    cwDiskCacher cacher(baseDir);
    const QByteArray data = cacher.entry(key);
    if (data.isEmpty()) {
        return {};
    }

    const int maxSize = std::max(requestedSize.width(), requestedSize.height());
    if (maxSize <= 0) {
        QImage image;
        image.loadFromData(data);
        if (size != nullptr) {
            *size = image.size();
        }
        return image;
    }

    const QString prefix = QStringLiteral("scaled-%1_%2")
                               .arg(requestedSize.width())
                               .arg(requestedSize.height());
    cwDiskCacher::Key scaledKey = key;
    scaledKey.id = prefix + QStringLiteral("-") + key.id + QStringLiteral(".") + cwImageProvider::imageCacheExtension();

    auto loadOriginal = [data]() {
        QImage image;
        image.loadFromData(data);
        return image;
    };

    qDebug() << "Requesting scaled image:" << requestedSize;

    return cwImageProvider::requestScaledImageFromCache(
        requestedSize,
        size,
        loadOriginal,
        scaledKey,
        baseDir);
}
