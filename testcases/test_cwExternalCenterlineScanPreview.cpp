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
