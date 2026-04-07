# Setup Remote Plan (Issue #341)

## 1. Scope

Allow users with a new or existing local project (no git remote configured) to set up a remote
directly from the UI. Three paths are supported:

- **Path A**: Create a new GitHub repository and wire it as the project remote
- **Path B**: Connect to an existing GitHub repository (pick from list or paste URL)
- **Path C**: Connect to any git remote via a custom URL (GitLab, Bitbucket, self-hosted, etc.)

Entry point: `SyncButton` when no remote is configured. The initial push after adding the remote
is handled by the existing sync path — no special-case push logic needed.

## 2. Principles

1. Use QQuickGit API exclusively — no raw git commands or direct libgit2 calls in CaveWhere code.
2. GitHub repo creation lives in `cwGitHubIntegration` — it already owns the network manager,
   auth header, and the `refreshRepositories()` pattern.
3. "No remote" is a distinct state from "needs login" or "auth expired" — surface it explicitly
   in `cwSyncStatus`.
4. Private visibility is the default for new repositories — cave map data is sensitive.
5. SSH key management is out of scope for this plan. Path C accepts SSH URLs as a manual option
   but does not automate key generation or upload.
6. Path A is the primary happy path. Paths B and C are secondary and should not compete visually
   with Path A on the initial screen.

## 3. State Detection

### 3.1 Rename `AuthState` → `SyncBlocker`; add `NoRemote`

`cwSyncStatus` currently has an `AuthState` enum that covers auth-related blockers. Rename it to
`SyncBlocker` and add `NoRemote` as the highest-priority case, making all "sync is blocked
because…" states live in one enum:

```cpp
enum class SyncBlocker {
    None,       // sync is unblocked
    NoRemote,   // no remote configured
    NeedsLogin, // HttpAuthFailed; credentials have never been loaded
    Expired     // HttpAuthFailed; credentials were loaded but rejected
};
Q_ENUM(SyncBlocker)
```

Add `QML_VALUE_TYPE(cwSyncStatus)` to the struct (alongside the existing `Q_GADGET`). This
registers the type with the QML engine so enum values are accessible by type name in QML
(`cwSyncStatus.NoRemote`, etc.). Without this registration, `cwSyncStatus.NoRemote` evaluates
to `undefined` in QML even though `Q_ENUM` is declared.

Replace the `m_authState` member (type `AuthState`) with `m_syncBlocker` (type `SyncBlocker`).
Rather than adding a `noRemote()` bool alongside the existing `needsLogin()` and `authExpired()`
accessors, expose the enum directly as a `Q_PROPERTY` so QML can switch on it without a
proliferating set of booleans:

```cpp
Q_PROPERTY(SyncBlocker syncBlocker READ syncBlocker CONSTANT)
SyncBlocker syncBlocker() const { return m_syncBlocker; }
```

Add `noRemote()` alongside the existing bool accessors for consistency and simpler QML bindings:

```cpp
bool noRemote()    const { return m_syncBlocker == SyncBlocker::NoRemote; }
bool needsLogin()  const { return m_syncBlocker == SyncBlocker::NeedsLogin; }
bool authExpired() const { return m_syncBlocker == SyncBlocker::Expired; }
```

Add a corresponding `Q_PROPERTY`:
```cpp
Q_PROPERTY(bool noRemote READ noRemote)
```

QML uses `syncHealth.status.noRemote` — same pattern as the existing `needsLogin` and
`authExpired` properties. The `syncBlocker` enum property is still available for exhaustive
switches if needed, but day-to-day QML uses the bool accessors.

Update `operator==` to compare `m_syncBlocker` (replacing the old `m_authState` comparison):

```cpp
inline bool cwSyncStatus::operator==(const cwSyncStatus& other) const
{
    return m_hasLocalChanges == other.m_hasLocalChanges
           && m_aheadCount == other.m_aheadCount
           && m_behindCount == other.m_behindCount
           && m_stale == other.m_stale
           && m_syncBlocker == other.m_syncBlocker
           && m_message == other.m_message;
}
```

In `cwProjectSyncHealth::refresh()`, the no-remote check already runs first (before auth checks)
at the `configuredRemotes.isEmpty()` guard. Set `m_syncBlocker = SyncBlocker::NoRemote` there.
Update the auth-failure branch to use `SyncBlocker::NeedsLogin` / `SyncBlocker::Expired`
instead of `AuthState::NeedsLogin` / `AuthState::Expired`. The priority order
`NoRemote` > `NeedsLogin` > `Expired` > `None` is already enforced by the existing code
structure — no reordering needed.

### 3.2 `hasRemoteConfigured()` in `cwSaveLoad`

This helper already exists and checks `repository->remotes().isEmpty()`. No change needed — it
will continue to guard sync operations correctly.

## 4. C++ Changes

### 4.1 `cwGitHubIntegration` — add `createRepository()`

Add to the public API:

```cpp
Q_INVOKABLE void createRepository(const QString& name,
                                   bool isPrivate,
                                   const QString& org = QString());
```

New signals:
```cpp
void repositoryCreated(const cwGitHubRepositoryItem& repo);
void repositoryCreationFailed(const QString& errorMessage);
```

Implementation:
- `POST https://api.github.com/user/repos` (no org) or
  `POST https://api.github.com/orgs/{org}/repos` (with org)
- JSON body: `{ "name": name, "private": isPrivate, "auto_init": false }`
- Reuse `authorizationHeader()` and `m_network` (same pattern as `refreshRepositories()`)
- Add private `handleCreateRepositoryReply(QNetworkReply*)` handler
- Parse response for `clone_url`, populate `cwGitHubRepositoryItem`, emit `repositoryCreated`
- On HTTP 401/403: call `invalidateActiveAccountToken()` then emit `repositoryCreationFailed`
  with the `message` field from the JSON response body
