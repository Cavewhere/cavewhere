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

TEST_CASE("ensureGitExcludeHasCacheEntry writes .cw_cache entry", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());
    REQUIRE(QDir().mkpath(repoDir.filePath(QStringLiteral(".git/info"))));

    cwSaveLoad::ensureGitExcludeHasCacheEntry(repoDir);
    const QString contents = readGitExclude(repoDir);
    CHECK(contents.contains(".cw_cache/"));
}

TEST_CASE("ensureGitExcludeHasCacheEntry is idempotent", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());
    REQUIRE(QDir().mkpath(repoDir.filePath(QStringLiteral(".git/info"))));

    cwSaveLoad::ensureGitExcludeHasCacheEntry(repoDir);
    cwSaveLoad::ensureGitExcludeHasCacheEntry(repoDir);

    const QString contents = readGitExclude(repoDir);
    const QStringList lines = contents.split('\n');
    int count = 0;
    for (const QString& line : lines) {
        if (line.trimmed() == QStringLiteral(".cw_cache/")) {
            count++;
        }
    }
    CHECK(count == 1);
}

TEST_CASE("ensureGitExcludeHasCacheEntry respects existing .cw_cache entry", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());

    REQUIRE(QDir().mkpath(repoDir.filePath(QStringLiteral(".git/info"))));
    QFile file(repoDir.filePath(QStringLiteral(".git/info/exclude")));
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(".cw_cache\n");
    file.close();

    cwSaveLoad::ensureGitExcludeHasCacheEntry(repoDir);

    const QString contents = readGitExclude(repoDir);
    CHECK(contents.contains(".cw_cache"));
    const QStringList lines = contents.split('\n');
    int count = 0;
    for (const QString& line : lines) {
        if (line.trimmed() == QStringLiteral(".cw_cache") || line.trimmed() == QStringLiteral(".cw_cache/")) {
            count++;
        }
    }
    CHECK(count == 1);
}
