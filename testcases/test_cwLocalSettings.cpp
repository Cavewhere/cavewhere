/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwLocalSettings.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

namespace {

constexpr const char* kLocalSettingsFileName = "local_settings.json";

QString tempProjectDir(const QTemporaryDir& tempDir, const QString& baseName)
{
    const QString suffixed = QStringLiteral("%1-%2.cwproj")
                                 .arg(baseName)
                                 .arg(QCoreApplication::applicationPid());
    const QString path = QDir(tempDir.path()).filePath(suffixed);
    QDir().mkpath(path);
    return path;
}

QString localSettingsPath(const QString& projectDirPath)
{
    return QDir(projectDirPath).absoluteFilePath(QLatin1String(kLocalSettingsFileName));
}

} // namespace

TEST_CASE("cwLocalSettings load on missing file returns empty settings",
          "[LocalSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString settingsPath = QDir(tempDir.path()).filePath(QStringLiteral("does-not-exist.json"));

    auto result = cwLocalSettings::load(settingsPath);
    REQUIRE_FALSE(result.hasError());
    const cwLocalSettings settings = result.value();
    CHECK(settings.isEmpty());
    CHECK(settings.externalCenterlineSources().isEmpty());
}

TEST_CASE("cwLocalSettings load on present file with no entry for owner falls back",
          "[LocalSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString settingsPath =
        localSettingsPath(tempProjectDir(tempDir, QStringLiteral("fallback")));

    cwLocalSettings written;
    const QUuid recordedOwner = QUuid::createUuid();
    const QString sourcePath = QDir(tempDir.path()).filePath(QStringLiteral("recorded.svx"));
    written.setSourcePath(recordedOwner, sourcePath);
    REQUIRE_FALSE(written.save(settingsPath).hasError());

    auto result = cwLocalSettings::load(settingsPath);
    REQUIRE_FALSE(result.hasError());
    const cwLocalSettings loaded = result.value();

    const QUuid unrelatedOwner = QUuid::createUuid();
    CHECK(loaded.sourcePathFor(unrelatedOwner).isEmpty());
    CHECK_FALSE(loaded.hasSource(unrelatedOwner));

    CHECK(loaded.sourcePathFor(recordedOwner) == sourcePath);
    CHECK(loaded.hasSource(recordedOwner));
}

TEST_CASE("cwLocalSettings round-trips an ExternalCenterlineSource entry",
          "[LocalSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString settingsPath =
        localSettingsPath(tempProjectDir(tempDir, QStringLiteral("round-trip")));

    cwLocalSettings written;
    const QUuid ownerId = QUuid::createUuid();
    const QString sourcePath = QDir(tempDir.path()).filePath(QStringLiteral("source.svx"));
    written.setSourcePath(ownerId, sourcePath);
    REQUIRE_FALSE(written.save(settingsPath).hasError());
    REQUIRE(QFileInfo::exists(settingsPath));

    auto reloadResult = cwLocalSettings::load(settingsPath);
    REQUIRE_FALSE(reloadResult.hasError());
    const cwLocalSettings reloaded = reloadResult.value();
    CHECK(reloaded == written);
    CHECK(reloaded.sourcePathFor(ownerId) == sourcePath);
}

TEST_CASE("cwLocalSettings keeps an entry whose sourcePath no longer exists on disk",
          "[LocalSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString settingsPath =
        localSettingsPath(tempProjectDir(tempDir, QStringLiteral("missing-source")));

    cwLocalSettings written;
    const QUuid ownerId = QUuid::createUuid();
    const QString missingSourcePath =
        QDir(tempDir.path()).filePath(QStringLiteral("definitely-not-here.svx"));
    REQUIRE_FALSE(QFileInfo::exists(missingSourcePath));
    written.setSourcePath(ownerId, missingSourcePath);
    REQUIRE_FALSE(written.save(settingsPath).hasError());

    auto reloadResult = cwLocalSettings::load(settingsPath);
    REQUIRE_FALSE(reloadResult.hasError());
    const cwLocalSettings reloaded = reloadResult.value();
    // The loader returns the entry as-is - surfacing the missing-source
    // banner is the consumer's responsibility, not the loader's.
    CHECK(reloaded.sourcePathFor(ownerId) == missingSourcePath);
    CHECK(reloaded.hasSource(ownerId));
}

