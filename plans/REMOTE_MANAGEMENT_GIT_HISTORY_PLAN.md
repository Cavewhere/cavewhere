# Remote Management Page + Git History Graph

Addresses issues #350 (remote management page) and #272 (show Git repo info).

---

## Goals

- Dedicated remote management page showing all configured remotes
- Read-only Git history graph with branch/merge visualization
- Per-remote status (ahead/behind, auth state) via existing `cwProjectSyncHealth`
- Reuse `SetupRemoteWizard` for adding/editing remotes (unchanged in sync button flow)
- Architecture designed to accommodate multiple remotes (field server, local bundles) in the future

---

## UI Layout

The remote management page lives in the existing `cwPageSelectionModel` navigation. The sync button continues to use `SetupRemoteWizard` as-is.

```
RemoteManagementPage.qml
├── Remote list (one card per remote)
│   ├── Remote name + URL
│   ├── Ahead/behind indicator (from cwProjectSyncHealth)
│   ├── Auth status badge (None / NeedsLogin / Expired — NoRemote does not apply per-card)
│   ├── "Check access" button → GitTestConnection
│   ├── Edit button → SetupRemoteWizard (reused)
│   └── Remove button
├── "Add Remote" button → SetupRemoteWizard
└── GitHistoryView.qml (read-only, below remote list)
    ├── ListView (one row per commit)
    │   └── delegate: GitHistoryRow.qml
    │       ├── QCanvasPainterItem  (graph lanes, width = numLanes * laneWidth)
    │       ├── Text: short commit message
    │       ├── Text: author + timestamp
    │       └── RefBadge repeater (local branch + remote branch labels)
    └── (future: column headers per remote)
```

---

## Phase 1: Git Graph Data Model in QQuickGit

All graph logic lives in the `QQuickGit` submodule so it can be reused outside CaveWhere.

### Coding Conventions for Ported Code

All code ported from GitQlient must conform to QQuickGit's style, not GitQlient's. Key differences:

| Convention | GitQlient | QQuickGit (use this) |
|------------|-----------|----------------------|
| Member prefix | `m_camelCase` | `mCamelCase` (no underscore) |
| Include guard | `#pragma once` | `#ifndef FILENAME_H` / `#define` / `#endif` |
| Namespace | none | `namespace QQuickGit { }` in headers; `using namespace QQuickGit;` in `.cpp` |
| Export macro | none | `QQUICKGIT_EXPORT` on all public classes |
| Include grouping | unsorted | `//Qt includes` then `//Our includes` comment blocks |
| Inline accessors | in-class body | after the class body in the header |
| Enum naming | mixed | `UpperCamelCase` enum class names, `ValueName` (not `VALUE_NAME`) |
| Role names | N/A | `FooRole` suffix (e.g. `ShaRole`, `AuthorRole`) |

GitQlient's `Lanes`, `LaneType`, and `Lane` classes are self-contained algorithmic code with no Qt or framework dependencies. Port the logic only — rename members, restructure to match QQuickGit patterns, and discard any GitQlient-specific infrastructure.

### Port GitQlient Lane Algorithm

Source reference: https://github.com/francescmaestre/GitQlient — `src/cache/Lanes.*`, `LaneType.h`, `CommitInfo.*`

New classes in `QQuickGit/src/`:

**`GitLaneType.h`** — enum (19 confirmed values — verify count against `LaneType.h` in the GitQlient source before finalizing, camelCase per QQuickGit convention):
`Active`, `NotActive`, `Empty`, `Cross`, `CrossEmpty`,
`MergeFork`, `MergeForkLeft`, `MergeForkRight`,
`Join`, `JoinLeft`, `JoinRight`,
`Head`, `HeadLeft`, `HeadRight`,
`Tail`, `TailLeft`, `TailRight`,
`Branch`, `Initial`

**`GitLane.h`** — thin wrapper around `GitLaneType` with helpers:
`isHead()`, `isTail()`, `isJoin()`, `isMerge()`, `isActive()`, `isFreeLane()`

**`GitLanes.h/.cpp`** — stateful lane calculator, one instance per revwalk:
- `init(startSha)` — seed with first commit SHA
- `isFork(sha, &isDiscontinuity)` — detect fork points
- `setFork(sha)` — assign `Tail`/`MergeFork` lane types
- `setMerge(parents)` — assign `Join` lane types
- `changeActiveLane(sha)` — handle lane reuse
- `getLanes()` → `QVector<GitLane>` for the current row

