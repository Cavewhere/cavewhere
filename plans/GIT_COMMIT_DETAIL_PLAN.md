# Git Commit Detail Panel

Split `GitHistoryPage.qml` into two areas with a draggable `SplitView`. Top: existing git graph history. Bottom: detail panel that shows either commit details (for real commits) or a working tree panel (for uncommitted changes). A synthetic "Uncommitted Changes" entry appears at the top of the history when dirty files exist. HEAD is selected by default (or the synthetic entry when there are uncommitted changes). Clicking a file navigates to a dedicated diff page (following CaveWhere's page navigation pattern). Image diffs get their own compare page.

---

## Goals

- Synthetic "Uncommitted Changes" entry at top of history when `modifiedFileCount > 0`
- HEAD commit (or synthetic entry) is selected by default when the history page loads
- Select a commit in the history to view full metadata (author, date/time, SHA, subject, body)
- Copy commit SHA to clipboard
- Show list of changed files with status (added, deleted, modified, renamed)
- Navigate to a dedicated diff page per file (colored unified diff rendered as a `ListView` of diff lines)
- Handle binary files gracefully (show "Binary file" badge)
- Image compare page for image files (drag-bar overlay showing before/after) via `QQuickImageProvider`
- Handle large diffs (show "Diff too large" message above a configurable threshold)
- Parent selector for merge commits (diff against any parent)
- Working tree panel: view uncommitted files, write commit message, commit all changes
- No individual file staging/unstaging — all changes are always committed together (preserves CaveWhere's assumptions about what gets pushed)

---

## Threading & Async Patterns

All background work in QQuickGit must follow CaveWhere's established async patterns:

### Thread Pool

QQuickGit needs its own `GitConcurrent` utility (analogous to `cwConcurrent`) that accepts an externally-provided `QThreadPool*`. This lets CaveWhere share its `cwTask::threadPool()` with QQuickGit, keeping all background work on a single managed pool.

```cpp
// QQuickGit/src/GitConcurrent.h
class QQUICKGIT_EXPORT GitConcurrent
{
public:
    GitConcurrent() = delete;

    static void setThreadPool(QThreadPool* pool);
    static QThreadPool* threadPool();

    template <class Function, class ...Args>
    static auto run(Function &&f, Args &&...args)
    {
        return QtConcurrent::run(threadPool(), std::forward<Function>(f), std::forward<Args>(args)...);
    }

private:
    static QThreadPool* s_threadPool; // nullptr by default
};
```

**`setThreadPool` guard:**
```cpp
void GitConcurrent::setThreadPool(QThreadPool* pool)
{
    Q_ASSERT_X(!s_threadPool, "GitConcurrent::setThreadPool",
               "Thread pool has already been set — call setThreadPool() only once at startup");
    s_threadPool = pool;
}
```

**Startup:** CaveWhere calls `GitConcurrent::setThreadPool(cwTask::threadPool())` during startup. If `setThreadPool()` is never called, `threadPool()` falls back to `QThreadPool::globalInstance()`. This is set once at startup and never changed. QQuickGit unit tests can leave the default (global pool) or set their own.

### Restarter Pattern

All classes that do repeatable/cancellable async work must use `AsyncFuture::Restarter<T>` with a **concrete result type** (never `QVariant`). This replaces the current `GitGraphModel` pattern which incorrectly uses `Restarter<QVariant>`.

**Pre-requisite task:** Refactor `GitGraphModel` to use `AsyncFuture::Restarter<IndexPassResult>` instead of `Restarter<QVariant>`. This sets the pattern for all new classes.

Standard Restarter lifecycle:
```cpp
// Header: member declaration
AsyncFuture::Restarter<MyResultType> m_restarter;

// Constructor: initialize restarter with context, then register callback
MyClass::MyClass(QObject* parent)
    : QObject(parent)
    , m_restarter(this)  // Restarter requires QObject* context for signal marshalling
{
m_restarter.onFutureChanged([this]() {
    auto future = m_restarter.future();
    AsyncFuture::observe(future).context(this, [this, future]() {
        if (future.isCanceled()) return;
        auto result = future.result();
        // apply result to model...
        m_loading = false;
        emit loadingChanged();
    });
});
}

// Trigger: restart on property change
m_restarter.restart([repoPath, sha]() {
    return GitConcurrent::run([repoPath, sha]() -> MyResultType {
        // heavy work on thread pool...
    });
});

// Destructor: cancel + wait
~MyClass() {
    m_restarter.future().cancel();
    AsyncFuture::waitForFinished(m_restarter.future());
}
```

### Thread Safety

Background threads must open their own `git_repository*` handle from the repo path, never sharing the one owned by `GitRepository`. This is consistent with how `GitGraphModel` already works.

### Error Propagation

Background tasks should return errors inline in their result structs (e.g., an `errorMessage` field). The UI classes expose an `errorMessage` property. Transient errors (bad SHA, diff failure) are shown inline in the panel UI. Fatal errors (repo corruption, can't open repo) are forwarded to `cwErrorModel` via a signal so they appear in the app-level error display.

---

## Architecture: New C++ Classes (in QQuickGit)

### A. `GitConcurrent` (static utility)

Thread pool wrapper for QQuickGit (see Threading section above). Analogous to `cwConcurrent`.

**Files:** `QQuickGit/src/GitConcurrent.h`, `QQuickGit/src/GitConcurrent.cpp`

### B. `GitCommitInfo` (QObject, QML_ELEMENT)

Given a SHA, fetches **both** commit metadata and the changed file list in a single background task. This avoids opening the commit object twice. `GitCommitInfo` exposes the scalar metadata properties directly. The file list data is provided to `GitCommitFileModel` (see below) which takes a `GitCommitInfo*` as input.

**Properties:**
| Property | Type | Notes |
|----------|------|-------|
| `repository` | `GitRepository*` | Required |
| `commitSha` | `QString` | Set to load; empty clears |
| `parentIndex` | `int` | Which parent to diff against (default 0) |
| `author` | `QString` | Read-only |
| `authorEmail` | `QString` | Read-only |
| `timestamp` | `QDateTime` | Read-only |
| `subject` | `QString` | First line of commit message |
| `body` | `QString` | Remaining lines of commit message |
| `parentShas` | `QStringList` | Read-only |
| `parentSubjects` | `QStringList` | First line of each parent's message (for selector display) |
| `isMergeCommit` | `bool` | Read-only, `parentShas.length > 1` |
| `loading` | `bool` | Read-only, true while background thread is running |
| `errorMessage` | `QString` | Read-only, non-empty on error |

**Background result type (shared):**
```cpp
struct CommitLoadResult
{
    // Metadata
    QString author;
    QString authorEmail;
    QDateTime timestamp;
    QString subject;
    QString body;
    QStringList parentShas;
    QStringList parentSubjects;

    // File list
    struct FileEntry {
        QString filePath;
        QString oldFilePath;
        int status;
        QString statusText;
        bool isBinary;
        bool isImage;
    };
    QVector<FileEntry> files;

    QString errorMessage;
};
```

**Image detection:** `isImage` is determined using a two-tier approach:
1. **Extension-based (fast, no I/O):** Check the file extension against `QImageReader::supportedImageFormats()`. This is a static call and is computed inline when building each `FileEntry` on the background thread.
2. **Header-based (accurate, low I/O):** For files flagged as binary but without a recognized image extension, use `QImageReader::imageFormat(QIODevice*)` on a `QBuffer` wrapping the blob content. Since the blob is already available during diff generation on the background thread, the cost of reading the first few magic bytes is negligible.

```cpp
static bool isImageByExtension(const QString& filePath) {
    auto suffix = QFileInfo(filePath).suffix().toLower().toUtf8();
    return QImageReader::supportedImageFormats().contains(suffix);
}

// For binary files without recognized extension, check blob header
static bool isImageByHeader(const QByteArray& blobContent) {
    QBuffer buffer(const_cast<QByteArray*>(&blobContent));
    buffer.open(QIODevice::ReadOnly);
    return !QImageReader::imageFormat(&buffer).isEmpty();
}
```
```

The single background task opens the commit, reads metadata, then diffs the commit tree against the selected parent tree to build the file list — all in one `git_repository*` session.

**Loading:** Uses `AsyncFuture::Restarter<CommitLoadResult>` member. When `commitSha` or `parentIndex` changes, calls `m_restarter.restart(...)`. Parent subjects are fetched on the same background thread (capped at 8 parents — octopus merges beyond 8 show "and N more parents" in the UI).

**Signal for file model:**
```cpp
signals:
    void fileListReady(const QVector<CommitLoadResult::FileEntry>& files);
```

When the background task completes, `GitCommitInfo` applies metadata to its own properties and emits `fileListReady()` with the file entries. `GitCommitFileModel` connects to this signal.

**Destructor:** Cancels restarter future and waits for completion.

### C. `GitCommitFileModel` (QAbstractListModel, QML_ELEMENT)

Exposes the list of changed files as a list model. Takes a `GitCommitInfo*` as its data source instead of loading independently — the two classes share one background task.

**Properties:**
| Property | Type | Notes |
|----------|------|-------|
| `commitInfo` | `GitCommitInfo*` | Required — connects to its `fileListReady` signal |
| `loading` | `bool` | Read-only, mirrors `commitInfo->loading()` |
| `errorMessage` | `QString` | Read-only, mirrors `commitInfo->errorMessage()` |

**Model roles** (one row per changed file):
| Role | Type | Source |
|------|------|--------|
| `FilePathRole` | `QString` | `git_diff_delta.new_file.path` |
| `OldFilePathRole` | `QString` | `git_diff_delta.old_file.path` (for renames) |
| `StatusRole` | `int` | `GIT_DELTA_ADDED / DELETED / MODIFIED / RENAMED / COPIED` |
| `StatusTextRole` | `QString` | Human-readable: "Added", "Deleted", etc. |
| `IsBinaryRole` | `bool` | `GIT_DIFF_FLAG_BINARY` |
| `IsImageRole` | `bool` | Detected via extension (`QImageReader::supportedImageFormats`) + header check for unrecognized-extension binaries |

**Lazy line stats:** `AddedLinesRole` and `DeletedLinesRole` are NOT computed during the initial model load. They require creating a `git_patch` per file which is expensive for commits touching many files. Instead, line stats are fetched on demand per **file** (not per line):

- `Q_INVOKABLE void fetchLineStats(int row)` — Launches `GitConcurrent::run()` for a single file, creating a `git_patch` and calling `git_patch_line_stats()` for that file's delta. Uses `AsyncFuture::observe(...).context(this, ...)` to marshal results back and emit `dataChanged` for that row. The result is cached so subsequent calls for the same row return immediately. Outstanding futures are tracked in a `QHash<int, QFuture<LineStats>>` and cancelled both in the destructor **and** when `commitInfo` emits `fileListReady()` with new data (to prevent stale results from a previous commit arriving for wrong rows).
- The QML delegate calls `fetchLineStats(index)` on `Component.onCompleted`

| Role | Type | Source |
|------|------|--------|
| `AddedLinesRole` | `int` | From `git_patch_line_stats`, -1 until fetched |
| `DeletedLinesRole` | `int` | From `git_patch_line_stats`, -1 until fetched |
| `LineStatsFetchedRole` | `bool` | `true` once line stats have been loaded for this row |

**When `commitInfo` changes or emits `fileListReady`:** The model calls `beginResetModel()`, replaces its internal file list, cancels all outstanding line-stat futures, clears the line-stat cache, and calls `endResetModel()`.

**Destructor:** Cancels all outstanding line-stat futures and waits for completion.

### D. `GitFilePatch` (QAbstractListModel, QML_ELEMENT)

On-demand fetch of unified diff for a single file in a commit. **This class IS the list model** — each row is one diff line. This avoids the complexity of a QObject owning a separate child model with tricky lifetime management.

**Properties:**
| Property | Type | Notes |
|----------|------|-------|
| `repository` | `GitRepository*` | Required |
| `commitSha` | `QString` | Required |
| `parentIndex` | `int` | Which parent to diff against (default 0) |
| `filePath` | `QString` | Required |
| `maxDiffLines` | `int` | Threshold for large-diff detection (default 5000); configurable from QML |
| `loading` | `bool` | Read-only |
| `tooLarge` | `bool` | Read-only, set if patch stats exceed `maxDiffLines` before generating text |
| `isBinary` | `bool` | Read-only |
| `errorMessage` | `QString` | Read-only, non-empty on error |

**Model roles** (one row per diff line):
| Role | Type |
|------|------|
| `TextRole` | `QString` |
| `OriginRole` | `QString` | Single char mapped from `git_diff_line` origins (see below) |
| `OldLineNoRole` | `int` |
| `NewLineNoRole` | `int` |

**libgit2 diff line origin mapping:**

libgit2's `git_diff_line` provides these origin values. We map them to simplified categories for QML rendering:

| libgit2 constant | char | Our mapping | Notes |
|---|---|---|---|
| `GIT_DIFF_LINE_CONTEXT` | `' '` | `' '` (context) | Unchanged line |
| `GIT_DIFF_LINE_ADDITION` | `'+'` | `'+'` (added) | Added line |
| `GIT_DIFF_LINE_DELETION` | `'-'` | `'-'` (deleted) | Deleted line |
| `GIT_DIFF_LINE_HUNK_HDR` | `'H'` | `'H'` (hunk header) | `@@ -1,3 +1,5 @@` lines |
| `GIT_DIFF_LINE_FILE_HDR` | `'F'` | Filtered out | File-level header (redundant with our UI header) |
| `GIT_DIFF_LINE_BINARY` | `'B'` | Filtered out | Binary notice (handled by `isBinary` property) |
| `GIT_DIFF_LINE_CONTEXT_EOFNL` | `'='` | `' '` (context) | No newline at end of file (context) |
| `GIT_DIFF_LINE_ADD_EOFNL` | `'>'` | `'+'` (added) | No newline added |
| `GIT_DIFF_LINE_DEL_EOFNL` | `'<'` | `'-'` (deleted) | No newline deleted |

**Why GitFilePatch is itself a QAbstractListModel:**
- The diff page creates exactly one `GitFilePatch` per page instance
- No need for a separate "lines model" object with its own lifetime
- The `GitFilePatch` IS the model that the diff page's `ListView` binds to
- Properties like `loading`, `tooLarge`, `isBinary` live on the same object — no indirection
- When the page is destroyed, the model is destroyed — clean lifecycle

**Background result type:**
```cpp
struct FilePatchResult
{
    struct DiffLine {
        QString text;
        char origin; // '+', '-', ' ', 'H' (mapped from libgit2 origins)
        int oldLineNo;
        int newLineNo;
    };
    QVector<DiffLine> lines;
    bool tooLarge = false;
    bool isBinary = false;
    QString errorMessage;
};
```

**Large diff detection:** Before generating full patch text, check `git_diff_stats` on the delta. If the total added+deleted lines exceed `maxDiffLines`, set `tooLarge = true` and skip patch generation. This avoids the cost of generating 500KB+ of text just to discard it.

**Loading:** Uses `AsyncFuture::Restarter<FilePatchResult>` member. Auto-loads when all required properties (`repository`, `commitSha`, `filePath`) are set. If any property changes while a fetch is in flight, the restarter automatically cancels the old work.

**Destructor:** Cancels restarter future and waits for completion.

### E. `GitCommitImageProvider` (QQuickImageProvider)

A custom `QQuickImageProvider` registered on the QML engine that serves image content from git commits. This avoids writing temp files.

**URL scheme:** `image://gitcommit/<repo-id>/<commit-sha>/<file-path>`

Where `<repo-id>` is an integer assigned when a `GitRepository` is registered with the provider. This avoids ambiguity from `/` characters in repo paths.

**Implementation:**
- Subclass `QQuickImageProvider` with `Image` type
- Maintain a `QHash<int, QString>` mapping repo IDs to repo paths, protected by a `QReadWriteLock`
- `int registerRepository(const QString& repoPath)` — acquires write lock, assigns and returns an integer ID
- `void unregisterRepository(int id)` — acquires write lock, removes the mapping
- In `requestImage()`, parse the URL as `<repo-id>/<40-char-sha>/<file-path>`:
  - Split on first `/` to get repo ID (integer)
  - Next 40 characters are the SHA
  - Remainder after the next `/` is the file path (may contain `/`)
- Call `GitRepository::fileContentAtCommit()` (static), load the `QByteArray` into a `QImage`, and return it
- **Thread safety:** `requestImage()` is called from Qt's rendering thread. The static `fileContentAtCommit()` opens its own `git_repository*` handle, so this is thread-safe by design. The repo ID map is guarded by a `QReadWriteLock` — `requestImage()` acquires a read lock, `registerRepository()`/`unregisterRepository()` acquire a write lock. This allows concurrent image requests while ensuring safe registration/unregistration from the main thread.

**Registration:** The image provider must be registered on the `QQmlEngine` via `engine->addImageProvider("gitcommit", new GitCommitImageProvider)`. Since QQuickGit is a QML module (not a plugin with an init hook), the application is responsible for registration. Add a static convenience method:
```cpp
static void GitCommitImageProvider::registerOn(QQmlEngine* engine);
```
CaveWhere calls this from `main.cpp` after engine creation.

### F. Image content retrieval

`GitRepository::fileContentAtCommit()` already exists as a static method returning `Monad::Result<QByteArray>` (verified in `QQuickGit/src/GitRepository.h:185`). Add a non-static `Q_INVOKABLE` wrapper so QML can call it directly for non-image use cases:

```cpp
Q_INVOKABLE QByteArray fileContentAtCommit(const QString& commitOid,
                                            const QString& relativePath) const;
```

The wrapper checks that the repository is open (returns empty `QByteArray` and logs a warning if not) before delegating to the static method with the repository's path.

### G. `GitWorkingTreeModel` (QAbstractListModel, QML_ELEMENT)

Exposes the list of uncommitted (modified, added, deleted, untracked) files in the working directory as a list model. This is the data source for the working tree panel that appears when the synthetic "Uncommitted Changes" entry is selected.

**Properties:**
| Property | Type | Notes |
|----------|------|-------|
| `repository` | `GitRepository*` | Required |
| `loading` | `bool` | Read-only |
| `errorMessage` | `QString` | Read-only, non-empty on error |

**Model roles** (one row per changed file — same roles as `GitCommitFileModel` for UI consistency):
| Role | Type | Source |
|------|------|--------|
| `FilePathRole` | `QString` | `git_status_entry` new file path |
| `OldFilePathRole` | `QString` | `git_status_entry` old file path (for renames) |
| `StatusRole` | `int` | Mapped from `git_status_t` flags to match `GIT_DELTA_*` values |
| `StatusTextRole` | `QString` | Human-readable: "Added", "Modified", "Deleted", "Untracked" |
| `IsBinaryRole` | `bool` | Detected via `git_diff_file` flags |
| `IsImageRole` | `bool` | Detected via extension (`QImageReader::supportedImageFormats`) + header check for unrecognized-extension binaries |

**Refresh trigger:** Refreshes when `GitRepository::modifiedFileCountChanged` fires (which already happens after saves, commits, pulls, etc.). Uses `AsyncFuture::Restarter<WorkingTreeResult>` with `GitConcurrent::run()`. The background task calls `git_status_list_new()` with `GIT_STATUS_SHOW_INDEX_AND_WORKDIR` and builds the file entry list. Filters out LFS false-positives using the same `hydratedLfsFileMatchesIndexPointer()` logic already in `GitRepository.cpp`.

**Lazy line stats:** Same mechanism as `GitCommitFileModel`. `AddedLinesRole`, `DeletedLinesRole`, and `LineStatsFetchedRole` are provided but not computed during initial load. `Q_INVOKABLE void fetchLineStats(int row)` launches `GitConcurrent::run()` to create a `git_patch` from the working tree diff for that file and calls `git_patch_line_stats()`. Results are cached per row. Outstanding futures are cancelled on model refresh (when `modifiedFileCountChanged` fires) and in the destructor. The QML delegate calls `fetchLineStats(index)` on `Component.onCompleted`.

| Role | Type | Source |
|------|------|--------|
| `AddedLinesRole` | `int` | From `git_patch_line_stats`, -1 until fetched |
| `DeletedLinesRole` | `int` | From `git_patch_line_stats`, -1 until fetched |
| `LineStatsFetchedRole` | `bool` | `true` once line stats have been loaded for this row |

**No staging:** This model is read-only. There are no stage/unstage methods. All files shown will be committed together when the user clicks "Commit".

**Background result type:**
```cpp
struct WorkingTreeResult
{
    struct FileEntry {
        QString filePath;
        QString oldFilePath;
        int status;
        QString statusText;
        bool isBinary;
        bool isImage;
    };
    QVector<FileEntry> files;
    QString errorMessage;
};
```

**Destructor:** Cancels restarter future and waits for completion.

### H. `GitWorkingTreeFilePatch` support in `GitFilePatch`

When viewing diffs for uncommitted files, `GitFilePatch` needs to diff HEAD vs the working tree (instead of parent commit vs commit). Add an optional `workingTree` mode:

**Additional property on `GitFilePatch`:**
| Property | Type | Notes |
|----------|------|-------|
| `workingTree` | `bool` | Default `false`. When `true`, diffs HEAD vs working directory for `filePath`. `commitSha` and `parentIndex` are ignored. |

**Implementation:** When `workingTree` is `true`, the background task:
1. Opens its own `git_repository*` from the repo path (standard thread safety pattern)
2. Resolves HEAD via `git_repository_head()` → `git_reference_peel()` → `git_commit_tree()` to get the HEAD tree
3. Calls `git_diff_tree_to_workdir_with_index()` to produce a diff of HEAD vs the working directory (includes both staged and unstaged changes — consistent with CaveWhere's "commit all" model)
4. Filters the diff to the single `filePath` and generates patch lines

`commitSha` and `parentIndex` are ignored — HEAD is always the implicit baseline for uncommitted changes. The rest of the pipeline (line parsing, large-diff detection, origin mapping) is identical to committed diffs.

---

## Architecture: QML Components

### I. Synthetic "Uncommitted Changes" row in `GitGraphModel`

When `GitRepository::modifiedFileCount > 0`, `GitGraphModel` inserts a synthetic row at index 0 representing uncommitted working tree changes. This follows the pattern used by GitKraken, Sourcetree, Fork, and GitHub Desktop.

**Implementation:**
- `GitGraphModel` observes `GitRepository::modifiedFileCountChanged`
- When `modifiedFileCount` transitions from 0 → >0, insert the synthetic row via `beginInsertRows(QModelIndex(), 0, 0)` / `endInsertRows()`
- When `modifiedFileCount` transitions from >0 → 0 (e.g., after a commit), remove it via `beginRemoveRows(QModelIndex(), 0, 0)` / `endRemoveRows()`
- The synthetic row returns special values for each role:
  - `ShaRole`: empty string `""` (distinguishes it from real commits)
  - `MessageRole`: `"Uncommitted Changes"`
  - `AuthorRole`: empty string
  - `TimestampRole`: current date/time
  - `LanesRole`: mirrors the HEAD commit's lane layout (the working tree is on the same branch)
  - `ActiveLaneRole`: same as HEAD commit's active lane
  - `RefsRole`: empty list
- Add `Q_PROPERTY(bool hasUncommittedChanges)` — true when the synthetic row exists
- All real commit indices shift by 1 when the synthetic row is present. The internal `mOids`, `mGraph`, `mCache` arrays are unchanged — `data()` and `fetchDetail()` adjust the index by subtracting 1 when the synthetic row exists.

**Incremental model updates:** Avoid `beginResetModel`/`endResetModel` where possible — full resets destroy UI state (scroll position, selection, animations). The synthetic row uses incremental insert/remove as described above. For the main commit graph refresh, the model should:
- Track whether the synthetic row existed before refresh
- Perform the graph update (which may use reset for topology changes like rebase/force-push)
- After reset completes, re-insert the synthetic row via `beginInsertRows`/`endInsertRows` if `modifiedFileCount > 0`
- For incremental updates (new commits appended), prefer `beginInsertRows`/`endInsertRows` over full reset

**Visual distinction in `GitHistoryRow`:** When `commitSha === ""` (synthetic row), the row uses a dashed border, a distinct background color, and a pencil/edit icon instead of the commit dot. Colors come from color properties (overridden by Theme in CaveWhere).

### J. Selection in `GitHistoryView.qml` + `GitHistoryRow.qml`

- Add `property string selectedSha` (read-only, exposed to parent)
- Add `property bool selectedIsUncommitted` (read-only, `true` when synthetic row is selected)
- Add highlight/selection coloring on `GitHistoryRow`
- Wire `TapHandler` in `GitHistoryRow` to set `listView.currentIndex`
- The delegate in `GitHistoryView` already has `required property string sha` (bound from `ShaRole`), and `GitHistoryRow` already has `required property string commitSha` — no new properties needed
- Derive `selectedSha` from the current delegate's `sha` required property via `ListView.currentItem.commitSha`
- Derive `selectedIsUncommitted` from `selectedSha === ""`
- Use `ListView.highlight` delegate for the selection visual
- **Default selection:** Set `listView.currentIndex` to `0` after the model finishes loading — this selects the synthetic "Uncommitted Changes" row if it exists, or HEAD otherwise

### K. `GitCommitDetailPanel.qml` (new, in QQuickGit/qml)

Shows commit metadata and file list. File diffs are NOT inline — clicking a file navigates to a dedicated diff page.

**Important:** This component lives in QQuickGit and must NOT import CaveWhere types. Navigation is handled via signals — the parent `GitHistoryPage.qml` (CaveWhere side) connects to these signals and performs navigation via `cwPageSelectionModel`.

**Color properties** — exposed as `property color` declarations with sensible defaults (QQuickGit has no theming system). `GitHistoryPage.qml` overrides these from `Theme`:
```qml
property color addedColor: "#4caf50"
property color deletedColor: "#cf222e"
property color modifiedColor: "#1a7f37"
property color renamedColor: "#d4a72c"
```

Top section — commit metadata (from `GitCommitInfo`):
```
Author: Name <email>              2026-03-28 14:32:05
SHA: abc123def456... [copy button]
[Parent selector ComboBox — only visible for merge commits]
[For >8 parents: "and N more parents" text below ComboBox]

Subject line in bold
Body text in lighter font (if any)
```

The SHA is displayed with a copy-to-clipboard button. Since `GitCommitDetailPanel` lives in QQuickGit and must not import CaveWhere types, clipboard access is handled via a signal: clicking the copy button emits `copyRequested(string text)`. The parent `GitHistoryPage.qml` (CaveWhere side) connects to this signal and calls `RootData.copyText(text)` (which wraps `QGuiApplication::clipboard()->setText()`).

**Error display:** When `commitInfo.errorMessage` is non-empty, show an inline error banner at the top of the panel instead of metadata.

Bottom section — file list (`ListView` backed by `GitCommitFileModel`):
- Each row: status icon (green + / red - / blue M / yellow arrow) + file path + `+N / -M` line counts (shown as `"-"` while `lineStatsFetched` is `false`)
- Delegate calls `model.fetchLineStats(index)` on `Component.onCompleted`
- Click emits `fileClicked(filePath, isBinary, isImage, statusText)` signal (NOT direct navigation)
- `isImage` comes from the model's `IsImageRole`
- If `isBinary` and non-image: show "Binary file" badge, click disabled

**Signals:**
```qml
signal fileClicked(string filePath, bool isBinary, bool isImage, string statusText)
signal copyRequested(string text)
```

### L. `GitWorkingTreePanel.qml` (new, in QQuickGit/qml)

Shows uncommitted file list with commit message input. Displayed in the detail area when the synthetic "Uncommitted Changes" row is selected. Like `GitCommitDetailPanel`, this lives in QQuickGit and must NOT import CaveWhere types.

**Properties:**
```qml
required property GitRepository repository
```

**Color properties** — same pattern as `GitCommitDetailPanel`:
```qml
property color addedColor: "#4caf50"
property color deletedColor: "#cf222e"
property color modifiedColor: "#1a7f37"
property color untrackedColor: "#8250df"
```

**Signals:**
```qml
signal fileClicked(string filePath, bool isBinary, bool isImage, string statusText)
signal commitRequested(string subject, string description)
```

**Layout:**
```
┌─────────────────────────────────────────────────┐
│  Uncommitted Changes (N files)                  │
├─────────────────────────────────────────────────┤
│                                                 │
│  Changed Files (ListView, GitWorkingTreeModel)  │
│  - status icon + file path                      │
│  - click emits fileClicked signal               │
│                                                 │
├─────────────────────────────────────────────────┤
│  Subject: [____________________________________]│
│  Description (optional):                        │
│  [______________________________________________]│
│  [______________________________________________]│
│                                                 │
│  [ Commit All Changes ]                         │
│                                                 │
│  Note: All modified files will be committed.    │
└─────────────────────────────────────────────────┘
```

- **File list:** `ListView` backed by `GitWorkingTreeModel`. Each row has status icon + file path + `+N / -M` line counts (shown as `"-"` while `lineStatsFetched` is `false`). Delegate calls `model.fetchLineStats(index)` on `Component.onCompleted`. Clicking emits `fileClicked` signal (same signature as `GitCommitDetailPanel`).
- **Commit message:** Subject `TextField` (required, single line) + Description `TextArea` (optional, multi-line). Subject is validated — "Commit" button is disabled when empty.
- **Commit button:** Emits `commitRequested(subject, description)`. The parent `GitHistoryPage` handles the actual commit by calling `GitRepository::commitAll(subject, description)`. The button is disabled while the repository is committing. Note: `commitAll()` requires `account()` to be set (`Q_ASSERT`), but CaveWhere guarantees this at startup before the UI is reachable — no need to disable the button for missing account. Add a code comment documenting this assumption.
- **Empty state:** When `GitWorkingTreeModel` has 0 rows (race condition where files were committed between selection and panel display), show "No uncommitted changes" message.

### M. `GitFileDiffPage.qml` (new, in cavewherelib/qml)

A full page (using `StandardPage`) for viewing the colored unified diff of a single file. Follows the same navigation pattern as Cave -> Trip pages — the user navigates here from the commit detail panel and uses the back button to return. Supports both committed diffs and working tree diffs.

**Properties:**
```qml
required property GitRepository repository
required property string commitSha
required property int parentIndex
required property string filePath
required property string statusText  // "Added", "Modified", etc.
property bool workingTree: false     // When true, diffs HEAD vs working directory
```

**Layout:**
- Header bar: file path + status badge + back button (standard page navigation)
- Body: `ListView` bound to a `GitFilePatch` model instance
  - Each delegate is a single `Text` item in monospace font
  - Background `Rectangle` color determined by `origin` role (add/delete/hunk/context)
  - Colors come from `Theme` diff color tokens
  - Line numbers shown in a gutter column
  - This approach virtualizes rendering for large diffs
- If `gitFilePatch.tooLarge`: show "Diff too large to display (N lines)" message
- If `gitFilePatch.isBinary`: show "Binary file" badge
- If `gitFilePatch.errorMessage` is non-empty: show inline error text

**GitFilePatch lifecycle:** The `GitFilePatch` instance is created as a child of the page. When the user navigates back, the page is destroyed, which destroys the `GitFilePatch` and cancels any in-flight async work. Clean lifecycle with no manual cleanup needed.

```qml
StandardPage {
    id: diffPage

    GitFilePatch {
        id: filePatch
        repository: diffPage.repository
        commitSha: diffPage.commitSha
        parentIndex: diffPage.parentIndex
        filePath: diffPage.filePath
        workingTree: diffPage.workingTree
    }

    // Header with file path, status badge
    // ListView { model: filePatch; delegate: DiffLineDelegate {} }
    // Error / tooLarge / isBinary states
}
```

### N. `GitImageComparePage.qml` (new, in cavewherelib/qml)

A full page for swipe-comparing image diffs. Navigated to from the file list when clicking a binary image file.

**Properties:**
```qml
required property GitRepository repository
required property string commitSha
required property int parentIndex
required property string filePath
property bool workingTree: false  // When true, "after" image loads from disk
```

**Layout:**
- Header bar: file path + "Before" / "After" labels + back button
- Two `Image` items layered on top of each other
- A vertical divider bar the user can drag left/right
- Left side: `clip: true`, width bound to divider x-position, shows "before" image
- Right side: full "after" image underneath
- Supports all formats handled by `QImage` (PNG, JPG, BMP, SVG, etc.)

**Image sources depend on mode:**

| Mode | "Before" (left) | "After" (right) |
|------|-----------------|-----------------|
| Committed diff (`workingTree: false`) | `"image://gitcommit/" + repoId + "/" + parentSha + "/" + filePath` | `"image://gitcommit/" + repoId + "/" + commitSha + "/" + filePath` |
| Working tree diff (`workingTree: true`) | `"image://gitcommit/" + repoId + "/" + headSha + "/" + filePath` | `"file://" + repository.directory.path() + "/" + filePath` (direct filesystem; `directory` is a `QDir`, use `.path()` for the string) |
| Newly added image (no HEAD version) | Hidden / placeholder ("No previous version") | File URL or commit URL as appropriate |
| Deleted image | Commit URL as appropriate | Hidden / placeholder ("File deleted") |

Because this is a full page, there is no gesture conflict with `SplitView` drag handles.

### O. `GitHistoryPage.qml` update

Since HEAD (or the synthetic uncommitted row) is always selected by default, the detail panel is always visible — no collapse/expand logic needed. The detail area swaps between `GitCommitDetailPanel` and `GitWorkingTreePanel` based on whether the selected row is a real commit or the synthetic "Uncommitted Changes" entry.

```qml
SplitView {
    orientation: Qt.Vertical
    anchors.fill: parent
    anchors.margins: Theme.pageMargin

    GitHistoryView {
        id: historyView
        SplitView.fillHeight: true
        repository: RootData.project.repository
        laneColors: Theme.laneColors
        scrollBarSpacing: Theme.pageMargin
    }

    // Detail area — swaps between commit detail and working tree panel
    Item {
        SplitView.preferredHeight: 300
        SplitView.minimumHeight: 100
        clip: true

        GitCommitDetailPanel {
            id: commitDetailPanel
            anchors.fill: parent
            visible: !historyView.selectedIsUncommitted
            repository: RootData.project.repository
            commitSha: historyView.selectedSha
            addedColor: Theme.success
            deletedColor: Theme.danger
            modifiedColor: Theme.info
            renamedColor: Theme.warning
        }

        GitWorkingTreePanel {
            id: workingTreePanel
            anchors.fill: parent
            visible: historyView.selectedIsUncommitted
            repository: RootData.project.repository
            addedColor: Theme.success
            deletedColor: Theme.danger
            modifiedColor: Theme.info
            untrackedColor: Theme.accent
        }
    }
}
```

**Commit handling:** `GitHistoryPage` connects to `GitWorkingTreePanel::commitRequested` and calls `GitRepository::commitAll()`. After the commit completes, `GitRepository` emits signals that cause `GitGraphModel` to refresh — the synthetic row disappears (if no more dirty files), and the new HEAD commit appears at index 0 and is auto-selected.

**Error handling:** `commitAll()` uses `check()` internally, which throws `std::runtime_error` on libgit2 errors. Since QML cannot catch C++ exceptions, the commit call is wrapped in a C++ helper on `cwProject` (or a small bridge class):

```cpp
// In cwProject (or a thin helper exposed to QML)
Q_INVOKABLE void safeCommitAll(const QString& subject, const QString& description) {
    try {
        repository()->commitAll(subject, description);
    } catch (const std::runtime_error& e) {
        errorModel()->append(cwError(QString("Commit failed: %1").arg(e.what()),
                                     cwError::Fatal));
    }
}
```

This routes commit failures to `RootData.project.errorModel` so they appear in the app-level error display.

```qml
Connections {
    target: workingTreePanel
    function onCommitRequested(subject, description) {
        RootData.project.safeCommitAll(subject, description)
    }
}
```

**Page registration for sub-pages:** `GitHistoryPage` registers `GitFileDiffPage` and `GitImageComparePage` as child pages. Both panels' `fileClicked` signals are handled by the same navigation logic. For uncommitted files, the diff page uses `GitFilePatch` with `workingTree: true`.

**Accessing the parent page:** `PageView` is a QML attached type (`cwPageView` / `cwPageViewAttachedType`). Any item inside a `PageView` can access `page.PageView.page` to get the `cwPage*` for the current page. `GitHistoryPage` (a `StandardPage`) lives inside a `PageView`, so `page.page.PageView.page` provides the parent page reference needed by `registerPage()`.

```qml
QQ.Component {
    id: diffPageComponent
    GitFileDiffPage { }
}

QQ.Component {
    id: imageComparePageComponent
    GitImageComparePage { }
}

// Shared navigation handler for both panels
function navigateToFileDiff(filePath, isBinary, isImage, statusText, isWorkingTree) {
    if (isImage) {
        var page = RootData.pageSelectionModel.registerPage(
            page.PageView.page, "Diff=" + filePath,
            imageComparePageComponent,
            { repository: RootData.project.repository,
              commitSha: historyView.selectedSha,
              parentIndex: 0,
              filePath: filePath }
        )
        RootData.pageSelectionModel.gotoPage(page)
    } else if (!isBinary) {
        var page = RootData.pageSelectionModel.registerPage(
            page.PageView.page, "Diff=" + filePath,
            diffPageComponent,
            { repository: RootData.project.repository,
              commitSha: historyView.selectedSha,
              parentIndex: 0,
              filePath: filePath,
              statusText: statusText,
              workingTree: isWorkingTree }
        )
        RootData.pageSelectionModel.gotoPage(page)
    }
}

Connections {
    target: commitDetailPanel
    function onFileClicked(filePath, isBinary, isImage, statusText) {
        navigateToFileDiff(filePath, isBinary, isImage, statusText, false)
    }
    function onCopyRequested(text) {
        RootData.copyText(text)
    }
}

Connections {
    target: workingTreePanel
    function onFileClicked(filePath, isBinary, isImage, statusText) {
        navigateToFileDiff(filePath, isBinary, isImage, statusText, true)
    }
}
```

### P. `Theme.qml` additions

Add diff color tokens to `Theme.qml`:
```qml
// Diff colors
readonly property color diffAddedBackground: dark ? "#1a3d2a" : "#e6ffec"
readonly property color diffDeletedBackground: dark ? "#3d1a1a" : "#ffebe9"
readonly property color diffHunkBackground: dark ? "#1a2b3d" : "#ddf4ff"
readonly property color diffAddedText: dark ? "#76e596" : "#1a7f37"
readonly property color diffDeletedText: dark ? "#ff8080" : "#cf222e"
readonly property color diffHunkText: dark ? "#8cb4d8" : "#656d76"
readonly property color diffContextBackground: "transparent"
```

---

## Edge Cases

| Case | Handling |
|------|----------|
| Root commit (no parent) | Diff against empty tree (`nullptr` as old tree in `git_diff_tree_to_tree`) |
| Merge commit (multiple parents) | Show `ComboBox` with each parent's truncated SHA + subject; default to parent[0]; cap display at 8 parents; show "and N more parents" text for octopus merges exceeding 8 |
| Binary file | Show "Binary file" badge; suppress diff text; disable click unless image |
| Image binary file | Detected via `QImageReader::supportedImageFormats()` (extension) + `QImageReader::imageFormat(QBuffer)` (header) for unrecognized extensions. Show "Binary file" badge + click navigates to `GitImageComparePage` |
| Large diff (>`maxDiffLines` lines) | Detect via `git_diff_stats` before generating text; set `tooLarge = true`; show message with line count |
| Renamed file | Show old path -> new path with rename icon; diff page shows content changes if any |
| Empty commit | Show metadata; file list is empty with "No files changed" message |
| File added/deleted | Entire file content shown as added (green) or deleted (red) lines |
| Invalid SHA / repo error | Show inline error banner in panel; log fatal errors to `cwErrorModel` |
| Repository not open | `fileContentAtCommit` wrapper returns empty QByteArray + warning |
| Empty repository (no commits) | No selection possible; detail panel shows "No commits" message |
| Uncommitted changes exist | Synthetic "Uncommitted Changes" row at index 0 with dashed visual; detail area shows `GitWorkingTreePanel` |
| Uncommitted changes committed | Synthetic row removed; `GitGraphModel` refreshes; new HEAD auto-selected at index 0 |
| Commit with empty subject | "Commit" button disabled; subject field shows validation hint |
| Race: files committed externally while working tree panel is shown | `GitWorkingTreeModel` refreshes via `modifiedFileCountChanged`; if empty, shows "No uncommitted changes" |
| Working tree diff for new untracked file | Entire file shown as added (green) lines — `git_diff_tree_to_workdir_with_index` handles this |
| Working tree diff for deleted file | Entire file shown as deleted (red) lines |
| LFS-tracked file in committed diff | `fileContentAtCommit` returns LFS pointer stub, not actual content. Detect via `LfsPointer::parse()` — if it's a pointer, resolve the real content from the LFS store on disk (`<repo>/.git/lfs/objects/`). If the LFS object hasn't been downloaded, show "LFS object not available" badge instead of diff/image content |
| LFS-tracked image in image compare | Same detection — check if blob is LFS pointer. If LFS object exists locally, load from LFS store path. If not, show placeholder with "LFS object not downloaded" message |
| LFS-tracked file in working tree diff | Working tree diffs use the actual file on disk (already hydrated by git-lfs smudge filter), so LFS is transparent — no special handling needed |

---

## LFS Handling

CaveWhere uses Git LFS for binary assets. When viewing committed diffs and images, the blob stored in the git object database may be an LFS pointer (a ~130-byte text stub) rather than the actual file content.

**Detection:** Use `LfsPointer::parse(blobData, &pointer)` (already exists in `QQuickGit/src/LfsStore.h`) to check if a blob is an LFS pointer.

**Resolution:** If the blob is an LFS pointer, attempt to load the real content from the local LFS store at `<repo-path>/.git/lfs/objects/<oid[0:2]>/<oid[2:4]>/<oid>`. If the file exists, return its content. If not (object not yet downloaded), return an error/placeholder.

**Where this applies:**
- `GitCommitImageProvider::requestImage()` — must resolve LFS pointers before loading as `QImage`
- `GitFilePatch` committed diffs — `git_diff_tree_to_tree` with LFS filter configured will show pointer text diffs, not content diffs. Since CaveWhere registers the LFS smudge filter via `ensureStandardLfsFilterConfig()`, diffs should go through the filter. Verify this works correctly in tests; if libgit2 doesn't apply filters during diff, the diff will show pointer stubs and should be treated as binary ("LFS file — content diff not available").

**Where this does NOT apply:**
- Working tree diffs — files on disk are already smudged (hydrated) by the LFS filter, so `git_diff_tree_to_workdir_with_index()` sees the real content. No special handling needed.
- Working tree image compare — the "after" image loads via `file://` from disk (already hydrated). Only the "before" (HEAD) image needs LFS resolution via the image provider.

---

## cwProjectSyncHealth Interaction

`cwProjectSyncHealth` does NOT need modification. It already independently observes `GitRepository::modifiedFileCountChanged` and `headBranchNameChanged` to compute sync status. When the working tree panel triggers a commit via `GitRepository::commitAll()`, the repository emits these signals, and `cwProjectSyncHealth` automatically picks them up and refreshes. No coupling between the commit detail feature and sync health is needed.

---

## Test Data

All new C++ tests need a test git repository with known commits covering the edge cases. Following the existing pattern in `QQuickGit/tests/test_GitRepository.cpp`, tests create a `QTemporaryDir` and build a repo programmatically using `GitRepository::initRepository()`, file writes, and commits. Each test section constructs the specific scenario it needs (merge commits, binary files, renames, large files, root commits, dirty working trees with uncommitted changes). No shared fixture directory is needed.

---

## Implementation Phases

Each phase is a single committable checkpoint — it compiles, tests pass, and no feature is left half-wired. Phases are ordered so each commit adds a testable slice of functionality.

**Parallelism:** Some phases can be developed in parallel since they share only Phase 1 as a dependency:
- **Track A (graph/selection):** 1 → 2 → 3 → 4a → 4b
- **Track B (commit detail):** 1 → 5 → 6 → 8a → 8b
- **Track C (working tree):** 1 → 9 → 10
- **Track D (diff viewer):** 1 → 12a → 12b → 12c → 13
- **Independent:** 7 (Theme), 14a, 14b can start at any time
- **Integration:** 11a (requires 4b + 8b + 10), 11b (requires 11a), 15 (requires 14a), 16 (requires 1 + 14a)

| Phase | Commit | Work | Location | Dependencies | Tests |
|-------|--------|------|----------|--------------|-------|
| **1** | `Add GitConcurrent thread pool utility` | Create `GitConcurrent` utility (static thread pool wrapper with `Q_ASSERT` guard, falls back to global pool) | QQuickGit/src | None | Unit test: verify `threadPool()` returns global pool by default; verify `setThreadPool()` overrides; verify `run()` executes on correct pool |
| **2** | `Refactor GitGraphModel to typed Restarter` | Change `Restarter<QVariant>` to `Restarter<IndexPassResult>` and use `GitConcurrent::run()` | QQuickGit/src | Phase 1 | Run existing `GitGraphModel` tests — no regression; add test that model loads commits correctly with typed restarter |
| **3** | `Add synthetic Uncommitted Changes row` | Add synthetic row to `GitGraphModel` when `modifiedFileCount > 0` (`hasUncommittedChanges` property, empty SHA, incremental insert/remove, index offset) | QQuickGit/src | Phase 2 | Unit test: dirty repo → synthetic row at index 0 with empty SHA; clean repo → no synthetic row; transition between states uses incremental insert/remove; real commit indices shift correctly |
| **4a** | `Add selection support to GitHistoryView` | Add `selectedSha` + `selectedIsUncommitted` properties, `required property string sha` on delegate, `TapHandler`, highlight delegate, default to index 0 | QQuickGit/qml | Phase 3 | QML test: index 0 selected on load; click row changes `selectedSha`; `selectedIsUncommitted` correct for synthetic vs real rows |
| **4b** | `Add dashed visual for synthetic row` | Dashed border, distinct background, pencil icon on `GitHistoryRow` when `commitSha === ""` | QQuickGit/qml | Phase 4a | Visual verification; QML test: synthetic row has dashed style |
| **5** | `Add GitCommitInfo with metadata and file list` | Create `GitCommitInfo` C++ class (single background task for both metadata + file list, `fileListReady` signal) | QQuickGit/src | Phase 1 | Unit tests: valid SHA → metadata + files; root commit → empty parentShas + all Added; merge → multiple parents + subjects; invalid SHA → error; empty SHA → cleared; cancel → no crash; >8 parents → capped |
| **6** | `Add GitCommitFileModel with lazy line stats` | Create `GitCommitFileModel` (receives file list from `GitCommitInfo`, roles, `fetchLineStats` with caching + cancellation) | QQuickGit/src | Phase 5 | Unit tests: populates from `fileListReady`; correct statuses; isBinary; fetchLineStats updates via dataChanged; cached re-fetch is immediate; SHA change cancels futures; empty commit → 0 rows |
| **7** | `Add diff color tokens to Theme` | Add `diffAddedBackground`, `diffDeletedBackground`, `diffHunkBackground`, `diffAddedText`, `diffDeletedText`, `diffHunkText`, `diffContextBackground` to `Theme.qml` | cavewherelib/qml | None | Visual verification |
| **8a** | `Add GitCommitDetailPanel metadata section` | Create `GitCommitDetailPanel.qml` with commit metadata display (author, date, SHA + copy button, subject/body, parent selector for merges, error banner) | QQuickGit/qml | Phases 5, 7 | QML test: panel shows metadata; copy button copies SHA; parent selector visible only for merges; ">8 parents" indicator; error banner on error |
| **8b** | `Add file list to GitCommitDetailPanel` | Add file list `ListView` to panel (backed by `GitCommitFileModel`, status icons, line counts, `fileClicked` signal) | QQuickGit/qml | Phases 6, 8a | QML test: file list populates; line stats shown; fileClicked emitted on click; binary badge for non-image binaries |
| **9** | `Add GitWorkingTreeModel with lazy line stats` | Create `GitWorkingTreeModel` (uncommitted file list from `git_status_list`, refreshes on `modifiedFileCountChanged`, `fetchLineStats`, LFS false-positive filtering) | QQuickGit/src | Phase 1 | Unit tests: dirty repo → correct statuses; clean → 0 rows; modify → refreshes; LFS filtered; fetchLineStats works; refresh cancels futures |
| **10** | `Add GitWorkingTreePanel with commit UI` | Create `GitWorkingTreePanel.qml` (file list with line stats, commit message input, Commit button, `fileClicked` / `commitRequested` signals, empty state) | QQuickGit/qml | Phases 4a, 7, 9 | QML test: file list shown; commit button disabled with empty subject; commitRequested emitted; fileClicked emitted; empty state when no changes |
| **11a** | `Wire SplitView with panel swap in GitHistoryPage` | Add `SplitView` to `GitHistoryPage.qml` with detail area that swaps between `GitCommitDetailPanel` and `GitWorkingTreePanel` based on `selectedIsUncommitted` | cavewherelib/qml | Phases 8b, 10 | QML test: selecting real commit shows commit detail; selecting synthetic row shows working tree panel |
| **11b** | `Wire commit flow and file navigation` | Add `safeCommitAll()` to `cwProject` (try/catch wrapper → `errorModel`); add `Connections` for `commitRequested` (calls `safeCommitAll`), `fileClicked` (page navigation), and `copyRequested` (calls `RootData.copyText`) in `GitHistoryPage` | cavewherelib | Phase 11a | QML test: commit flow end-to-end; HEAD selected after commit; file click navigates to diff page stub; commit error routes to errorModel; copy SHA works |
| **12a** | `Add GitFilePatch for committed diffs` | Create `GitFilePatch` C++ class (diff lines model, `maxDiffLines` threshold, origin mapping, committed mode via `git_diff_tree_to_tree`) | QQuickGit/src | Phase 1 | Unit tests: modified file → correct origins; added → all '+'; deleted → all '-'; binary → isBinary; large → tooLarge; custom threshold; line numbers; EOFNL mapped; invalid path → error; cancel → no crash |
| **12b** | `Add working tree mode to GitFilePatch` | Add `workingTree` property to `GitFilePatch` (uses `git_diff_tree_to_workdir_with_index`, resolves HEAD internally) | QQuickGit/src | Phase 12a | Unit tests: workingTree mode diffs HEAD vs working dir; modified file → correct lines; new file → all added; deleted → all deleted |
| **12c** | `Add LFS pointer detection to GitFilePatch` | Detect LFS pointers in committed diffs; show "LFS file" badge instead of pointer text diff | QQuickGit/src | Phase 12a | Unit test: LFS-tracked file in committed diff → isBinary true or LFS badge; working tree diff unaffected (files already hydrated) |
| **13** | `Add GitFileDiffPage with colored diff` | Create `GitFileDiffPage.qml` (monospace ListView, colored backgrounds by origin, gutter line numbers, tooLarge/isBinary/error states, `workingTree` support) + register in `GitHistoryPage` | cavewherelib/qml | Phase 12b | QML test: navigate from file click; diff lines render; back returns; tooLarge/isBinary/error states; working tree mode |
| **14a** | `Add GitCommitImageProvider` | Create image provider with repo ID registration, `requestImage` parsing, LFS pointer resolution from local store | QQuickGit/src | None | Unit tests: register/unregister repo; valid image → QImage; non-existent → null; LFS pointer → resolved from store if available; missing LFS object → null |
| **14b** | `Add fileContentAtCommit Q_INVOKABLE wrapper` | Add non-static wrapper on `GitRepository` with null-check | QQuickGit/src | None | Unit test: wrapper delegates to static method; returns empty + warning when repo not open |
| **15** | `Add GitImageComparePage with working tree support` | Create `GitImageComparePage.qml` (overlay compare, drag divider, `workingTree` mode with `file://` for after-image, placeholder for added/deleted) + register in `GitHistoryPage` | cavewherelib/qml | Phase 14a | QML test: navigate from image file click; before/after images load; divider drags; back returns; working tree mode uses file:// |
| **16** | `Wire GitConcurrent and image provider at startup` | Call `GitConcurrent::setThreadPool(cwTask::threadPool())` and `GitCommitImageProvider::registerOn(engine)` in `main.cpp` | main.cpp | Phases 1, 14a | Integration: app starts without crash; thread pool is shared; image provider registered |

---

## Build System

New `.h`, `.cpp`, and `.qml` files are automatically picked up by CMake via glob patterns in `QQuickGit/CMakeLists.txt` and `cavewherelib/CMakeLists.txt`. No manual CMakeLists.txt edits are needed for adding source files. However, verify after adding files that they appear in the build (re-run cmake configure if needed, as glob-based builds may require it).

---

## Files to Create

| File | Module |
|------|--------|
| `QQuickGit/src/GitConcurrent.h` | QQuickGit |
| `QQuickGit/src/GitConcurrent.cpp` | QQuickGit |
| `QQuickGit/src/GitCommitInfo.h` | QQuickGit |
| `QQuickGit/src/GitCommitInfo.cpp` | QQuickGit |
| `QQuickGit/src/GitCommitFileModel.h` | QQuickGit |
| `QQuickGit/src/GitCommitFileModel.cpp` | QQuickGit |
| `QQuickGit/src/GitWorkingTreeModel.h` | QQuickGit |
| `QQuickGit/src/GitWorkingTreeModel.cpp` | QQuickGit |
| `QQuickGit/src/GitFilePatch.h` | QQuickGit |
| `QQuickGit/src/GitFilePatch.cpp` | QQuickGit |
| `QQuickGit/src/GitCommitImageProvider.h` | QQuickGit |
| `QQuickGit/src/GitCommitImageProvider.cpp` | QQuickGit |
| `QQuickGit/qml/GitCommitDetailPanel.qml` | QQuickGit |
| `QQuickGit/qml/GitWorkingTreePanel.qml` | QQuickGit |
| `cavewherelib/qml/GitFileDiffPage.qml` | cavewherelib |
| `cavewherelib/qml/GitImageComparePage.qml` | cavewherelib |

## Files to Modify

| File | Changes |
|------|---------|
| `QQuickGit/src/GitGraphModel.h` | Change `Restarter<QVariant>` to `Restarter<IndexPassResult>`; add `hasUncommittedChanges` property; add synthetic row logic |
| `QQuickGit/src/GitGraphModel.cpp` | Update `refresh()` to use `GitConcurrent::run()` and typed restarter; add synthetic row insert/remove on `modifiedFileCountChanged`; offset real commit indices |
| `QQuickGit/qml/GitHistoryView.qml` | Add selection (currentIndex, selectedSha + selectedIsUncommitted via required property, highlight, default to index 0) |
| `QQuickGit/qml/GitHistoryRow.qml` | Add `required property string sha`, click handler, selection visual state, dashed style for synthetic row |
| `QQuickGit/src/GitRepository.h` | Add Q_INVOKABLE `fileContentAtCommit` wrapper (delegates to existing static `fileContentAtCommit` at line 185) |
| `QQuickGit/src/GitRepository.cpp` | Implement the wrapper with null-check |
| `cavewherelib/src/cwProject.h` | Add `Q_INVOKABLE safeCommitAll()` wrapper that catches `std::runtime_error` and routes to `errorModel()` |
| `cavewherelib/src/cwProject.cpp` | Implement `safeCommitAll()` |
| `cavewherelib/qml/GitHistoryPage.qml` | SplitView layout (always visible) + detail area swaps between commit/working tree panels + `Connections` for `fileClicked` + `commitRequested` + `copyRequested` signals + page registration for diff/image pages |
| `cavewherelib/qml/Theme.qml` | Add diff color tokens (added/deleted/hunk backgrounds and text) |
| `main.cpp` | Call `GitConcurrent::setThreadPool()` and `GitCommitImageProvider::registerOn()` |
