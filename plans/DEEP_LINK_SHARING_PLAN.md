# Plan: Deep Link Project Sharing (Issue #274)

## Context

CaveWhere users need a way to share a cave project with collaborators via a simple link (email, chat, etc.). The project is already Git-backed on GitHub; the missing piece is generating a shareable URL and handling it on the recipient's side to trigger an authenticated clone.

The approach mirrors Slack/Figma: generate `https://cavewhere.com/open?repo=<github_url>`, which works in any email client or browser. On macOS/iOS the app intercepts this URL via Universal Links before the browser opens. On Windows the app registers a `cavewhere://` custom URL scheme via the registry. On Linux a `.desktop` file registers the scheme with the XDG MIME system. All platforms share the same C++ URL parsing/validation logic; only the registration mechanism differs.

**Goal for this plan**: Full cross-platform implementation covering macOS, Windows, and Linux.

---

## Phase 1: Register the `cavewhere://` URL Scheme (All Platforms)

All platforms need the OS to know that `cavewhere://` links should open CaveWhere. The registration mechanism differs per platform but the result is the same.

### macOS
Qt uses `MACOSX_BUNDLE_INFO_PLIST` in CMake to supply a custom plist template.
- `CMakeLists.txt` already sets `MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in"` — edit `Info.plist.in` at the **repo root** directly (do not create `installer/macos/Info.plist.in`)
- Add `CFBundleURLTypes` array to `Info.plist.in`:
```xml
<key>CFBundleURLTypes</key>
<array>
  <dict>
    <key>CFBundleURLName</key>
    <string>com.cavewhere.open</string>
    <key>CFBundleURLSchemes</key>
    <array>
      <string>cavewhere</string>
    </array>
  </dict>
</array>
```
- No CMake change needed — `MACOSX_BUNDLE_INFO_PLIST` is already wired

### Windows
- Write registry keys at install time using the existing Inno Setup script (`installer/windows/cavewhere.iss.in`). A `[Registry]` section already exists (for `.cwproj`/`.cw` file type associations) — append to it:
```ini
Root: HKCR; Subkey: "cavewhere";                          ValueType: string; ValueData: "URL:CaveWhere Protocol"; Flags: uninsdeletekey
Root: HKCR; Subkey: "cavewhere";                          ValueName: "URL Protocol"; ValueType: string; ValueData: ""
Root: HKCR; Subkey: "cavewhere\shell\open\command";       ValueType: string; ValueData: """{app}\CaveWhere.exe"" ""%1"""
```
- On Windows, Qt delivers custom-scheme URLs via command-line argument (`argv[1]`), not `QEvent::FileOpen` — handle in `main.cpp`'s existing command-line parser
- **Multi-instance caveat**: if CaveWhere is already running when the user clicks a `cavewhere://` link, Windows launches a second process rather than routing to the existing one. No IPC is implemented. Accepted limitation at prototype stage.

### Linux
CaveWhere ships as an AppImage. AppImage `.desktop` file and URL scheme integration with XDG is poorly supported and unreliable across distros. **Linux `cavewhere://` URL scheme registration is deferred** — the landing page fallback (Phase 6) is the practical path for Linux users.

**Files to modify:**
- `Info.plist.in` (repo root) — add `CFBundleURLTypes` for `cavewhere://` scheme
- `installer/windows/cavewhere.iss.in` — add `[Registry]` section for URL scheme

> **Commit checkpoint 1 — "Register cavewhere:// URL scheme (macOS + Windows)"**
> Verify: app builds and launches normally. On macOS, `plutil -p CaveWhere.app/Contents/Info.plist` should show the new `CFBundleURLTypes` entry. No functional change yet.

---

## Phase 2: Handle Incoming `cavewhere://` URLs in C++

URL delivery differs by platform, and behaviour depends on whether the app is already running:

