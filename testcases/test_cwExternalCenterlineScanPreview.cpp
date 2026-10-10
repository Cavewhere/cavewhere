/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwExternalCenterlineScanPreview.h"
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDate>
#include <QDir>
#include <QTemporaryDir>

namespace {

// Bounded event-loop settle used to prove a discarded scan does NOT
// surface - long enough for the near-instant fixture scans to land.
constexpr int kStaleSettleMs = 500;

} // namespace

TEST_CASE("scan preview reports format, counts, warnings, and errors per path",
          "[ScanPreview]")
{
    cwExternalCenterlineScanPreview preview;
    CHECK(preview.sourcePath().isEmpty());
    CHECK(preview.formatName().isEmpty());
    CHECK_FALSE(preview.scanning());
    CHECK_FALSE(preview.valid());

    // Clean fixture: valid, one file, no warnings.
    preview.setSourcePath(fixturePath(QStringLiteral("survex_simple.svx")));
    CHECK(preview.formatName() == QStringLiteral("Survex"));
    CHECK(preview.scanning());
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    CHECK(preview.valid());
    CHECK(preview.fileCount() == 1);
    CHECK(preview.warnings().isEmpty());
    CHECK(preview.errorMessage().isEmpty());

    // Missing *include: still valid, with a warning (q18).
    preview.setSourcePath(fixturePath(QStringLiteral("survex_with_missing_include.svx")));
    CHECK_FALSE(preview.valid()); // the stale result clears immediately
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    CHECK(preview.valid());
    REQUIRE_FALSE(preview.warnings().isEmpty());
    CHECK(preview.warnings().first().contains(QStringLiteral("missing *include")));

    // Nonexistent file: hard error, not valid.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    preview.setSourcePath(QDir(tempDir.path()).filePath(QStringLiteral("nope.svx")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    CHECK_FALSE(preview.valid());
    CHECK_FALSE(preview.errorMessage().isEmpty());

    // Clearing the path clears everything without a scan.
    preview.setSourcePath(QString());
    CHECK_FALSE(preview.scanning());
    CHECK_FALSE(preview.valid());
    CHECK(preview.errorMessage().isEmpty());
    CHECK(preview.formatName().isEmpty());
}