**Per-commit call order (must be followed exactly for correct lane state):**

Follows GitQlient's `calculateLanes()` + `resetLanes()` pattern. For each commit yielded by `git_revwalk_next`, call in this sequence:

*calculateLanes phase:*
1. `isFork(sha, &isDiscontinuity)` — query fork state; captures discontinuity flag for missing commits
2. If `isDiscontinuity` is true: `changeActiveLane(sha)` — switch active lane to this commit's lane (only on discontinuity, not on every commit)
3. If `isFork` returned true: `setFork(sha)` — assigns `Tail`/`MergeFork`/`MergeForkLeft`/`MergeForkRight` types for the active lane
4. If the commit has multiple parents (`parentCount > 1`): `setMerge(parents)` — assigns `Join`/`JoinLeft`/`JoinRight` types for incoming parent lanes
5. If the commit has no parents: `setInitial()` — marks the root commit lane
6. `getLanes()` — retrieves the finalized `QVector<GitLane>` for this row (store into `GitRowGraph`)

*resetLanes phase (must happen after getLanes):*
7. `nextParent(firstParentSha)` — sets the expected SHA for the active lane so the next commit's `isFork` can find it (pass empty string for root commits)
8. If merge: `afterMerge()` — transitions join/head/cross lanes to inactive
9. If fork: `afterFork()` — transitions tail/cross lanes to empty, trims trailing empty lanes
10. If `isBranch()`: `afterBranch()` — transitions branch lane to active

**`GitRowGraph.h`** — per-row graph data (value type):
```cpp
struct GitRowGraph {
    QString sha;
    QVector<GitLane> lanes;
    int activeLane;
};
```

### New Model: `GitGraphModel`

`QAbstractListModel` in `QQuickGit/src/GitGraphModel.h/.cpp`:

- Default (no-arg) constructor; `GitRepository` is set via a `Q_PROPERTY(GitRepository* repository READ repository WRITE setRepository NOTIFY repositoryChanged)` property. A constructor taking `GitRepository*` is incompatible with `QML_ELEMENT` (QML cannot pass constructor arguments); use the property setter instead.
- Uses `git_revwalk` pushing all refs: `refs/heads/*` + `refs/remotes/*`, with sort flags `GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME`. `GIT_SORT_TOPOLOGICAL` ensures parent commits never appear before their children (required for the lane algorithm to produce correct fork/merge assignments). `GIT_SORT_TIME` breaks topological ties by committer timestamp so the graph matches `git log --graph` output.
- Tags each commit with which refs point at it (local branches + per-remote branches)
- Runs `GitLanes` calculator to produce lane data per row
- Async population via `AsyncFuture` + `AsyncFuture::Restarter` using `beginInsertRows`/`endInsertRows` — never `modelReset`

Exposed to QML as `QML_ELEMENT`.

#### Virtual Loading Strategy

`ListView` only requests data for rows currently visible. To support this efficiently:

1. **Index pass (runs once on load)**: Walk all refs with `git_revwalk`. For each OID, call `git_commit_lookup()` to get parent count and parent OIDs (required by the lane calculator), then close the commit. Store the OID in `QVector<git_oid> m_oids` and the lane result in a parallel `QVector<GitRowGraph> m_graph`. Message, author, and timestamp are **not** extracted here — this keeps the pass fast even for large repos. This gives `rowCount()`, a position-to-OID mapping, and complete lane data. **Cancellation**: The index pass runs as a single `QtConcurrent::run()` with a long revwalk loop. Because `QFuture::cancel()` does not interrupt an already-running function on a thread pool thread, the loop must check `QFutureInterface::isCanceled()` on each iteration (or every N iterations) and return early when true. This cooperates with `AsyncFuture::Restarter`'s cancellation signal to stop the pass promptly.

2. **Detail fetch (lazy, per visible row)**: `data()` receives a row index. Look up `m_oids[row]`, call `git_commit_lookup()` to open the commit and extract message/author/timestamp. Cache results in a `QHash<int, GitCommitDetail> m_cache` so repeated scrolling doesn't re-open commits.

