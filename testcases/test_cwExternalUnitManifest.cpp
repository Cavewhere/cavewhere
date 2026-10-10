/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwExternalCenterlineScanner.h"
#include "cwExternalUnitManifest.h"
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDate>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using cwSurveyNodeKind::Kind;

TEST_CASE("units.json round-trips the scan rows with canonical station names",
          "[Manifest]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = QDir(dir.path()).filePath(cwExternalUnitManifest::fileName());
    CHECK(cwExternalUnitManifest::fileName() == QStringLiteral("units.json"));

    cwScanRow root;
    root.kind = Kind::SurvexFile;
    root.displayName = QStringLiteral("Master");
    root.isNode = true;
    root.isUnit = true;
    root.hasSurveyContent = true;
    root.stationCount = 4;
    root.date = QDate(2024, 1, 5);
    root.stations = {QStringLiteral("R1")};

    cwScanRow block;
    block.kind = Kind::SurvexBlock;
    block.sourcePath = QStringLiteral("Big-Passage");
    block.displayName = QStringLiteral("Big-Passage");
    block.cavernLabel = QStringLiteral("Big-Passage");
    block.depth = 1;
    block.isNode = true;
    block.isUnit = true;
    block.hasSurveyContent = true;
    block.stationCount = 3;
    // Dotted, mixed-case, and two spellings of one station.
    block.stations = {QStringLiteral("Big-Passage.P1"), QStringLiteral("Big-Passage.east.E1"),
                      QStringLiteral("big-passage.p1"), QStringLiteral("Big-Passage.A2")};

    cwScanRow missing;
    missing.kind = Kind::CompassFile;
    missing.sourcePath = QStringLiteral("Big-Passage/DELTA.DAT");
    missing.displayName = QStringLiteral("Delta");
    missing.cavernLabel = QStringLiteral("delta");
    missing.depth = 2;
    missing.isNode = true;
    missing.missing = true;

    REQUIRE_FALSE(cwExternalUnitManifest::write(path, {root, block, missing}).hasError());

    const auto read = cwExternalUnitManifest::read(path);
    REQUIRE_FALSE(read.hasError());

    // Every field survives; each unit's names come back lower-cased,
    // de-duplicated, and sorted, as cwExternalStationHarvest spells them.
    cwScanRow expectedRoot = root;
    expectedRoot.stations = {QStringLiteral("r1")};
    cwScanRow expectedBlock = block;
    expectedBlock.stations = {QStringLiteral("big-passage.a2"),
                              QStringLiteral("big-passage.east.e1"),
                              QStringLiteral("big-passage.p1")};
    const QList<cwScanRow> expected = {expectedRoot, expectedBlock, missing};
    CHECK(read.value() == expected);

    // Writing what was read changes nothing.
    const QString again = QDir(dir.path()).filePath(QStringLiteral("again.json"));
    REQUIRE_FALSE(cwExternalUnitManifest::write(again, read.value()).hasError());
    const auto readAgain = cwExternalUnitManifest::read(again);
    REQUIRE_FALSE(readAgain.hasError());
    CHECK(readAgain.value() == expected);
}

TEST_CASE("units.json round-trips a scanned Compass project", "[Manifest]")
{
    auto scan = cwExternalCenterlineScanner::scan(
        fixturePath(QStringLiteral("compass_project/PROJECT.MAK")));
    REQUIRE_FALSE(scan.hasError());
    const QList<cwScanRow> rows = scan.value().rows;
    REQUIRE(rows.size() == 9);

    const auto read = cwExternalUnitManifest::fromJson(cwExternalUnitManifest::toJson(rows));
    REQUIRE_FALSE(read.hasError());
    REQUIRE(read.value().size() == rows.size());
    for (qsizetype i = 0; i < rows.size(); ++i) {
        cwScanRow expected = rows.at(i);
        for (QString& station : expected.stations) {
            station = station.toLower();
        }
        CHECK(read.value().at(i) == expected);
    }
    // The station ALPHA's two surveys share reads back in both units.
    CHECK(read.value().at(2).stations.contains(QStringLiteral("alpha.a5")));
    CHECK(read.value().at(3).stations.contains(QStringLiteral("alpha.a5")));
}

TEST_CASE("units.json refuses a missing file, malformed JSON, and an unknown kind",
          "[Manifest]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    CHECK(cwExternalUnitManifest::read(QDir(dir.path()).filePath(QStringLiteral("absent.json")))
              .hasError());
    CHECK(cwExternalUnitManifest::fromJson(QByteArrayLiteral("not json")).hasError());
    CHECK(cwExternalUnitManifest::fromJson(QByteArrayLiteral(R"({"version": 99, "rows": []})"))
              .hasError());
    CHECK(cwExternalUnitManifest::fromJson(
              QByteArrayLiteral(R"({"version": 1, "rows": [{"kind": "Nonsense"}]})"))
              .hasError());
}
