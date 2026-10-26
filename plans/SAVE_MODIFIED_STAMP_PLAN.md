# The modified star after a Save (ST.1 to ST.3)

Written 2026-10-25, after SB.5 (f2cda2e6, plan
SOURCE_BREADCRUMB_LOCAL_STORE_PLAN.md) landed and the merge of origin/dev
d87cd850 was committed as 2d4f5028. Follow-up to the three items SB.5's
commit body filed: the star that lingers after a mid-fill Save that did
absorb the late rows, the `.cwproj` Save whose commit fails yet clears the
star, and the sync completion that clears the star without looking.

## 0. What happens today

Line numbers are from 2d4f5028 and drift.

**Two counters count the same thing.** `cwSaveLoadPrivate::localMutationEpoch`
(cwSaveLoadPrivate.h:390) is incremented in `saveObject` (:773) just before
`emit context->localMutationOccurred()` (:775), and only there. It serves the
sync reconcile (cwSaveLoad.cpp:1898, :1947, :6163, :6341, :6513). The other
six emit sites bypass it: the region-tree row removal handler
(cwSaveLoad.cpp:4256), the LAZ layer row removal (:4476), the attachment
copy (:4736), the attachment file GC (:4826), the attachment tree removal
(:4851) and `reportProjectFileChangedOnDisk` (:4911). SB.5 then added a
second counter, `cwProject::m_mutationGeneration` (cwProject.h:270),
incremented in cwProject's `localMutationOccurred` handler (cwProject.cpp:229).

**The save captures too early.** Each of the four save paths reads
`m_mutationGeneration` the instant it enqueues its operation (cwProject.cpp:435
bundled save, :455 `.cwproj` save, :766 bundled Save As, :833 `.cwproj` Save
As) and hands that value to `completeSave` (:1759), which clears the bit only
when the value still equals the live counter. The write itself happens later:
`saveFlushImpl` (cwSaveLoad.cpp:1775) observes `completeSaveJobs()` (:5499),
which is `m_pendingJobsDeferred.future()`, and that deferred completes in
`execFileSystemJobs` (cwSaveLoadPrivate.cpp:1908) only when the job list is
empty after a job finishes. The drain is open-ended: a save job enqueued
while an earlier job is on the pool is written before the deferred
completes. A bundled save zips only after that (cwSaveLoad.cpp:2702, the
BundlePackage operation follows the SaveFlush operation), and a `.cwproj`
save commits only after that (:2751). So every mutation whose job was
enqueued before the drain ended is on disk, and in the bundle or the commit.

The symptom. The attach fill installs one batch per event-loop turn. A Save
pressed after batch 1 captures generation g1. Batches 2..N land while the
queue is still draining batch 1's jobs, so their rows are written, zipped and
committed, but the counter is now gN, and `completeSave(g1)` leaves the star
on. The star is wrong in the safe direction: the user saves again and
nothing was lost. On CKKC that is a Save that appears to have had no effect.

**Capturing at `saveFlushCompleted` is wrong in the unsafe direction.**
The emit (cwSaveLoad.cpp:1780) runs in a continuation queued after the
deferred completes. A batch landing in that gap is counted and enqueued but
unwritten; capturing there would clear the star over rows that are missing
from the bundle. SB.5's plan text named both candidates and the pipeline
chose the early one, correctly, given the choice it had.

**The failed commit.** The `.cwproj` paths (cwProject.cpp:457, :835) append
the commit error and then call `completeSave(generation)` anyway, so a Save
whose `commitProjectChanges` failed clears the star and emits `fileSaved`.
The bundled paths (:440, :771) return before `completeSave` on error, and
`fileSaved` stays silent. A `.cwproj` whose files are flushed but whose
commit failed is exactly the project `discardChanges` (git reset --hard +
clean untracked) would erase, which is the reason the commit precedes
`fileSaved` in the first place (comment at :828).

**Sync.** `completeSyncOperation` (cwProject.cpp:631) calls `setModified(false)`
on the error path (:643, "the working tree may be clean even on push failure")
and `setModified(bundleNeedsResave)` on success (:651). Neither looks at the
counter. The sync's own flush (cwSaveLoad.cpp:6076) has the same open-ended
drain, so the same stamp answers the same question.

## 1. Decisions

1. **One counter, in cwSaveLoad.** `localMutationEpoch` is the counter.
   Every `localMutationOccurred` emit goes through one private helper that
   increments it and emits (the `suppressLocalMutationTracking` check moves
   into the helper; `saveObject`'s `remoteApplyGuard.noteMutation()` stays
   where it is unless the investigation shows the other sites need it too,
   which is a sync question the owner decides). `cwProject::m_mutationGeneration`
   is deleted; cwProject reads the epoch through a public const getter.