3. **Async insertion via AsyncFuture**: The index pass runs using `AsyncFuture` (`asyncfuture` submodule). The future is stored as `m_indexFuture`. As batches of OIDs + graph rows are produced, continuations dispatched with `observe(future).subscribe(...)` marshal results to the main thread, which calls `beginInsertRows`/`endInsertRows` to append them. The `ListView` can display early rows while the rest of history loads.

4. **Invalidation and cancellation via `AsyncFuture::Restarter`**: When the repo changes (new commit, fetch), use `AsyncFuture::Restarter` to cancel the in-flight index pass before starting a new one. `Restarter` calls `QFuture::cancel()` on the active future, which sets the cancellation flag that the index pass loop checks via `QFutureInterface::isCanceled()` (see step 1). Once the loop returns early, `Restarter` observes the cancellation via `QFutureWatcher::canceled()` and automatically triggers the next run. After cancellation, use `beginRemoveRows`/`endRemoveRows` to clear stale rows, then restart the index pass. Note: `Restarter` also uses generation tracking to discard stale completions from previous runs, so even if the loop finishes a few extra iterations before seeing the cancellation flag, the results are safely ignored.

**Model roles:**
| Role | Type | Description |
|------|------|-------------|
| `sha` | `QString` | Full commit SHA |
| `message` | `QString` | First line of commit message (fetched lazily) |
| `author` | `QString` | Author name (fetched lazily) |
| `timestamp` | `QDateTime` | Commit time (fetched lazily) |
| `lanes` | `QList<int>` | GitLaneType values per column (computed in index pass). Register `GitLaneType` with `QML_ELEMENT` (or expose via a `Q_GADGET`/`Q_ENUM` wrapper) so QML delegates can compare against named enum values rather than magic integers. |
| `activeLane` | `int` | Index of this commit's lane (computed in index pass) |
| `refs` | `QStringList` | e.g. `["main", "origin/main"]` (computed in index pass) |

Note: `shortSha` and `laneCount` are omitted — both are trivially derivable in QML (`sha.substring(0, 7)` and `lanes.length` respectively).

---

## Phase 2: Git History View (QML + QCanvasPainterItem)

Requires Qt 6.11 (`QCanvasPainterItem`).

### `GitHistoryRow.qml`

Delegate for the `ListView`. The `QCanvasPainterItem` width is dynamic:
`width: lanes.length * laneWidth`

Paint logic mirrors GitQlient's `paintGraphLane()`:
- Vertical lines: `Active`, `NotActive`, `Cross`, `CrossEmpty`, etc.
- Arcs (QPainter::drawArc): `Join`, `JoinLeft`, `JoinRight`, `Head`, `HeadLeft`, `HeadRight`, `Tail`, `TailLeft`, `TailRight`
- Ellipse node at active lane center
- Colors: use `Theme.laneColors` (a `list<color>` cycling palette defined in `Theme.qml`). Index into it by lane index modulo list length. The palette must contain **at least 8 entries** — typical cave survey repositories have 5–8 concurrent open branches, so fewer than 8 colors causes visible repeats at the most common branch counts. 8 colors matches GitQlient's default palette size.

### `RefBadge.qml`

Small pill label for branch/remote ref names. Background uses `Theme.tag`; text uses `Theme.textInverse`. Lane-matched accent color is applied as a left border or tint using the same `Theme.laneColors` palette. All ref badges on a given row share the same lane color: look up `Theme.laneColors[activeLane % Theme.laneColors.length]` from the row's `activeLane` role — the ref string itself carries no lane index. Future: different hue families per remote, added to `Theme.qml` at that time.

### `GitHistoryView.qml`

Top-level component wrapping the `ListView` + `GitGraphModel`. Accepts a `GitRepository` property and instantiates the model internally.

---

## Phase 3: Remote Management Page

### `RemoteManagementPage.qml`

- Instantiates `GitGraphModel` and `GitHistoryView`
- For each remote, instantiates a `cwProjectSyncHealth` (one per remote — see future work below)
- "Check access" button triggers `GitTestConnection` (in QQuickGit; needs `QML_ELEMENT` added — see Prerequisites)
- Add/Edit flow: opens `SetupRemoteWizard` in a popup (same component, unchanged)
- Remote list: use `Repeater { model: repository.remotes }` — `GitRepository.remotes` is already a `QVector<GitRemoteInfo>` exposed as a `Q_PROPERTY`. `GitRemotesModel` exists in QQuickGit but is not QML-registered and is not needed here since `Repeater` can consume the vector directly.
- Remove remote: calls `GitRepository.removeRemote(name)` (confirmed missing — must be added to `QQuickGit`; see Prerequisites)