| Platform | App not running | App already running |
|----------|----------------|---------------------|
| macOS | OS launches app, delivers `QEvent::FileOpen` to new instance | OS delivers `QEvent::FileOpen` to the **existing** instance (no new process) |
| Windows | OS launches new instance with URL as `argv[1]` | OS launches another new instance with URL as `argv[1]` |
| Linux (AppImage) | URL scheme not registered — falls back to landing page | Same |

On macOS this means the URL always arrives in a live, fully-initialized app. On Windows a fresh process starts, so the startup race (see below) applies.

**Extend `cwOpenFileEventHandler`** (`cavewherelib/src/cwOpenFileEventHandler.h/.cpp`):
- The current `eventFilter()` casts to `QFileOpenEvent*` and calls `.file()` (a `QString`) for all events — it never checks the URL scheme
- Add `deepLinkReceived(QUrl)` to the (currently empty) `signals:` block in the header
- In `eventFilter()`, call `QFileOpenEvent::url()` (returns `QUrl`) and check its scheme before falling back to `.file()`:
  ```cpp
  QUrl url = fileOpenEvent->url();
  if (url.scheme() == QLatin1String("cavewhere")) {
      emit deepLinkReceived(url);
  } else {
      project()->loadOrConvert(fileOpenEvent->file());
  }
  ```
- `QFileOpenEvent` provides both `.url()` and `.file()`; the existing `.file()` path is preserved for normal file opens

**Extend `main.cpp` command-line handling**:
- In the existing `handleCommandline()` function, check if the positional filename argument starts with `cavewhere://`
- If so, call `rootData->deepLinkHandler()->handleUrl(url)` instead of `loadOrConvert()`
- This covers Windows (and Linux if URL scheme ever gets registered)

**Startup race on Windows**: the URL arrives via `argv` before QML is loaded and the `openRepoRequested` signal connection exists. `cwDeepLinkHandler::handleUrl()` always emits `openRepoRequested` immediately (a no-op if nothing is connected yet) and stores the validated repo URL as `m_pendingUrl`. `CavewhereMainWindow.qml`'s `Component.onCompleted` calls `takePendingUrl()` to drain any URL that arrived before the connection was established, then connects to `openRepoRequested` for future URLs.

**Create `cwDeepLinkHandler`** (`cavewherelib/src/cwDeepLinkHandler.h/.cpp`):
- `handleUrl(QUrl)` — called from both `cwOpenFileEventHandler` (macOS) and `main.cpp` (Windows); validates and always emits `openRepoRequested` + stores `m_pendingUrl`
- `takePendingUrl()` — returns and clears `m_pendingUrl`; called once from `Component.onCompleted` to handle the Windows startup case
- Parses `cavewhere://open?repo=<url>`
- Validates the `repo` param against an allowlist: `github.com`, `gitlab.com`, `bitbucket.org`
- Requires `repo` param to use `https://` scheme — rejects `git://`, `ssh://`, `http://`
- Emits `openRepoRequested(QUrl repoUrl)` after validation passes
- Emits `invalidLink(QString reason)` for bad URLs
- Exposed to QML via `cwRootData`

> **Commit checkpoint 2 — "Add cwDeepLinkHandler and wire URL delivery (macOS + Windows)"**
> Verify: all `[DeepLink]` and `[FileOpen]` C++ tests pass. App builds and launches. `open "cavewhere://open?repo=https://github.com/x/y"` on macOS should log/no-op (dialog not yet wired). No regression on normal file open.

---

## Phase 3: Confirmation Dialog + Clone Trigger

Never auto-clone. Show a confirmation dialog first.

**Create `DeepLinkConfirmDialog.qml`** (`cavewherelib/qml/DeepLinkConfirmDialog.qml`):
- Pattern: `QC.Dialog` (same as `AskToSaveDialog.qml`)
- Shows: repo host + owner/name parsed from URL
- Buttons: "Clone & Open" (accept) / "Cancel" (reject)
- On accept: calls `RootData.remote.cloner.clone(repoUrl.toString())` — reuses existing `cwRemoteRepositoryCloner` (`clone()` takes `QString`, not `QUrl`)

