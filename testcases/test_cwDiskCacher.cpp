#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QVector>
#include <QtConcurrent>
#include <algorithm>
#include <numeric>

#include "cwDiskCacher.h"

//Qt private includes
#include <private/qabstractfileengine_p.h>

namespace {
    // Records every path Qt asks the file engine layer about from the thread
    // that installed it, the way iOS's security-scoped handler sees them.
    // Construction installs it and destruction removes it.
    class FileEngineRecorder : public QAbstractFileEngineHandler
    {
    public:
        std::unique_ptr<QAbstractFileEngine> create(const QString& fileName) const override
        {
            if (QThread::currentThread() == m_thread) {
                QMutexLocker locker(&m_mutex);
                m_fileNames.append(fileName);
            }
            return nullptr;
        }

        QStringList takeFileNames()
        {
            QMutexLocker locker(&m_mutex);
            return std::exchange(m_fileNames, QStringList());
        }

    private:
        QThread* const m_thread = QThread::currentThread();
        mutable QMutex m_mutex;
        mutable QStringList m_fileNames;
    };

    cwDiskCacher::Key nodeKeyAt(const QString& keyPath)
    {
        return cwDiskCacher::Key{QStringLiteral("node"), keyPath, QString()};
    }
}

// Test single-threaded insert and entry
TEST_CASE("cwDiskCacher single-threaded insert and entry", "[cwDiskCacher]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    cwDiskCacher cacher(tempDir.path());

    cwDiskCacher::Key key{QStringLiteral("testfile"), QStringLiteral("subdir"), QStringLiteral("checksum123")};
    QByteArray data("Hello, World!");

    auto badResult = cacher.entry(key);
    CHECK(badResult.isEmpty());

    cacher.insert(key, data);
    auto result = cacher.entry(key);
    CHECK(result == data);

    SECTION("hasEntry reflects cache presence") {
        cwDiskCacher::Key missing{QStringLiteral("missing"), QString(), QStringLiteral("chk")};
        CHECK_FALSE(cacher.hasEntry(missing));
        CHECK(cacher.hasEntry(key));
    }

    SECTION("Change the checksum") {
        key.checksum = "different";
        auto result = cacher.entry(key);
        CHECK(result != data);
        CHECK(result.isEmpty());
    }

    SECTION("Absolute key paths are stored relative to cache dir") {
        cwDiskCacher::Key absKey{
            QStringLiteral("absfile"),
            tempDir.filePath(QStringLiteral("images/subdir")),
            QStringLiteral("absChecksum")};
        QByteArray absData("Absolute Path Data");

        cacher.insert(absKey, absData);

        const QString cachePath = QFileInfo(cacher.filePath(absKey)).absoluteFilePath();
        const QString expectedPath = QFileInfo(
                                         QDir(tempDir.path())
                                             .filePath(QStringLiteral(".cw_cache/images/subdir/absfile")))
                                         .absoluteFilePath();

        CHECK(cachePath == expectedPath);
        CHECK(QFileInfo(cachePath).exists());
        CHECK(cacher.entry(absKey) == absData);
    }

    SECTION("Absolute key paths outside cache dir do not escape via ../") {
        // Triangulate-task scenario: cacher lives in a temp dir but the key path points
        // to a directory with no common prefix (e.g. source tree). On Windows this also
        // covers cross-drive paths, where no relative path exists.
        QTemporaryDir outerDir;
        REQUIRE(outerDir.isValid());

        cwDiskCacher outerCacher(outerDir.path());
        cwDiskCacher::Key outsideKey{
            QStringLiteral("image.cw_img_cache"),
            QStringLiteral("/some/completely/different/directory"),
            QStringLiteral("chk")};

        outerCacher.insert(outsideKey, QByteArray("outside data"));

        const QString cachedFilePath = QFileInfo(outerCacher.filePath(outsideKey)).absoluteFilePath();
        CHECK(cachedFilePath.startsWith(outerDir.path()));  // no ../.. escaping
        CHECK(cacher.entry(outsideKey) == QByteArray());    // different cacher dir, miss
        CHECK(outerCacher.entry(outsideKey) == QByteArray("outside data"));
    }
}

// The sketch-icon versioning scheme reads QFileInfo(cacher.filePath(key))
// .lastModified() and embeds the epoch-ms into `?v=` on the image URL so
// QML's pixmap cache invalidates on each rewrite. That only works if
// filePath() returns a path pointing at the actual file insert() wrote,
// not some indirected location. Pin the contract.
TEST_CASE("cwDiskCacher filePath() points at the file written by insert()", "[cwDiskCacher]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    cwDiskCacher cacher(tempDir.path());

    cwDiskCacher::Key key{QStringLiteral("sketch-abc"),
                          QStringLiteral("sketch-icons"),
                          QString()};
    cacher.insert(key, QByteArray("v1-bytes"));

    const QString path = cacher.filePath(key);
    QFileInfo info(path);
    REQUIRE(info.exists());
    REQUIRE(info.isFile());

    // Backdate mtime on the first-insert file so the second insert is
    // guaranteed to produce a strictly-greater mtime without sleeping past
    // the filesystem's 1s mtime granularity.
    {
        QFile f(path);
        REQUIRE(f.open(QIODevice::ReadOnly));
        const QDateTime backdated = QDateTime::currentDateTime().addSecs(-5);
        REQUIRE(f.setFileTime(backdated, QFileDevice::FileModificationTime));
    }
    info.refresh();
    const QDateTime mtimeAfterFirstInsert = info.lastModified();

    // Re-inserting under the same key must rewrite the same file so the
    // QML cache-busting token (derived from mtime) changes deterministically.
    cacher.insert(key, QByteArray("v2-bytes"));

    info.refresh();
    REQUIRE(info.exists());
    CHECK(cacher.filePath(key) == path);
    CHECK(info.lastModified() > mtimeAfterFirstInsert);
    CHECK(cacher.entry(key) == QByteArray("v2-bytes"));
}