- On HTTP 422 (Unprocessable Entity — name already taken or invalid): do **not** invalidate the
  token. Parse the GitHub error body (`errors[0].message` or top-level `message`) and emit
  `repositoryCreationFailed` with a human-readable string, e.g.
  `tr("Repository name '%1' is already taken or invalid: %2").arg(name, githubMessage)`.
- On any other non-2xx code: emit `repositoryCreationFailed(tr("GitHub returned an error (%1): %2")
  .arg(httpStatus).arg(githubMessage))`. Do not invalidate the token for 5xx errors.
- In all failure cases the wizard stays on the current screen — never redirect to Path B/C.

No new class needed.

### 4.2 `cwRemoteAccountCoordinator` — add `addRemoteToProject()`

`cwRemoteAccountCoordinator.h` currently has no reference to `QQuickGit::GitRepository`. Add a
forward declaration before the class:

```cpp
namespace QQuickGit { class GitRepository; }
```

Then declare the new method:

```cpp
Q_INVOKABLE void addRemoteToProject(QQuickGit::GitRepository* repository,
                                    const QUrl& remoteUrl,
                                    bool bindToGitHubAccount = true);
```

New signal:
```cpp
void addRemoteFailed(const QString& errorMessage);
```

Implementation:
1. Call `repository->addRemote("origin", remoteUrl)` — `Q_INVOKABLE` on `GitRepository`,
   returns an empty QString on success or an error message on failure.
2. If the returned string is non-empty, emit `addRemoteFailed(errorString)` and return.
3. If `bindToGitHubAccount` is true, call `bindRemoteToActiveGitHubAccount(remoteUrl.toString())`.

Path A and Path B callers pass `bindToGitHubAccount = true` (default).
Path C callers pass `bindToGitHubAccount = false` — non-GitHub remotes must not be bound.

### 4.3 `cwSyncStatus` / `cwProjectSyncHealth`

- Rename `AuthState` → `SyncBlocker`, add `NoRemote` case (see §3.1)
- Replace `m_authState` member with `m_syncBlocker`; update `operator==`
- In `cwProjectSyncHealth::refresh()`, set `m_syncBlocker = SyncBlocker::NoRemote` in the
  existing `configuredRemotes.isEmpty()` branch; update auth-failure branch to use new enum names

## 5. QML Changes

### 5.1 `SyncButton.qml` — `noRemote` state

Add property and signal:
```qml
readonly property bool noRemote: syncHealth.status.noRemote
signal setupRemoteRequested()
```

Update icon logic (add `noRemote` as highest-priority branch):
```qml
icon.source: noRemote
    ? "qrc:/twbs-icons/icons/cloud-upload.svg"
    : (needsLogin
       ? "qrc:/twbs-icons/icons/lock.svg"
       : (authExpired
          ? "qrc:/twbs-icons/icons/exclamation-triangle.svg"
          : "qrc:/twbs-icons/icons/arrow-repeat.svg"))
```

Update `onClicked`:
```qml
onClicked: {
    if (noRemote) {
        setupRemoteRequested()
    } else if (authExpired) {
        reconnectRequested()
    } else {
        syncRequested()
    }
}
```

Add a `tooltipText` property and bind `QC.ToolTip.text` to it (attached-property values are
not readable from outside the item, so tests use this property directly):
```qml
readonly property string tooltipText: noRemote
    ? qsTr("No remote configured — click to set up sync")
    : (needsLogin
       ? qsTr("Sign in to GitHub — click to sync")
       : (authExpired
          ? qsTr("GitHub access expired — click to reconnect")
          : syncHealth.status.message))

QC.ToolTip.text: tooltipText
```

Hide the status badge when `noRemote` is true — it would otherwise show "!" from the stale state
and conflict with the cloud-upload icon:
```qml
visible: !noRemote
         && (syncHealth.status.stale
             || syncHealth.status.hasLocalChanges
             || syncHealth.status.aheadCount > 0
             || syncHealth.status.behindCount > 0
             || syncInProgress)
```

Update the context menu so "Sync now" is disabled when `noRemote` is active and add a
"Set up remote…" item as the top entry:
```qml
QC.Menu {
    id: syncMenu

    QC.MenuItem {
        text: "Set up remote…"
        visible: noRemote
        onTriggered: setupRemoteRequested()
    }

    QC.MenuItem {
        objectName: "syncNowMenuItem"
        text: "Sync now"
        enabled: !syncInProgress && !noRemote
        onTriggered: syncRequested()
    }

    QC.MenuItem {
        text: "Remote settings..."
        onTriggered: remoteSettingsRequested()
    }
}
```

### 5.2 `GitHubAuthFlow.qml` — shared component (extracted)

The device-flow auth UI currently exists only inside `RemoteRepositoryPage.qml`. Extract it into
a reusable component so both `RemoteRepositoryPage` and `SetupRemoteWizard` can use it without
duplication.

Required properties:
```qml
required property GitHubIntegration gitHubIntegration
required property string contextMessage   // e.g. "Sign in to GitHub to create a repository"
```

Displays:
- `contextMessage` as a brief header label above the auth controls, so users understand why
  they are being asked to sign in (the wizard and RemoteRepositoryPage have different contexts)
- Status text that switches on `authState` (RequestingCode / AwaitingVerification / Error)
- User code field + "Copy and Open GitHub" button (AwaitingVerification only)
- Countdown text when verification is opened and poll timer is running
- Connect / Reconnect button + Cancel button

`RemoteRepositoryPage` replaces its inline auth group box with `GitHubAuthFlow`, passing
`contextMessage: qsTr("Sign in to GitHub to access your repositories")`.

### 5.3 `SetupRemoteWizard.qml` — new component

Modal `Popup` (or `Dialog`) with a two-screen structure:

**Screen 1 — Create on GitHub (default)**

Shown immediately. Contains `CreateGitHubRepoForm` (§5.4). At the bottom, a single secondary
text link:

```
Already have a remote? →
```

Clicking it navigates to Screen 2. If the user is not GitHub-authorized, Screen 1 shows
`GitHubAuthFlow` first with `contextMessage: qsTr("Sign in to GitHub to create a repository
for this project")`; the form appears once `authState === Authorized`.

