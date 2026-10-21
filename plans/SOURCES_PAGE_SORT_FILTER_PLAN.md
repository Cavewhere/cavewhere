# Sources page: sort, filter, and a Name / Last opened header

Status: reviewed 2026-10-13, decisions locked (see "Decisions from review"). Execution: Workflow, four stages per
checkpoint (implement → simplify → review read-only → fix and commit), all on
opus / medium.

## Goal

The Sources page (`cavewherelib/qml/SourceListPage.qml`) is a flat list of
recent projects in insertion order with no way to find a project once the list
grows. Add:

1. A **Last opened** timestamp per recent project, persisted in `QSettings`.
2. A **header row** with two sortable columns, **Name** and **Last opened**.
   Default sort is Last opened, newest first.
3. A **filter field** that narrows the list by name or path as you type.

## Current state (what the implementer should know)

- `cwRecentProjectModel` (`cavewherelib/src/cwRecentProjectModel.{h,cpp}`) is a
  `QAbstractListModel` with `PathRole`/`NameRole`. Entries are
  `struct RepositoryEntry { QString projectPath; }`, persisted as a
  `QStringList` under the settings key `repositories`. `loadSettings()` prunes
  missing paths.
- Entries get added from three places: `cwRootData` on `cwProject::fileSaved`
  (`cwRootData.cpp:84`), `LoadProjectDialog.qml` after calling
  `RootData.loadProject`, and `cwRemoteRepositoryCloner` after a clone.
  Clicking a row on the Sources page calls `RootData.loadProject` without
  touching the model.
- `cwRootData::loadProject(filename)` (`cwRootData.cpp:341`) is the single C++
  funnel every open path goes through, with the user-facing path (legacy `.cw`
  conversion happens later inside `cwProject::loadFile`).
- Sorting/filtering infrastructure already exists and is used by
  `CaveLeadPage.qml`: `SortFilterProxyModel` (`cwSortFilterProxyModel`, int
  `sortRole`, `filterRole` by role name, `filterString` as wildcard,
  `Q_INVOKABLE index/mapToSource`), `TableStaticColumnModel` +
  `TableStaticColumn { text, columnWidth, sortRole }`,
  `HorizontalHeaderStaticView { view: TableStaticView }` and
  `TableStaticHeaderColumn` (click cycles None → Asc → Desc, shows an arrow,
  drives `proxy.sortRole` + `proxy.sort(order)`).
- `TableRowBackground` supplies the alternating row color; `LinkText` is the
  clickable name; `RevealInFileManagerMenuItem` is the path context menu.
- Tests: `testcases/test_cwRecentProjectModel.cpp` (Catch2, tag
  `[cwRecentProjectModel]`) and `test-qml/tst_SourcePage.qml` (uses the
  ObjectFinder chains `rootId->addRepositoryButton`,
  `rootId->repositoryListView`, `rootId->loadProjectDialog`, and
  `findChild(..., "repoLinkText_0")`). Adding an `objectName` to any ancestor
  of those items breaks the chains, so new objectNames go only on new leaf
  items.

## Design decisions

- **Timestamp lives in the model, sort/filter live in a QML proxy.** The
  source model stays insertion-ordered (the cloner iterates it by row), and
  `SourceListPage.qml` wraps it in a `SortFilterProxyModel`. No new C++ proxy
  class.
- **"Opened" is stamped in `cwRootData::loadProject`.** That is the one funnel
  every open path uses, so the Sources click, the file dialog, the remote
  clone page, and the deep link all update the timestamp without each caller
  remembering to. `LoadProjectDialog.qml` drops its explicit
  `addRepositoryFromProjectFile` call because the funnel now covers it.
  Saving also bumps the stamp: the `fileSaved` hook in `cwRootData` switches
  from `addRepositoryFromProjectFile` to `recordProjectOpened`, so "Last
  opened" reads as "last time you worked on it" and a Save As lands at the
  top.
- **Why QSettings and not the filesystem.** Opening a project is a read, and
  read timestamps (atime) are unreliable everywhere we run: macOS updates
  lazily, Windows has NTFS last-access off by default, Linux uses relatime.
  mtime only says "last saved", a `.cwproj` save may leave the project
  file's own mtime untouched, and a git pull bumps mtime with nobody opening
  the project. So the stamp is recorded by the app in `QSettings`.
- **Persistence is a second settings key, not a format change.** Keep
  `repositories` as the `QStringList` it is today (older builds keep working)
  and add `repositoryLastOpened`, a `QVariantMap` of absolute path →
  `QDateTime` (UTC). Entries with no stamp (settings from before this change)
  carry an invalid `QDateTime`, display as `—`, and sort after every stamped
  entry in both directions.
