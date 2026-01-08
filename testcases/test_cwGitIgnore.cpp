//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

//Our includes
#include "cwGitIgnore.h"

static QString readGitIgnore(const QDir& dir)
{
    QFile file(dir.filePath(QStringLiteral(".gitignore")));
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

TEST_CASE("ensureGitIgnoreHasCacheEntry writes .cw_cache entry", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());

    cw::git::ensureGitIgnoreHasCacheEntry(repoDir);
    const QString contents = readGitIgnore(repoDir);
    CHECK(contents.contains(".cw_cache/"));
}

TEST_CASE("ensureGitIgnoreHasCacheEntry is idempotent", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());

    cw::git::ensureGitIgnoreHasCacheEntry(repoDir);
    cw::git::ensureGitIgnoreHasCacheEntry(repoDir);

    const QString contents = readGitIgnore(repoDir);
    const QStringList lines = contents.split('\n');
    int count = 0;
    for (const QString& line : lines) {
        if (line.trimmed() == QStringLiteral(".cw_cache/")) {
            count++;
        }
    }
    CHECK(count == 1);
}

TEST_CASE("ensureGitIgnoreHasCacheEntry respects existing .cw_cache entry", "[cwGitIgnore]") {
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QDir repoDir(tmpDir.path());

    QFile file(repoDir.filePath(QStringLiteral(".gitignore")));
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(".cw_cache\n");
    file.close();

    cw::git::ensureGitIgnoreHasCacheEntry(repoDir);

    const QString contents = readGitIgnore(repoDir);
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
