# Source breadcrumb in a local project store (SB.1, SB.2)

Written 2026-10-23, revised 2026-10-24 after the owner settled storage, after
profiling the remaining CKKC attach hang on top of MP.11 (MT.1 to MT.6,
commits 2a7f290d..dd93b79f).

## 0. The measurement

The owner's profile of a CKKC.wpj attach (3.56 min wall clock) puts 3.40 min
(97.4%) in one synchronous call at the end of the fill:

    cwSourceSubtreeFill::finish -> completeAttach
      -> cwExternalSourceSettings::setBreadcrumb
        -> writeFingerprint -> QSettings::setValue (x4 per source file)
          -> cfprefsd -> __CFPREFERENCES_HAS_DETECTED_THIS_APP_TRYING_TO_STORE_TOO_MUCH_DATA__

Everything else in the profile (fill, node inserts, tree model) is under 100 ms.

Why: the breadcrumb (absolute path of the original external file) and the
source fingerprint (per dependency file: canonical absolute path, size, mtime,
sha256) are written to QSettings. On macOS QSettings is the app's preferences
plist, held by cfprefsd as one document. CKKC has 1294 survey files, so one
attach writes about 5200 keys. Past CoreFoundation's size threshold every
setValue re-fetches and re-parses the whole plist. Nothing removed old owners,
so 57 attaches had accumulated: 79,520 keys, 10.6 MB, each attach slower than
the last.

The owner cleaned the plist by hand on 2026-10-23 (back to 232 keys / 20 KB)
and deleted 18,523 leftover per-process test plists from ~/Library/Preferences.
No migration code is needed: this code has run on no other machine.

## 1. Owner decisions (binding)

1. **The record lives in a hidden, git-excluded directory in the project
   root**, `.cw_local/`, beside `.cw_cache/` and `.cw_trash/`. One binary
   protobuf file per owner (source root node or trip), named by the owner's
   id: `.cw_local/external-sources/<owner uuid>.pb`. Rationale, from the
   2026-10-24 discussion: the paths are machine-specific and must never
   sync, so they stay out of `.cwcave`/`.cwtrip` (rejected: in-project
   proto fields) and out of git. A per-machine app-data store was rejected
   because it needs garbage collection that cannot tell a deleted project
   from an unmounted volume or a renamed folder. `.cw_cache` was rejected
   because the record is a fact about the attach, not something recomputable.
   In the project root the record is deleted, moved, renamed and copied with
   the project by the file system, so no sweep exists.
2. **The bundle carries it.** `.cw_local/` is excluded from git but
   INCLUDED in the bundled `.cw` zip (unlike `.cw_cache` and `.cw_trash`),
   so a bundled project keeps its record across close and reopen (a bundle
   is extracted to a fresh temporary directory on every open). The accepted
   cost: a `.cw` sent to a collaborator carries the attacher's source
   directory path. A collaborator sees missing-source status at recompute
   time and the file dialog opens at the nearest existing folder, the
   behavior already accepted for a clone.
3. **Record shape**: the breadcrumb path (absolute path of the entry file),
   the fingerprint base directory (absolute, the canonical directory the
   scan's dependencies are relative to; `copyContentKey` in
   cwExternalCenterlineManager.cpp already keys relative to the attachment
   dir), and one record per dependency with a path RELATIVE to that base,
   size, last modified, sha256. One absolute directory per record, not 1300
   absolute paths. Readers that need absolute paths join base + relative.
4. **Format is protobuf**, a new message in `cavewhere.proto` (sibling of
   `PointOctreeManifest`, the `.cw_cache` precedent). Proto stays simple
   between releases: no reserved fields, no migration, no compatibility read
   of the old QSettings keys. `ExternalCenterline` in the node and trip
   messages is unchanged.
5. **QSettings holds no breadcrumb or fingerprint data after this.** The
   `externalCenterlineSources` group is gone from the code.
6. **Lifecycle**: written on attach, Reload and Replace (Replace overwrites);
   deleted by the existing `ownersDeleted` relay
   (cwExternalCenterlineManager::clearBreadcrumbsForDeletedOwners) and by
   `clearBreadcrumb`. Orphan files left by a discard that reverts an attach,
   or by a sync pull that deletes a root, are accepted (an id is never
   reused; a record is at most a few hundred KB). No reconcile-on-open, no
   age-out, no path registry.
