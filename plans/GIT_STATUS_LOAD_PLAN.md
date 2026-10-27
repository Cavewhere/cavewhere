# GS — The git status check on a native project open

Written 2026-10-24 from three Instruments profiles the owner took while opening native
`.cwproj` projects (`loadHang1.txt`, `loadHang2.txt`, `loadHang3.txt`, main-thread time
sampled inside the load continuation). The projects are the owner's private data: counts and
stack shapes only, never a fixture. This is a separate problem from MP.11: the cost here
scales with the total bytes of LFS-tracked files (scans, LAZ), not with survey size.

## 0. What the profiles show

| Profile | Main-thread stall | Git status | Region setData | Status cost dominated by |
|---|---|---|---|---|
| loadHang1 | 1.03 s | 0.49 s (two runs) | 0.58 s | gitattributes lookups + sha1 of small files |
| loadHang2 | 2.69 s | 2.18 s | 0.41 s | sha256 in the LFS clean filter |
| loadHang3 | 1.22 s | 1.13 s | 0.06 s | sha256 in the LFS clean filter |

The call chain (anchors at HEAD ef896120; search for the text when lines drift):

1. `cwSaveLoad::loadImpl` continuation → `initializeRepositoryForCurrentFile`
   (cwSaveLoad.cpp:2009) → `QQuickGit::GitRepository::initRepository`
   (GitRepository.cpp:2369), which ends by emitting `remotesChanged` and
   `headBranchNameChanged`.
2. Both signals are connected to `cwProjectSyncHealth::refresh` (cwProjectSyncHealth.cpp:116
   to 124), and refresh calls the synchronous `m_repository->checkStatus()` (:162).
3. `checkStatus` (GitRepository.cpp:3026) → `computeModifiedFileCount` (:302) →
   `git_status_list_new` → `git_diff_index_to_workdir` → `maybe_modified` →
   `git_diff__oid_for_entry` → `git_odb__hashfd_filtered` → the LFS clean filter
   (`lfsStreamWrite`, LfsFilter.cpp:123) → `LfsStore::StreamWriter::write` →
   `QCryptographicHash::addData` (sha256 over the whole blob).
4. The first status ends in `setModifiedFileCount`, whose `modifiedFileCountChanged` is also
   connected to refresh, so loadHang1 shows a second full status nested inside the first.
   The second init signal fires a third. The five-minute poll timer
   (cwProjectSyncHealth.cpp:14) repeats the same synchronous walk for as long as the project
   is open.

Why libgit2 hashes at all: `maybe_modified` in libgit2 1.9.1 (diff_generate.c, the conan
source under `~/.conan2/p/libgi353f037599ebc/s/src/src/libgit2/`) re-hashes every entry whose
mtime, ctime, inode, or racy-clean check disagrees with the index stat cache. The refreshed
stat data is written back only when the status was opened with `GIT_STATUS_OPT_UPDATE_INDEX`
(status.c:326 → `GIT_DIFF_UPDATE_INDEX`; the write at diff_generate.c:1493). Our
`computeModifiedFileCount` passes only `INCLUDE_UNTRACKED | RECURSE_UNTRACKED_DIRS`
(GitRepository.cpp:308), so a clean re-hash is discarded and every status pays the whole hash
again. An index written by another tool (a CLI git clone, a checkout, a `git reset --hard`
from discard) keeps every entry in that state until something rewrites it.

Other synchronous `checkStatus` callers run inside an explicit user operation and stay as
they are: the save commit path (cwSaveLoad.cpp:5928 and :5947), the sync precondition
(:6270), the push continuation (:1948), and `cwProject::safeCommitAll` (cwProject.cpp:1712).
They all get cheaper from GS.2.

## 1. Decisions (2026-10-24, binding)

1. **Signposts use MarkScope.** `MarkScope::Profiler` + `ScopedProfiler`
   (MarkScope/MarkScope/Profiler.h) are already linked into cavewherelib; main.cpp:304
   already runs the `FrameProfiler`. No new helper. One `Profiler` instance per interval
   kind, because a `Profiler` owns one signpost id and os_signpost pairs begin and end by
   that id, so two overlapping intervals through one instance would mis-pair. Phase intervals
   inside the load continuation are sequential and share one instance; anything that nests
   gets its own.
2. **The status walk refreshes the index.** `computeModifiedFileCount` passes
   `GIT_STATUS_OPT_UPDATE_INDEX`. Writing `.git/index` is safe on both paths: the watcher
   excludes `.git/` (cwSaveLoad.cpp:538), the async worker holds the per-repo mutex from the
   #546 fix while it runs, and the main thread's repository re-reads the index on its next
   status (status.c:297, `git_index_read_safely`). When the index cannot be written (read-only
   volume) the status must still report a count, so the walk falls back to a run with the
   flag off instead of returning 0.
3. **The sync-health refresh runs its status on the worker.** `cwProjectSyncHealth::refresh`
   calls `checkStatusAsync` and continues from the future with
   `AsyncFuture::observe(...).context(this, ...)`. `checkStatusAsync` already coalesces a
   burst into one worker plus one pending re-run, which collapses the three refreshes during
   open into at most two walks, the second cheap after GS.2.
4. **A count change recomposes the status without a new walk.** `modifiedFileCountChanged`
   feeds a slot that rebuilds the composed `cwSyncStatus` from the cached count and kicks the
   remote ahead/behind restarter. Only `remotesChanged`, `headBranchNameChanged`, the poll
   timer, `setRepository`, and the public `refresh()` start a status walk.