**Trigger from QML** in `CavewhereMainWindow.qml`'s `Component.onCompleted`:
```qml
Component.onCompleted: {
    // Drain any URL that arrived before QML was loaded (Windows cold start)
    var pending = RootData.deepLinkHandler.takePendingUrl()
    if (pending.toString() !== "")
        deepLinkConfirmDialog.open(pending)

    // Handle URLs that arrive while the app is running (macOS)
    RootData.deepLinkHandler.openRepoRequested.connect(function(url) {
        deepLinkConfirmDialog.open(url)
    })
}
```
- Clone result handled by existing `repositoryCloned` / `cloneFailedDueToAuthError` signals on `cwRemoteRepositoryCloner`

> **Commit checkpoint 3 — "Add DeepLinkConfirmDialog and wire clone trigger"**
> Verify: `tst_DeepLinkConfirmDialog` QML tests pass. End-to-end receive flow works on macOS: `open "cavewhere://open?repo=https://github.com/x/y"` shows the confirmation dialog. Cancel closes without cloning; "Clone & Open" triggers the clone flow.

---

## Phase 4: Share Link Generation

**Add to `cwProject`** (`cavewherelib/src/cwProject.h/.cpp`):
- `Q_INVOKABLE QUrl remoteUrl() const` — use `GitRepository::remoteUrl(name)` which already exists on `QQuickGit::GitRepository`:
  ```cpp
  QUrl cwProject::remoteUrl() const {
      if (!repository()) return {};
      QUrl url = repository()->remoteUrl(QStringLiteral("origin"));
      if (!url.isEmpty()) return url;
      return repository()->remoteUrl(); // default arg = first remote, or empty
  }
  ```
  `GitRepository::remoteUrl(QString name = QString())` is declared in `QQuickGit/src/GitRepository.h:126` — no need to iterate `remotes()` manually.
- `Q_INVOKABLE QUrl shareLink() const` — calls `remoteUrl()` and encodes it as `https://cavewhere.com/open?repo=<percent-encoded-url>`, returns empty `QUrl` if `remoteUrl()` is empty

No separate generator class — these are one-liners that belong on `cwProject` since they depend on project state.

---

## Phase 5: File → Share UI

**Add "Share..." to `FileMenu.qml`** (`cavewherelib/qml/FileMenu.qml`):
- New `QC.MenuItem` after Save As, enabled only when `RootData.project.fileType === CwProject.GitFileType && !RootData.project.syncHealth.noRemote`
- `RootData.project.syncHealth` is a `cwProjectSyncHealth*` with `noRemote` bool property — property path confirmed valid
- Triggers `shareDialog.open()`

**Create `ShareDialog.qml`** (`cavewherelib/qml/ShareDialog.qml`):
- `QC.Dialog` with title "Share Project"
- Displays the `https://cavewhere.com/open?repo=...` link in a read-only `TextField`
- "Copy Link" button calls `RootData.copyText(link)` (already implemented in `cwRootData`)
- Note: "The recipient will need a GitHub account and repository access"
- Optional: "Manage collaborators on GitHub" link via `Qt.openUrlExternally()`

> **Commit checkpoint 4 — "Add share link generation and Share... menu/dialog"**
> Verify: `[ShareLink]` C++ tests pass. `tst_ShareDialog` QML tests pass. Open a `.cwproj` with a GitHub remote → File → Share shows the correct `https://cavewhere.com/open?repo=...` URL. Copy Link works. Share... is greyed out for non-git projects and projects with no remote.

---

## Phase 6: macOS Universal Links (Production Polish)

This phase makes `https://cavewhere.com/open?...` open the app directly rather than the browser, skipping the `cavewhere://` redirect entirely on macOS/iOS.

