# Install-check sync gate — follow-up items

## Context

The GitHub App install pre-flight check was recently moved from
`LinkBar.qml` into `cwProject::sync()` via a virtual interface on
`cwRemoteAuthProvider` (`supportsInstallationCheck()`,
`verifyInstallation()`, `installationVerified(bool)` signal). The
implementation is in place and tested, but a `/simplify` review surfaced
three items that were intentionally left out of that change — each is
worth doing on its own merits but is a scope expansion from the original
refactor. This document captures them for later.

The three items are independent and can be tackled in any order.

---

## 1. Consolidate `MockAuthProvider` / `StubAuthProvider` into `testlib/`

### Problem

Two near-duplicate mock implementations of `cwRemoteAuthProvider` now
exist, both file-scoped and not shared:

- `MockAuthProvider` in `testcases/test_cwRemoteAuthProvider.cpp:28-58`
  — exposes `completeLoad(token)` and `setToken(token)`; tests the
  credentials-loaded path.
- `StubAuthProvider` in `testcases/test_cwProject.cpp:~705-740`
  — adds `supportsInstallationCheck()`, `verifyInstallation()`,
  `setInstalled(bool)`, `setSupportsInstallationCheck(bool)`,
  `verifyCalls()` counter; tests the install-check gate.

Any future test that needs both credentials-flow and install-check
behavior would triple the duplication.

### Plan

1. Create `testlib/MockAuthProvider.{h,cpp}` that exposes the superset
   of both files' APIs:
   - `hasLoadedCredentials()`, `accessToken()`, `ensureCredentialsLoaded()`
     — overrides of the `cwRemoteAuthProvider` base.
   - `completeLoad(token)`, `setToken(token)` — from the existing
     `MockAuthProvider`.
   - `supportsInstallationCheck()`, `verifyInstallation()` — overrides
     that return a caller-controlled bool and emit
     `installationVerified()` via `QMetaObject::invokeMethod` with
     `Qt::QueuedConnection` (the async emission matters — the
     `Qt::SingleShotConnection` in `cwProject::sync()` is observed
     through the event loop).
   - `setInstalled(bool)`, `setSupportsInstallationCheck(bool)`,
     `verifyCalls()` counter — from the new `StubAuthProvider`.
2. Add `CAVEWHERE_TESTLIB_EXPORT` on the class and wire the new files
   into `testlib/CMakeLists.txt` following the existing pattern for
   `LoadProjectHelper` / `TestHelper`.
3. Delete the `MockAuthProvider` definition from
   `test_cwRemoteAuthProvider.cpp` and the `StubAuthProvider`
   definition from `test_cwProject.cpp`; replace both with `#include
   "MockAuthProvider.h"` and update the call-sites.
4. Keep the local `namespace { … }` wrapper for any helpers beyond the
   provider class itself that are still file-scoped.

### Files to touch

- `testlib/MockAuthProvider.h` (new)
- `testlib/MockAuthProvider.cpp` (new)
- `testlib/CMakeLists.txt` — add the new files to the sources list
- `testcases/test_cwRemoteAuthProvider.cpp` — remove local class,
  add include
- `testcases/test_cwProject.cpp` — remove local class, add include

### Verification

- Run `cavewhere-test "[cwProject]"` and
  `cavewhere-test "[cwRemoteAuthProvider]"` — all existing tests must
  pass unchanged.
- Build `cavewhere-test` clean (no unused-class warnings).

### What this does **not** do

- Does not change any test behavior; this is a pure DRY refactor.
- Does not introduce a `FakeAuthProvider` / `RealAuthProvider` split —
  one mock is enough.

---

## 2. Split `/user/installations` fetch from repository listing

### Problem

`cwGitHubIntegration::verifyInstallation()` currently calls
`refreshRepositoriesInternal(true)` to answer the "is the app
installed?" question. `refreshRepositoriesInternal` fetches
`/user/installations`, then iterates each installation and fetches
`/user/installations/{id}/repositories` to populate `m_repositories`.

