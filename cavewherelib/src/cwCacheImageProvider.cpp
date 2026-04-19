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
#include <QUrlQuery>

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
    // The `?v=<token>` query is a cache-busting suffix: it makes QML see a
    // fresh URL when the underlying entry changes, but is not part of the
    // on-disk key. The token is folded into the scaled-image sub-cache key
    // below so the scaled layer invalidates in lockstep with the original.
    QString idWithoutQuery = id;
    QString versionToken;
    const int queryStart = id.indexOf(QLatin1Char('?'));
    if (queryStart >= 0) {
        idWithoutQuery = id.left(queryStart);
        versionToken = QUrlQuery(id.mid(queryStart + 1))
            .queryItemValue(QStringLiteral("v"));
    }

    cwDiskCacher::Key key;
    if (!decodeKey(idWithoutQuery, &key)) {
        qWarning() << "cwCacheImageProvider: decodeKey failed for id:" << id;
        return {};
    }

    QDir baseDir = cacheBaseDir();
    if (!baseDir.exists()) {
        qWarning() << "cwCacheImageProvider: baseDir does not exist:"
                   << baseDir.absolutePath()
                   << "(request id:" << id << ")";
        return {};
    }

    cwDiskCacher cacher(baseDir);
    const QByteArray data = cacher.entry(key);
    if (data.isEmpty()) {
        qWarning() << "cwCacheImageProvider: empty cache entry for key"
                   << key.path.path() << "/" << key.id
                   << "under baseDir:" << baseDir.absolutePath();
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
    const QString versionSuffix = versionToken.isEmpty()
        ? QString()
        : (QStringLiteral("-v") + versionToken);
    scaledKey.id = prefix + QStringLiteral("-") + key.id + versionSuffix
        + QStringLiteral(".") + cwImageProvider::imageCacheExtension();

    auto loadOriginal = [data]() {
        QImage image;
        image.loadFromData(data);
        return image;
    };

    return cwImageProvider::requestScaledImageFromCache(
        requestedSize,
        size,
        loadOriginal,
        scaledKey,
        baseDir);
}