1. Add `com.apple.developer.associated-domains` entitlement: `applinks:cavewhere.com`
2. Host `https://cavewhere.com/.well-known/apple-app-site-association`:
```json
{
  "applinks": {
    "details": [{
      "appIDs": ["<TEAM_ID>.com.cavewhere.CaveWhere"],
      "components": [{ "/": "/open*" }]
    }]
  }
}
```
3. Register `QDesktopServices::setUrlHandler("https", ...)` in the app so macOS routes matching HTTPS `cavewhere.com` URLs directly to CaveWhere. The handler **must** verify the host is `cavewhere.com` before acting — pass all other HTTPS URLs back through `QDesktopServices::openUrl()` to avoid intercepting unrelated links
4. The landing page at `cavewhere.com/open` serves as fallback for Windows/Linux — redirects to `cavewhere://open?repo=...` and shows a "Download CaveWhere" button if that fails

**Note**: Phase 6 requires control of the `cavewhere.com` server and a paid Apple Developer account entitlement. Defer until after prototype is validated.

> **Commit checkpoint 5 — "Add macOS Universal Links support"** *(deferred — implement after prototype validated)*
> Verify: on macOS, clicking `https://cavewhere.com/open?repo=...` in Safari opens CaveWhere directly without hitting the browser. Unrelated HTTPS links are not intercepted. Windows/Linux users still reach the landing page fallback.

---

## Security

- **Allowlist** in `cwDeepLinkHandler`: only `github.com`, `gitlab.com`, `bitbucket.org` accepted as repo hosts
- **Confirmation dialog** (Phase 3) is mandatory — no silent cloning
- **Clone destination** hardcoded to `~/CaveWhere/Projects/` via `cwRemoteRepositoryCloner` — the URL cannot control where files land
- **HTTPS only** in the `repo=` param — reject `git://`, `ssh://`, `http://`

---

## Key Files

| File | Change |
|------|--------|
| `Info.plist.in` (repo root) | Add `CFBundleURLTypes` for `cavewhere://` scheme |
| `installer/windows/cavewhere.iss.in` | Add `[Registry]` section for `cavewhere://` URL scheme |
| `main.cpp` | Handle `cavewhere://` as command-line arg (Windows) |
| `cavewherelib/src/cwOpenFileEventHandler.h/.cpp` | Detect `cavewhere://` URLs, emit signal (macOS) |
| `cavewherelib/src/cwDeepLinkHandler.h/.cpp` | New — parse, validate, queue, emit openRepoRequested |
| `cavewherelib/src/cwRootData.h/.cpp` | Expose cwDeepLinkHandler |
| `cavewherelib/src/cwProject.h/.cpp` | Add remoteUrl() and shareLink() |
| `cavewherelib/qml/FileMenu.qml` | Add Share... menu item |
| `cavewherelib/qml/ShareDialog.qml` | New — share link display + copy |
| `cavewherelib/qml/DeepLinkConfirmDialog.qml` | New — clone confirmation |
| `cavewherelib/qml/CavewhereMainWindow.qml` | `Component.onCompleted`: call `takePendingUrl()` then connect `openRepoRequested` to dialog |
| `cavewherelib/CMakeLists.txt` | **No change needed** — `src/*.cpp`, `src/*.h`, and `qml/*.qml` are all picked up by `file(GLOB ... CONFIGURE_DEPENDS ...)` automatically |
| `testcases/` (root `CMakeLists.txt`) | **No change needed** — `testcases/*.cpp` is also globbed; just drop `tst_cwDeepLinkHandler.cpp` in the directory |

## Reused Infrastructure

- `cwOpenFileEventHandler` — extend rather than replace
- `cwRemoteRepositoryCloner::clone(const QString& urlText)` — handles auth, progress, errors; pass `repoUrl.toString()` from QML
- `cwRootData::copyText()` — already clipboard-ready
- `AskToSaveDialog.qml` — dialog pattern to follow
- `RootData.remote.cloner` — already QML-accessible
- `cwProject::shareLink()` / `cwProject::remoteUrl()` — no separate generator class needed