2. **The stamp is taken when the drain ends.** In `execFileSystemJobs`, at
   the two places `m_pendingJobsDeferred.complete()` runs after the list
   empties (cwSaveLoadPrivate.cpp:1886, :1909), the private records
   `drainedEpoch = localMutationEpoch`. That instant is unambiguous: every
   emit so far ran synchronously on the main thread beside the enqueue of
   its job (the emit precedes the enqueue in `saveObject`, :775 then
   `saveProtoMessage`; the same shape at :4736/:4826/:4851 and the trash
   moves at :4256), and the list is empty, so every one of those jobs has
   finished. A mutation after the stamp is counted and uncovered, which is
   what keeps the star on. The two emit sites with no job
   (`reportProjectFileChangedOnDisk` :4911, LAZ row removal :4476 when the
   file already vanished) describe disk state that already is what it is,
   and a bundle or commit started after the drain reads that state.
3. **The stamp travels in the result, never through a getter.** The flush's
   continuation reads `drainedEpoch` and returns it. The queued continuation
   runs after the drain; a job enqueued in the gap restarts the queue but
   leaves `drainedEpoch` at the finished drain's value until the next drain
   ends, so the gap is safe. `saveBundledArchive` and `enqueueFlushAndCommit`
   return `QFuture<Monad::Result<cwSaveLoad::SaveReceipt>>` where
   `SaveReceipt { quint64 mutationEpoch; }`; the investigation decides whether
   `saveFlushImpl` itself changes type or stays `ResultBase` with the receipt
   assembled by the two callers (sync, discard and the other `saveFlushImpl`
   callers want no receipt). A `lastFlushEpoch()` getter is rejected: two
   saves queued back to back would let the second drain's stamp answer the
   first save's question.
4. **`completeSave` takes the receipt.** It clears the bit when
   `receipt.mutationEpoch == m_saveLoad->localMutationEpoch()`, leaves it
   otherwise, and emits `fileSaved`. An already-drained queue is covered by
   the same rule: `completeSaveJobs()` returns a finished future, the
   continuation reads the stamp of that finished drain, and any mutation
   since has a larger epoch.
5. **A failed commit keeps the star and stays silent.** The `.cwproj` paths
   match the bundled paths: on error append the cwError and return before
   `completeSave`, so `fileSaved` means saved on every format. `SaveFuture`
   still finishes, so `waitSaveToFinish()` and shutdown are unchanged.
6. **Sync compares the same stamp.** `cwSaveLoad::SyncReport` gains
   `mutationEpoch`, the stamp from the sync's flush; `completeSyncOperation`
   clears the bit on success only when the stamp equals the live epoch
   (and still sets it when a bundle needs a resave). On the error path the
   report is present whenever the flush ran (the investigation confirms when
   `lastSyncReport()` is populated); compare when it is, leave the bit alone
   when it is absent. "The working tree may be clean" becomes a fact the
   stamp proves instead of an assumption.
7. **No new test scaffolding.** The epoch getter is production API with a
   production reader (cwProject). Tests observe `fileSaved`, `modified()`,
   `saveFlushCompleted`, the receipt in the returned future, and disk state.
8. **No undo step. Main-thread budget unchanged**: everything added is a
   counter read or write on paths that already run on the main thread.

## 2. Items

### ST.1  The save receipt carries the epoch stamped when the drain ended

Decisions 1 to 4. One commit.

Changes:
- cwSaveLoadPrivate: the emit helper (name it for what it reports, e.g.
  `noteLocalMutation(cwSaveLoad*)`), `drainedEpoch`, the two stamp sites in
  `execFileSystemJobs`. The discard path's own `m_pendingJobsDeferred.complete()`
  calls (cwSaveLoadPrivate.cpp:632, cwSaveLoad.cpp:1262) are reviewed by the
  investigation: a discard rewrites the tree and ends with `discardCompleted`
  clearing the bit by a different rule, so they likely leave the stamp alone.
- cwSaveLoad: `SaveReceipt`, `localMutationEpoch()` getter, the two public
  entry points return the receipt, the seven emit sites call the helper.
- cwProject: `m_mutationGeneration` deleted, the four save paths stop
  capturing, `completeSave(const SaveReceipt&)`.