**Screen 2 — Connect existing remote**

Contains a toggle between:
- **"GitHub repository"** (Path B) — repo list picker; single click selects and enables the
  Connect button. No separate URL field needed here: the selected repo's `cloneUrl` is used
  directly.
- **"Custom URL"** (Path C) — single URL text field; note: "For SSH URLs, ensure your SSH key
  is already configured."

A back chevron or "← Create instead" link returns to Screen 1.

When the wizard opens (`onOpened`), if `gitHubIntegration.authState === Authorized`, call
`gitHubIntegration.refreshRepositories()` so the list is fresh for both name validation on
Screen 1 and the repo picker on Screen 2 Path B.

Required properties:
```qml
required property GitHubIntegration gitHubIntegration    // RootData.remote.gitHubIntegration
required property RemoteAccountCoordinator accountCoordinator  // RootData.remote.accountCoordinator
required property GitRepository repository               // The project's GitRepository
signal remoteSetupComplete(bool syncNow)
signal remoteSetupCancelled()
```

Internal state machine:
- `"createRepo"` — Screen 1: Path A form (or auth flow if not yet authorized)
- `"connectRepo"` — Screen 2: Path B / C toggle
- `"working"` — async operation in progress, inputs disabled, Cancel button remains active
- `"done"` — success: show "Remote configured. Push your project to GitHub to back it up." with
  two buttons: **"Sync now"** (primary) and **"Close"** (secondary). "Sync now" emits
  `remoteSetupComplete(true)`; "Close" emits `remoteSetupComplete(false)`.

**State ownership**: `SetupRemoteWizard` owns all state transitions and all C++ calls
(`createRepository`, `addRemoteToProject`). The child form components (`CreateGitHubRepoForm`,
`ConnectRemoteForm`) are pure input components — they emit signals when the user confirms and
expose an `errorMessage` property that the wizard populates on failure. The wizard listens to
those signals and to the C++ backend signals, then transitions state and sets error text
accordingly. Form components never write wizard state directly.

Error handling: inline error text below the active form; stays on the current screen so the
user can correct and retry. Never navigate away from the current screen on error.

### 5.4 `CreateGitHubRepoForm.qml` — new component

Used in `SetupRemoteWizard` Screen 1 (Path A).

Properties / fields:
- `repoName` — text field, default populated from project directory name
- `isPrivate` — toggle, **default: true**, label: "Private — recommended for cave data"
- `orgName` — optional; omitted in v1 (user repos only)

**Name validation (client-side, before any API call):**
The Create button is disabled when `repoName` is empty. Before submitting, check whether
`repoName` (case-insensitive) matches the `name` of any entry in
`gitHubIntegration.repositories`. If a match is found, show an inline error immediately
without making an API call:

> "A repository named '{name}' already exists in your account. Choose a different name or
> use 'Connect existing' to wire it as your remote."

The form exposes:

```qml
signal createRequested(string repoName, bool isPrivate)
property string errorMessage   // set by wizard on any failure; cleared when user edits repoName
```

On confirm (name passes client-side check): emits `createRequested(repoName, isPrivate)`.

The wizard wires `onCreateRequested`:
1. Transitions to `"working"`.
2. Calls `gitHubIntegration.createRepository(repoName, isPrivate)`.
3. On `gitHubIntegration.repositoryCreated`: calls
   `accountCoordinator.addRemoteToProject(repository, repo.cloneUrl)` (default
   `bindToGitHubAccount = true`), then transitions to `"done"`.
4. On `gitHubIntegration.repositoryCreationFailed(errorMessage)`: transitions back to
   `"createRepo"`, sets `createForm.errorMessage = errorMessage`. Stays on Screen 1 —
   no redirect to Path B/C regardless of error type.
5. On `accountCoordinator.addRemoteFailed(errorMessage)`: transitions back to `"createRepo"`,
   sets `createForm.errorMessage` to the error text (which should include the clone URL so the
   user can recover via Path B if needed, but no automatic redirect).

### 5.5 `ConnectRemoteForm.qml` — new component

Used in `SetupRemoteWizard` Screen 2 (Paths B and C). Contains an internal toggle.

The form exposes:

```qml
signal connectRequested(url remoteUrl, bool bindToGitHubAccount)
property string errorMessage   // set by wizard on failure; cleared when selection/URL changes
```

The wizard wires `onConnectRequested`:
1. Transitions to `"working"`.
2. Calls `accountCoordinator.addRemoteToProject(repository, remoteUrl, bindToGitHubAccount)`.
3. On success: transitions to `"done"`.
4. On `accountCoordinator.addRemoteFailed(errorMessage)`: transitions back to `"connectRepo"`,
   sets `connectForm.errorMessage = errorMessage`.

**Path B mode (GitHub repository):**
- Shows `gitHubIntegration.repositories` list (same model and delegate as `RemoteRepositoryPage`)
- Single click on a list item selects it and enables the Connect button
- On Connect: emits `connectRequested(selectedRepo.cloneUrl, true)`
- If the user is not yet GitHub-authorized, shows `GitHubAuthFlow` in place of the list

**Path C mode (Custom URL):**
- Use `TextFieldWithError` (same component as the clone URL field in `RemoteRepositoryPage`);
  placeholder: `https://github.com/user/repo.git`
- Note below field: "For SSH URLs, ensure your SSH key is already configured."
- The Connect button is disabled when the field is empty — same pattern as the existing
  Clone button (`enabled: urlField.textField.text.length > 0`). No client-side format
  validation; errors surface via `addRemoteFailed` exactly as clone errors surface via
  `cloneErrorMessage`.
- On Connect: emits `connectRequested(url, false)`

### 5.6 Wire `setupRemoteRequested` in the host

In `LinkBar.qml`, inside the `SyncButton` block:

```qml
onSetupRemoteRequested: {
    setupRemoteWizardId.open()
}

SetupRemoteWizard {
    id: setupRemoteWizardId
    parent: QC.Overlay.overlay
    gitHubIntegration: RootData.remote.gitHubIntegration
    accountCoordinator: RootData.remote.accountCoordinator
    repository: RootData.project.repository
    onRemoteSetupComplete: function(syncNow) {
        setupRemoteWizardId.close()
        if (syncNow) { RootData.project.sync() }
    }
    onRemoteSetupCancelled: setupRemoteWizardId.close()
}
```

## 6. Implementation Order

| Step | Work | Files touched |
|------|------|---------------|
| 1 | Rename `AuthState` → `SyncBlocker`, add `NoRemote`, update `operator==` + `cwProjectSyncHealth` | `cwProjectSyncHealth.h/.cpp` |
| 2 | Add `addRemoteFailed` signal + `addRemoteToProject()` to `cwRemoteAccountCoordinator` | `cwRemoteAccountCoordinator.h/.cpp` |
| 3 | Add `createRepository()` + signals to `cwGitHubIntegration`; add `setApiBaseUrl()` (§11.1) | `cwGitHubIntegration.h/.cpp` |
| **✓ Commit 1** | **C++ backend + tests** — see §6.1 | |
| 4 | Update `SyncButton.qml`: `noRemote` property, `tooltipText`, badge guard, context menu | `SyncButton.qml` |
| **✓ Commit 2** | **SyncButton `noRemote` state + tests** — see §6.2 | |
| 5 | Extract `GitHubAuthFlow.qml`; update `RemoteRepositoryPage` to use it | `GitHubAuthFlow.qml` (new), `RemoteRepositoryPage.qml` |
| 6 | Build `CreateGitHubRepoForm.qml` | new file |
| 7 | Build `ConnectRemoteForm.qml` (Path B + C toggle) | new file |
| 8 | Build `SetupRemoteWizard.qml` (two-screen structure) | new file |
| 9 | Wire `setupRemoteRequested` in `LinkBar.qml` | `LinkBar.qml` |
| 10 | Register new QML files in `CMakeLists.txt` | `cavewherelib/CMakeLists.txt` |
| **✓ Commit 3** | **SetupRemoteWizard UI + tests** — see §6.3 | |

## 6.1 Commit 1 — C++ backend + tests

**Steps covered:** 1, 2, 3

**Files:**
- `cavewherelib/src/cwProjectSyncHealth.h/.cpp` — `SyncBlocker` enum, `noRemote()`, `operator==`
- `cavewherelib/src/cwRemoteAccountCoordinator.h/.cpp` — `addRemoteToProject()`, `addRemoteFailed`
- `cavewherelib/src/cwGitHubIntegration.h/.cpp` — `createRepository()`, `setApiBaseUrl()`
- `testcases/test_cwProjectSyncHealth.cpp` — extended with `SyncBlocker` assertions + 3 new cases (§11.2)
- `testcases/test_cwGitHubIntegration.cpp` — new file (§11.3)
- `testcases/test_cwRemoteAccountCoordinator.cpp` — new file (§11.4)
- `cavewherelib/CMakeLists.txt` — add new test `.cpp` files; link `Qt::HttpServer` to `cavewhere-test`

**Gate:** both test suites pass:
```bash
./build/<preset>/cavewhere-test "[cwProjectSyncHealth]" "[cwSyncStatus]" "[cwGitHubIntegration]" "[cwRemoteAccountCoordinator]"
```

No QML changes — `SyncButton` and `RemoteRepositoryPage` still compile because the QML-facing
properties remain backward-compatible (old `needsLogin` / `authExpired` bool accessors are
kept; `noRemote` is additive).

## 6.2 Commit 2 — SyncButton `noRemote` state + tests

**Steps covered:** 4

**Files:**
- `cavewherelib/qml/SyncButton.qml` — `noRemote` property, `setupRemoteRequested` signal,
  `tooltipText` property, badge guard, context menu `objectName` + `noRemote` guard
- `test-qml/tst_RemoteRepositoryPage.qml` — 5 new `noRemote` tests (§11.6)

**Gate:**
```bash
./build/<preset>/cavewhere-qml-test --platform offscreen -testall tst_RemoteRepositoryPage
```

The wizard does not exist yet; `setupRemoteRequested` fires but nothing opens — acceptable
because no host wires it until Commit 3.

## 6.3 Commit 3 — SetupRemoteWizard UI + tests

**Steps covered:** 5, 6, 7, 8, 9, 10

**Files:**
- `cavewherelib/qml/GitHubAuthFlow.qml` — new
- `cavewherelib/qml/RemoteRepositoryPage.qml` — use `GitHubAuthFlow`
- `cavewherelib/qml/CreateGitHubRepoForm.qml` — new
- `cavewherelib/qml/ConnectRemoteForm.qml` — new
- `cavewherelib/qml/SetupRemoteWizard.qml` — new
- `cavewherelib/qml/LinkBar.qml` — wire `onSetupRemoteRequested` + `SetupRemoteWizard`
- `cavewherelib/CMakeLists.txt` — register new QML files; add `tst_SetupRemoteWizard.qml` to `cavewhere-qml-test`
- `test-qml/tst_SetupRemoteWizard.qml` — new file (§11.5)

**Gate:** both suites pass:
```bash
./build/<preset>/cavewhere-qml-test --platform offscreen -testall tst_SetupRemoteWizard
./build/<preset>/cavewhere-qml-test --platform offscreen -testall tst_RemoteRepositoryPage
```

## 11. Tests

### 11.1 Prerequisite: injectable API base URL in `cwGitHubIntegration`

`GitHubApiBase` is a `static const QString` in an anonymous namespace in `cwGitHubIntegration.cpp`.
The HTTP-mock tests below require pointing requests at a local HTTP server. Promote
`GitHubApiBase` to a private member and add a setter:

```cpp
// cwGitHubIntegration.h — add under the existing public API:
void setApiBaseUrl(const QString& url);   // test-only; no signal needed
```