---

## Test Cases

### C++ Unit Tests (`testcases/`) — Catch2

**`tst_cwDeepLinkHandler`** — `testcases/tst_cwDeepLinkHandler.cpp`
- `[DeepLink] Valid github.com link emits openRepoRequested`
- `[DeepLink] Valid gitlab.com link emits openRepoRequested`
- `[DeepLink] Valid bitbucket.org link emits openRepoRequested`
- `[DeepLink] Disallowed host emits invalidLink`
- `[DeepLink] http:// repo URL rejected (HTTPS only)`
- `[DeepLink] Missing repo param emits invalidLink`
- `[DeepLink] Malformed URL emits invalidLink`
- `[DeepLink] IP address as host rejected`
- `[DeepLink] Path traversal in repo URL rejected`
- `[DeepLink] takePendingUrl() returns the validated repo URL`
- `[DeepLink] takePendingUrl() clears the pending URL`
- `[DeepLink] takePendingUrl() returns empty when no URL was handled`
- `[DeepLink] Second handleUrl() replaces first pending URL`
- `[DeepLink] Invalid URL does not set a pending URL`

**`tst_cwProject` (share link methods)** — extend existing project tests
- `[ShareLink] shareLink() generates correct https://cavewhere.com/open?repo=... URL`
- `[ShareLink] Repo URL is percent-encoded in output`
- `[ShareLink] shareLink() prefers origin remote when multiple remotes present`
- `[ShareLink] shareLink() falls back to first remote when origin absent`
- `[ShareLink] shareLink() returns empty QUrl when no remotes`
- `[ShareLink] Round-trip: shareLink() output parses back to original repo URL via cwDeepLinkHandler`

**`tst_cwOpenFileEventHandler`** — extend existing tests
- `[FileOpen] cavewhere:// URL emits deepLinkReceived, not loadOrConvert`
- `[FileOpen] Regular file path still calls loadOrConvert`

### QML Tests (`test-qml/`) — QtTest

**`tst_ShareDialog.qml`**
- Share... menu item is enabled when project is GitFileType with a remote
- Share... menu item is disabled for non-git project
- Share... menu item is disabled when project has no remote (`syncHealth.noRemote`)
- Dialog displays correct `https://cavewhere.com/open?repo=...` link
- Copy Link button puts the link on the clipboard

**`tst_DeepLinkConfirmDialog.qml`**
- Dialog shows repo owner/name parsed from the URL
- Cancel button closes dialog without cloning
- "Clone & Open" button triggers `RootData.remote.cloner.clone()`

---

## Known Limitations

- **Windows multi-instance**: clicking a `cavewhere://` link when the app is already open launches a second process. The second process shows the confirmation dialog normally, but the user ends up with two CaveWhere windows. macOS handles this correctly via the OS (the existing instance receives the URL). IPC-based single-instance routing is deferred.
- **Linux URL scheme**: AppImage `cavewhere://` registration is deferred; the Phase 6 landing page is the fallback.

---

## Verification

1. Build: `cmake --build build/<preset> --target all`
2. **Share flow**: Open a `.cwproj` project with a GitHub remote → File → Share → dialog shows `https://cavewhere.com/open?repo=...` → Copy Link puts it on clipboard
3. **Receive flow (macOS)**: `open "cavewhere://open?repo=https://github.com/someuser/somerepo"` → confirmation dialog appears → clone proceeds
4. **Receive flow (Windows/Linux)**: `CaveWhere "cavewhere://open?repo=https://github.com/someuser/somerepo"` → same confirmation dialog
5. **Security**: `open "cavewhere://open?repo=https://evil.com/bad"` → rejected with error, no clone
6. **Disabled state**: Open a non-git `.cw` project → File → Share... is greyed out
7. Run test suites: `./build/<preset>/cavewhere-test "[DeepLink]" "[ShareLink]" "[FileOpen]"` and `./build/<preset>/cavewhere-qml-test --platform offscreen`