7. **No undo step** for anything here (undo is hidden in this version).
8. **Main-thread budget applies** (CLAUDE.md): nothing at attach completion
   or status read may scale with the number of source files on the main
   thread. The fingerprint is computed on the worker today; serialize and
   write the file (QSaveFile, atomic) on that same worker. Comparison
   against the stored fingerprint is at most O(files) of in-memory compares.
   Queued passes use `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`
   behind a pending flag, never a `QTimer`.
9. **Test harness settings go to a per-process temporary directory** so test
   runs leave nothing in ~/Library/Preferences.

## 2. Items

### SB.1  The source breadcrumb and fingerprint live in `.cw_local/` and QSettings holds none of it

- `cavewhere.proto`: new top-level message (for example `ExternalSourceRecord`
  with `breadcrumb_path`, `base_dir`, repeated `files { path, size,
  last_modified, sha256 }`). Binary serialization, one file per owner at
  `<project root>/.cw_local/external-sources/<owner uuid>.pb`.
- Store location follows the project root. `cwExternalSourceSettings` (owned
  by cwRootData, exposed to QML as `externalSourceSettings`) gets the project
  root directory from cwSaveLoad/cwProject and follows it through load,
  `transferProjectTo` (Save As move/copy, which changes the root) and bundle
  extraction. The investigation names the signal to follow
  (`cwSaveLoad::fileNameChanged` / `dataRootChanged` / cwProject's filename
  change) and verifies the temporary project of a new, unsaved project has a
  root directory from the start.
- Exclusions, one list drives both (cwSaveLoad.cpp, anonymous namespace
  around line 1044: kCacheDirPattern, kTrashDirPattern,
  `ensureGitExcludeHasLocalEntries`, `readGitExcludePatterns`,
  `saveBundledArchive` ~2667, `copyDirectoryRecursively` in
  cwSaveLoadPrivate.cpp ~126): add `.cw_local/` to the git exclude entries;
  the bundle zip currently excludes exactly the git exclude patterns, so the
  zip step must REMOVE `.cw_local/` from the excludes it applies (the zip
  keeps `.cw_local/`, git ignores it). `copyDirectoryRecursively` already
  copies hidden directories (it skips only the trash); a Move renames the
  whole root. `discardChanges` is `git reset --hard HEAD`, which leaves
  ignored files alone: confirm with a test rather than by reading.
- `cwExternalSourceSettings` keeps its QML-facing surface (`breadcrumbPath`,
  `hasBreadcrumb`, `breadcrumbFolder`, `clearBreadcrumb`, `breadcrumbsChanged`,
  `fingerprint`, `setBreadcrumb`, `computeFingerprint`) and becomes the
  facade over the store: an in-memory map per owner, loaded from the files
  (lazily per owner, or once per project on a worker; the investigation
  decides, with the main-thread budget), written through on set, removed on
  clear. Every reader keeps working: cwExternalCenterlineAttach.cpp
  completeAttach (~253-305, computeFingerprint at ~260) and the
  temporary/cave/replace paths (~566-695), clearBreadcrumb at ~671 (trip)
  and ~718 (root); cwExternalCenterlineManager.cpp statsMatchDisk ~171,
  contentsMatch ~187, copyContentKey ~289, sourceStatusFor ~1443-1510
  (refreshFingerprintStats ~1480, fingerprint ~1506), reload ~1839,
  clearBreadcrumbsForDeletedOwners ~1542; QML
  ExternalCenterlineAttachedHeader.qml and the source root card.
- The write that cost 3.4 minutes moves off the main thread: completeAttach
  passes the computed fingerprint to the store, and the store serializes and
  writes on a worker (or the fingerprint worker writes the file itself and
  hands the facade the in-memory record). The main thread does an O(1)
  map insert and emits `breadcrumbsChanged`.
- `statsMatchDisk` stats every fingerprinted file. Report where it runs and
  how often; move it to a worker when the fix is contained in this item,
  otherwise record it as a follow-up with numbers.
