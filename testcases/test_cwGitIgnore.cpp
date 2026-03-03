//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

//Our includes
#include "cwSaveLoad.h"

static QString readGitExclude(const QDir& dir)
{
    QFile file(dir.filePath(QStringLiteral(".git/info/exclude")));
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

TEST_CASE("ensureGitExcludeHasLocalEntries writes local sync excludes", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());
    REQUIRE(QDir().mkpath(repoDir.filePath(QStringLiteral(".git/info"))));

    cwSaveLoad::ensureGitExcludeHasLocalEntries(repoDir);
    const QString contents = readGitExclude(repoDir);
    CHECK(contents.contains(".cw_cache/"));
    CHECK(contents.contains(".DS_Store"));
}

TEST_CASE("ensureGitExcludeHasLocalEntries is idempotent", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());
    REQUIRE(QDir().mkpath(repoDir.filePath(QStringLiteral(".git/info"))));

    cwSaveLoad::ensureGitExcludeHasLocalEntries(repoDir);
    cwSaveLoad::ensureGitExcludeHasLocalEntries(repoDir);

    const QString contents = readGitExclude(repoDir);
    const QStringList lines = contents.split('\n');
    int cacheCount = 0;
    int dsStoreCount = 0;
    for (const QString& line : lines) {
        if (line.trimmed() == QStringLiteral(".cw_cache/")) {
            cacheCount++;
        }
        if (line.trimmed() == QStringLiteral(".DS_Store")) {
            dsStoreCount++;
        }
    }
    CHECK(cacheCount == 1);
    CHECK(dsStoreCount == 1);
}

TEST_CASE("ensureGitExcludeHasLocalEntries respects existing entries", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());

    REQUIRE(QDir().mkpath(repoDir.filePath(QStringLiteral(".git/info"))));
    QFile file(repoDir.filePath(QStringLiteral(".git/info/exclude")));
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(".cw_cache\n");
    file.write(".DS_Store\n");
    file.close();

    cwSaveLoad::ensureGitExcludeHasLocalEntries(repoDir);

    const QString contents = readGitExclude(repoDir);
    CHECK(contents.contains(".cw_cache"));
    CHECK(contents.contains(".DS_Store"));
    const QStringList lines = contents.split('\n');
    int cacheCount = 0;
    int dsStoreCount = 0;
    for (const QString& line : lines) {
        if (line.trimmed() == QStringLiteral(".cw_cache") || line.trimmed() == QStringLiteral(".cw_cache/")) {
            cacheCount++;
        }
        if (line.trimmed() == QStringLiteral(".DS_Store")) {
            dsStoreCount++;
        }
    }
    CHECK(cacheCount == 1);
    CHECK(dsStoreCount == 1);
}