5. **Main-thread budget.** After GS.3 the only main-thread git work in a project open is
   `git_repository_open` plus the attributes and config reads in `initRepository`. Tests pin
   that no synchronous `checkStatus` runs on the open path, and that the number of status
   walks per open is bounded, with `QSignalSpy` on `modifiedFileCountChanged`.
6. **No change to libgit2, LfsStore, or the LFS filter.** The sha256 is the clean filter's
   job; the fix is to stop calling it for unchanged files and to stop calling it on the GUI
   thread.

## 2. Items (one workflow, sequential, one commit each)

Pipeline shape as in plans/SOURCE_BREADCRUMB_LOCAL_STORE_PLAN.md: investigate → failing
tests → implement → simplify → review → fix+commit, Opus medium. Never run the full suites
inside a workflow stage; the last stage runs the tagged gates only.

### GS.1 — Load phases are signposted with MarkScope

A file-scope `MarkScope::Profiler` in cwSaveLoad.cpp (domain `com.cavewhere.load`) and
`ScopedProfiler` scopes around the sequential phases of the load continuation at
cwSaveLoad.cpp:1665 onward: parse wait end, external source records, `initializeRepository`,
region `setData`, and the first solve kick. `GitRepository::checkStatus` and the
`checkStatusAsync` worker body get their own `Profiler` (domain `com.cavewhere.git`), so the
status walk shows as an interval on whichever thread runs it. The same phases write one
`cw.profile.load` line each through `cw::profile::write` (cwProfileLog.h), which is the
measurement on platforms without signposts and in the Release build.

Test: none beyond compile and the existing `[SaveLoad]` gate; the signposts are
observation. Owner runs Instruments' Points of Interest track on the same project to record
the before numbers; the plan's §3 gets them.

### GS.2 — The status walk refreshes the index stat cache

`computeModifiedFileCount` (GitRepository.cpp:302) adds `GIT_STATUS_OPT_UPDATE_INDEX`. When
`git_status_list_new` fails with that flag, it runs once more without it, so a read-only
project still reports a count. `stageFilteredStatusEntries`' caller and the other two
status walks (cleanUntracked :3109, commitAll :3161) keep their flags; they run inside
operations that write the index themselves.

Failing tests (QQuickGit/tests/test_GitRepository.cpp, tag `[GitRepository]`, next to
"checkStatusAsync returns accurate file count" at :1843, plus one in test_Lfs.cpp for the
LFS-tracked case):

- Commit a tracked file (one LFS-tracked, one plain), then rewrite its mtime with the same
  contents so the stat cache disagrees. After one `checkStatus`, the on-disk index entry's
  mtime equals the file's mtime (read it back with `git_repository_index` +
  `git_index_get_bypath` on a fresh `git_repository_open`), and the reported count is 0.
- The same, through `checkStatusAsync`, from a repository whose main-thread handle stays
  open: the main thread's next `checkStatus` also reports 0 and does not rewrite the index
  again (index file mtime unchanged across the second call).
- A repository on a read-only directory (chmod the `.git` dir read-only for the test, restore
  in a scope guard) still reports the correct modified count.

Gate: `qquickgit-test "[GitRepository],[Lfs]"`, `cavewhere-test "[cwProject],[SaveLoad]"`.

### GS.3 — The sync-health refresh runs its status walk off the main thread

`cwProjectSyncHealth::refresh` (cwProjectSyncHealth.cpp:145) becomes: start or stop the poll
timer as today, then `m_repository->checkStatusAsync()` observed with `.context(this, ...)`;
the continuation reads `modifiedFileCount()`, `remotes()`, `headBranchName()`, and
`remoteUrl()` on the main thread exactly as the current body does from the
`const bool localChanges = ...` line on, then calls `scheduleRemoteStatusRefresh`. That body
moves into a private `composeStatus()` that the `modifiedFileCountChanged` connection also
calls directly (decision 4). A repository replaced mid-flight is guarded the way the remote
restarter already guards it: `m_repository` is a `QPointer`, and a continuation whose
repository changed returns without setting status.

Failing tests (testcases/test_cwProjectSyncHealth.cpp, tag `[cwProjectSyncHealth]`):

- Set a repository on a sync health whose work tree has N committed files with stale stat
  data. `refresh()` returns before any `statusChanged`, and `statusChanged` arrives after the
  event loop spins (`tryVerify` style wait, existing helpers in that file).
- During `initRepository` + the status that follows, `modifiedFileCountChanged` is emitted at
  most once and the number of `statusChanged` emissions is bounded by a constant, not by the
  number of connected signals (QSignalSpy).
- Changing the count through a file write + `checkStatusAsync` recomposes the status (the
  `m_hasLocalChanges` flag flips) with no second worker run visible: `longestMainThreadStall`
  (testcases/ExternalCenterlineTestHelpers.h:851) around the recompose stays under one tick
  for a fixture with 4x the files.

Gate: `cavewhere-test "[cwProjectSyncHealth],[cwProject],[SaveLoad],[sync]"` and the
Sync page QML tests that read `ProjectSyncHealth.status`.

## 3. Order and bookkeeping

GS.1, GS.2, GS.3, in that order, so the before numbers exist before the fix lands. After
GS.3 the owner re-profiles the same three projects; the expected shape is the status walk
gone from the main thread and the region `setData` (0.06 to 0.58 s above) left as the
remaining open-time cost, which is #542's plan, plans/REGION_LOAD_STALL_PLAN.md.

Measured, not scheduled here: the main-thread `setData` breakdown from loadHang1 and the
teardown cost from loadHang2 are recorded in plans/REGION_LOAD_STALL_PLAN.md §0.