TEST_CASE("cwLocalSettings::setSourcePath upserts existing owner without duplicating",
          "[LocalSettings]")
{
    cwLocalSettings settings;
    const QUuid ownerId = QUuid::createUuid();

    settings.setSourcePath(ownerId, QStringLiteral("/path/one.svx"));
    settings.setSourcePath(ownerId, QStringLiteral("/path/two.svx"));

    CHECK(settings.externalCenterlineSources().size() == 1);
    CHECK(settings.sourcePathFor(ownerId) == QStringLiteral("/path/two.svx"));
}

TEST_CASE("cwLocalSettings::clearSourcePath removes only the matching entry",
          "[LocalSettings]")
{
    cwLocalSettings settings;
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();

    settings.setSourcePath(first, QStringLiteral("/path/first.svx"));
    settings.setSourcePath(second, QStringLiteral("/path/second.svx"));
    REQUIRE(settings.externalCenterlineSources().size() == 2);

    settings.clearSourcePath(first);
    CHECK(settings.externalCenterlineSources().size() == 1);
    CHECK_FALSE(settings.hasSource(first));
    CHECK(settings.sourcePathFor(second) == QStringLiteral("/path/second.svx"));
}

TEST_CASE("cwLocalSettings emits hand-editable JSON",
          "[LocalSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString settingsPath =
        localSettingsPath(tempProjectDir(tempDir, QStringLiteral("hand-editable")));

    cwLocalSettings settings;
    const QUuid ownerId = QUuid::createUuid();
    const QString sourcePath = QStringLiteral("/Users/alice/projects/big-cave.svx");
    settings.setSourcePath(ownerId, sourcePath);
    REQUIRE_FALSE(settings.save(settingsPath).hasError());

    QFile file(settingsPath);
    REQUIRE(file.open(QFile::ReadOnly));
    const QByteArray contents = file.readAll();
    file.close();

    // Parse the file as plain JSON and assert the structure surfaces
    // camelCase field names a human user can edit.
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(contents, &parseError);
    REQUIRE(parseError.error == QJsonParseError::NoError);
    REQUIRE(doc.isObject());

    const QJsonObject root = doc.object();
    REQUIRE(root.contains(QStringLiteral("externalCenterlineSources")));
    const auto arr = root.value(QStringLiteral("externalCenterlineSources")).toArray();
    REQUIRE(arr.size() == 1);

    const QJsonObject entry = arr.first().toObject();
    CHECK(entry.value(QStringLiteral("ownerId")).toString()
          == ownerId.toString(QUuid::WithoutBraces));
    CHECK(entry.value(QStringLiteral("sourcePath")).toString() == sourcePath);
}

TEST_CASE("cwLocalSettings load on malformed JSON returns an error",
          "[LocalSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString settingsPath =
        localSettingsPath(tempProjectDir(tempDir, QStringLiteral("malformed")));
    REQUIRE(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    {
        QFile file(settingsPath);
        REQUIRE(file.open(QFile::WriteOnly));
        file.write("{ not-json");
    }

    auto result = cwLocalSettings::load(settingsPath);
    CHECK(result.hasError());
}

TEST_CASE("cwLocalSettings load drops entries with unparseable owner ids",
          "[LocalSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString settingsPath =
        localSettingsPath(tempProjectDir(tempDir, QStringLiteral("junk-owner")));
    REQUIRE(QDir().mkpath(QFileInfo(settingsPath).absolutePath()));

    // Hand-write a JSON file with one valid entry and one with a
    // garbage ownerId - the loader must keep the valid row and drop
    // the junk one.
    const QUuid validOwner = QUuid::createUuid();
    const QString validSource = QStringLiteral("/path/valid.svx");
    const QString json = QStringLiteral(
        R"({
  "externalCenterlineSources": [
    { "ownerId": "%1", "sourcePath": "%2" },
    { "ownerId": "not-a-uuid", "sourcePath": "/path/junk.svx" }
  ]
})").arg(validOwner.toString(QUuid::WithoutBraces), validSource);

    {
        QFile file(settingsPath);
        REQUIRE(file.open(QFile::WriteOnly));
        file.write(json.toUtf8());
    }

    auto result = cwLocalSettings::load(settingsPath);
    REQUIRE_FALSE(result.hasError());
    const cwLocalSettings loaded = result.value();
    CHECK(loaded.externalCenterlineSources().size() == 1);
    CHECK(loaded.sourcePathFor(validOwner) == validSource);
}