```cpp
// cwGitHubIntegration.cpp — replace the static variable with:
QString m_apiBaseUrl = QStringLiteral("https://api.github.com");
// Use m_apiBaseUrl wherever GitHubApiBase was used.
```

**Note:** `Qt6::HttpServer` is not in the Qt installation bundled with this project. The tests
use a `QTcpServer`-based `HttpMockServer` helper (defined in `test_cwGitHubIntegration.cpp`)
that serves a single HTTP response and then disconnects. Add `Qt6::Network` to
`cavewhere-test`'s link libraries in `CMakeLists.txt`.

### 11.2 `test_cwProjectSyncHealth.cpp` — extend existing file

The two existing tests that check `message() == "No git remote is configured for this project."` and
`stale()` are correct but incomplete. Add assertions for `syncBlocker()` and `noRemote()` to each of
those tests **and** add the three new cases below.

**New: verify `SyncBlocker::NoRemote` is set and `noRemote()` returns true:**

```cpp
TEST_CASE("cwProjectSyncHealth sets SyncBlocker::NoRemote when no remote configured", "[cwProjectSyncHealth]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    repo.setAccount(&account);

    writeFile(tempDir.path(), QStringLiteral("state.txt"), QStringLiteral("initial\n"));
    CHECK_NOTHROW(repo.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));

    cwProjectSyncHealth syncHealth;
    syncHealth.setRepository(&repo);
    syncHealth.refresh();

    REQUIRE(waitUntil([&syncHealth]() {
        return syncHealth.status().syncBlocker() == cwSyncStatus::SyncBlocker::NoRemote;
    }));
    CHECK(syncHealth.status().noRemote());
    CHECK(!syncHealth.status().needsLogin());
    CHECK(!syncHealth.status().authExpired());
}
```

**New: `noRemote()` clears when a remote is added (remotesChanged re-triggers refresh):**

```cpp
TEST_CASE("cwProjectSyncHealth clears NoRemote after remote is added", "[cwProjectSyncHealth]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bareRepo = nullptr;
    REQUIRE(git_repository_init(&bareRepo, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bareRepo);

    Account account;
    account.setName(QStringLiteral("Tester"));
    account.setEmail(QStringLiteral("tester@example.com"));

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    repo.setAccount(&account);

    writeFile(tempDir.path(), QStringLiteral("state.txt"), QStringLiteral("initial\n"));
    CHECK_NOTHROW(repo.commitAll(QStringLiteral("Initial"), QStringLiteral("initial commit")));

    cwProjectSyncHealth syncHealth;
    syncHealth.setRepository(&repo);
    syncHealth.refresh();

    REQUIRE(waitUntil([&syncHealth]() { return syncHealth.status().noRemote(); }));

    // Adding a remote emits remotesChanged, which should re-trigger refresh()
    REQUIRE(repo.addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(remotePath)).isEmpty());

    REQUIRE(waitUntil([&syncHealth]() { return !syncHealth.status().noRemote(); }));
    CHECK(syncHealth.status().syncBlocker() != cwSyncStatus::SyncBlocker::NoRemote);
}
```

**New: `operator==` distinguishes `SyncBlocker` values:**

```cpp
TEST_CASE("cwSyncStatus operator== compares syncBlocker", "[cwSyncStatus]")
{
    cwSyncStatus a, b;
    CHECK(a == b);

    a.m_syncBlocker = cwSyncStatus::SyncBlocker::NoRemote;
    CHECK(a != b);

    b.m_syncBlocker = cwSyncStatus::SyncBlocker::NoRemote;
    CHECK(a == b);
}
```

### 11.3 `test_cwGitHubIntegration.cpp` — new file

All four cases start a local `QHttpServer` (Qt 6.10+, `Qt::HttpServer`) on a free port and point
`integration.setApiBaseUrl(...)` at it. `cwRemoteCredentialStore` can be passed as `nullptr` for
these tests — no keychain access occurs.

**Success path — 201 Created:**

```cpp
TEST_CASE("cwGitHubIntegration::createRepository emits repositoryCreated on 201", "[cwGitHubIntegration]")
{
    QHttpServer server;
    server.route("/user/repos", QHttpServerRequest::Method::Post,
                 [](const QHttpServerRequest&) {
                     return QHttpServerResponse(
                         R"({"name":"my-cave","clone_url":"https://github.com/user/my-cave.git",)"
                         R"("ssh_url":"git@github.com:user/my-cave.git",)"
                         R"("html_url":"https://github.com/user/my-cave","private":true})",
                         QHttpServerResponse::StatusCode::Created);
                 });
    const quint16 port = server.listen(QHostAddress::LocalHost);
    REQUIRE(port != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));

    cwGitHubRepositoryItem created;
    bool fired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreated,
                     [&](const cwGitHubRepositoryItem& r) { created = r; fired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&fired]() { return fired; }));
    CHECK(created.name == QStringLiteral("my-cave"));
    CHECK(created.cloneUrl == QStringLiteral("https://github.com/user/my-cave.git"));
    CHECK(created.isPrivate);
}
```

**401/403 — token must be invalidated:**

```cpp
TEST_CASE("cwGitHubIntegration::createRepository invalidates token on 401", "[cwGitHubIntegration]")
{
    QHttpServer server;
    server.route("/user/repos", QHttpServerRequest::Method::Post,
                 [](const QHttpServerRequest&) {
                     return QHttpServerResponse(
                         R"({"message":"Bad credentials"})",
                         QHttpServerResponse::StatusCode::Unauthorized);
                 });
    const quint16 port = server.listen(QHostAddress::LocalHost);
    REQUIRE(port != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));

    bool failureFired = false;
    bool invalidatedFired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreationFailed,
                     [&](const QString&) { failureFired = true; });
    QObject::connect(&integration, &cwGitHubIntegration::tokenInvalidated,
                     [&](const QString&, const QString&) { invalidatedFired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&failureFired]() { return failureFired; }));
    CHECK(invalidatedFired);
}
```

**422 Unprocessable Entity — token must NOT be invalidated; repo name in message:**