TEST_CASE("scan preview supersedes in-flight scans on rapid path changes",
          "[ScanPreview]")
{
    cwExternalCenterlineScanPreview preview;

    // Two paths in one event-loop turn: only the newest result may
    // land. The fixtures are distinguishable - survex_simple has no
    // warnings, the missing-include fixture always warns.
    preview.setSourcePath(fixturePath(QStringLiteral("survex_simple.svx")));
    preview.setSourcePath(fixturePath(QStringLiteral("survex_with_missing_include.svx")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    CHECK(preview.valid());
    REQUIRE_FALSE(preview.warnings().isEmpty());
    CHECK(preview.warnings().first().contains(QStringLiteral("missing *include")));

    // Nonempty -> empty -> nonempty in one turn: the cleared state
    // must not poison the follow-up scan (a restarter-future cancel
    // here used to leave the queued start tracking a canceled
    // deferred, stranding scanning == true forever).
    preview.setSourcePath(fixturePath(QStringLiteral("survex_with_missing_include.svx")));
    preview.setSourcePath(QString());
    preview.setSourcePath(fixturePath(QStringLiteral("survex_simple.svx")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    CHECK(preview.valid());
    CHECK(preview.fileCount() == 1);
    CHECK(preview.warnings().isEmpty());

    // Nonempty -> empty in one turn: ends cleared, never scanning,
    // and the stale scan's result (or an empty-path error) must not
    // surface afterwards. The bounded pump (predicate never true)
    // gives the discarded scan every chance to land wrongly.
    preview.setSourcePath(fixturePath(QStringLiteral("survex_with_missing_include.svx")));
    preview.setSourcePath(QString());
    CHECK_FALSE(preview.scanning());
    tryWait(kStaleSettleMs, [] { return false; });
    CHECK_FALSE(preview.scanning());
    CHECK_FALSE(preview.valid());
    CHECK(preview.errorMessage().isEmpty());
    CHECK(preview.warnings().isEmpty());
}

TEST_CASE("scan preview exposes the block tree and the multi-cave signals",
          "[ScanPreview]")
{
    cwExternalCenterlineScanPreview preview;
    CHECK(preview.blocks().isEmpty());
    CHECK(preview.topLevelBlockCount() == 0);
    CHECK_FALSE(preview.entryHasOwnShots());
    CHECK(preview.entryDirectIncludes().isEmpty());

    // A single cave with a nested block tree.
    preview.setSourcePath(fixturePath(QStringLiteral("survex_blocks.svx")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    REQUIRE(preview.valid());
    CHECK(preview.blocks().size() == 4);
    CHECK(preview.blocks().first().path == QStringLiteral("doghill"));
    CHECK(preview.topLevelBlockCount() == 1);
    CHECK(preview.rootStationCount() == 0);
    CHECK(preview.entryHasOwnShots());
    CHECK(preview.entryDirectIncludes().isEmpty());

    // A region master: several top-level blocks, no shots of its own -
    // the multi-cave heuristic's trigger.
    preview.setSourcePath(fixturePath(QStringLiteral("survex_master/master.svx")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    REQUIRE(preview.valid());
    CHECK(preview.blocks().size() == 3);
    CHECK(preview.topLevelBlockCount() == 3);
    CHECK_FALSE(preview.entryHasOwnShots());
    CHECK(preview.entryDirectIncludes().size() == 3);

    // A Compass project: no blocks, every station in the root, so
    // the dialog's whole-file row is the only thing to show.
    preview.setSourcePath(fixturePath(QStringLiteral("compass_multi.mak")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    REQUIRE(preview.valid());
    CHECK(preview.blocks().isEmpty());
    CHECK(preview.rootStationCount() == 5);
    CHECK(preview.rootDate() == QDate(2025, 1, 1));

    // Clearing the path wipes the block tree with everything else.
    preview.setSourcePath(QString());
    CHECK(preview.blocks().isEmpty());
    CHECK(preview.topLevelBlockCount() == 0);
    CHECK(preview.rootStationCount() == 0);
    CHECK_FALSE(preview.rootDate().isValid());
    CHECK_FALSE(preview.entryHasOwnShots());
    CHECK(preview.entryDirectIncludes().isEmpty());
}

TEST_CASE("scan preview exposes the tree's rows and summary counts for compass_project",
          "[ScanPreview]")
{
    cwExternalCenterlineScanPreview preview;
    CHECK(preview.rows().isEmpty());
    CHECK(preview.surveyCount() == 0);
    CHECK(preview.stationCount() == 0);
    CHECK(preview.missingFileCount() == 0);

    preview.setSourcePath(fixturePath(QStringLiteral("compass_project/PROJECT.MAK")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    REQUIRE(preview.valid());

    // The .mak and its three present DATs.
    CHECK(preview.fileCount() == 4);
    // ALPHA1, ALPHA2, BRAVO1, CHARLIE1.
    CHECK(preview.surveyCount() == 4);
    // Distinct cavern-relative names: alpha's 7, bravo's 4 (its link
    // station bravo.A5 included), charlie's 3.
    CHECK(preview.stationCount() == 14);
    CHECK(preview.missingFileCount() == 1);

    const QList<cwScanRow> rows = preview.rows();
    REQUIRE(rows.size() == 9);
    CHECK(rows.at(0).kind == cwSurveyNodeKind::Kind::CompassProject);
    CHECK(rows.at(0).displayName == QStringLiteral("PROJECT"));
    CHECK(preview.kindLabel(rows.at(0)) == QStringLiteral("Compass project"));

    CHECK(rows.at(1).displayName == QStringLiteral("Alpha"));
    CHECK(rows.at(1).depth == 1);
    CHECK(rows.at(1).stationCount == 7);
    CHECK(preview.kindLabel(rows.at(1)) == QStringLiteral("Compass file"));

    CHECK(rows.at(2).displayName == QStringLiteral("ALPHA1"));
    CHECK(rows.at(2).depth == 2);
    CHECK(preview.kindLabel(rows.at(2)) == QStringLiteral("Trip"));

    const cwScanRow delta = rows.constLast();
    CHECK(delta.displayName == QStringLiteral("Delta"));
    CHECK(delta.missing);
    CHECK_FALSE(delta.hasSurveyContent);

    // Clearing the path wipes the rows and the counts with everything else.
    preview.setSourcePath(QString());
    CHECK(preview.rows().isEmpty());
    CHECK(preview.surveyCount() == 0);
    CHECK(preview.stationCount() == 0);
    CHECK(preview.missingFileCount() == 0);
}
