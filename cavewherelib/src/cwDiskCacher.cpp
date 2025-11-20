// cwDiskCacher.cpp
#include "cwDiskCacher.h"
#include "cavewhere.pb.h"

//Qt includes
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

cwDiskCacher::FileLockerSingleton* cwDiskCacher::singleton = new cwDiskCacher::FileLockerSingleton();

cwDiskCacher::cwDiskCacher(const QDir& directory)
    : m_dir(directory)
{
}

cwDiskCacher::~cwDiskCacher()
{
    //This isn't obviouses but because file mutexes remove themselves from
    //the hash when they are being delete, clearing the hash will cause a crash
    //by default m_fileMutexes will clear the hash in the destructor. We copy the
    //mutexes into other, so the reference count in the mutexes increase. Clearing
    //m_fileMutexes will work correctly. This prevents a use after free memory issue
    // auto other = m_fileMutexes;
    // m_fileMutexes.clear();
}

QDir cwDiskCacher::dir() const
{
    return m_dir;
}

void cwDiskCacher::setDir(const QDir& directory)
{
    m_dir = directory;
}

// Private helpers
QDir cwDiskCacher::relativeDir(const Key& key) const
{
    return QDir(m_dir.filePath(QStringLiteral(".cw_cache/") + key.path.path()));
}

QString cwDiskCacher::filePath(const Key& key) const
{
    QDir dir = relativeDir(key);
    return dir.filePath(key.id);
}

std::shared_ptr<QMutex> cwDiskCacher::fileMutexForPath(const QString& path) const
{
    QMutexLocker locker(&singleton->hashMutex);
    auto it = singleton->fileMutexes.find(path);
    if (it == singleton->fileMutexes.end()) {
        // Custom deleter: remove from map when last reference goes away
        auto deleter = [this, path](QMutex* ptr) {
            {
                QMutexLocker hashLocker(&singleton->hashMutex);
                singleton->fileMutexes.remove(path);
            }
            delete ptr;
        };
        std::shared_ptr<QMutex> mutexPtr(new QMutex(), deleter);
        singleton->fileMutexes.insert(path, mutexPtr);
        return mutexPtr;
    }
    return it.value();
}

QByteArray cwDiskCacher::entry(const Key& key) const
{
    // Determine cache file path under global lock
    QString cacheFile = filePath(key);
    return entry(cacheFile, key.checksum);
}

QByteArray cwDiskCacher::entry(const QString &cacheFile, const QString& checksum) const
{
    // Acquire per-file mutex
    auto fileMutex = fileMutexForPath(cacheFile);
    QMutexLocker fileLocker(fileMutex.get());

    QByteArray result;
    QFile file(cacheFile);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray raw = file.readAll();
        file.close();

        CavewhereProto::DiskCacheEntry entryProto;
        if (entryProto.ParseFromArray(raw.constData(), raw.size()) &&
            //Make sure the checksum works out
            (checksum.isEmpty() || QString::fromStdString(entryProto.checksum()) == checksum))
        {
            result = QByteArray::fromStdString(entryProto.entry());
        }
    }
    return result;
}

bool cwDiskCacher::hasEntry(const Key& key) const
{
    QFileInfo info(filePath(key));
    return info.exists() && info.isFile();
}

void cwDiskCacher::insert(const Key& key, const QByteArray& data)
{
    // Determine cache file path under global lock
    QString cacheFile = filePath(key);

    // Acquire per-file mutex
    auto fileMutex = fileMutexForPath(cacheFile);
    QMutexLocker fileLocker(fileMutex.get());

    // Ensure directory exists
    QDir dir = QFileInfo(cacheFile).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(dir.path());
    }

    CavewhereProto::DiskCacheEntry entryProto;
    entryProto.set_checksum(key.checksum.toStdString());
    entryProto.set_entry(data.constData(), data.size());
    std::string bytes = entryProto.SerializeAsString();

    QFile file(cacheFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(bytes.data(), static_cast<qint64>(bytes.size()));
        file.close();
    }
}