```cpp
TEST_CASE("cwGitHubIntegration::createRepository does not invalidate token on 422", "[cwGitHubIntegration]")
{
    QHttpServer server;
    server.route("/user/repos", QHttpServerRequest::Method::Post,
                 [](const QHttpServerRequest&) {
                     return QHttpServerResponse(
                         R"({"message":"Repository creation failed.",)"
                         R"("errors":[{"message":"name already exists on this account"}]})",
                         QHttpServerResponse::StatusCode::UnprocessableEntity);
                 });
    const quint16 port = server.listen(QHostAddress::LocalHost);
    REQUIRE(port != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));

    QString failureMessage;
    bool invalidatedFired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreationFailed,
                     [&](const QString& msg) { failureMessage = msg; });
    QObject::connect(&integration, &cwGitHubIntegration::tokenInvalidated,
                     [&](const QString&, const QString&) { invalidatedFired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&failureMessage]() { return !failureMessage.isEmpty(); }));
    CHECK(!invalidatedFired);
    // Message must contain the name so the user knows what conflicted
    CHECK(failureMessage.contains(QStringLiteral("my-cave")));
}
```

**5xx — token must NOT be invalidated; HTTP status in message:**

```cpp
TEST_CASE("cwGitHubIntegration::createRepository does not invalidate token on 500", "[cwGitHubIntegration]")
{
    QHttpServer server;
    server.route("/user/repos", QHttpServerRequest::Method::Post,
                 [](const QHttpServerRequest&) {
                     return QHttpServerResponse(
                         R"({"message":"Internal Server Error"})",
                         QHttpServerResponse::StatusCode::InternalServerError);
                 });
    const quint16 port = server.listen(QHostAddress::LocalHost);
    REQUIRE(port != 0);

    cwGitHubIntegration integration(nullptr);
    integration.setApiBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));

    QString failureMessage;
    bool invalidatedFired = false;
    QObject::connect(&integration, &cwGitHubIntegration::repositoryCreationFailed,
                     [&](const QString& msg) { failureMessage = msg; });
    QObject::connect(&integration, &cwGitHubIntegration::tokenInvalidated,
                     [&](const QString&, const QString&) { invalidatedFired = true; });

    integration.createRepository(QStringLiteral("my-cave"), true);

    REQUIRE(waitUntil([&failureMessage]() { return !failureMessage.isEmpty(); }));
    CHECK(!invalidatedFired);
    CHECK(failureMessage.contains(QStringLiteral("500")));
}
```

### 11.4 `test_cwRemoteAccountCoordinator.cpp` — new file

`addRemoteToProject()` delegates to `repository->addRemote()`, which returns synchronously. Use a
real `GitRepository` + `QTemporaryDir`; no mock needed.

**Success, `bindToGitHubAccount = false` — no binding attempted:**

```cpp
TEST_CASE("cwRemoteAccountCoordinator::addRemoteToProject adds remote and skips binding when flag is false",
          "[cwRemoteAccountCoordinator]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bare = nullptr;
    REQUIRE(git_repository_init(&bare, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bare);

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwGitHubIntegration integration(nullptr);
    cwRemoteAccountModel accountModel;
    cwRemoteBindingStore bindingStore;
    cwRemoteAccountCoordinator coordinator(&integration, &accountModel, &bindingStore);

    bool failed = false;
    QObject::connect(&coordinator, &cwRemoteAccountCoordinator::addRemoteFailed,
                     [&]() { failed = true; });

    coordinator.addRemoteToProject(&repo, QUrl::fromLocalFile(remotePath), false);

    REQUIRE(waitUntil([&repo]() { return !repo.remotes().isEmpty(); }));
    CHECK(!failed);
    CHECK(repo.remotes().constFirst().name() == QStringLiteral("origin"));
}
```

**Failure — duplicate "origin" causes `addRemote()` to fail; `addRemoteFailed` emitted:**

```cpp
TEST_CASE("cwRemoteAccountCoordinator::addRemoteToProject emits addRemoteFailed on duplicate remote",
          "[cwRemoteAccountCoordinator]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bare = nullptr;
    REQUIRE(git_repository_init(&bare, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bare);

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();
    // Pre-add "origin" so the second addRemote fails
    repo.addRemote(QStringLiteral("origin"), QUrl::fromLocalFile(remotePath));

    cwGitHubIntegration integration(nullptr);
    cwRemoteAccountModel accountModel;
    cwRemoteBindingStore bindingStore;
    cwRemoteAccountCoordinator coordinator(&integration, &accountModel, &bindingStore);

    QString errorMessage;
    QObject::connect(&coordinator, &cwRemoteAccountCoordinator::addRemoteFailed,
                     [&](const QString& msg) { errorMessage = msg; });

    coordinator.addRemoteToProject(&repo, QUrl::fromLocalFile(remotePath), false);

    REQUIRE(waitUntil([&errorMessage]() { return !errorMessage.isEmpty(); }));
}
```

**Success, `bindToGitHubAccount = true` — binding is stored in the binding store:**

```cpp
TEST_CASE("cwRemoteAccountCoordinator::addRemoteToProject stores binding when flag is true",
          "[cwRemoteAccountCoordinator]")
{
    auto tempDir = QTemporaryDir();
    REQUIRE(tempDir.isValid());

    const QString remotePath = QDir(tempDir.path()).filePath(QStringLiteral("remote.git"));
    git_repository* bare = nullptr;
    REQUIRE(git_repository_init(&bare, remotePath.toLocal8Bit().constData(), 1) == GIT_OK);
    git_repository_free(bare);

    GitRepository repo;
    repo.setDirectory(QDir(tempDir.path()));
    repo.initRepository();

    cwGitHubIntegration integration(nullptr);
    cwRemoteAccountModel accountModel;
    cwRemoteBindingStore bindingStore;
    cwRemoteAccountCoordinator coordinator(&integration, &accountModel, &bindingStore);

    bool failed = false;
    QObject::connect(&coordinator, &cwRemoteAccountCoordinator::addRemoteFailed,
                     [&]() { failed = true; });

    const QUrl remoteUrl = QUrl::fromLocalFile(remotePath);
    coordinator.addRemoteToProject(&repo, remoteUrl, true);

    REQUIRE(waitUntil([&repo]() { return !repo.remotes().isEmpty(); }));
    CHECK(!failed);
    // bindRemoteToActiveGitHubAccount stores an entry in the binding store
    CHECK(!bindingStore.accountIdForRemote(remoteUrl.toString()).isEmpty());
}
```

