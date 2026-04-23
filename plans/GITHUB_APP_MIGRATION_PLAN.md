# Migrate CaveWhere GitHub integration from OAuth App to GitHub App

## Context

CaveWhere currently uses a GitHub **OAuth App** (`cwGitHubIntegration.cpp:26`)
with the OAuth Device Flow. Users who authenticate the
same GitHub account on multiple devices (desktop + iPhone) observe a "last device
wins" pattern: whoever completed device flow most recently has a working token; the
previous device's token stops working.

Root cause investigation:

1. **Confirmed via runtime diagnostic**: GitHub's token response for this OAuth App
   contains only `access_token`, `scope`, `token_type` — **no** `refresh_token` or
   `expires_in`. So this is *not* a mishandled-refresh-token problem.
2. **Confirmed with user**: The app is registered as an OAuth App (not a GitHub
   App), and OAuth Apps do not have an "Expire user authorization tokens" toggle
   (that setting exists only on GitHub Apps). So the "just flip a setting" fix
   does not apply.
3. **Remaining explanation**: GitHub rotates/revokes the OAuth App user token on
   new device-flow completions for the same `(app, user)` pair. Only one live
   token per pair.

The only durable fix is to migrate to a **GitHub App**, which issues distinct
user-access tokens per device-flow completion and supports refresh tokens.

## Scope decisions

- **Keep device flow**; don't switch to browser redirect. Desktop + iPhone both
  work with device flow today; redirect adds OS-specific callback handling.
- **Straight swap of the client ID**. The OAuth App path is deleted outright, no
  feature flag or dual code path. We're in a dev environment — no shipped users
  to migrate, no dual-support period needed.
- **LFS auth header unchanged**. `cwGitHubLfsAuthProvider.cpp:48-70` uses the
  token as a password in `x-access-token:{token}` Basic auth — this works
  identically with GitHub App user tokens.

## GitHub-side setup (do first, before code)

1. Register a new **GitHub App** at github.com/settings/apps/new.
   - Homepage URL: the CaveWhere project URL.
   - Webhook: disable.
   - Permissions (Repository):
     - `Contents`: Read and write
     - `Metadata`: Read-only (implicit)
   - Permissions (Account): none needed for basic flow; add `Email addresses:
     Read` only if CaveWhere actually uses the email.
   - User permissions: leave empty unless a specific endpoint needs it.
   - Enable **Device Flow** (Optional features → Device Flow).
   - Leave "Expire user authorization tokens" at the default (enabled).
   - "Where can this GitHub App be installed?": Any account.
2. Note the new `client_id` (starts with `Iv23...`).
3. Note the app's public slug (used in install URLs:
   `https://github.com/apps/<slug>/installations/new`).

## Code changes

### 1. Device flow: parse refresh token and expiry

**`cavewherelib/src/cwGitHubDeviceAuth.h`** — extend `AccessTokenResult`:

```cpp
QString refreshToken;         // empty for OAuth App, populated for GitHub App
qint64 expiresInSec = -1;     // seconds until access token expires; -1 if absent
qint64 refreshExpiresInSec = -1;
```

**`cwGitHubDeviceAuth.cpp:162-165`** — populate the new fields from the response
JSON (`refresh_token`, `expires_in`, `refresh_token_expires_in`).

### 2. Keychain: store the refresh token + expiry

**`cavewherelib/src/cwRemoteCredentialStore.{h,cpp}`** — add companion entries:

- Current key: `RemoteAccount/{provider}/{accountId}/AccessToken`
- Add: `RemoteAccount/{provider}/{accountId}/RefreshToken`
- Add: `RemoteAccount/{provider}/{accountId}/AccessTokenExpiresAt` (store absolute
  epoch seconds, computed at save time)