Tests, tag `[AttachFill][Save]` and `[cwProject]`
(testcases/test_cwExternalCenterlineAttachFill.cpp helpers `saveMidFill`,
`watchFileSaved`, `saveAndSettle`; cwProject cases in test_cwProject.cpp):
- A mutation enqueued while the save's drain is still open is covered by the
  save: call `save()`, then on the same event-loop turn mutate a tracked
  object (a cave rename is enough; `saveObject` enqueues its job while the
  first job is still on the pool), then wait for `fileSaved`. `modified()` is
  false at `fileSaved` (today: true, the capture predates the rename), and
  the renamed name is in the file on disk. Same for `saveAs` to a bundle:
  the reopened bundle has the new name and the indicator was clear.
- A mutation after the drain keeps the star: mutate from a slot on
  `cwSaveLoad::saveFlushCompleted` (fires after the stamp), `modified()` is
  true at `fileSaved`. Passes today; it pins the "never capture late" side.
- The two existing SB.5 cases stay as written. The first one's assertion
  that the star is on after a mid-fill Save changes: with batches landing
  during the drain the star may now be off, so the case asserts instead
  that the star is off exactly when the rows on disk match the tree (count
  the trip files in the project directory against `region->allTrips()`
  at `fileSaved`), and on after a batch that lands post-drain. If the fill
  cannot be made to land a batch post-drain deterministically, the case
  keeps only the on-disk equivalence check plus the "Save after the fill
  clears it" tail.
- `[cwProject]`: a Save with no concurrent mutation still clears the bit on
  `.cwproj` and bundle; two saves queued back to back both report through
  their own receipt (the first `fileSaved` sees the bit cleared only if no
  mutation landed after the first drain).

Gate: `cavewhere-test "[AttachFill],[cwProject],[SaveLoad],[Attach]"` and
`tst_DataMainPageTree.qml`.

### ST.2  A `.cwproj` Save whose commit fails keeps the project modified

Decision 5. One commit, small.

Changes: cwProject.cpp save() and saveAs() `.cwproj` branches return after
appending the error, before `completeSave`. The comment at :828 is extended
by one line saying the bit stays set so the user is asked again.

Tests, tag `[cwProject][Save]`:
- Make `commitProjectChanges` fail on a `.cwproj` fixture (an `index.lock`
  left in `.git` is the lightest way; the investigation confirms QQuickGit
  reports it as an error rather than waiting), call `save()`, wait for the
  save future. `modified()` stays true, `fileSaved` did not fire, the error
  model has one Fatal entry, and the trip files are on disk (the flush ran).
  Remove the lock, `save()` again: `fileSaved` fires with the bit clear.

Gate: `cavewhere-test "[cwProject],[SaveLoad]"`.

### ST.3  Sync completion clears the star only when its flush covered every mutation

Decision 6. One commit.

Changes: `SyncReport::mutationEpoch` set from the sync flush's stamp
(cwSaveLoad.cpp:6076 region, the same receipt plumbing as ST.1);
`completeSyncOperation` compares; the error path's unconditional clear is
replaced by the compare-or-leave rule.

Tests, tag `[Sync][cwProject]` (existing bare-remote helpers in
testcases/test_cwProjectSync*.cpp or the sync helper header the
investigation finds):
- Sync with a mutation enqueued on the same turn as `sync()`: the bit is
  clear at `syncFinished` and the pushed commit contains the mutation.
- Sync with a mutation made from a `saveFlushCompleted` slot: the bit is set
  at `syncFinished`.
- Sync whose push fails (remote path removed after setup): the bit reflects
  the compare, which is clear when nothing landed after the flush.

Gate: `cavewhere-test "[Sync],[cwProject],[SaveLoad]"`.

## 3. Order and bookkeeping

ST.1, ST.2, ST.3, one pipeline, one commit each, through the standard
seven stages (investigate -> owner gate -> failing tests + empty API ->
implement -> simplify -> review -> fix+commit). ST.2 is independent of ST.1
and could go first, but ST.1 changes `completeSave`'s signature and the
`.cwproj` branch bodies that ST.2 edits, so ST.1 leads. ST.3 reuses ST.1's
receipt. After the last commit run both full suites outside the workflow.

Owner questions for the gate after ST.1's investigation:
- whether the six non-`saveObject` emit sites should also call
  `remoteApplyGuard.noteMutation()` now that they share the helper (it
  changes what a sync pull treats as a local mutation);
- whether `saveFlushImpl` changes its return type for every caller or only
  the two public save entry points assemble the receipt.

Filed by this plan and left alone: the SB.5 commit body's "accepted
lingering star" is what ST.1 removes; the other SB follow-ups (armPaths
whole-set re-arm, reconcile's main-thread computePlan, the validator's
inline triggers) stay with issue #724.
