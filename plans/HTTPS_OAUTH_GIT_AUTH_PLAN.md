# Remote Authentication Plan (HTTPS via OAuth Token)

Closes #271, #269.

## Goal

Replace SSH key management with HTTPS + OAuth token for all GitHub git operations (clone,
push, pull, sync). A user who completes the GitHub OAuth device flow once should be able to
clone and sync indefinitely without re-authenticating. Remove the "Upload SSH Key" button
entirely.

HTTPS is chosen over SSH because:
- The OAuth token is already in hand after the device flow — no second auth system needed
- Eliminates SSH key generation, storage, upload, host key verification, and known_hosts management
- Works through corporate firewalls (port 443 vs port 22)
- Speed difference vs SSH is negligible for cave survey data sizes
- Fixes the HTTPS username error (#269) as a side effect

---

## User-Facing Flows

### First-time user
1. User clicks "Connect to GitHub" → completes OAuth device flow
2. Token stored in keychain (already happens today)
3. Repository list appears
4. User selects a repo → HTTPS clone URL pre-filled → clicks Clone → works

### Returning user (valid token in keychain)
1. App reloads token from keychain on launch (already happens today)
2. Repository list appears
3. Clone / sync works — no re-authentication needed

### Token revoked or expired
1. Clone or sync fails with an HTTP 401 from GitHub
2. Inline message: *"GitHub access expired."* + **Reconnect** button
3. User clicks Reconnect → OAuth device flow → token refreshed → retry

### Manual HTTPS URL (non-GitHub or no account selected)
- Clone proceeds; libgit2 tries HTTPS with no token (empty `httpsToken`)
- If auth fails, show the standard clone error — no special handling needed

### Manual SSH URL (`git@github.com:…` or self-hosted)
- SSH credential callback path is unchanged; app keys and SSH agent are still tried
- No auto-upload of SSH keys

---

## Architecture

### Credential flow

```
cwGitHubIntegration (holds OAuth token in memory)
    │   accessToken() → QString
    │   signal: accessTokenChanged()
    │
    ├── cwSaveLoad::setGitHubIntegration()
    │       connects accessTokenChanged → GitRepository::setCredentials()
    │       └── GitRepository (holds m_credentials: GitCredentials)
    │               fetch() / push() / clone()
    │                   snapshot m_credentials by value before QtConcurrent::run
    │                   └── credentailCallBack() ← SshCallbackPayload::httpsToken
    │
    └── cwRemoteRepositoryCloner::setGitHubIntegration()
            connects accessTokenChanged → m_cloneRepository->setCredentials()
            └── GitRepository (clone instance, holds m_credentials: GitCredentials)
                    clone()
                        snapshot m_credentials by value before QtConcurrent::run
                        └── credentailCallBack() ← SshCallbackPayload::httpsToken
```

**Thread safety:** `m_credentials` is read on the main thread and captured by value into
the `SshCallbackPayload` before `QtConcurrent::run` starts. The background thread never
reads `GitRepository` members directly — only the stack-allocated copy inside the lambda.
No lock needed.

**Token lifetime:** `cwSaveLoad` and `cwRemoteRepositoryCloner` hold a `QPointer` to
`cwGitHubIntegration`. On `accessTokenChanged()` they call `repo->setCredentials()`,
which updates `GitRepository` on the main thread. Any in-flight operation already holds
a value copy, so clearing the token mid-flight does not affect it — correct behaviour.

---

## Changes Required

### 1. Extend `SshCallbackPayload` with HTTPS token field

**File:** `QQuickGit/src/GitRepository.cpp`

Add one field to the existing struct (line 436):

```cpp
struct SshCallbackPayload {
    std::function<void(const ProgressState&)> reportProgress;
    int agentMaxAttempts = 1;
    int agentAttempts = 0;
    bool allowAgent = true;
    QString httpsToken;   // empty = SSH-only path (default, no behaviour change)
};
```

Also extend `GitErrorCode` enum in `QQuickGit/src/GitRepository.h` with a new value:

```cpp
enum class GitErrorCode {
    PushRejectedByRemoteAdvance = CustomError + 1,
    PushWildcardRefSpecUnsupported = CustomError + 2,
    PushFailed = CustomError + 3,
    HttpAuthFailed = CustomError + 4,   // NEW: HTTP 401 from remote
};
```

---

### 2. Add HTTPS branch to `credentailCallBack`

**File:** `QQuickGit/src/GitRepository.cpp` — `GitRepositoryData::credentailCallBack()` (line 1765)

Insert before the existing `GIT_CREDENTIAL_SSH_KEY` check:

```cpp
if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) {
    auto* p = reinterpret_cast<SshCallbackPayload*>(payload);
    if (p && !p->httpsToken.isEmpty()) {
        return git_credential_userpass_plaintext_new(
            out,
            "x-access-token",
            p->httpsToken.toUtf8().constData());
    }
    // No token and no URL-embedded credentials: return GIT_PASSTHROUGH
    // so libgit2 tries other methods (e.g. SSH_KEY if the URL is ambiguous).
    return GIT_PASSTHROUGH;
}
```

GitHub accepts `x-access-token` as the username with the OAuth token as the password.
All existing call sites leave `httpsToken` empty, so SSH behaviour is entirely unchanged.
The explicit `GIT_PASSTHROUGH` when the token is empty makes the fallthrough intentional
rather than implicit.

---

### 3. Bridge the token through `GitRepository`

This is the key architectural piece. The token must cross the `GitRepository` API boundary
because `SshCallbackPayload` is constructed inside `GitRepository`'s private lambdas —
`cwSaveLoad` and `cwRemoteRepositoryCloner` never touch it directly.

#### 3a. Add `accessToken()` to `cwGitHubIntegration`

**File:** `cavewherelib/src/cwGitHubIntegration.h`

`m_accessToken` is private with no public C++ getter today. Add one (no `Q_PROPERTY`
— never visible to QML):

```cpp
QString accessToken() const { return m_accessToken; }
```

`m_accessToken` is already populated from the keychain at startup via
`reloadAccessTokenFromCredentialStore()` and kept live through the OAuth flow.
No async read is needed at token-use time.

**This getter is a prerequisite for steps 3d and 3g** — both call
`m_gitHubIntegration->accessToken()` and will not compile without it.

#### 3b. Add `GitCredentials` struct and `setCredentials()` to `GitRepository`

**File:** `QQuickGit/src/GitRepository.h`

Add a `GitCredentials` struct in the `QQuickGit` namespace before the `GitRepository`
class declaration:

```cpp
struct QQUICKGIT_EXPORT GitCredentials {
    QString httpsToken;   // empty = SSH-only path (default, no behaviour change)
};
```

Add to `GitRepository` public interface:

```cpp
void setCredentials(const GitCredentials& credentials);
```

In `GitRepositoryData` (private data class in `.cpp`), add:

```cpp
GitCredentials m_credentials;
```

Implementation:

```cpp
void GitRepository::setCredentials(const GitCredentials& credentials)
{
    d->m_credentials = credentials;
}
```

#### 3c. Snapshot credentials before each `QtConcurrent::run`

**File:** `QQuickGit/src/GitRepository.cpp`

`fetch` uses a free function `fetchRefsForDirectory` — add `const GitCredentials& credentials`
as a parameter and pass `d->m_credentials` at each of the three call sites (in `pull`,
`pullRebaseOrMerge`, and `fetch` methods). Inside the function, capture `credentials` by
value in both lambdas and assign to `callbackPayload.httpsToken`:

```cpp
GitRepository::GitFuture fetchRefsForDirectory(..., const GitCredentials& credentials)
{
    return QtConcurrent::run([..., credentials]() mutable {
        return mtry([..., credentials]() mutable -> ResultBase {
            // ...
            callbackPayload.httpsToken = credentials.httpsToken;
        });
    });
}
```

For **push** and **clone**, snapshot `d->m_credentials` by value on the main thread
before `QtConcurrent::run` and assign `credentials.httpsToken` to the payload:

```cpp
const GitCredentials credentials = d->m_credentials;   // snapshot on main thread
// ... QtConcurrent::run([..., credentials]() { ... callbackPayload.httpsToken = credentials.httpsToken; })
```

#### 3d. Wire token into `cwSaveLoad`

**File:** `cavewherelib/src/cwSaveLoad.h`

```cpp
void setGitHubIntegration(cwGitHubIntegration* gh);

private:
    QPointer<cwGitHubIntegration> m_gitHubIntegration;
```

**File:** `cavewherelib/src/cwSaveLoad.cpp`

```cpp
void cwSaveLoad::setGitHubIntegration(cwGitHubIntegration* gh)
{
    if (m_gitHubIntegration) {
        disconnect(m_gitHubIntegration, &cwGitHubIntegration::accessTokenChanged,
                   this, nullptr);
    }
    m_gitHubIntegration = gh;
    auto updateToken = [this]() {
        const QString token = m_gitHubIntegration
                              ? m_gitHubIntegration->accessToken()
                              : QString{};
        d->repository->setCredentials(QQuickGit::GitCredentials{token});
    };
    if (gh) {
        connect(gh, &cwGitHubIntegration::accessTokenChanged, this, updateToken);
    }
    updateToken();   // apply current token immediately
}
```

No change needed inside `sync()` itself — `d->repository` already holds the live token.

#### 3e. Expose `setGitHubIntegration` on `cwProject`

**File:** `cavewherelib/src/cwProject.h` / `.cpp`

`cwProject` owns `m_saveLoad` privately and does not expose a `saveLoad()` accessor.
Add a thin forwarding method instead:

```cpp
// cwProject.h
void setGitHubIntegration(cwGitHubIntegration* gh);

// cwProject.cpp
void cwProject::setGitHubIntegration(cwGitHubIntegration* gh)
{
    if (m_saveLoad) {
        m_saveLoad->setGitHubIntegration(gh);
    }
}
```

#### 3f. Wire in `cwRootData`

**File:** `cavewherelib/src/cwRootData.cpp`

After both `Project` and `remote()` are constructed (line 95 already calls `remote()`):

```cpp
Project->setGitHubIntegration(remote()->gitHubIntegration());
```

#### 3g. Wire token into `cwRemoteRepositoryCloner`

**File:** `cavewherelib/src/cwRemoteRepositoryCloner.h`

```cpp
Q_PROPERTY(cwGitHubIntegration* gitHubIntegration
           WRITE setGitHubIntegration NOTIFY gitHubIntegrationChanged)

void setGitHubIntegration(cwGitHubIntegration* gh);

signals:
    void gitHubIntegrationChanged();

private:
    QPointer<cwGitHubIntegration> m_gitHubIntegration;
```

**File:** `cavewherelib/src/cwRemoteRepositoryCloner.cpp`

```cpp
void cwRemoteRepositoryCloner::setGitHubIntegration(cwGitHubIntegration* gh)
{
    if (m_gitHubIntegration) {
        disconnect(m_gitHubIntegration, &cwGitHubIntegration::accessTokenChanged,
                   this, nullptr);
    }
    m_gitHubIntegration = gh;
    auto updateToken = [this]() {
        const QString token = m_gitHubIntegration
                              ? m_gitHubIntegration->accessToken()
                              : QString{};
        m_cloneRepository->setCredentials(QQuickGit::GitCredentials{token});
    };
    if (gh) {
        connect(gh, &cwGitHubIntegration::accessTokenChanged, this, updateToken);
    }
    updateToken();
    emit gitHubIntegrationChanged();
}
```

**File:** `cavewherelib/qml/RemoteRepositoryPage.qml`

Bind the pointer (not the token) on the existing `RemoteRepositoryCloner` instantiation:

```qml
RemoteRepositoryCloner {
    gitHubIntegration: RootData.remote.gitHubIntegration
    // existing properties unchanged
}
```

---

### 4. Zero the token on account forget / token invalidation

When the user clicks "Forget Account" or the token is invalidated, `cwGitHubIntegration`
already clears `m_accessToken` and emits `accessTokenChanged()`. The signal connections
added in steps 3d and 3g forward this to `GitRepository::setCredentials({})` automatically.
No additional work is needed.

Any in-flight operation already holds a value copy in its `SshCallbackPayload` —
completing with the credentials it started with is correct behaviour.

---

### 5. 401 detection in `GitRepository`, `cwSaveLoad`, and `cwRemoteRepositoryCloner`

#### 5a. Detect `HttpAuthFailed` in `GitRepository`

**File:** `QQuickGit/src/GitRepository.cpp`

After each libgit2 operation that can produce an HTTP 401 (`git_remote_fetch`,
`git_remote_push`, `git_clone`), check the libgit2 error class before constructing
the `ResultBase`. A helper keeps the detection in one place:

```cpp
static bool isHttpAuthFailure(int libgitError, const QString& errorMessage)
{
    if (libgitError == GIT_OK) {
        return false;
    }
    const git_error* err = git_error_last();
    return err
        && err->klass == GIT_ERROR_HTTP
        && (errorMessage.contains(QLatin1String("401"))
            || errorMessage.contains(QLatin1String("authentication")));
}
```

Use it at each error site, e.g. after `git_remote_fetch`:

```cpp
if (fetchError != GIT_OK) {
    const QString msg = gitErrorMessageWithPrefix(QStringLiteral("Failed to fetch"));
    const int code = isHttpAuthFailure(fetchError, msg)
                     ? static_cast<int>(GitRepository::GitErrorCode::HttpAuthFailed)
                     : fetchError;
    return ResultBase(msg, code);
}
```

Apply identically after `git_remote_push` and `git_clone`.

#### 5b. Propagate through `cwSaveLoad`

**File:** `cavewherelib/src/cwSaveLoad.h`

Add `HttpAuthFailed` to the `SyncErrorCode` enum:

```cpp
enum class SyncErrorCode {
    RetryEpochChanged = CustomError + 1,
    IncompatibleProjectVersion = CustomError + 2,
    HttpAuthFailed = CustomError + 3,   // NEW
};
```

**File:** `cavewherelib/src/cwSaveLoad.cpp`

In the sync result handler, before the generic error path, check for auth failure and
re-wrap with `cwSaveLoad`'s own error code so callers don't need to import
`GitRepository.h`:

```cpp
if (result.errorCode() == static_cast<int>(GitRepository::GitErrorCode::HttpAuthFailed)) {
    return ResultBase(result.errorMessage(),
                      static_cast<int>(cwSaveLoad::SyncErrorCode::HttpAuthFailed));
}
```

#### 5c. Emit `syncAuthFailed` from `cwProject`

**File:** `cavewherelib/src/cwProject.h`

```cpp
signals:
    void syncAuthFailed();
```

**File:** `cavewherelib/src/cwProject.cpp` — `completeSyncOperation()`

Insert before the generic `ErrorModel->append(...)`:

```cpp
if (result.errorCode() == static_cast<int>(cwSaveLoad::SyncErrorCode::HttpAuthFailed)) {
    emit syncAuthFailed();
    return;
}
```

#### 5d. `cwProjectSyncHealth` — add `authExpired` to `cwSyncStatus`

**File:** `cavewherelib/src/cwProjectSyncHealth.h`

`cwSyncStatus` is a `Q_GADGET` with `m_stale`, `m_message`, and count fields.
Add an `authExpired` flag to the existing struct:

```cpp
// cwSyncStatus struct — add field and Q_PROPERTY:
Q_PROPERTY(bool authExpired READ authExpired)
bool authExpired() const { return m_authExpired; }
bool m_authExpired = false;
```

Update `operator==` and `operator!=` to include `m_authExpired`.

**File:** `QQuickGit/src/GitRepository.cpp` — `remoteAheadBehindCommitCounts` (line 3978)

`remoteAheadBehindCommitCounts` does `git_remote_connect` with `PrePushCredentialPayload`
(line 3974–3977). After the connect fails, detect auth failure with the same helper from
step 5a, and return `HttpAuthFailed` so callers can distinguish it from other errors:

```cpp
if (connectResult != GIT_OK) {
    const QString msg = gitErrorMessageWithPrefix(
        QStringLiteral("Failed to connect remote for ahead/behind"));
    const int code = isHttpAuthFailure(connectResult, msg)
                     ? static_cast<int>(GitRepository::GitErrorCode::HttpAuthFailed)
                     : connectResult;
    return Monad::Result<AheadBehindCounts>(msg, code);
}
```

**File:** `cavewherelib/src/cwProjectSyncHealth.cpp` — `onFutureChanged` lambda (line 30)

The call chain is: `refresh()` → `scheduleRemoteStatusRefresh()` →
`repoPtr->remoteAheadBehindCommitCounts()`. The result (`Monad::Result<AheadBehindCounts>`)
is handled in the `onFutureChanged` lambda in the constructor (line 29). In the
`aheadBehindResult.hasError()` branch, before writing the generic stale status, check
for the auth failure code and set `m_authExpired`:

```cpp
if (aheadBehindResult.hasError()) {
    const bool authFailed =
        aheadBehindResult.errorCode()
        == static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed);
    setStatus(cwSyncStatus{
        .m_hasLocalChanges = localChanges,
        .m_aheadCount = 0,
        .m_behindCount = 0,
        .m_stale = true,
        .m_authExpired = authFailed,
        .m_message = authFailed
            ? QStringLiteral("GitHub access expired.")
            : (aheadBehindResult.errorMessage().isEmpty()
               ? QStringLiteral("Sync status unavailable: upstream branch is missing.")
               : aheadBehindResult.errorMessage())});
    return;
}
```

On success, `m_authExpired` defaults to `false` in the success-path `cwSyncStatus{}`,
so it is cleared automatically on the next successful refresh — no extra logic needed.

#### 5e. Show reconnect indicator on `SyncButton`

Do **not** navigate the user away from their current page on auth failure — it interrupts
work and `syncInProgress` is not an existing property. Instead, bind the `authExpired`
field of `cwSyncStatus` in `SyncButton.qml` to show a distinct indicator (tooltip or
icon badge) and a **Reconnect** action.

**File:** `cavewherelib/qml/SyncButton.qml`

```qml
// Bind to the authExpired flag on the project's syncHealth status gadget:
property bool authExpired: RootData.project.syncHealth.status.authExpired

ToolTip.text: authExpired
    ? qsTr("GitHub access expired — click to reconnect")
    : normalTooltipText

onClicked: {
    if (authExpired) {
        RootData.pageSelectionModel.gotoPageByName(null, "Remote")
        remoteAccountCoordinator.startAddGitHubAccount()
    } else {
        RootData.project.sync()
    }
}
```

This lets the user finish what they are doing before re-authenticating. The button's
visual state should reflect `authExpired` (e.g. a warning-coloured icon) so the cue is
visible without being disruptive.

#### 5f. Clone auth failure in `cwRemoteRepositoryCloner`

**File:** `cavewherelib/src/cwRemoteRepositoryCloner.h`

```cpp
Q_PROPERTY(bool cloneFailedDueToAuthError READ cloneFailedDueToAuthError
           NOTIFY cloneFailedDueToAuthErrorChanged)
bool cloneFailedDueToAuthError() const { return m_cloneFailedDueToAuthError; }
```

In `handleCloneWatcherStateChanged()`, detect `HttpAuthFailed` via error code (not
string matching) and set the property. Clear it when a new clone attempt starts.

---

### 6. Extend `PrePushCredentialPayload` for HTTPS remotes

**File:** `QQuickGit/src/GitRepository.cpp`

`prePushRemoteCredentialCallback` (line 723) is used at two sites:
- `hasUnpushedCommitsToRemote()` (line 828) — pre-push read-only check
- `fetchAheadBehindCounts()` (line 3975) — used by `cwProjectSyncHealth` for status polling

Both do `git_remote_connect` over HTTPS if the remote is a GitHub HTTPS URL. Without a
token, they will fail silently or return a misleading auth error.

Add `httpsToken` to `PrePushCredentialPayload`:

```cpp
struct PrePushCredentialPayload {
    int totalMaxAttempts = 8;
    int totalAttempts = 0;
    int sshAgentMaxAttempts = 1;
    int sshAgentAttempts = 0;
    int sshKeyMaxAttempts = 1;
    int sshKeyAttempts = 0;
    QString httpsToken;   // NEW: empty = SSH-only path
};
```

In `prePushRemoteCredentialCallback`, insert before the `GIT_CREDENTIAL_USERPASS_PLAINTEXT`
block (line 746) — it already exists in this callback but only uses URL-embedded
credentials. Replace with token-aware logic:

```cpp
if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) {
    if (prePushPayload && !prePushPayload->httpsToken.isEmpty()) {
        return git_credential_userpass_plaintext_new(
            out, "x-access-token",
            prePushPayload->httpsToken.toUtf8().constData());
    }
    // Fall through to URL-embedded credential check (existing behaviour)
    if (!userNameText.isEmpty() && !urlPassword.isEmpty()) { ... }
}
```

At both call sites, populate the payload before `git_remote_connect`:

```cpp
PrePushCredentialPayload credentialPayload;
credentialPayload.httpsToken = d->m_credentials.httpsToken;   // snapshot on main thread
```

---

### 7. Use HTTPS clone URL from GitHub repo list

**File:** `cavewherelib/qml/RemoteRepositoryPage.qml`

When the user taps a repo in the list (line 637), populate the URL field with `cloneUrl`
(HTTPS) instead of `sshUrl`:

```qml
// was:  manualUrlField.textField.text = sshUrl
manualUrlField.textField.text = cloneUrl
```

---

### 8. Remove "Upload SSH Key" menu item

**File:** `cavewherelib/qml/RemoteRepositoryPage.qml`

Delete (lines 506-510):
```qml
QC.MenuItem {
    text: "Upload SSH Key"
    enabled: !gitHub.busy
    onTriggered: gitHub.uploadPublicKey("")
}
```

`uploadPublicKey()` in `cwGitHubIntegration` can be removed entirely — it is no longer
called from any UI path. Remove the declaration from `cwGitHubIntegration.h`, the
implementation from `.cpp`, and the `Q_INVOKABLE` line.

---

### 9. Add inline "Reconnect" prompt on auth clone failure

**File:** `cavewherelib/qml/RemoteRepositoryPage.qml`

`cwRemoteRepositoryCloner` exposes `cloneFailedDueToAuthError` (set via the typed
`HttpAuthFailed` error code from step 5f — no string matching). Show inline below the
existing `ErrorHelpArea`:

```qml
RowLayout {
    visible: remoteRepositoryCloner.cloneFailedDueToAuthError
             && page.state === "githubAccount"
    Layout.fillWidth: true

    Text {
        text: "GitHub access expired."
        color: Theme.textSecondary
        Layout.fillWidth: true
    }

    QC.Button {
        text: "Reconnect"
        onClicked: {
            page.state = "addAccount"
            remoteAccountCoordinator.startAddGitHubAccount()
        }
    }
}
```

Note: `startAddGitHubAccount()` initiates a new device flow. If an account already exists
with an expired token, verify that this either re-authenticates the existing account or
replaces it cleanly — it should not create a duplicate account entry.

---

## What Is Not Changing

- OAuth device flow — unchanged
- SSH credential path — unchanged (empty `GitCredentials::httpsToken` = existing SSH behaviour)
- `Account` class and RSA key generation — unchanged (still used for non-GitHub SSH remotes)
- `FolderDialog` / clone destination logic — unchanged
- `RemoteAccountCoordinator` / account model — unchanged
- "Forget Account" flow — unchanged (token zeroing via signal is automatic, see step 4)

---

## Testing Checklist

- [ ] First-time OAuth: clone from GitHub repo list succeeds via HTTPS without any SSH key prompt
- [ ] Returning user: token reloaded from keychain on launch, clone works immediately
- [ ] Manual HTTPS URL: clone works with GitHub account selected
- [ ] Manual HTTPS URL with no account: clone fails gracefully with a clear error (not a crash or hang)
- [ ] Manual SSH URL: SSH credential path still works (agent + file-based keys)
- [ ] Push via HTTPS: `git_remote_push` uses token from `SshCallbackPayload`
- [ ] Token revoked: clone fails, "Reconnect" button appears, re-auth restores clone
- [ ] Sync (push + pull) works via HTTPS with token
- [ ] Token refreshed after re-auth: next sync uses new token without app restart (signal connection picks it up)
- [ ] Sync with expired token: `syncAuthFailed` emitted, user notified
- [ ] No GitHub account selected: sync falls back to SSH (empty `httpsToken`)
- [ ] "Upload SSH Key" menu item is gone
- [ ] `GitCredentials` is captured by value before `QtConcurrent::run` — no data race
- [ ] `hasUnpushedCommitsToRemote()` works over HTTPS — `PrePushCredentialPayload.httpsToken` is populated (step 6)
- [ ] `remoteAheadBehindCommitCounts()` works over HTTPS — `PrePushCredentialPayload.httpsToken` is populated (step 6)
- [ ] `remoteAheadBehindCommitCounts()` with expired token returns `HttpAuthFailed` error code; `cwSyncStatus.authExpired` becomes `true`; `SyncButton` shows warning indicator
- [ ] `cwSyncStatus.authExpired` clears to `false` after a successful `remoteAheadBehindCommitCounts()` call (next poll cycle)
- [ ] `startAddGitHubAccount()` re-auth does not create a duplicate account entry
- [ ] #269: HTTPS clone no longer produces a username error
- [ ] QML test: `tst_RemoteRepositoryPage` covers post-OAuth HTTPS clone path

---

## Implementation Order

Steps have hard dependencies. Implement in this sequence to avoid compile errors:

1. **3a** — Add `accessToken()` getter to `cwGitHubIntegration.h`
   _(prerequisite for 3d and 3g; nothing compiles without it)_
2. **1** — Add `httpsToken` field to `SshCallbackPayload`; add `GitCredentials` struct and `setCredentials()` to `GitRepository`; add `m_credentials` to `GitRepositoryData`
   _(prerequisite for 3c)_
3. **3b** — _(done as part of step 2 above)_
4. **3c** — Snapshot `d->m_credentials` before each `QtConcurrent::run` (fetch via `fetchRefsForDirectory` parameter, push and clone by value capture)
5. **3d** — `cwSaveLoad::setGitHubIntegration()` + `m_gitHubIntegration` member
6. **3e** — `cwProject::setGitHubIntegration()` forwarding method
7. **3f** — Wire in `cwRootData` (`Project->setGitHubIntegration(...)`)
8. **3g** — `cwRemoteRepositoryCloner::setGitHubIntegration()` + QML binding
9. **7** — Switch clone URL from `sshUrl` → `cloneUrl` in `RemoteRepositoryPage.qml`
10. **8** — Remove "Upload SSH Key" menu item and `uploadPublicKey()` from `cwGitHubIntegration`
11. **5a** — Add `isHttpAuthFailure()` helper + `HttpAuthFailed` to `GitErrorCode`
    _(prerequisite for 5b, 5d; use at fetch/push/clone + remoteAheadBehindCommitCounts error sites)_
12. **5b** — Add `HttpAuthFailed` to `cwSaveLoad::SyncErrorCode`; re-wrap in sync result handler
13. **5c** — Add `syncAuthFailed` signal to `cwProject`; emit from `completeSyncOperation`
14. **5d** — Add `m_authExpired` to `cwSyncStatus`; detect in `cwProjectSyncHealth`'s `onFutureChanged` lambda
15. **5e** — Wire `authExpired` into `SyncButton.qml` reconnect indicator
16. **6** — Add `httpsToken` to `PrePushCredentialPayload`; snapshot at `hasUnpushedCommitsToRemote` and `remoteAheadBehindCommitCounts` call sites
17. **5f** — Add `cloneFailedDueToAuthError` property to `cwRemoteRepositoryCloner`
18. **9** — Add inline "Reconnect" prompt to `RemoteRepositoryPage.qml`