Mirror the existing `writeAccessToken` / `readAccessToken` / `clearStoredAccessToken`
pattern (`cwRemoteCredentialStore.cpp:15-101`) for the new keys. Keep the three
writes atomic enough that we don't end up with half-written credentials on failure
(existing code is best-effort; match that standard, don't regress it).

### 3. Integration: refresh flow

**`cwGitHubIntegration.cpp`**:

- On successful device-flow completion
  (`persistCurrentAccessTokenForAccount`, roughly line 242-255): if the response
  included a refresh token, persist access + refresh + expiry together.
- Add `refreshAccessToken(accountId)`:
  - POST `https://github.com/login/oauth/access_token` with `client_id`,
    `grant_type=refresh_token`, `refresh_token=<stored>`.
  - On success, overwrite keychain entries (GitHub issues a new refresh token each
    time — old one is invalidated after use).
  - On failure (`bad_refresh_token`, 401), clear credentials and force re-auth via
    the device flow.
- In `invalidateActiveAccountToken()` (`cwGitHubIntegration.cpp:536-560`) and the
  three 401 call-sites (lines 449, 484, 615): **before** clearing and forcing
  re-auth, first try `refreshAccessToken`. Only clear if refresh fails. Use
  `AsyncFuture::observe(...).context(this, ...)` per CLAUDE.md (no
  `waitForFinished` in production).
- Pre-emptive refresh: when the app loads a stored access token on startup
  (`loadStoredAccessToken` around line 379-416), check `AccessTokenExpiresAt`. If
  it's within 60s of expiry or past, refresh before using.

### 4. Repo listing: installation-aware

**`cwGitHubIntegration.cpp:145-157`** (`refreshRepositories`) and the response
handler at lines 479-517:

- Replace `GET /user/repos` with:
  1. `GET /user/installations` → list of installations the user has authorized
     for this GitHub App.
  2. If list is empty → emit a new signal / set state to `NeedsInstallation`, so
     the QML can surface the "install the app" prompt. **Do not** clear
     credentials — the user is authenticated, they just haven't installed the
     app on any account.
  3. For each installation:
     `GET /user/installations/{installation_id}/repositories` → aggregate into
     the existing repo list structure.
- `createRepository(..., org)` retains the `POST /orgs/{org}/repos` endpoint —
  that creates a repo in an org's namespace and works with GitHub App user
  tokens when the app is installed on the org. It is a *create* endpoint, not a
  listing endpoint, so it isn't made redundant by the installation-aware flow.

### 5. QML: "Install the app" state

**`cavewherelib/qml/GitHubAuthFlow.qml`** — add a new state `NeedsInstallation`:

- Triggered by the new signal from `cwGitHubIntegration`.
- Shows a button "Install CaveWhere on GitHub" that opens
  `https://github.com/apps/<slug>/installations/new` via `Qt.openUrlExternally`.
- Includes a "I've installed it — continue" button that re-runs
  `refreshRepositories()`. No polling loop — user confirms manually, matching the
  pattern already used for device code entry.

Other QML (`SetupRemoteWizard.qml`, `ReconnectPopup.qml`, `ConnectRemoteForm.qml`)
consume states from `cwGitHubIntegration` already; they need to be aware of
`NeedsInstallation` at minimum so they don't treat it as an error.

### 6. Client ID

**`cwGitHubIntegration.cpp:26`** — replace `DefaultGitHubClientId` with the new
GitHub App's `Iv23...` client ID. No dual-mode gate, no detection logic. The
existing `CAVEWHERE_GITHUB_CLIENT_ID` env-var override in
`cwGitHubIntegration.cpp:654-661` stays as-is (useful for local testing against a
personal GitHub App before committing the default).

## Verification

**Build:**
```
cmake --build build/Qt_6_11_0_for_macOS-Debug --target CaveWhere cavewhere-test cavewhere-qml-test
```

**Automated (narrow):**

- Extend `testcases/test_cwGitHubDeviceAuth.cpp` to cover parsing the new
  response fields when present and when absent (backward compat).
- Add a test for `cwRemoteCredentialStore` round-tripping refresh token + expiry.
- Add a test for `refreshAccessToken` — mock the HTTP layer if feasible,
  otherwise unit-test the request-building helper in isolation.

**Manual (cannot be automated without a live GitHub account):**

1. Start with a clean keychain for the CaveWhere service.
2. Run device flow on desktop → complete in browser → install app on a test
   account when prompted → confirm repos appear.
3. Run device flow on a second device (or simulate by wiping local keychain)
   with the same GitHub account → install prompt should already be satisfied
   (app is installed) → repos appear.
4. **Regression test for the original bug**: after step 3, return to desktop and
   verify the desktop's token still works (make an API call, e.g. open/refresh
   a repo). If it does, the migration resolves the bug.
5. Wait 8+ hours (or force expiry by editing the stored epoch) → verify the app
   refreshes silently on next API call without prompting for re-auth.
6. Revoke the app from https://github.com/settings/installations and confirm 401
   handling now prompts re-auth / re-install cleanly.

## Critical files

- `cavewherelib/src/cwGitHubIntegration.{h,cpp}` — client ID, refresh flow,
  installation-aware repo listing, 401 handling
- `cavewherelib/src/cwGitHubDeviceAuth.{h,cpp}` — parse new response fields
- `cavewherelib/src/cwRemoteCredentialStore.{h,cpp}` — extended keychain schema
- `cavewherelib/qml/GitHubAuthFlow.qml` — new `NeedsInstallation` UI state
- `cavewherelib/qml/SetupRemoteWizard.qml`,
  `cavewherelib/qml/ReconnectPopup.qml`,
  `cavewherelib/qml/ConnectRemoteForm.qml` — handle `NeedsInstallation` state
- `testcases/test_cwGitHubDeviceAuth.cpp` — new parse tests
- `cavewherelib/src/cwGitHubLfsAuthProvider.cpp` — **no change**, confirm still
  works with GitHub App user tokens in manual test

## What this plan deliberately does **not** do

- Does not introduce a new auth path (web redirect) — device flow stays.
- Does not keep OAuth App support. The old code path is deleted outright.
- Does not add per-repo fine-grained scope UI beyond what "install on account
  with selected repos" already gives you via GitHub's own install page.
- Does not handle GitHub Enterprise Server endpoints — existing code already
  doesn't, and the migration scope stays the same.
