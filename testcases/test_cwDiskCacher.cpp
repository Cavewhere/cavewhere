#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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

    SECTION("Change the checksum") {
        key.checksum = "different";
        auto result = cacher.entry(key);
        CHECK(result != data);
        CHECK(result.isEmpty());
    }
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
