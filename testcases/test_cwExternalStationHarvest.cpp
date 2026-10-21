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

TEST_CASE("Harvest loses the components netskel's one fix cannot reach",
          "[ExternalStationHarvest]")
{
    // A known limitation (issue #651), pinned so it is visible rather than
    // surprising, and so this fails loudly when it is fixed: netskel fixes
    // exactly one station when
    // nothing else is fixed, and articulate() then drops every component that
    // one station cannot reach. PartB's names come back missing, and — the part
    // that bites — cavern exits 0, so the harvest reports success.
    const auto result = cwExternalStationHarvest::harvest(
        externalCenterlinePath(QStringLiteral("survex_two_parts.svx")));

    INFO(result.errorMessage().toStdString());
    CHECK_FALSE(result.hasError());
    CHECK(result.value() == QStringList({QStringLiteral("parta.a1"),
                                         QStringLiteral("parta.a2"),
                                         QStringLiteral("parta.a3")}));
}

TEST_CASE("Harvest keeps cavern's reason when cavern blames the driver",
          "[ExternalStationHarvest]")
{
    // An entry file that exists but cannot be opened is reported against the
    // throwaway driver, not against the entry, so the driver-noise filter is the
    // only thing standing between the user and "This file has errors" with no
    // reason under it.
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const QString path = QDir(directory.path()).filePath(QStringLiteral("locked.svx"));
    {
        QFile file(path);
        REQUIRE(file.open(QFile::WriteOnly | QFile::Text));
        file.write("*begin L\n*data normal from to tape compass clino\nl1 l2 5.0 0 0\n*end L\n");
    }
    REQUIRE(QFile::setPermissions(path, QFile::Permissions()));

    const auto result = cwExternalStationHarvest::harvest(path);
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);

    CHECK(result.hasError());

    INFO(result.errorMessage().toStdString());
    // Matched without the apostrophe: cavern writes a typographic one (U+2019).
    CHECK(result.errorMessage().contains(QStringLiteral("open file")));
    // Still no throwaway driver in the message, and no temp path to make two
    // identical failures read differently.
    CHECK_FALSE(result.errorMessage().contains(QStringLiteral("harvest.svx")));
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
    // Stated rather than compared: two failed harvests both yield a default
    // QStringList, so comparing them to each other can never fail.
    CHECK(first.value().isEmpty());
    CHECK(second.value().isEmpty());
    CHECK(first.errorMessage().toStdString() == second.errorMessage().toStdString());
    CHECK(first.errorCode() == second.errorCode());

    // The message points at the entry file, at the line that names the missing
    // target — the whole reason for attributing the failure per file.
    CHECK(first.errorMessage().contains(QStringLiteral("survex_with_missing_include.svx:9")));
    CHECK(first.errorMessage().contains(QStringLiteral("not_on_disk.svx")));
    CHECK_FALSE(first.errorMessage().contains(QStringLiteral("harvest.svx")));
}

TEST_CASE("Harvest reads a file whose fix names a projected *cs and no *cs out",
          "[ExternalStationHarvest][OwnCsHarvest]")
{
    // The file names no *cs out, so the harvest finds the output system on
    // its own.
    const auto result = cwExternalStationHarvest::harvestWithFixes(
        externalCenterlinePath(QStringLiteral("survex_own_cs_no_out/own_cs_no_out.svx")));
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().names
          == QStringList({QStringLiteral("ridge.e1"),
                          QStringLiteral("ridge.e2"),
                          QStringLiteral("ridge.e3")}));
    CHECK(result.value().fixedNames == QStringList({QStringLiteral("ridge.e1")}));
}

TEST_CASE("Harvest reads a file whose fix names a geographic *cs and no *cs out",
          "[ExternalStationHarvest][OwnCsHarvest]")
{
    // cavern refuses a geographic *cs out, so the file's own system cannot
    // simply be repeated as the output.
    const auto result = cwExternalStationHarvest::harvestWithFixes(
        externalCenterlinePath(QStringLiteral("survex_own_cs_no_out/latlong_no_out.svx")));
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().names
          == QStringList({QStringLiteral("plateau.l1"), QStringLiteral("plateau.l2")}));
    CHECK(result.value().fixedNames == QStringList({QStringLiteral("plateau.l1")}));
}

TEST_CASE("Harvest reads a tree that mixes bare fixes with a fix under its own *cs",
          "[ExternalStationHarvest][OwnCsHarvest]")
{
    // The entry's bare fix comes first, before any *cs, so the output system
    // has to be in force before the file starts.
    const auto result = cwExternalStationHarvest::harvestWithFixes(
        externalCenterlinePath(QStringLiteral("survex_subtree_fixes/ridge.svx")));
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().fixedNames.contains(QStringLiteral("r0")));
    CHECK(result.value().fixedNames.contains(QStringLiteral("lower.l1")));
    CHECK(result.value().names.contains(QStringLiteral("empty.x2")));
}

TEST_CASE("Component harvest reads a file whose fix names its own *cs",
          "[ExternalStationHarvest][OwnCsHarvest]")
{
    const auto result = cwExternalStationHarvest::harvestComponents(
        externalCenterlinePath(QStringLiteral("survex_own_cs_no_out/own_cs_no_out.svx")),
        {QStringLiteral("ridge.e3")});
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value()
          == QStringList({QStringLiteral("ridge.e1"),
                          QStringLiteral("ridge.e2"),
                          QStringLiteral("ridge.e3")}));
}

TEST_CASE("Harvest still reads a file with only bare fixes and no *cs out",
          "[ExternalStationHarvest][OwnCsHarvest]")
{
    const auto result = cwExternalStationHarvest::harvestWithFixes(
        externalCenterlinePath(QStringLiteral("survex_bare_fix/bare_fix.svx")));
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().fixedNames == QStringList({QStringLiteral("bare.a")}));
}

TEST_CASE("Harvest reads a block whose fix names its own *cs",
          "[ExternalStationHarvest][OwnCsHarvest]")
{
    const auto result = cwExternalStationHarvest::harvestWithFixes(
        externalCenterlinePath(QStringLiteral("survex_native_cs_scope/own_cs_fix.svx")));
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value().names
          == QStringList({QStringLiteral("scoped.s1"), QStringLiteral("scoped.s2")}));
    CHECK(result.value().fixedNames == QStringList({QStringLiteral("scoped.s1")}));
}
