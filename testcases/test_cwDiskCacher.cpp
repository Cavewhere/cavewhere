#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVector>
#include <QtConcurrent>
#include <numeric>

#include "cwDiskCacher.h"

// Test single-threaded insert and entry
TEST_CASE("cwDiskCacher single-threaded insert and entry", "[cwDiskCacher]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    cwDiskCacher cacher(tempDir.path());

    cwDiskCacher::Key key{QStringLiteral("testfile"), QDir{QStringLiteral("subdir")}, QStringLiteral("checksum123")};
    QByteArray data("Hello, World!");

    auto badResult = cacher.entry(key);
    CHECK(badResult.isEmpty());

    cacher.insert(key, data);
    auto result = cacher.entry(key);
    CHECK(result == data);

    SECTION("hasEntry reflects cache presence") {
        cwDiskCacher::Key missing{QStringLiteral("missing"), QDir(), QStringLiteral("chk")};
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
            QDir{tempDir.filePath(QStringLiteral("images/subdir"))},
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
        // covers cross-drive paths where relativeFilePath() returns the absolute path
        // unchanged; that branch is guarded by QDir::isAbsolutePath().
        QTemporaryDir outerDir;
        REQUIRE(outerDir.isValid());

        cwDiskCacher outerCacher(outerDir.path());
        cwDiskCacher::Key outsideKey{
            QStringLiteral("image.cw_img_cache"),
            QDir{QStringLiteral("/some/completely/different/directory")},
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
                          QDir{QStringLiteral("sketch-icons")},
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
        auto key = cwDiskCacher::Key({QString::number(i), QDir{QString()}, QString("chk%1").arg(i)});
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
            cacher.insert(keys[i], datas[i]);
        });

        // Concurrent entries
        QtConcurrent::blockingMap(idxs, [tempPath, &checkMutex, &checks, keys, datas](int i) {
            cwDiskCacher cacher(tempPath);
            auto res = cacher.entry(keys[i]);
            QMutexLocker locker(&checkMutex);
            checks.append(res == datas[i]);
        });
    }

    for(auto check : checks) {
        CHECK(check);
    }
}