// Test concurrent insert and entry
TEST_CASE("cwDiskCacher concurrent insert and entry", "[cwDiskCacher]") {
    QTemporaryDir tempDir;
    // tempDir.setAutoRemove(false);
    // qDebug() << "Temp dir:" << tempDir.path();
    REQUIRE(tempDir.isValid());
    auto tempPath = tempDir.path();

    const int n = 100;
    QVector<cwDiskCacher::Key> keys;
    QVector<QByteArray> datas;
    keys.reserve(n);
    datas.reserve(n);
    for (int i = 0; i < n; ++i) {
        auto key = cwDiskCacher::Key({QString::number(i), QString(), QString("chk%1").arg(i)});
        keys.emplaceBack(std::move(key));
        datas.emplaceBack(QByteArray::number(i).repeated(8));
    }

    // Create an index array [0,1,...]
    QVector<int> idxs(n);
    std::iota(idxs.begin(), idxs.end(), 0);

    QMutex checkMutex;
    QList<bool> checks;

    for(int j = 0; j < 100; j++) {
        // Concurrent inserts
        QtConcurrent::blockingMap(idxs, [&](int i) {
            cwDiskCacher cacher(tempPath);
            cacher.insert(keys.at(i), datas.at(i));
        });

        // Concurrent entries
        QtConcurrent::blockingMap(idxs, [tempPath, &checkMutex, &checks, keys, datas](int i) {
            cwDiskCacher cacher(tempPath);
            auto res = cacher.entry(keys.at(i));
            QMutexLocker locker(&checkMutex);
            checks.append(res == datas.at(i));
        });
    }

    for(auto check : checks) {
        CHECK(check);
    }
}

TEST_CASE("cwDiskCacher resolves a cache path without the file engine", "[cwDiskCacher]") {
    const QString root = QStringLiteral("/cwDiskCacherEngineProbe/project");

    const auto checkEngineHits = [&root](const QString& keyPath) {
        const cwDiskCacher cacher(root);
        const cwDiskCacher::Key key{QStringLiteral("node"), keyPath, QStringLiteral("chk")};

        FileEngineRecorder recorder;

        const QString path = cacher.filePath(key);
        const QStringList filePathHits = recorder.takeFileNames();
        INFO("filePath engine hits: " << filePathHits.join(QStringLiteral(" | ")).toStdString());
        CHECK(filePathHits.isEmpty());

        CHECK(cacher.entry(key).isEmpty());
        const QStringList entryHits = recorder.takeFileNames();
        INFO("entry engine hits: " << entryHits.join(QStringLiteral(" | ")).toStdString());
        CHECK_FALSE(entryHits.isEmpty());
        CHECK(std::all_of(entryHits.cbegin(), entryHits.cend(), [&path](const QString& hit) { return hit == path; }));
    };

    SECTION("relative key path") {
        checkEngineHits(QStringLiteral("GIS Layers"));
    }

    SECTION("absolute key path inside the root") {
        checkEngineHits(root + QStringLiteral("/GIS Layers"));
    }

    SECTION("absolute key path outside the root") {
        checkEngineHits(QStringLiteral("/Volumes/data/lidar"));
    }
}