### 11.5 `tst_SetupRemoteWizard.qml` — new file

All tests below run without GitHub credentials. Path A tester-assisted tests are gated with
`TesterAssistedGateDialog` (auto-skipped after 5 s unless `CW_QML_TESTER_ASSISTED_MODE=run_all`).

The `init()` helper saves the project as a `.cwproj` directory first so `RootData.project.repository`
is initialised. Path C (custom URL) tests use `TestHelper.createLocalBareRemoteForCloneTest()` to get
a real local bare-repo URL.

`objectName` values referenced below (`"alreadyHaveRemoteLink"`, `"customUrlTab"`, `"customUrlField"`,
`"connectButton"`, `"createButton"`, `"repoNameField"`, `"syncNowButton"`, `"closeButton"`,
`"statusBadge"`) must be set on the corresponding QML items when they are built.

```qml
import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

Item {
    id: rootId
    width: 800
    height: 600

    CWTestCase {
        name: "SetupRemoteWizard"
        when: windowShown

        property var wizard: null

        function init() {
            RootData.newProject()
            tryVerify(() => RootData.project.isTemporaryProject)
            // Must save as .cwproj so RootData.project.repository is initialised
            let tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/wizard-test.cwproj"))
            tryVerify(() => !RootData.project.isTemporaryProject)
        }

        function cleanup() {
            if (wizard !== null) {
                wizard.close()
                wizard.destroy()
                wizard = null
            }
        }

        function openWizard() {
            let comp = Qt.createComponent("SetupRemoteWizard.qml")
            wizard = comp.createObject(rootId, {
                gitHubIntegration: RootData.remote.gitHubIntegration,
                accountCoordinator: RootData.remote.accountCoordinator,
                repository: RootData.project.repository
            })
            wizard.open()
            tryVerify(() => wizard.visible)
            return wizard
        }

        // ── Screen navigation ──────────────────────────────────────────────

        function test_opensOnCreateRepoScreen() {
            let w = openWizard()
            compare(w.currentState, "createRepo")
        }

        function test_alreadyHaveRemoteLink_navigatesToConnectScreen() {
            let w = openWizard()
            let link = findChild(w, "alreadyHaveRemoteLink")
            verify(link !== null)
            mouseClick(link)
            tryVerify(() => w.currentState === "connectRepo")
        }

        function test_backLink_returnsToCreateScreen() {
            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo")
            mouseClick(findChild(w, "createInsteadLink"))
            tryVerify(() => w.currentState === "createRepo")
        }

        // ── Path A form guards ─────────────────────────────────────────────

        function test_pathA_createButtonDisabledWhenNameEmpty() {
            let w = openWizard()
            compare(w.currentState, "createRepo")
            let nameField = findChild(w, "repoNameField")
            verify(nameField !== null)
            nameField.text = ""
            let createButton = findChild(w, "createButton")
            verify(createButton !== null)
            verify(!createButton.enabled)
        }

        // ── Path C (custom URL) — fully automated with local bare repo ─────

        function test_pathC_connectButtonDisabledWhenUrlEmpty() {
            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo")
            mouseClick(findChild(w, "customUrlTab"))
            let connectButton = findChild(w, "connectButton")
            verify(connectButton !== null)
            verify(!connectButton.enabled)
        }

        function test_pathC_connectButtonEnabledWhenUrlFilled() {
            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo")
            mouseClick(findChild(w, "customUrlTab"))
            let urlField = findChild(w, "customUrlField")
            verify(urlField !== null)
            urlField.text = "https://example.com/repo.git"
            tryVerify(() => findChild(w, "connectButton").enabled)
        }

        function test_pathC_connectLocalBareRepo_reachesDoneState() {
            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")

            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo")
            mouseClick(findChild(w, "customUrlTab"))
            findChild(w, "customUrlField").text = fixture.remoteUrl
            tryVerify(() => findChild(w, "connectButton").enabled)
            mouseClick(findChild(w, "connectButton"))

            tryVerify(() => w.currentState === "done", 10000)
        }

        function test_doneScreen_showsSyncNowAndCloseButtons() {
            let fixture = TestHelper.createLocalBareRemoteForCloneTest()
            compare(fixture.errorMessage, "")

            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo")
            mouseClick(findChild(w, "customUrlTab"))
            findChild(w, "customUrlField").text = fixture.remoteUrl
            tryVerify(() => findChild(w, "connectButton").enabled)
            mouseClick(findChild(w, "connectButton"))
            tryVerify(() => w.currentState === "done", 10000)

            let syncNowButton = findChild(w, "syncNowButton")
            let closeButton = findChild(w, "closeButton")
            verify(syncNowButton !== null && syncNowButton.visible)
            verify(closeButton !== null && closeButton.visible)
        }

        function test_pathC_badUrl_errorShown_staysOnConnectScreen() {
            let w = openWizard()
            mouseClick(findChild(w, "alreadyHaveRemoteLink"))
            tryVerify(() => w.currentState === "connectRepo")
            mouseClick(findChild(w, "customUrlTab"))
            findChild(w, "customUrlField").text = "not-a-valid-url-that-will-fail"
            tryVerify(() => findChild(w, "connectButton").enabled)
            mouseClick(findChild(w, "connectButton"))

            // Must stay on connectRepo (never navigate away on error)
            tryVerify(() => w.currentState === "connectRepo"
                            || w.connectForm.errorMessage !== "", 5000)
            compare(w.currentState, "connectRepo")
        }

        // ── Path A client-side duplicate-name check ────────────────────────
        // Populate gitHubIntegration.repositories with a stub entry, then
        // confirm the form shows an inline error without emitting createRepository.

        function test_pathA_duplicateName_inlineError_noApiCall() {
            // Inject a fake repo into the repositories model so the duplicate check fires.
            // Use the real integration's refreshRepositories() if already authorized,
            // otherwise skip — the duplicate-check logic is in the form, not in the network layer.
            if (RootData.remote.gitHubIntegration.authState !== GitHubIntegration.Authorized) {
                skip("Not authorized — duplicate name check requires a populated repo list")
                return
            }
            RootData.remote.gitHubIntegration.refreshRepositories()
            tryVerify(() => RootData.remote.gitHubIntegration.repositories.rowCount() > 0, 15000)

            let existingName = RootData.remote.gitHubIntegration.repositories
                .data(RootData.remote.gitHubIntegration.repositories.index(0, 0),
                      Qt.DisplayRole)

            let w = openWizard()
            compare(w.currentState, "createRepo")

            let nameField = findChild(w, "repoNameField")
            nameField.text = existingName

            let createButton = findChild(w, "createButton")
            mouseClick(createButton)

            // Error must appear inline; wizard must not transition to "working"
            tryVerify(() => w.createForm.errorMessage !== "", 2000)
            compare(w.currentState, "createRepo")
        }

        // ── Tester-assisted: Path A requires live GitHub ───────────────────

        function test_testerAssisted_pathA_createRepo_reachesDone() {
            testerAssistedGate.beginDecision(
                "Path A: Create GitHub repo",
                "Requires a GitHub account.\n"
                + "Sign in when prompted, then confirm repo creation completes.")
            tryVerify(() => testerAssistedGate.decisionReady, testerAssistedGate.decisionTimeoutMs)
            if (!testerAssistedGate.runCurrentTest) {
                skip("Tester-assisted test skipped.")
                return
            }

            let w = openWizard()
            if (RootData.remote.gitHubIntegration.authState !== GitHubIntegration.Authorized) {
                tryVerify(() => findChild(w, "copyAndOpenButton") !== null, 10000)
                mouseClick(findChild(w, "copyAndOpenButton"))
                tryVerify(() => RootData.remote.gitHubIntegration.authState
                                === GitHubIntegration.Authorized, 300000)
            }

            let createButton = findChild(w, "createButton")
            tryVerify(() => createButton.enabled)
            mouseClick(createButton)
            tryVerify(() => w.currentState === "done", 30000)
        }
    }

    TesterAssistedGateDialog {
        id: testerAssistedGate
        dialogParent: rootId
        autoSkipCountdownSeconds: 5
    }
}
```

