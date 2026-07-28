/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwExternalStationHarvest.h"

// Test helpers
#include "LoadProjectHelper.h"

// Qt
#include <QDir>
#include <QStringList>
#include <QTemporaryDir>

namespace {

QString externalCenterlinePath(const QString& fileName)
{
    // The in-source path, so the entry file can reach its sibling fixtures
    // (MAIN.SRV next to walls_simple.wpj, for one).
    return testcasesDatasetSourcePath(QStringLiteral("external-centerlines/%1").arg(fileName));
}

QStringList harvestNames(const QString& fileName)
{
    const auto result = cwExternalStationHarvest::harvest(externalCenterlinePath(fileName));
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    return result.value();
}

} // namespace

TEST_CASE("Harvest names a survey nothing fixes", "[ExternalStationHarvest]")
{
    // The whole point of the harvest: cavern drops this survey from a region
    // solve, so these names exist nowhere else.
    CHECK(harvestNames(QStringLiteral("survex_hanging.svx"))
          == QStringList({QStringLiteral("hanging.h1"),
                          QStringLiteral("hanging.h2"),
                          QStringLiteral("hanging.h3")}));
}

TEST_CASE("Harvest names a self-fixing survey the same way", "[ExternalStationHarvest]")
{
    // survex_simple.svx fixes A1 itself, so netskel's implicit origin fix never
    // fires — the harvest has to yield the same names either way.
    CHECK(harvestNames(QStringLiteral("survex_simple.svx"))
          == QStringList({QStringLiteral("simple.a1"),
                          QStringLiteral("simple.a2"),
                          QStringLiteral("simple.a3")}));
}

TEST_CASE("Harvest names a Compass survey", "[ExternalStationHarvest]")
{
    // Cavern reads .dat through *include, so the harvest covers Compass without
    // knowing anything about the format. compass_solvable.dat rather than the
    // other .dat fixtures: those exist for the scanner, which treats a .dat as
    // an opaque leaf, so their shot data is deliberately not real Compass and
    // cavern rejects it.
    // Bare names, not "s.s1": cavern's Compass and Walls readers don't turn the
    // file's own survey names into naming levels the way "*begin" does. The
    // region solve sees the same bare tails, which is what matters.
    CHECK(harvestNames(QStringLiteral("compass_solvable.dat"))
          == QStringList({QStringLiteral("s1"),
                          QStringLiteral("s2"),
                          QStringLiteral("s3")}));
}

TEST_CASE("Harvest names a Walls project", "[ExternalStationHarvest]")
{
    // walls_simple.wpj names MAIN.SRV, which the harvest reaches because the
    // driver *includes the entry by absolute path — nested references still
    // resolve against the entry's own directory.
    CHECK(harvestNames(QStringLiteral("walls_simple.wpj"))
          == QStringList({QStringLiteral("a1"),
                          QStringLiteral("a2"),
                          QStringLiteral("a3"),
                          QStringLiteral("a4")}));
}

TEST_CASE("Harvest reports cavern's error for a broken file", "[ExternalStationHarvest]")
{
    const auto result =
        cwExternalStationHarvest::harvest(externalCenterlinePath(QStringLiteral("broken.svx")));

    CHECK(result.hasError());
    CHECK(result.value().isEmpty());

    // The error is cavern's own log text, which is what makes the failure
    // attributable to this one file.
    INFO(result.errorMessage().toStdString());
    CHECK_FALSE(result.errorMessage().isEmpty());
}

TEST_CASE("Harvest reports an entry file that isn't there", "[ExternalStationHarvest]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto result = cwExternalStationHarvest::harvest(
        QDir(directory.path()).filePath(QStringLiteral("not_on_disk.svx")));

    CHECK(result.hasError());
    CHECK(result.value().isEmpty());
    CHECK(result.errorMessage().contains(QStringLiteral("not_on_disk.svx")));
}

TEST_CASE("Harvest of an entry with a missing *include target is deterministic",
          "[ExternalStationHarvest]")
{
    const QString path = externalCenterlinePath(QStringLiteral("survex_with_missing_include.svx"));

    // Cavern can't open the *include target, so the run fails. Two runs use two
    // throwaway drivers in two temp directories, so an unfiltered log would
    // make the same failure read as a different one every time.
    const auto first = cwExternalStationHarvest::harvest(path);
    const auto second = cwExternalStationHarvest::harvest(path);

    CHECK(first.hasError());
    CHECK(first.value() == second.value());
    CHECK(first.errorMessage().toStdString() == second.errorMessage().toStdString());
    CHECK(first.errorCode() == second.errorCode());

    // The message points at the entry file, at the line that names the missing
    // target — the whole reason for attributing the failure per file.
    CHECK(first.errorMessage().contains(QStringLiteral("survex_with_missing_include.svx:9")));
    CHECK(first.errorMessage().contains(QStringLiteral("not_on_disk.svx")));
    CHECK_FALSE(first.errorMessage().contains(QStringLiteral("harvest.svx")));
}
