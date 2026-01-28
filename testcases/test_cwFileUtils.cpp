//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

//System includes
#include <atomic>
#include <thread>

//Our includes
#include "cwFileUtils.h"

TEST_CASE("waitForFileReady waits for file creation and size stability", "[cwFileUtils]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("ready.txt"));
    std::atomic_bool writeOk(false);

    std::thread writer([&]() {
        QThread::msleep(100);
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return;
        }
        file.write("hello");
        file.flush();
        QThread::msleep(150);
        file.write(" world");
        file.flush();
        file.close();
        writeOk.store(true);
    });

    const bool ready = cwFileUtils::waitForFileReady(filePath, 2000, 200, 2000);
    writer.join();

    REQUIRE(writeOk.load());
    CHECK(ready);

    QFile file(filePath);
    REQUIRE(file.open(QIODevice::ReadOnly));
    CHECK(QString::fromUtf8(file.readAll()) == QStringLiteral("hello world"));
}