### Status display per remote

`cwProjectSyncHealth` already provides:
- `status.aheadCount` / `status.behindCount`
- `status.syncBlocker` → `None` / `NeedsLogin` / `Expired` (`NoRemote` does not apply per-card; each card is already bound to a specific remote)
- `status.stale` — loading indicator
- `refresh()` — explicit recheck (bound to "Check access" button)
- `m_pollTimer` — background refresh

Currently `cwProjectSyncHealth` auto-discovers the remote in `refresh()`: it prefers `"origin"`, falling back to `configuredRemotes.constFirst()`. For Phase 3, `setRemoteName(QString)` must be added before `RemoteManagementPage.qml` can be implemented — this is a Phase 3 prerequisite, not optional.

When implementing `setRemoteName`:
- Store the name and bypass the auto-discovery path in `refresh()` when a name is explicitly set.
- If the named remote is not found in `configuredRemotes` at refresh time (e.g. it was removed between calls), emit a `SyncBlocker::NoRemote` status with message `"Remote '<name>' is no longer configured."` rather than silently falling back to a different remote.
- The existing `remotesChanged` → `refresh()` connection is retained — it naturally re-runs the check when remotes change, which will catch the removal case.
- Verify existing sync button tests still pass (the sync button path never calls `setRemoteName`, so auto-discovery must remain the default when no name is set).

---

## Prerequisites

These must be resolved before or during Phase 3:

- **Qt 6.11 minimum**: Required for `QCanvasPainterItem`. **Known open blocker**: Qt 6.11 is not yet in `conan-center-index` (tracking issue: conan-io/conan-center-index#29775); a custom Conan package may be needed. Update the Qt version constraint in `conanfile.py` from `6.10` to `6.11` and update build documentation once the package is available. Phases 1 and 3 can proceed independently; Phase 2 is blocked until resolved.
- **`GitRepository::removeRemote()`**: Confirmed missing — `addRemote()` exists but `removeRemote()` does not. Add `Q_INVOKABLE QString removeRemote(const QString& name) noexcept` to `GitRepository` in `QQuickGit`. Needed for the Remove button on the remote management page.
- **`cwProjectSyncHealth::setRemoteName(QString)`**: Confirmed missing. Currently auto-discovers the remote at refresh time. Add a setter so one instance per remote card can track its specific remote independently. See Phase 3 status display section for full implementation requirements.
- **`GitTestConnection` QML exposure**: Confirmed missing — `QML_ELEMENT` is not present in `GitTestConnection.h`. Add the macro so it is accessible from `RemoteManagementPage.qml`.

---

## Testing Strategy

### Unit tests: `GitLanes` algorithm

The lane calculator is the most complex non-trivial code in this plan. Test it in complete isolation before wiring to the model.

Test cases to cover:

| Topology | Description |
|----------|-------------|
| Linear | Single branch, no merges — all rows should have one `Active` lane |
| Simple fork | Branch diverges from main — verify `Tail`/`MergeFork` lane types at split |
| Simple merge | Two branches rejoin — verify `Join`/`MergeFork` at merge commit |
| Nested merges | Multiple merges in sequence — lane reuse should minimize column count |
| Criss-cross merge | A merges B, B merges A — verify `Cross`/`CrossEmpty` handling |
| Lane reuse | After a branch joins, its column should be reused by a new branch |
| Discontinuity | Revwalk with hidden commits — verify `isDiscontinuity` flag triggers correctly |

**Torture test**: Generate a synthetic repo with 50–100 branches all merging into main in random order. Run the lane calculator and assert:
- No two active commits share the same lane column at the same row
- Every `Tail` has a matching `MergeFork` at a later row (topologically earlier)
- Every `Join` has a matching `MergeFork` at the same row
- Lane count never exceeds the actual number of concurrent open branches

Use `git_repository_init` + `git_commit_create` in a temp directory to build the synthetic repo in C++. This avoids test data files and makes the topology programmatically verifiable.

### Integration tests: `GitGraphModel`

- Verify `rowCount()` equals total commit count after the index pass completes
- Verify `data()` returns non-empty message/author for a visible row (lazy fetch works)
- Verify `beginInsertRows`/`endInsertRows` signals are emitted (never `modelReset`)
- Verify invalidation (steady state): add a new commit to the repo after the index pass completes, call `refresh()`, check the new row appears at top with no duplicate rows
- Verify invalidation (mid-pass cancellation): use a large synthetic repo so the index pass does not complete instantly; call `refresh()` before the pass finishes; assert the old pass is cancelled (verify via `beginRemoveRows`/`endRemoveRows` before re-population), no duplicate rows appear, and the final `rowCount()` equals the correct total. This is the critical race — two concurrent index passes modifying `m_oids` and `m_graph` must never be observable.

### QML tests

- `GitHistoryView` renders without crashing for a repo with 0, 1, and N commits
- `RefBadge` displays the correct label text and is visible for commits pointed to by refs

---

## Future: Multiple Remote Support

Designed for but not implemented in initial phases.

### Use Cases

1. **Field mini-server**: A small computer running a bare Git server on LAN
   (`ssh://user@192.168.x.x/cave.cwproj` or `git://192.168.x.x/cave.cwproj`).
   Multiple surveyors sync to this offline while underground, push to GitHub when back.

2. **AirDrop / QuickShare bundle as remote**: A `.cwproj` directory shared locally
   can be added as a `file:///path/to/bundle.cwproj` remote — libgit2 supports `file://`
   natively. SetupRemoteWizard needs a "local path" input option.

3. **Offline-first merge**: Each surveyor's device is a remote of the others.
   The history graph makes divergence immediately visible.

### What needs to change

- **`GitGraphModel`**: Already designed for multi-remote — `git_revwalk_push_glob("refs/remotes/*")` includes all remotes. `refs` role already returns all pointing refs per commit.
- **RefBadge colors**: Assign a distinct hue family per remote (local = blue, origin = green, field-server = orange, etc.) so the graph is self-explanatory. Define these as additional tokens in `Theme.qml` (e.g. `Theme.remoteColors: list<color>`) rather than hardcoding in the delegate.
- **`cwProjectSyncHealth`**: Add `setRemoteName(QString)` to track a specific remote rather than always defaulting to the primary one. One instance per remote card.
- **SetupRemoteWizard**: Add a "local path" input for `file://` remotes (AirDrop/QuickShare case). Bare IP addresses must be allowed (no hostname validation for LAN servers).
- **RemoteManagementPage**: Layout multiple remote cards; decide ordering (primary remote first).

---

## Assumptions / Decisions

| Decision | Rationale |
|----------|-----------|
| Read-only graph initially | Sufficient for #272; avoids reset/cherry-pick complexity |
| `QCanvasPainterItem` (Qt 6.11) | Native QML, QPainter API is identical to GitQlient's — port is straightforward. CaveWhere minimum Qt version is bumped to 6.11; `conanfile.py` must be updated before Phase 2 begins. Qt 6.11 is a known open blocker in Conan (see Prerequisites). |
| Graph lives in QQuickGit | Reusable outside CaveWhere; keeps cavewherelib thin |
| SetupRemoteWizard unchanged in sync button | Avoid regressions; management page reuses it in a popup |
| One `cwProjectSyncHealth` per remote | Cleanest model; existing class already does polling + stale state |
| Push access via `cwProjectSyncHealth` + `GitTestConnection` | Already implemented; no new network logic needed |
| Virtual loading via indexed OID cache | `ListView` only requests visible rows; `data()` fetches commit detail on demand (see Phase 1) |
| `beginInsertRows`/`endInsertRows` for all updates | Never use `modelReset` — it resets scroll position and forces full delegate recreation |
| `AsyncFuture` + `AsyncFuture::Restarter` for threading | Enables future chaining for batch insertion callbacks and safe cancellation of in-flight index passes on invalidation; avoids raw `QFuture`/`QMutex` boilerplate |
| Lane and ref colors via `Theme.qml` | All colors go through `Theme.qml` tokens (`Theme.laneColors`, future `Theme.remoteColors`) — no hardcoded hex values in delegate QML |