- Relative paths: `computeFingerprint(files)` keeps producing the canonical
  absolute paths the scanner gives it; the store relativizes against the
  base on write and absolutizes on read, so in-memory consumers see what
  they see today. Which directory is the base (the entry file's directory,
  the attachment dir's source counterpart, or the common ancestor) is the
  investigation's call; it must be the same directory the scanner treats as
  the containment boundary (B7) so every dependency is below it.

Tests (fail today), tag suggestion `[Attach][Breadcrumb]`:
- `writeWallsProject(B=10, S=130, ...)` (about 1300 files) attached: after
  completion `QSettings` contains no key under `externalCenterlineSources`
  (today: ~5200 keys); `<root>/.cw_local/external-sources/<owner>.pb`
  exists, parses, holds the breadcrumb path, a base directory, and one
  record per dependency with a relative path; the project's git status
  shows nothing from `.cw_local/`.
- Reopen (close, open again) of a `.cwproj` and of a bundled `.cw` (Save As
  to `.cw`, close, open the `.cw`): the facade reports the same breadcrumb
  path and an equal fingerprint, read from the extracted or on-disk store
  (make the assertion unable to pass through QSettings: for example clear
  QSettings between the two opens, or check the new file exists under the
  reopened root).
- Save As of the attached `.cwproj` to a new `.cwproj`: the copy's root has
  the record; the original keeps its own.
- Delete the source root node: the `.pb` file is gone. `clearBreadcrumb`:
  the file is gone and `hasBreadcrumb` is false after reopen.
- Edit one source file after attach: `sourceStatusFor` reports Changed, as
  today (the existing [ExternalSourceStatus]/[ExternalSourceFingerprint]
  cases stay green).
- Secondary growth case: completion-time main-thread gap (longest event-loop
  gap during completeAttach, or a QSignalSpy count on a public signal) at
  S=130 vs S=520 at or below 5x.

Gate: `cavewhere-test "[Attach],[ExternalSourceStatus],[ExternalSourceWatch],[ExternalSourceFingerprint],[ExternalCenterline],[SaveLoad],[cwProject]"` (these tags exist) plus `tst_SourceRootPage.qml`,
`tst_ExternalCenterlinePanelComponents.qml`, `tst_DataMainPageTree.qml`.

### SB.2  Test harness settings live in a per-process temporary directory

- `testcases/main.cpp` and the QML test main: before any QSettings use, set
  `QSettings::setDefaultFormat(QSettings::IniFormat)` and
  `QSettings::setPath(IniFormat, UserScope, <QTemporaryDir kept for the
  process>)`, so no `com.cavewhere.cavewhere-test-<pid>.plist` is ever
  created in ~/Library/Preferences. The per-pid application name may stay
  for concurrency.
- Tests that swap the application name (test_cwColorBy, test_cwKeywordColorBy)
  must still isolate the way they intend.
- cwSettings::initialize and anything reading QSettings at startup keep
  working from the INI path.

Verification: run one small C++ tag and one QML file, then confirm no new
cavewhere test plist appeared in ~/Library/Preferences (quote the before and
after counts in the commit body).

Gate: `cavewhere-test "[cwRootData],[ColorBy]"` then `tst_ActiveTasks.qml`.

### SB.3  The source sweep and the attach verify pass run off the main thread

Filed in the body of 40d3c3b0 (SB.1). Measured there, synthetic Walls
fixture, Debug + ASAN: the attach-completion event-loop turn is 33 ms at
260 files and 107 ms at 1040 files AFTER SB.1 (97 ms and 398 ms before).
Both numbers are the quiet path.

Today (`cwExternalCenterlineManager.cpp`, line numbers drift, search for the
text):
- `sweepSources` (~1490) runs synchronously from five triggers: every
  `breadcrumbsChanged` (~559), every `QGuiApplication::focusWindowChanged`
  (~396), every scan apply (~1087), and both file-watcher slots (~1592,
  ~1597). Per attached owner it calls `sourceStatusFor` (~1440), which stats
  every fingerprinted file in `statsMatchDisk` (~171), then rebuilds the
  watch set through `collectSourceWatchPaths` and arms the
  `QFileSystemWatcher`. About 25 ms of raw syscalls per sweep at 1300 files.
- The quiet path is the small case. On any stat difference `sourceStatusFor`
  (~1474) calls `computeFingerprint` on EVERY file of that owner, reading
  and sha256-hashing all of them on the main thread, then writes the
  refreshed stats back through `refreshFingerprintStats` (~1480). A git
  checkout of the source tree, Walls re-saving its project, or a cloud
  client touching mtimes sends a 1300-file project down this path on the
  next focus change: seconds of UI freeze.
- `cwExternalCenterlineAttach.cpp` ~460: the attach's verify pass calls
  `cwExternalCenterlineSync::computePlan(scan, attachmentDir)` inside the
  `AsyncFuture::observe(reconcileFuture).context(saveLoad, ...)`
  continuation, which is main thread. `computePlan` stats every dependency
  and its destination copy, O(files) per attach completion.

Change:
- The sweep becomes: snapshot on the main thread (the attached owner ids
  from `m_lastScanRows`, each owner's stored fingerprint by value: records
  are value types) -> worker computes the rows and, where stats differ, the
  fresh fingerprint, and returns rows plus the desired watch sets plus any
  stat refreshes -> main thread, through
  `AsyncFuture::observe(future).context(this, ...)`, installs the rows,
  arms the watcher (addPaths/removePaths stay on the main thread, the
  `armPaths` shape is kept), and applies the stat refreshes to the store.
- One sweep in flight at a time, a pending flag and one queued re-run
  (`QMetaObject::invokeMethod(this, &..., Qt::QueuedConnection)`, never a
  `QTimer`), so a burst of focus changes or watcher events costs one sweep.
- A generation counter captured at dispatch and checked at install: an
  attach, Replace, Reload, clearBreadcrumb or scan apply that lands while a
  sweep is in flight bumps it, and the stale result is dropped (rows and
  stat refreshes alike). This is the one correctness hazard: without it a
  Replace's new record can be overwritten with stats for the file it
  replaced, or a Changed banner can show for a source just replaced.
- Rows for an owner that is gone by install time are dropped.
- The verify pass's `computePlan` moves onto the worker that already runs
  the reconcile (or a hop of its own), with the same guard against a
  deleted owner the hashing hop in completeAttach uses.
- `statsMatchDisk`, `contentsMatch`, `fingerprintedPaths`,
  `computeFingerprint` are free functions over value types and need no
  change to run on a worker.

Accepted behavior change: the source status model and the banner update
one hop after the trigger instead of in the same call. No QML binding
depends on the synchronous update (checked 2026-10-24: readers are the
banner, the attention rows and the Reload verb, all driven by the model's
own signals).

Tests, tag suggestion `[ExternalSourceStatus][Sweep]`:
- The eleven existing cases in test_cwExternalSourceStatus.cpp and
  test_cwExternalSourceWatch.cpp assume the sweep is complete when the
  trigger returns. Each gains a wait on a public signal (the status model's
  rows-changed/reset or a new `sweepFinished` NOTIFY on the manager), never
  a fixed `wait()`. Keep every assertion they make today.
- New: a sweep triggered N times in one event-loop turn (focus + watcher +
  breadcrumb) produces one worker pass (QSignalSpy on the finish signal
  counts 1, or at most 2 with the re-run).
- New: Replace during an in-flight sweep: the installed rows and the store's
  stats describe the replacement, never the replaced file (make the sweep
  slow with a large synthetic fixture, or stage a stat mismatch so the hash
  path runs, then Replace before it finishes).
- New: delete the owner during an in-flight sweep: no row for it is
  installed, no crash under ASAN.
- Growth pin: longest main-thread event-loop gap across a sweep in the hash
  path (every file touched) at S=130 vs S=520 at or below 2x (the main
  thread does O(owners) work, so growth with file count is the failure).
  Absolute timing is for the owner in the app.
- Attach completion: the existing growth case from SB.1 (same fixture
  sizes) now bounds the completion turn tighter; quote before/after in the
  commit body.

Gate: `cavewhere-test "[ExternalSourceStatus],[ExternalSourceWatch],[ExternalSourceFingerprint],[Attach],[Breadcrumb],[ExternalCenterline]"` plus `tst_ExternalCenterlineTripPanel.qml`, `tst_SourceRootPage.qml`, `tst_ExternalCenterlinePanelComponents.qml`.

### SB.4  The record types live in a plain header, and cwSaveLoad.h stops including the QML facade

Filed in the body of 40d3c3b0. Today `cwSaveLoad.h:44` includes
`cwExternalSourceSettings.h`, a `QML_ELEMENT` class whose header pulls in
`<QQmlEngine>`. It needs only the nested types and two static functions:
`cwExternalSourceSettings::Records` in `ProjectLoadData` (~157) and in the
`externalSourceRecordsLoaded` signal (~460), `::Record` in
`enqueueExternalSourceRecordWrite` (~374), and `readRecords` / `writeRecord`
from cwSaveLoad.cpp (~1678, ~4867). The facade's .cpp includes cwSaveLoad.h
back to enqueue its jobs, so the persistence core and the UI facade now
include each other, and every translation unit that includes cwSaveLoad.h
compiles the QML engine header.

Change:
- New `cwExternalSourceRecord.h` / `.cpp` (Qt core includes only): the
  structs now nested at cwExternalSourceSettings.h ~79 (`Breadcrumb`), ~91
  (`SourceFileFingerprint`), ~107 (`SourceFingerprint`), ~118 (`Record`),
  the `Records` alias (~126), and `readRecords` / `writeRecord` (~217,
  ~223) as free functions in a namespace (for example
  `cwExternalSourceRecord`). `computeFingerprint` goes with them if it has
  no dependency on the facade.
- cwSaveLoad.h and cwExternalSourceSettings.h both include the new header;
  cwSaveLoad.h no longer includes the facade. The facade keeps `using`
  aliases for the old nested names so its readers (cwExternalCenterlineAttach,
  cwExternalCenterlineManager, tests) compile unchanged, or the callers are
  swept in the same commit; either way the commit is behavior-free.
- Exit check: `grep -n "cwExternalSourceSettings" cavewherelib/src/cwSaveLoad.h`
  is empty, and `cwExternalSourceSettings.cpp` is the only production file
  that includes both headers.

Tests: none new. The commit is structural; the proof is the gate plus a
clean build of all three targets.

Gate: `cavewhere-test "[Attach],[Breadcrumb],[ExternalSourceStatus],[SaveLoad],[cwProject]"`.

### SB.5  A Save pressed mid-fill leaves the project marked modified until the fill's rows are on disk

Filed in the body of 0fe84110 (MT.2). The attach fill installs the derived
tree in batches, one per event-loop turn, under a task-list row. Today
(`cwProject.cpp`, line numbers drift): `fileSaved` sets modified false
(~156); `cwSaveLoad::localMutationOccurred` sets it true (~232). A Save
mid-fill flushes the batches installed so far, a later batch lands and
dirties the project, then the QUEUED `fileSaved` (~449, ~464) arrives and
clears the bit. The project reports saved while rows added after the flush
are unsaved. A `.cwproj` recovers because every node insert schedules its
own save job. A bundled `.cw` is rewritten only on Save, so a user who
quits sees no prompt and reopens a partial tree (Reload re-derives it, so
nothing is lost for good, but the indicator is wrong on the default
format).

Change: a mutation generation on cwProject. `localMutationOccurred`
increments it; a save captures the generation when its flush begins
(`saveFlushCompleted` ~235 or the point the save job is enqueued, the
investigation picks the one that is ordered before any later batch);
`fileSaved` clears modified only when the generation still equals the
captured one, and leaves it set otherwise. The commit body's other option,
holding the save open until the fill finishes, is rejected: Save would
appear stuck for the length of a large fill. No undo step.

Tests, tag `[AttachFill][Save]` (helpers in
testcases/test_cwExternalCenterlineAttachFill.cpp and
ExternalCenterlineTestHelpers.h):
- Attach a synthetic Walls project large enough to fill over several
  turns; call save() after the first batch installs and before the fill
  finishes; when `fileSaved` fires, `modified()` is true (today: false).
  Then let the fill finish, save again, and `modified()` is false.
- Save with no concurrent mutation still clears modified (the existing
  [cwProject] cases pin this; keep them).
- Bundled `.cw`: Save As to `.cw` mid-fill, let the fill finish, close
  without saving: the reopened bundle has the partial tree AND the project
  reported modified before the close (the test asserts the indicator; the
  partial bundle is the accepted consequence of ignoring it).

Gate: `cavewhere-test "[AttachFill],[cwProject],[SaveLoad],[Attach]"` plus `tst_DataMainPageTree.qml`.

### SB.6  The fix station validator queues its subtree revalidate like every other trigger

Filed in the body of 376494c0 (MT.3). `cwFixStationValidator.cpp` ~111:
`subtreeChanged` on the root runs `syncNodeConnections(); revalidate();`
inline, while `attachedFixesChanged`, `localProjectionChanged`,
`networkChanged` and `memberViewsChanged` go through `scheduleRevalidate`
(~296: pending flag + `QMetaObject::invokeMethod(...,
Qt::QueuedConnection)`). The inline call exists because
`m_firstOutlierCave` (`cwFixStationValidator.h` ~235) is a raw `cwCave*`
set at ~633, and a synchronous pass after a node leaves is what keeps it
from dangling. Cost: a fill of B batches runs B full validation passes
(every node, every fix) on the main thread, and any tree edit pays one
pass at once.

Change:
- `m_firstOutlierCave` becomes a `QPointer<cwCave>` (or the node id,
  resolved on read through the region); `firstOutlierCave()` returns the
  live pointer or null. Checked 2026-10-24: no QML file, test or other C++
  reads `firstOutlierCave`; the Q_PROPERTY (~100) stays for the panel that
  may bind it later, with NOTIFY unchanged.
- `subtreeChanged` connects to a slot that marks node connections stale and
  calls `scheduleRevalidate`; `runPendingRevalidate` runs
  `syncNodeConnections()` first when the stale flag is set, then
  `revalidate()`. One pass per event-loop turn however many nodes join or
  leave in it. Per-node `attachedFixesChanged` connections made in
  `syncNodeConnections` (~575) keep `Qt::UniqueConnection`.
- `setLoading(false)` / the initial `revalidate()` at construction (~124)
  stay direct, as cwLocalProjectionManager does, so a loaded project has
  its diagnostics before the first paint.
- Observable change: for one hop after a node is deleted the warnings list
  and `firstOutlierCave` describe the previous tree; `firstOutlierCave` is
  null if that node was the one deleted. No consumer reads it in that hop.

Tests, tag `[cwFixStationValidator][Budget]`:
- Add N nodes (or delete N) in one turn: `fixStationWarningsChanged` fires
  at most once per turn, pinned with QSignalSpy (today: N times).
- Delete the node that holds the first outlier, drain the queue: no crash
  under ASAN, `firstOutlierCave()` is null until the pass runs, then names
  the next outlier or stays null.
- Growth pin: longest main-thread gap during a 4x larger fill, validator
  share, at or below 4x (secondary; the signal count is primary).
- The three existing [cwFixStationValidator] cases and the fix-station
  tree tests ([FixStationTree] or the tags the investigation names) stay
  green; a test that asserted a synchronous warning after a subtree change
  gains a drain of the event queue.

Gate: `cavewhere-test "[cwFixStationValidator],[AttachFill],[FixStation],[LocalProjection],[Attach]"` (the investigation confirms which of these tags exist and substitutes the real ones) plus `tst_FixStationTree.qml` or whichever QML test drives the Fix Stations page.

## 3. Order and bookkeeping

SB.1 then SB.2, one commit each, through the standard pipeline
(investigate -> owner gate -> failing tests -> implement -> simplify ->
review -> fix+commit). DONE 2026-10-24: SB.1 40d3c3b0, SB.2 ef896120, both
full suites green, the owner confirmed the CKKC attach no longer hangs.

SB.3 to SB.6 (added 2026-10-24) are a second pipeline, in the order SB.4,
SB.3, SB.5, SB.6: SB.4 first because it is mechanical and SB.3 builds on the
split header; SB.5 and SB.6 are independent main-thread-budget cleanups
filed by MP.11. One commit each. After the last commit run both full suites
outside the workflow. Measured but unscheduled: single-row keyword removal
(cwKeywordItemModel::remove renumbers every later row and each removal is
replayed through the group-by, filter and unique-value models one row at a
time); it becomes an item only if a Replace on the 1300-file synthetic
fixture shows a stall growing with trip count.