The sync pre-flight only needs the *first* response. Emitting
`installationVerified(true)` happens immediately after the
installations list is parsed; the per-installation repo fetches then
run in the background for no sync-time benefit. For users with
multiple installations, this is N extra HTTP requests per sync — not
catastrophic (repos are cached locally afterward and the UI may reuse
them), but wasteful on every sync click.

### Plan

1. Add a second private entry point in `cwGitHubIntegration`:
   ```cpp
   void checkInstallationOnly(bool allowRefreshRetry);
   ```
   Implementation mirrors the first half of
   `refreshRepositoriesInternal`: guard on token, build the
   `/user/installations` request, but route the reply to a new slot
   `handleInstallationsCheckReply(QNetworkReply*, bool)`.
2. `handleInstallationsCheckReply` does exactly three things:
   - Handle 401 via `handleUnauthorized` (same retry path).
   - On non-empty response: `setNeedsInstallation(false)`, emit
     `installationVerified(true)`. **Do not** call
     `fetchInstallationRepositories`.
   - On empty response: `setNeedsInstallation(true)`, emit
     `installationVerified(false)`.
3. Change `verifyInstallation()` to call `checkInstallationOnly(true)`
   instead of `refreshRepositoriesInternal(true)`.
4. Leave `refreshRepositories()` / `refreshRepositoriesInternal` /
   `handleInstallationsReply` untouched — the UI still needs the full
   repo list when the user opens Remote Settings.

### Semantic change to be aware of

Before this change, every sync click left `m_repositories` populated
with a fresh copy of the installation's repos as a side effect.
Downstream UI that reads `m_repositories` was getting a free refresh
on every sync. After this change, `m_repositories` is only refreshed
by an explicit `refreshRepositories()` call (e.g., the Remote Settings
page opening, or the SyncButton popup invoking it). Audit:

- `RemoteRepositoryPage.qml` — calls `refreshRepositories()` on
  page activation. Unaffected.
- `SetupRemoteWizard.qml` — calls it on wizard open. Unaffected.
- `SyncButton.qml` — previously relied on the side effect? Check the
  `GitHubInstallAppPopup` / `RemoteRepositoryPage` flows; if any
  QML reads `gitHubIntegration.repositories` without calling
  `refreshRepositories()` first, add the explicit call.

### Files to touch

- `cavewherelib/src/cwGitHubIntegration.h` — new private method
  declarations: `checkInstallationOnly`,
  `handleInstallationsCheckReply`.
- `cavewherelib/src/cwGitHubIntegration.cpp` — implementation; change
  the one-line body of `verifyInstallation()`.
- Audit the QML layer for implicit reliance on the old side effect
  (grep `gitHubIntegration.repositories`).

### Tests