### 11.6 SyncButton `noRemote` state — add to `tst_RemoteRepositoryPage.qml`

Open a `.cwproj` project with no remote (new project saved as directory). Verify the button reflects
`noRemote` and emits `setupRemoteRequested` on click.

```qml
function test_syncButton_noRemote_showsCloudUploadIcon() {
    // newProject + save as .cwproj leaves no remote configured
    let syncButton = findChild(mainWindow, "syncButton")
    verify(syncButton !== null)
    tryVerify(() => syncButton.noRemote, 5000)
    verify(syncButton.icon.source.toString().indexOf("cloud-upload") >= 0)
}

function test_syncButton_noRemote_tooltip() {
    let syncButton = findChild(mainWindow, "syncButton")
    tryVerify(() => syncButton.noRemote, 5000)
    // Use the exposed tooltipText property — QC.ToolTip attached properties
    // are not readable from outside the item.
    verify(syncButton.tooltipText.indexOf("No remote") >= 0)
}

function test_syncButton_noRemote_badge_hidden() {
    let syncButton = findChild(mainWindow, "syncButton")
    tryVerify(() => syncButton.noRemote, 5000)
    let badge = findChild(syncButton, "statusBadge")
    verify(badge !== null)
    verify(!badge.visible)
}

function test_syncButton_noRemote_click_emitsSetupRemoteRequested() {
    let syncButton = findChild(mainWindow, "syncButton")
    tryVerify(() => syncButton.noRemote, 5000)

    let spy = Qt.createQmlObject(
        'import QtTest; SignalSpy { target: syncButton; signalName: "setupRemoteRequested" }',
        rootId)
    mouseClick(syncButton)
    compare(spy.count, 1)
}

function test_syncButton_noRemote_contextMenu_syncNowDisabled() {
    let syncButton = findChild(mainWindow, "syncButton")
    tryVerify(() => syncButton.noRemote, 5000)
    // Right-click to open context menu
    mouseClick(syncButton, syncButton.width / 2, syncButton.height / 2, Qt.RightButton)
    // Use objectName set on the menu item (§5.1) rather than searching by text
    let syncNowItem = findChild(syncButton, "syncNowMenuItem")
    verify(syncNowItem !== null)
    verify(!syncNowItem.enabled)
}
```

### 11.7 CMakeLists.txt additions (step 10)

- Add `test_cwGitHubIntegration.cpp` and `test_cwRemoteAccountCoordinator.cpp` to `cavewhere-test`
- Link `Qt::HttpServer` to `cavewhere-test`
- Add `tst_SetupRemoteWizard.qml` to `cavewhere-qml-test`

Run the new tests in isolation:

```bash
./build/<preset>/cavewhere-test "[cwProjectSyncHealth]" "[cwSyncStatus]" "[cwGitHubIntegration]" "[cwRemoteAccountCoordinator]"
./build/<preset>/cavewhere-qml-test --platform offscreen -testall tst_SetupRemoteWizard
```

## 7. Out of Scope

- SSH key generation or automatic upload to GitHub
- GitLab / Bitbucket OAuth flows (Path C accepts their URLs as manual input only)
- Organization picker (v1: user repos only; org support can be added later)
- Renaming or removing an existing remote (separate settings concern)