- **Filter matches `pathRole`.** The name is the last path component, so one
  role covers name and path and keeps the date string out of the match.
  Pattern is `"*" + text + "*"`, case-insensitive, same as the leads page.
- **Row click maps the proxy row back to the source row** with
  `proxy.mapToSource(proxy.index(row, 0)).row` before calling
  `repositoryProjectFile(sourceRow)`. The model's index-based API is unchanged.
- **Layout: two columns, path stays as a second line under the name.** Name
  fills the remaining width; Last opened is a fixed column
  (`Theme`-tokenized width, about 150 px). The path keeps `ElideMiddle` and
  its right-click context menu. Date format is `yyyy-MM-dd HH:mm` via
  `Qt.formatDateTime`, matching `CavePage.qml`.
- Sort choice is session-only. Persisting it is out of scope.

## Checkpoint 1: model carries Last opened (C++)

Files: `cwRecentProjectModel.{h,cpp}`, `cwRootData.cpp`,
`LoadProjectDialog.qml`, `testcases/test_cwRecentProjectModel.cpp`.

1. Add `LastOpenedRole` to `RepositoryRoles` and `"lastOpenedRole"` to
   `roleNames()`. `data()` returns the `QDateTime` (local time, converted from
   the stored UTC value).
2. Extend `RepositoryEntry` with `QDateTime lastOpened`.
3. Add `Q_INVOKABLE Monad::ResultBase recordProjectOpened(const QUrl& projectFileUrl)`:
   runs `addRepositoryFromProjectFile` (which now stamps `now` on a brand new
   entry), then stamps `now` on the matching entry and emits `dataChanged` for
   `LastOpenedRole` on that row, then saves. Existing-entry stamping is the
   only thing this adds over `addRepositoryFromProjectFile`.
4. `saveRepositories()` writes both keys; `loadSettings()` reads the map and
   drops map entries whose path was pruned. Keep the two keys consistent on
   every save.
5. `cwRootData::loadProject` calls `recordProjectOpened` with the given
   filename after `Project->loadFile` is kicked off (the path is the
   user-facing one; a warning on error, same as the `fileSaved` hook).
   Remove the now-duplicate add in `LoadProjectDialog.qml`.
   Change the `fileSaved` hook to call `recordProjectOpened` too (save bumps
   the stamp).
6. Follow the existing setter conventions (plain members; the file still uses
   a bindable for `defaultRepositoryDir`, leave it).

Tests (Catch2, `[cwRecentProjectModel]`):

- `roleNames()` contains `LastOpenedRole` → `"lastOpenedRole"`.
- `addRepositoryFromProjectFile` on a new path yields a valid `lastOpened`
  within a few seconds of now; calling it again on the same path keeps the
  original stamp.
- `recordProjectOpened` on an existing path moves the stamp forward and emits
  `dataChanged` for that row (use `SignalSpyChecker`), and the stamp survives a
  fresh `cwRecentProjectModel` instance reading the same `QSettings`.
- Saving an already-listed project (drive `cwProject::fileSaved` through a
  real save in a `cwRootData` test, or a focused test on the hook) moves the
  stamp forward.
- Settings with `repositories` but no `repositoryLastOpened` load with invalid
  stamps and no crash.
- `loadSettings` pruning a missing path also removes it from the map (check
  `QSettings` after construction).

Settings hygiene: the existing tests `settings.clear()` at the top; keep that.
Temp files must use `QTemporaryDir` (parallel test processes).

Run: `cavewhere-test "[cwRecentProjectModel]" 2>&1 | tee /tmp/recent.log`
(with `ASAN_OPTIONS=detect_container_overflow=0` on macOS Debug).

## Checkpoint 2: Sources page header, sort, and filter (QML)

Files: `SourceListPage.qml`, `Theme.qml` (one width token),
`test-qml/tst_SourcePage.qml`.

1. **Proxy.** Add a `SortFilterProxyModel { id: recentProxyId; source:
   RootData.recentProjectModel; sortRole: RecentProjectModel.LastOpenedRole;
   sortCaseSensitivity: Qt.CaseInsensitive; filterRole: "pathRole";
   filterCaseSensitivity: Qt.CaseInsensitive; filterString: "*" + filterText + "*" }`
   and call `sort(Qt.DescendingOrder)` on completion. The ListView's `model`
   becomes the proxy; it also gains `columnModel` so the header can bind to it
   (make the ListView a `TableStaticView`, or give `HorizontalHeaderStaticView`
   what it needs the same way `CaveLeadPage` does).