- Add to `testcases/test_cwGitHubIntegration.cpp`:
  - `verifyInstallation does not fetch repositories on success` —
    uses `PathRoutedMockServer` with `/user/installations` returning
    one installation and **no** route for
    `/user/installations/{id}/repositories`. Asserts that
    `installationVerified(true)` fires and
    `repositoryCount() == 0` (or however that's exposed). A route
    miss would otherwise 404 and show up as a network error.
  - Confirm existing `refreshRepositories` tests still pass — they
    exercise the full path via `refreshRepositories()`, not
    `verifyInstallation()`.
- No changes needed in `test_cwProject.cpp` — the stub provider is
  orthogonal to the real provider's network-layer refactor.

### Verification

- `cavewhere-test "[cwGitHubIntegration]"` — 13 existing cases pass,
  new case passes.
- Manual: open Remote Settings, confirm repo list still populates.
  Click Sync repeatedly while watching network requests (Charles /
  Little Snitch) — should see 1 HTTP request per sync (installations
  only), not 1+N.

### What this does **not** do

- Does not cache installation state between syncs. Re-checking on
  every sync is intentional — the user may have uninstalled the
  GitHub App since the last sync click.
- Does not change the `needsInstallation` property semantics or the
  `setNeedsInstallation` no-op guard.

---

## 3. Timeout guard on `installationVerified()`

### Problem

`cwProject::continueSyncAfterGates()` uses
`Qt::SingleShotConnection` to wait for one
`installationVerified(bool)` emission and then proceed. If the
provider starts the check but never emits (process killed mid-flight,
unresponsive network stack, a subclass bug), the `deferredSync`
shared_ptr is held by the connection lambda indefinitely, the
`SyncFuture` stays `Running`, and the UI's `syncInProgress` sticks at
`true` until the user kills the app.

`cwGitHubIntegration::supportsInstallationCheck() =
!m_accessToken.isEmpty()` prevents the common stall case (no token →
gate skipped entirely), so this is a genuine edge case, not a
near-term risk. But it's the only gate in `sync()` without a
timeout, and the credentials-loaded gate has the same shape and
should get the same treatment for consistency.

### Plan

1. In `cwProject::continueSyncAfterGates()`, wrap the deferred
   connection with a `QTimer::singleShot` (e.g. 15s) that fires
   `installationVerified(false)` equivalently on timeout:
   ```cpp
   auto watchdog = new QTimer(this);
   watchdog->setSingleShot(true);
   watchdog->setInterval(15000);
   connect(watchdog, &QTimer::timeout, this, [this, deferredSync, watchdog]() {
       deferredSync->complete();
       emit syncInProgressChanged();
       emit syncNeedsInstallation();   // same fallback as installed=false
       watchdog->deleteLater();
   });
   // In the installationVerified lambda: watchdog->deleteLater() on entry.
   watchdog->start();
   provider->verifyInstallation();
   ```
2. Do the same for the credentials-loaded branch in `sync()` — same
   failure mode, same fix. The fallback emission there is
   `authProviderCredentialsNeeded()` again (or a new
   `syncCredentialsTimedOut()` signal if it matters to distinguish;
   probably doesn't).
3. Decide on the user-visible behavior on timeout:
   - **Install check timeout** → treat as `installed=false`, popup
     opens. User retries; the retry either succeeds or times out
     again. Acceptable.
   - **Credentials-loaded timeout** → the sync silently drops. May
     want to surface an error via `ErrorModel` so the user knows
     something went wrong; alternatively, the keychain read should
     not be able to take 15s in practice, so treat the timeout as a
     bug indicator and log it.

### Files to touch

- `cavewherelib/src/cwProject.cpp` — add watchdogs to both gates.
- `cavewherelib/src/cwProject.h` — no new signals unless the
  credentials-timeout path needs its own.

### Tests

- `test_cwProject.cpp` — extend `StubAuthProvider` with a
  `setDelayInstallationVerified(ms)` flag. Add:
  - `sync() emits syncNeedsInstallation when install check times
    out` — stub never emits; advance time via
    `QTest::qWait(16000)` or mock the timer. Asserts the fallback
    signal fires within the timeout window + grace.
  - Match test for the credentials-loaded timeout.

### Verification

- `cavewhere-test "[cwProject][installCheck]"` — existing three
  cases pass, new case passes.
- Manual: not easy to trigger in production; this is a safety net.

### What this does **not** do

- Does not add a retry mechanism — on timeout, the user clicks Sync
  again.
- Does not set a timeout on the underlying `QNetworkRequest`; that's
  a separate concern (the HTTP request may already have its own
  timeout from Qt defaults).

---

## Critical files (across all three items)

- `cavewherelib/src/cwGitHubIntegration.{h,cpp}` — item 2
- `cavewherelib/src/cwProject.{h,cpp}` — item 3
- `cavewherelib/src/cwRemoteAuthProvider.h` — context only; no
  changes needed
- `testlib/MockAuthProvider.{h,cpp}` — item 1 (new files)
- `testlib/CMakeLists.txt` — item 1
- `testcases/test_cwRemoteAuthProvider.cpp` — item 1
- `testcases/test_cwProject.cpp` — item 1, item 3
- `testcases/test_cwGitHubIntegration.cpp` — item 2