TEST_CASE("cwDiskCacher filePath() pins where each key shape lands", "[cwDiskCacher]") {
    const QString root = QStringLiteral("/projects/cave");
    const cwDiskCacher cacher(root);

    const auto filePathFor = [&cacher](const QString& keyPath) {
        return cacher.filePath(nodeKeyAt(keyPath));
    };

    SECTION("a relative key path lands under .cw_cache") {
        CHECK(filePathFor(QStringLiteral("sketch-icons")) == QStringLiteral("/projects/cave/.cw_cache/sketch-icons/node"));
    }

    SECTION("an absolute key path inside the root is made relative to it") {
        CHECK(filePathFor(QStringLiteral("/projects/cave/GIS Layers")) == QStringLiteral("/projects/cave/.cw_cache/GIS Layers/node"));
    }

    SECTION("a key path equal to the root lands directly in .cw_cache") {
        CHECK(filePathFor(root) == QStringLiteral("/projects/cave/.cw_cache/node"));
    }

    SECTION("an absolute key path outside the root is mirrored under .cw_cache") {
        CHECK(filePathFor(QStringLiteral("/Volumes/data/lidar")) == QStringLiteral("/projects/cave/.cw_cache/Volumes/data/lidar/node"));
        CHECK(filePathFor(QStringLiteral("/projects")) == QStringLiteral("/projects/cave/.cw_cache/projects/node"));
        CHECK(filePathFor(QStringLiteral("/projects/cave2/lidar")) == QStringLiteral("/projects/cave/.cw_cache/projects/cave2/lidar/node"));
        CHECK(filePathFor(QStringLiteral("/")) == QStringLiteral("/projects/cave/.cw_cache/node"));
    }

    SECTION("an empty key path and \".\" both land directly in .cw_cache") {
        CHECK(filePathFor(QString()) == QStringLiteral("/projects/cave/.cw_cache/node"));
        CHECK(filePathFor(QStringLiteral(".")) == QStringLiteral("/projects/cave/.cw_cache/node"));
    }

    SECTION("a trailing slash on the key path changes nothing") {
        CHECK(filePathFor(QStringLiteral("sketch-icons/")) == filePathFor(QStringLiteral("sketch-icons")));
        CHECK(filePathFor(QStringLiteral("/projects/cave/GIS Layers/")) == filePathFor(QStringLiteral("/projects/cave/GIS Layers")));
        CHECK(filePathFor(QStringLiteral("/projects/cave/")) == filePathFor(root));
        CHECK(filePathFor(QStringLiteral("/Volumes/data/lidar/")) == filePathFor(QStringLiteral("/Volumes/data/lidar")));
    }

    SECTION("a trailing slash on the root changes nothing") {
        const cwDiskCacher withSlash(QStringLiteral("/projects/cave/"));
        for (const QString& keyPath : {QStringLiteral("sketch-icons"), QStringLiteral("/projects/cave/GIS Layers"), root}) {
            CHECK(withSlash.filePath(nodeKeyAt(keyPath)) == filePathFor(keyPath));
        }
    }

    SECTION("a filesystem root joins with a single slash") {
        const cwDiskCacher topLevel(QStringLiteral("/"));
        CHECK(topLevel.filePath(nodeKeyAt(QStringLiteral("sketch-icons")))
              == QStringLiteral("/.cw_cache/sketch-icons/node"));
        CHECK(topLevel.filePath(nodeKeyAt(QStringLiteral("/data/lidar")))
              == QStringLiteral("/.cw_cache/data/lidar/node"));
        CHECK(topLevel.filePath(nodeKeyAt(QStringLiteral("/")))
              == QStringLiteral("/.cw_cache/node"));
    }

    SECTION("the QDir and QString constructors agree") {
        for (const QString& rootPath : {root, QStringLiteral("/projects/cave/"), QStringLiteral("/")}) {
            const cwDiskCacher fromString(rootPath);
            const cwDiskCacher fromDir{QDir(rootPath)};
            CHECK(fromDir.dir().path() == fromString.dir().path());
            for (const QString& keyPath : {QStringLiteral("sketch-icons"), QStringLiteral("/projects/cave/GIS Layers"),
                                           QStringLiteral("/Volumes/data/lidar"), QString()}) {
                const cwDiskCacher::Key key = nodeKeyAt(keyPath);
                CHECK(fromDir.filePath(key) == fromString.filePath(key));
            }
        }
    }

    SECTION("a relative root still resolves an absolute key path") {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        const QString relativeRoot = QDir::current().relativeFilePath(tempDir.path());
        const cwDiskCacher relative(relativeRoot);
        const cwDiskCacher absolute(tempDir.path());
        const cwDiskCacher::Key key = nodeKeyAt(tempDir.path() + QStringLiteral("/sub"));
        CHECK(QFileInfo(relative.filePath(key)).absoluteFilePath() == absolute.filePath(key));
        CHECK(absolute.filePath(key) == tempDir.path() + QStringLiteral("/.cw_cache/sub/node"));
    }
}

TEST_CASE("cwDiskCacher round-trips every key shape through the disk", "[cwDiskCacher]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString root = tempDir.path();
    cwDiskCacher cacher(root);

    const QString keyPath = GENERATE_COPY(
        QStringLiteral("sketch-icons"),
        QStringLiteral("sketch-icons/"),
        root + QStringLiteral("/GIS Layers"),
        root + QStringLiteral("/GIS Layers/"),
        root,
        QStringLiteral("/Volumes/data/lidar"),
        QString(),
        QStringLiteral("."));
    INFO("key path: " << keyPath.toStdString());

    const cwDiskCacher::Key key{QStringLiteral("node"), keyPath, QStringLiteral("chk")};
    const QByteArray data("round-trip bytes");

    CHECK_FALSE(cacher.hasEntry(key));
    REQUIRE(cacher.insert(key, data));

    CHECK(cacher.entry(key) == data);
    CHECK(cacher.hasEntry(key));

    const QString path = cacher.filePath(key);
    CHECK(path.startsWith(root + QStringLiteral("/.cw_cache/")));
    CHECK(QFileInfo(path).isFile());
}
