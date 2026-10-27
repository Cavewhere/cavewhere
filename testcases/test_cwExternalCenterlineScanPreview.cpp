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
#include "cwCave.h"
#include "cwNote.h"
#include "cwNoteStation.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDate>
#include <QDir>
#include <QTemporaryDir>
#include <QUuid>

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

TEST_CASE("scan preview counts the files a Compass project reads",
          "[ScanPreview]")
{
    cwExternalCenterlineScanPreview preview;
    CHECK(preview.fileCount() == 0);

    preview.setSourcePath(fixturePath(QStringLiteral("compass_project/PROJECT.MAK")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    REQUIRE(preview.valid());

    // The .mak and its three present DATs.
    CHECK(preview.fileCount() == 4);

    preview.setSourcePath(QString());
    CHECK(preview.fileCount() == 0);
}

namespace {

// A note on \a trip holding one scrap per entry of \a scrapStations.
void addNoteWithScraps(cwTrip* trip, const QList<QStringList>& scrapStations)
{
    auto* note = new cwNote();
    trip->notes()->addNotes({note});
    for (const QStringList& names : scrapStations) {
        auto* scrap = new cwScrap();
        note->addScrap(scrap);
        QList<cwNoteStation> stations;
        for (const QString& name : names) {
            cwNoteStation station;
            station.setName(name);
            stations.append(station);
        }
        scrap->setStations(stations);
    }
}

} // namespace

TEST_CASE("scan preview counts the notes and scraps a Replace strands",
          "[ScanPreview]")
{
    // A source root holding one derived trip and one native trip.
    cwCave root;
    root.setSourceId(QUuid::createUuid());
    root.setReadOnly(true);

    auto* derived = new cwTrip();
    derived->setSourcePath(QStringLiteral("ALPHA.DAT/ALPHA1"));
    root.addTrip(derived);
    auto* native = new cwTrip();
    root.addTrip(native);

    // The first note keeps both scraps: every station is in compass_project,
    // spelled in another case. The second loses one scrap of two to ZZ9,
    // which the file names nowhere. The third loses its one scrap.
    addNoteWithScraps(derived, {{QStringLiteral("ALPHA.A1"), QStringLiteral("alpha.a2")},
                                {QStringLiteral("bravo.B1")}});
    addNoteWithScraps(derived, {{QStringLiteral("alpha.A1"), QStringLiteral("ZZ9")},
                                {QStringLiteral("charlie.C1")}});
    addNoteWithScraps(derived, {{QStringLiteral("QQ1")}});
    // A native trip's scraps sit on its own chunks, which Replace leaves alone.
    addNoteWithScraps(native, {{QStringLiteral("QQ2")}});

    cwExternalCenterlineScanPreview preview;
    CHECK(preview.notesLosingAnchors(&root) == 0);
    CHECK(preview.scrapsLosingAnchors(&root) == 0);

    preview.setSourcePath(fixturePath(QStringLiteral("compass_project/PROJECT.MAK")));
    REQUIRE(tryWait(kWatcherWaitMs, [&] { return !preview.scanning(); }));
    REQUIRE(preview.valid());

    CHECK(preview.notesLosingAnchors(&root) == 2);
    CHECK(preview.scrapsLosingAnchors(&root) == 2);
    CHECK(preview.notesLosingAnchors(nullptr) == 0);
}
