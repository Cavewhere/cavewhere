// cwDiskCacher.cpp
#include "cwDiskCacher.h"
#include "cavewhere.pb.h"

//Qt includes
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

//Std includes
#include <optional>

cwDiskCacher::FileLockerSingleton* cwDiskCacher::singleton = new cwDiskCacher::FileLockerSingleton();

// String-only ports of the QDir path arithmetic, which consults the file engine.
namespace {
    constexpr QChar kSeparator = u'/';
    constexpr QChar kColon = u':';

    // Matches QDir::isAbsolutePath: a leading "/" or a resource ":" anywhere,
    // and on Windows any "X:" drive prefix.
    bool isAbsolutePathString(const QString& path)
    {
        if (path.startsWith(kSeparator) || path.startsWith(kColon)) {
            return true;
        }
#ifdef Q_OS_WIN
        return path.size() >= 2 && path.at(1) == kColon;
#else
        return false;
#endif
    }

    // What QDir(path).path() returns, so a QString root or key path names the
    // same directory a QDir built from it would.
    QString normalizedDirPath(const QString& path)
    {
        if (path.isEmpty()) {
            return QStringLiteral(".");
        }
        QString dir = QDir::fromNativeSeparators(path);
#ifdef Q_OS_WIN
        const bool isDriveRoot = dir.size() == 3 && dir.at(0).isLetter() && dir.at(1) == kColon;
#else
        const bool isDriveRoot = false;
#endif
        if (dir.size() > 1 && dir.endsWith(kSeparator) && !isDriveRoot) {
            dir.chop(1);
        }
        return dir;
    }

    // QDir(dir).filePath(name) for the names this file joins: ids and
    // relative paths.
    QString joinPath(const QString& dir, const QString& name)
    {
        if (isAbsolutePathString(name)) {
            return name;
        }
        if (name.isEmpty()) {
            return dir;
        }
        if (dir.isEmpty() || dir.endsWith(kSeparator)) {
            return dir + name;
        }
        return dir + kSeparator + name;
    }

#ifdef Q_OS_WIN
    QString driveSpec(const QString& path)
    {
        if (path.size() >= 2 && path.at(0).isLetter() && path.at(1) == kColon) {
            return path.left(2);
        }
        return QString();
    }
#endif

    // QDir(base).relativeFilePath(target) for an absolute base and a target
    // that is base or lies under it (empty when they match); nullopt when
    // target lies elsewhere, including on another Windows drive.
    std::optional<QString> pathUnder(const QString& base, const QString& target)
    {
        QString dir = QDir::cleanPath(base);
        QString file = QDir::cleanPath(target);

#ifdef Q_OS_WIN
        const QString dirDrive = driveSpec(dir);
        const QString fileDrive = driveSpec(file);
        const bool fileHasDrive = !fileDrive.isEmpty();
        const QString comparedDrive = fileHasDrive ? fileDrive : dirDrive;
        if (comparedDrive.compare(dirDrive, Qt::CaseInsensitive) != 0
            || (file.startsWith(QStringLiteral("//")) && !dir.startsWith(QStringLiteral("//")))) {
            return std::nullopt;
        }
        dir.remove(0, dirDrive.size());
        if (fileHasDrive) {
            file.remove(0, fileDrive.size());
        }
        constexpr Qt::CaseSensitivity kSensitivity = Qt::CaseInsensitive;
#else
        constexpr Qt::CaseSensitivity kSensitivity = Qt::CaseSensitive;
#endif

        const QList<QStringView> dirParts = QStringView(dir).split(kSeparator, Qt::SkipEmptyParts);
        const QList<QStringView> fileParts = QStringView(file).split(kSeparator, Qt::SkipEmptyParts);
        if (fileParts.size() < dirParts.size()) {
            return std::nullopt;
        }
        for (qsizetype i = 0; i < dirParts.size(); ++i) {
            if (dirParts.at(i).compare(fileParts.at(i), kSensitivity) != 0) {
                return std::nullopt;
            }
        }

        QStringList rest;
        rest.reserve(fileParts.size() - dirParts.size());
        for (qsizetype i = dirParts.size(); i < fileParts.size(); ++i) {
            rest.append(fileParts.at(i).toString());
        }
        return rest.join(kSeparator);
    }

    // Strips the drive letter and all leading separators so an absolute path
    // outside the root stays contained within .cw_cache/.
    QString withoutRoot(const QString& path)
    {
        qsizetype start = 0;
        if (path.size() >= 2 && path.at(1) == kColon) {
            start = 2;
        }
        while (start < path.size() && path.at(start) == kSeparator) {
            ++start;
        }
        return path.mid(start);
    }
}

cwDiskCacher::cwDiskCacher(const QDir& directory)
    : m_dirPath(directory.path())
{
}

cwDiskCacher::cwDiskCacher(const QString& directoryPath)
    : m_dirPath(normalizedDirPath(directoryPath))
{
}

QDir cwDiskCacher::dir() const
{
    return QDir(m_dirPath);
}

void cwDiskCacher::setDir(const QDir& directory)
{
    m_dirPath = directory.path();
}

QString cwDiskCacher::relativeDirPath(const Key& key) const
{
    const QString cacheRoot = joinPath(m_dirPath, QStringLiteral(".cw_cache"));

    QString keyPath = normalizedDirPath(key.path);
    if (isAbsolutePathString(keyPath)) {
        const QString absoluteRoot = isAbsolutePathString(m_dirPath)
                                         ? m_dirPath
                                         : joinPath(QDir::currentPath(), m_dirPath);
        const std::optional<QString> under = pathUnder(absoluteRoot, keyPath);
        keyPath = under ? *under : withoutRoot(keyPath);
    }

    if (keyPath == QStringLiteral(".")) {
        return cacheRoot;
    }
    return joinPath(cacheRoot, keyPath);
}

QString cwDiskCacher::filePath(const Key& key) const
{
    return joinPath(relativeDirPath(key), key.id);
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
    return entry(filePath(key), key.checksum);
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

bool cwDiskCacher::insert(const Key& key, const QByteArray& data)
{
    const QString dirPath = relativeDirPath(key);
    const QString cacheFile = joinPath(dirPath, key.id);

    auto fileMutex = fileMutexForPath(cacheFile);
    QMutexLocker fileLocker(fileMutex.get());

    QDir().mkpath(dirPath);

    CavewhereProto::DiskCacheEntry entryProto;
    entryProto.set_checksum(key.checksum.toStdString());
    entryProto.set_entry(data.constData(), data.size());
    std::string bytes = entryProto.SerializeAsString();

    QFile file(cacheFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const qint64 size = static_cast<qint64>(bytes.size());
        const bool complete = file.write(bytes.data(), size) == size && file.flush();
        file.close();
        if (complete) {
            return true;
        }
        file.remove();
    }
    return false;
}