2. **Filter field.** `QC.TextField { objectName: "recentProjectFilterField";
   placeholderText: "Filter…" }` in the existing top `RowLayout`, right of the
   spacer, width tied to the list width. `onTextEdited` updates a
   `property string filterText` on the page (same pattern as `CaveLeadPage`).
   Empty state: when the proxy count is 0 and the source has rows, the label
   reads "No projects match the filter."; otherwise it keeps "No caving areas
   created or opened yet."
3. **Header.** `TableStaticColumnModel` with two `TableStaticColumn`s:
   `Name` (`sortRole: RecentProjectModel.NameRole`, `columnWidth` bound to
   list width minus the date column) and `Last opened`
   (`sortRole: RecentProjectModel.LastOpenedRole`, `columnWidth:
   Theme.recentProjectDateColumnWidth`). Render with
   `HorizontalHeaderStaticView` + `TableStaticHeaderColumn { model:
   recentProxyId }`. Seed the Last opened column's `sortMode` to
   `Descending` on completion so the indicator matches the default order.
   Header objectNames: `recentHeaderName`, `recentHeaderLastOpened` (new leaf
   items, so existing chains stay intact).
4. **Delegate.** Keep `TableRowBackground` and the two-line content, but lay it
   out as a `RowLayout`: left cell = `ColumnLayout { LinkText name; QC.Label
   path }` filling width; right cell = `QC.Label { objectName:
   "repoLastOpened_" + index; text: lastOpenedRole valid ?
   Qt.formatDateTime(lastOpenedRole, "yyyy-MM-dd HH:mm") : "—";
   color: Theme.textSecondary; width: date column }`. Add
   `required property date lastOpenedRole`. Row click maps proxy row → source
   row before `repositoryProjectFile`.
5. Theme: add `readonly property int recentProjectDateColumnWidth: 150` next
   to the other table/list tokens. No raw sizes or hex colors in the page.
6. Keep `pragma ComponentBehavior: Bound` if the delegate reads page ids
   (the file currently does not use it; add it only if qmllint asks).

Tests (`tst_SourcePage.qml`, Qt Quick Test, `--platform offscreen`):

- Existing four tests keep passing. `firstDelegate.pathRole` checks still hold
  because each test has one entry.
- `test_defaultOrder_mostRecentFirst`: copy the Phake Cave dataset to two
  temp names with `TestHelper.copyFile`, call
  `RootData.recentProjectModel.recordProjectOpened` on A then B, go to the
  Sources page, `tryCompare` row 0 `pathRole` to B and row 1 to A.
- `test_headerClick_sortsByName`: same two entries, click
  `recentHeaderName`, expect alphabetical; click again, expect reversed.
- `test_filter_narrowsList`: two entries, type part of one name into
  `recentProjectFilterField` (`mouseClick` + `keyClick`s), `tryCompare`
  proxy/list `count` to 1 and row 0 to the match; clear the field, count
  returns to 2. Also check the "No projects match the filter." label when the
  text matches nothing.
- `test_clickRow_afterSort_opensThatProject`: sort by name so proxy order
  differs from source order, click row 0's link, discard the ask-to-save
  dialog if it appears, and `tryCompare(RootData.project, "filename", ...)`
  to the clicked project. This is the regression guard for the proxy → source
  mapping.

Run: `cavewhere-qml-test --platform offscreen -input test-qml/tst_SourcePage.qml 2>&1 | tee /tmp/source-page.log`.
Re-run twice before blaming a change (cold-binary flake note in memory).

## Workflow shape

```
for checkpoint in [1, 2]:
    implement  (opus, medium)  → build + targeted tests green
    simplify   (opus, medium)  → /simplify-style pass, re-run targeted tests
    review     (opus, medium, read-only) → findings as JSON
    fix+commit (opus, medium)  → apply confirmed findings, re-run tests, commit
```

One commit per checkpoint. Commit subjects: "Track last opened time for recent
projects" and "Add sort, filter, and header to the Sources page". Both suites
(`cavewhere-test`, `cavewhere-qml-test`) run once at the end of checkpoint 2
before the final commit.

## Out of scope (deliberately)

- Persisting the chosen sort column/order across sessions.
- Removing a project from the recent list from the UI.
- Relative dates ("Today", "Yesterday").
- A combined name+path+date search role.
- Replacing the remaining bindable property in `cwRecentProjectModel`.

## Decisions from review (2026-10-13)

1. Saving bumps Last opened too. The stamp is app-recorded in `QSettings`;
   the filesystem cannot supply "last opened" (see Design decisions).
2. Date format is `yyyy-MM-dd HH:mm`.
3. Two columns only; the path stays as the second line under the name.
