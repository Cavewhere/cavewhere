# Issue #490 — Friendlier 404 / No-Access Error on Clone

## Context

When a user pastes a CaveWhere share link or repo URL into the Clone dialog
and their account doesn't have access to the target repository (or the repo
doesn't exist), the hosting service responds with HTTP 404. On GitHub this
surfaces from libgit2 as:

```
unexpected http status code: 404
```

Today this raw string is shown directly in `ErrorHelpArea` inside
`CloneArea.qml`, which is unhelpful — a first-time user has no idea that
hosting services intentionally return the same 404 for both "the repo
doesn't exist" and "you don't have access" (so private repos cannot be
enumerated).

Along with fixing this specific message, two structural problems make the
fix worth doing properly rather than piecemeal:

1. **The cloner's error-surfacing API is awkward.** A parallel boolean
   `cloneFailedDueToAuthError` gates auth-specific behavior, and the
   user-facing wording lives in QML (`CloneArea.qml::authErrorMessage`,
   overridden per caller). Adding a second case (404) would mean a second
   boolean and a second QML override.

2. **Host-specific knowledge is scattered and GitHub-flavored.** Today
   `cwDeepLinkHandler` has `hostDisplayName`, `collaboratorSettingsUrl`,
   and `allowedHosts` enumerating `github.com` / `gitlab.com` /
   `bitbucket.org`. A naive 404 message written in the cloner would bake
   "GitHub" and "collaborator" / "invitation" terminology into a sentence
   that the deep-link handler is happy to send to a Bitbucket or GitLab
   user.

This plan does:

1. Introduces a **`cwGitHostingProvider` namespace** holding all per-host
   knowledge as a small data table (`cwGitHostingProviderInfo` struct +
   array). Free functions consume the info to build URLs and friendly
   messages. The same algorithm runs against any host — only the data
   differs.
2. Replaces the cloner's parallel boolean with a single **`CloneErrorKind`
   enum** (`None` / `Auth` / `NotFoundOrAccess` / `HostUnreachable` /
   `Other`).
3. Moves user-facing message construction into C++ via the provider
   namespace.
4. Migrates existing host-specific lookups in `cwDeepLinkHandler` to
   delegate to the provider registry, so there's a single source of truth.

The QQuickGit submodule is **not** touched — classification lives one
layer up.

**On abstraction depth.** A polymorphic interface + concrete subclasses
was considered. Today every "provider" differs only in *data* (strings,
path suffixes) — no per-host algorithm divergence — so polymorphism would
add ~100 lines of class boilerplate without earning its keep. If a future
host (e.g. self-hosted GitLab with subgroup paths, Gitea/Forgejo) needs
a genuinely different algorithm, promoting the table to a polymorphic
interface is a ~30-minute mechanical refactor.

**On translation.** This plan does **not** wire `tr()` / lupdate for
the new strings. CaveWhere doesn't ship translations today, and the
existing pattern in `cwRemoteRepositoryCloner.cpp` and
`cwDeepLinkHandler.cpp` is `QStringLiteral` for C++ user-facing
strings (QML continues to use `qsTr` for its own text). We match that
convention. If translation is added later, this file is one of many
that will need the `tr()` retrofit; not worth doing piecemeal here.

## Files

### New

| File | Purpose |
|------|---------|
| `cavewherelib/src/cwGitHostingProvider.h` | `cwGitHostingProviderInfo` struct + namespace declarations |
| `cavewherelib/src/cwGitHostingProvider.cpp` | Provider table + algorithm functions |
| `testcases/test_cwGitHostingProvider.cpp` | Unit tests for the table and algorithms (verify the `testcases/` naming convention with `ls testcases/` before committing — adjust to match) |

### Modified

| File | Why |
|------|-----|
| `cavewherelib/src/cwRemoteRepositoryCloner.h` | Add `CloneErrorKind` enum + new properties/signals; remove `cloneFailedDueToAuthError` |
| `cavewherelib/src/cwRemoteRepositoryCloner.cpp` | Classify the error in `handleCloneWatcherStateChanged`; build friendly text + action URL via the provider namespace |
| `cavewherelib/src/cwDeepLinkHandler.h` / `.cpp` | Delegate `hostDisplayName`, `collaboratorSettingsUrl`, `allowedHosts`, `isHostAllowed` to the provider namespace (or remove if `grep` shows no callers — verify before deleting) |
| `cavewherelib/qml/CloneArea.qml` | Drop `authErrorMessage`; render `cloneErrorFriendlyMessage` (or fallback to raw); add "Details:" line; wire `linkActivated` |
| `cavewherelib/qml/DeepLinkConfirmDialog.qml` | Replace `cloneAreaId.cloneFailedDueToAuthError` reads with the enum comparison; drop the `authErrorMessage` assignment |
| `cavewherelib/qml/HelpArea.qml` | Forward `linkActivated` from the inner `QC.Label` so the embedded link works |
| `cavewherelib/CMakeLists.txt` | Add the two new source files (and the test file at the relevant test-target list — verify with `grep test_cw cavewherelib/CMakeLists.txt testcases/CMakeLists.txt` to find which target lists `testcases/*.cpp`) |

## Step 1 — Define `cwGitHostingProvider` as a data table

### Header (`cwGitHostingProvider.h`)

```cpp
#pragma once

#include "CaveWhereLibExport.h"
#include <QString>
#include <QStringList>
#include <QUrl>

struct cwGitHostingProviderInfo {
    QString host;                   // canonical lowercase, e.g. "github.com" (empty for generic fallback)
    QString displayName;            // "GitHub" (empty for generic)
    QString collaboratorPathSuffix; // "/settings/access" / "/-/project_members" / "/admin/access-keys" (empty for generic)
    QString openLinkLabel;          // "Open repository on GitHub" / etc.
    QString authMessage;            // shown when an auth error happens during clone
    QString notFoundMessage;        // shown when a 404 happens during clone (no <a>; link added by helper)
};

namespace cwGitHostingProvider {
    // Lookup. Returns a reference to the matching entry, or to the generic
    // fallback entry if no host matches. Never returns null/dangling.
    const cwGitHostingProviderInfo& forHost(const QString& host);
    const cwGitHostingProviderInfo& forUrl(const QUrl& cloneUrl);

    // Host strings of non-generic entries (used by deep-link allowlist).
    QStringList knownHosts();

    // Derived URLs. Both work for https:// and ssh://... (the normalized
    // form produced by cwRemoteRepositoryCloner::normalizeCloneUrl). Return
    // empty QUrl on failure or for the generic info.
    QUrl repositoryWebUrl(const cwGitHostingProviderInfo& info, const QUrl& cloneUrl);
    QUrl collaboratorSettingsUrl(const cwGitHostingProviderInfo& info, const QUrl& repoUrl);

    // Friendly RichText composition. Returns notFoundMessage with an
    // embedded <a href="..."> link suffix when repositoryWebUrl can be
    // derived; otherwise the plain notFoundMessage. The link rendering
    // is intentionally kept in this helper (not in the data) so the
    // table holds plain strings only.
    QString notFoundOrAccessMessage(const cwGitHostingProviderInfo& info, const QUrl& cloneUrl);
}
```

Both URL-input functions take `QUrl` (a small consistency win over the
previous draft's mix of `QString` and `QUrl`). The cloner's
`m_pendingCloneUrl` is already normalized, and `QUrl("ssh://git@github.com/owner/repo.git")`
parses cleanly to host `github.com`.

### Implementation (`cwGitHostingProvider.cpp`)

The table holds `QString` members built from `QStringLiteral`. Because
`QString` isn't a literal type for `constexpr` purposes, the table is
a function-local `static const std::array` (Meyers singleton — built
once, on first call, with thread-safe initialization):

```cpp
namespace {
    const std::array<cwGitHostingProviderInfo, 4>& providers()
    {
        static const std::array<cwGitHostingProviderInfo, 4> kProviders = {{
            {
                QStringLiteral("github.com"),
                QStringLiteral("GitHub"),
                QStringLiteral("/settings/access"),
                QStringLiteral("Open repository on GitHub"),
                QStringLiteral(
                    "Your GitHub credentials are invalid or have expired. "
                    "Sign in to GitHub to retry cloning."),
                QStringLiteral(
                    "This repository doesn't exist, or your GitHub account "
                    "doesn't have access to it. If you were invited as a "
                    "collaborator, check that you've accepted the pending "
                    "invitation.")
            },
            {
                QStringLiteral("gitlab.com"),
                QStringLiteral("GitLab"),
                QStringLiteral("/-/project_members"),
                QStringLiteral("Open project on GitLab"),
                QStringLiteral(
                    "Your GitLab credentials are invalid or have expired. "
                    "Sign in to GitLab to retry cloning."),
                QStringLiteral(
                    "This project doesn't exist, or your GitLab account "
                    "doesn't have access to it. If you were added as a "
                    "project member, check that you've accepted any "
                    "pending project invitations.")
            },
            {
                QStringLiteral("bitbucket.org"),
                QStringLiteral("Bitbucket"),
                QStringLiteral("/admin/access-keys"),
                QStringLiteral("Open repository on Bitbucket"),
                QStringLiteral(
                    "Your Bitbucket credentials are invalid or have expired. "
                    "Sign in to Bitbucket to retry cloning."),
                QStringLiteral(
                    "This repository doesn't exist, or your Bitbucket "
                    "account doesn't have access to it. If you were invited "
                    "to a workspace or repository, check that you've "
                    "accepted any pending invitations.")
                // Bitbucket vocabulary: re-check "workspace" against current
                // Bitbucket UX before final commit; the term may have evolved.
            },
            {
                /* host */                  QString(),
                /* displayName */           QString(),
                /* collaboratorPathSuffix */ QString(),
                QStringLiteral("Open repository in browser"),
                QStringLiteral(
                    "Your credentials are invalid or have expired. "
                    "Sign in again to retry cloning."),
                QStringLiteral(
                    "This repository doesn't exist, or your account "
                    "doesn't have access to it. If you were invited, "
                    "check that you've accepted any pending invitations.")
            }
        }};
        return kProviders;
    }

    const cwGitHostingProviderInfo& genericInfo() { return providers().back(); }
}
```

`QStringLiteral` stores the UTF-16 backing data in the binary's
read-only segment; QString construction at first use is just a
pointer-aliasing operation, no allocation.

`forHost` is a case-insensitive linear scan (4 entries; the last is the
generic fallback that always matches):

```cpp
const cwGitHostingProviderInfo& cwGitHostingProvider::forHost(const QString& host)
{
    const QString lower = host.toLower();
    for (const auto& info : providers()) {
        if (!info.host.isEmpty() && info.host == lower) return info;
    }
    return genericInfo();
}

const cwGitHostingProviderInfo& cwGitHostingProvider::forUrl(const QUrl& cloneUrl)
{
    return forHost(cloneUrl.host());
}

QStringList cwGitHostingProvider::knownHosts()
{
    QStringList out;
    for (const auto& info : providers()) {
        if (!info.host.isEmpty()) out << info.host;
    }
    return out;
}
```

The two URL builders share the same path-normalization logic, branching
only on whether the info has a host (concrete) or not (generic):

```cpp
namespace {
    QString trimRepoPath(QString path) {
        if (path.endsWith(QStringLiteral(".git"))) path.chop(4);
        while (path.endsWith(QLatin1Char('/'))) path.chop(1);
        return path;
    }
}

QUrl cwGitHostingProvider::repositoryWebUrl(const cwGitHostingProviderInfo& info,
                                            const QUrl& cloneUrl)
{
    if (!cloneUrl.isValid()) return {};

    const QString path = trimRepoPath(cloneUrl.path());
    if (path.isEmpty()) return {};

    QUrl out;
    out.setScheme(QStringLiteral("https"));

    if (info.host.isEmpty()) {
        // Generic: best-effort, only safe for https inputs.
        if (cloneUrl.scheme() != QLatin1String("https")) return {};
        out.setHost(cloneUrl.host());
    } else {
        // Defensive: never construct a github URL for a gitlab clone URL.
        // Callers should always pair info with cloneUrl via forUrl(), but this
        // guards against misuse.
        if (cloneUrl.host().toLower() != info.host) return {};
        out.setHost(info.host);
    }
    out.setPath(path);
    return out;
}

QUrl cwGitHostingProvider::collaboratorSettingsUrl(const cwGitHostingProviderInfo& info,
                                                   const QUrl& repoUrl)
{
    if (info.collaboratorPathSuffix.isEmpty()) return {};
    const QUrl webUrl = repositoryWebUrl(info, repoUrl);
    if (webUrl.isEmpty()) return {};
    QUrl out = webUrl;
    out.setPath(webUrl.path() + info.collaboratorPathSuffix);
    return out;
}
```

The friendly-message composer keeps link rendering out of the data
table:

```cpp
QString cwGitHostingProvider::notFoundOrAccessMessage(const cwGitHostingProviderInfo& info,
                                                     const QUrl& cloneUrl)
{
    const QUrl webUrl = repositoryWebUrl(info, cloneUrl);
    if (webUrl.isEmpty()) return info.notFoundMessage;

    return info.notFoundMessage
        + QStringLiteral(" <a href=\"%1\">%2</a>")
              .arg(webUrl.toString().toHtmlEscaped(),
                   info.openLinkLabel.toHtmlEscaped());
}
```

### Migration of `cwDeepLinkHandler`

Before editing, verify call sites:

```
grep -rn "hostDisplayName\|collaboratorSettingsUrl\|allowedHosts\|isHostAllowed" \
    cavewherelib/ test-qml/ testcases/
```

- If `hostDisplayName` or `collaboratorSettingsUrl` have **no callers
  outside `cwDeepLinkHandler` itself and tests**, delete them. No need
  to keep a shim for a dead function.
- Otherwise, keep the existing signatures and have them delegate:
  ```cpp
  QString cwDeepLinkHandler::hostDisplayName(const QUrl& url) {
      return cwGitHostingProvider::forUrl(url).displayName;
  }
  QUrl cwDeepLinkHandler::collaboratorSettingsUrl(const QUrl& repoUrl) {
      return cwGitHostingProvider::collaboratorSettingsUrl(
          cwGitHostingProvider::forUrl(repoUrl), repoUrl);
  }
  ```
- `allowedHosts()` returns `cwGitHostingProvider::knownHosts()`; drop
  the duplicated static `QStringList`.
- `isHostAllowed(host)` is unchanged in shape; it consumes
  `allowedHosts()` so it benefits automatically.

## Step 2 — Replace the cloner's boolean with `CloneErrorKind`

### Header (`cwRemoteRepositoryCloner.h`)

```cpp
class CAVEWHERE_LIB_EXPORT cwRemoteRepositoryCloner : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RemoteRepositoryCloner)

public:
    enum CloneErrorKind {        // plain enum, not enum class — see note below
        None,
        Auth,
        NotFoundOrAccess,
        HostUnreachable,
        Other
    };
    Q_ENUM(CloneErrorKind)

    Q_PROPERTY(CloneErrorKind cloneErrorKind
               READ cloneErrorKind NOTIFY cloneErrorKindChanged)
    Q_PROPERTY(QString cloneErrorFriendlyMessage
               READ cloneErrorFriendlyMessage
               NOTIFY cloneErrorFriendlyMessageChanged)
    // cloneErrorMessage (raw libgit2 string) is kept — powers the "Details:" line.
    // No separate cloneErrorActionUrl Q_PROPERTY: the link target is embedded
    // in cloneErrorFriendlyMessage's <a href> for NotFoundOrAccess, and the
    // QML side handles clicks via Qt.openUrlExternally. If a caller ever needs
    // the URL programmatically (e.g. a "View on GitHub" button), expose it then.
    // ...
};
```

**Why plain `enum` and not `enum class`:** Qt's QML registration is
straightforward and unambiguous for plain enums (`RemoteRepositoryCloner.Auth`
works directly). With `enum class` + `Q_ENUM`, QML exposure depends on Qt
version and registration shape and may require `RemoteRepositoryCloner.CloneErrorKind.Auth`.
Before the final commit, grep the codebase for an existing
`enum class ... Q_ENUM` pattern that's consumed from QML:
```
grep -rn "Q_ENUM" cavewherelib/src/ | head -20
```
If the codebase already uses `enum class` with `Q_ENUM` from QML successfully, follow that pattern. Otherwise stick to plain `enum`. The values are scoped under the class regardless.

Remove `cloneFailedDueToAuthError` Q_PROPERTY, getter, setter, member,
and `cloneFailedDueToAuthErrorChanged()` signal. Replace
`m_cloneFailedDueToAuthError` with:

```cpp
CloneErrorKind m_cloneErrorKind = None;
QString m_cloneErrorFriendlyMessage;
```

Add a private helper `void applyCloneError(CloneErrorKind kind);`.

### Implementation (`cwRemoteRepositoryCloner.cpp`)

Magic-string constants in an anonymous namespace per project convention:

```cpp
namespace {
    constexpr auto kHttp404Token = "404";

    // Substrings libgit2 / libcurl produce for DNS, refused, timeout failures.
    // Matched case-insensitively against the raw error message.
    constexpr auto kUnreachableTokens = std::array{
        "resolve",   // "failed to resolve address" / "could not resolve host"
        "connect",   // "failed to connect to ..." / "connection refused"
        "timed out",
        "timeout"
    };
}
```

Classify inside the `hasError()` branch of
`handleCloneWatcherStateChanged` (replacing today's lines 256–271):

```cpp
const auto result = m_cloneWatcher->future().result();
const QString rawMessage = m_cloneWatcher->errorMessage();

const bool isAuthError = result.errorCode()
    == static_cast<int>(QQuickGit::GitRepository::GitErrorCode::HttpAuthFailed);
const bool isNotFoundOrAccess = !isAuthError
    && rawMessage.contains(QLatin1String(kHttp404Token));
const bool isHostUnreachable = !isAuthError && !isNotFoundOrAccess
    && std::any_of(kUnreachableTokens.begin(), kUnreachableTokens.end(),
                   [&](const char* tok) {
                       return rawMessage.contains(QLatin1String(tok),
                                                  Qt::CaseInsensitive);
                   });

CloneErrorKind kind = Other;
if (isAuthError)             kind = Auth;
else if (isNotFoundOrAccess) kind = NotFoundOrAccess;
else if (isHostUnreachable)  kind = HostUnreachable;

setCloneErrorMessage(rawMessage);
applyCloneError(kind);
setCloneStatusMessage(QString());
// ... pendingCloneDir cleanup as before ...
if (kind != Auth) {
    m_pendingCloneUrl.clear();
    m_pendingCloneParentDir = QUrl();
}
return;
```

`applyCloneError` looks up the provider info once and dispatches on kind:

```cpp
void cwRemoteRepositoryCloner::applyCloneError(CloneErrorKind kind)
{
    const QUrl pendingUrl(m_pendingCloneUrl);
    const auto& info = cwGitHostingProvider::forUrl(pendingUrl);

    QString friendly;

    switch (kind) {
    case None:
    case Other:
        break;
    case Auth:
        friendly = info.authMessage;
        break;
    case NotFoundOrAccess:
        friendly = cwGitHostingProvider::notFoundOrAccessMessage(info, pendingUrl);
        break;
    case HostUnreachable: {
        const QString host = pendingUrl.host();
        const QString hostDisplay = host.isEmpty() ? QStringLiteral("the server") : host;
        friendly = QStringLiteral(
            "Couldn't reach %1. Check your internet connection and "
            "that the repository URL is correct.").arg(hostDisplay);
        break;
    }
    }

    setCloneErrorKind(kind);
    setCloneErrorFriendlyMessage(friendly);
}
```

`HostUnreachable` deliberately doesn't use the provider — the bare
hostname is the right thing to show, and the message phrasing is
provider-agnostic.

`resetCloneState()` becomes:
```cpp
setCloneErrorMessage(QString());
setCloneStatusMessage(QString());
applyCloneError(None);
```

Also update the `setGitHubIntegration` lambda at
`cwRemoteRepositoryCloner.cpp` lines 55–67: replace the read of
`m_cloneFailedDueToAuthError` with `m_cloneErrorKind == Auth`. This
drives the inline-auth auto-retry in `DeepLinkConfirmDialog` — a
regression-critical edit, must be verified by manual test in Step 5.

## Step 3 — Update `CloneArea.qml`

- Delete `property string authErrorMessage` (line 17) and its `qsTr`
  default.
- Replace `readonly property bool cloneFailedDueToAuthError` (line 22)
  with a forwarder so callers can read the kind directly:
  ```qml
  readonly property int cloneErrorKind: clonerId.cloneErrorKind
  ```
- Replace the `ErrorHelpArea` block (lines 135–140) with:

  ```qml
  ErrorHelpArea {
      objectName: "remoteCloneErrorArea"
      Layout.fillWidth: true
      text: clonerId.cloneErrorFriendlyMessage.length > 0
            ? clonerId.cloneErrorFriendlyMessage
            : clonerId.cloneErrorMessage
      visible: clonerId.cloneErrorKind !== RemoteRepositoryCloner.None
      onLinkActivated: function(link) { Qt.openUrlExternally(link) }
  }

  QC.Label {
      objectName: "remoteCloneErrorDetails"
      Layout.fillWidth: true
      visible: clonerId.cloneErrorFriendlyMessage.length > 0
               && clonerId.cloneErrorMessage.length > 0
      text: qsTr("Details: %1").arg(clonerId.cloneErrorMessage)
      color: Theme.textSubtle
      font.pixelSize: Theme.fontSizeSmall
      wrapMode: QC.Label.WrapAtWordBoundaryOrAnywhere
  }
  ```

  The outer `ColumnLayout` visibility condition at lines 111–117 still
  works correctly.

## Step 4 — Update `DeepLinkConfirmDialog.qml`

- `_showAuthFlow` (lines 69–73): replace
  `cloneAreaId.cloneFailedDueToAuthError` with
  `cloneAreaId.cloneErrorKind === RemoteRepositoryCloner.Auth`.
- Remove the `authErrorMessage: qsTr("Sign in to GitHub above to retry.")`
  override on the `CloneArea` (line 216). The provider supplies a
  neutral auth message; the inline `GitHubAuthFlow` GroupBox is
  visually right above it, making the action self-evident.

## Step 5 — Forward `linkActivated` in `HelpArea.qml`

The inner `QC.Label` (line 67) has `textFormat: QC.Label.RichText` but
swallows `linkActivated`. To make the embedded `<a>` clickable:

- Add `signal linkActivated(string link)` to the root `QQ.Rectangle`.
- On the inner `helpText` label, add
  `onLinkActivated: function(link) { helpArea.linkActivated(link) }`.

Other callers ignore the new signal; no breakage.

## Commit sequence

Five small commits, each one builds cleanly and passes tests on its own:

### Commit 1 — Introduce `cwGitHostingProvider` (no callers yet)

- Add `cavewherelib/src/cwGitHostingProvider.{h,cpp}` per **Step 1**:
  struct, table, free functions.
- Add `testcases/test_cwGitHostingProvider.cpp` covering:
  - `forHost("GITHUB.com")` returns the GitHub entry (case-insensitive).
  - `forHost("unknown.example")` returns the generic entry
    (empty `host`, empty `displayName`).
  - `repositoryWebUrl` for each known provider against
    `https://github.com/owner/repo.git`,
    `ssh://git@github.com/owner/repo.git`,
    `https://github.com/owner/repo` (no `.git`),
    and case-insensitive matching via `forUrl(QUrl("HTTPS://GITHUB.COM/owner/repo"))`.
  - Host-mismatch defensive check: `repositoryWebUrl(githubInfo, QUrl("https://gitlab.com/x/y"))`
    returns an empty `QUrl()` (never silently rewrites the host).
  - Generic provider's `repositoryWebUrl` returns empty for ssh inputs,
    valid for `https://example.com/x.git`.
  - `collaboratorSettingsUrl` returns the expected per-host paths.
  - Each entry's `authMessage` and `notFoundMessage` contain the
    expected provider-native words ("GitHub" + "collaborator" vs
    "GitLab" + "project member" vs "Bitbucket" + "workspace").
  - `notFoundOrAccessMessage` composes the `<a href>` suffix when a
    web URL can be derived, omits it otherwise.
- Register the new sources in `cavewherelib/CMakeLists.txt` and the
  test file in whichever target lists `testcases/*.cpp` (verify with
  `grep` before editing).
- **Testable**: provider unit tests pass; app behaves identically (no
  callers wired up yet).

### Commit 2 — Migrate `cwDeepLinkHandler` to the provider registry

- After `grep`-confirming call sites, either delete the dead
  delegates or have them call `cwGitHostingProvider::...`.
- `allowedHosts()` returns `cwGitHostingProvider::knownHosts()`; drop
  the duplicated static `QStringList`.
- **Testable**: existing deep-link tests pass unchanged; app behavior
  identical. Single source of truth for host knowledge.

### Commit 3 — Forward `linkActivated` from `HelpArea.qml`

- Add the signal forward per **Step 5**.
- **Testable**: existing QML/UI tests pass; signal has no consumers
  yet but breaks nothing.

### Commit 4 — Cloner: add `CloneErrorKind` (with `cloneFailedDueToAuthError` as compat shim)

- Add the enum, the three new properties, classification logic, and
  `applyCloneError` per **Step 2**.
- **Keep `cloneFailedDueToAuthError`** as a thin deprecated forwarder
  that returns `m_cloneErrorKind == Auth`, with its NOTIFY emitted
  whenever `cloneErrorKindChanged` fires. Lets existing QML keep
  compiling during the commit-by-commit migration.
- Update the `setGitHubIntegration` auto-retry path
  (`cwRemoteRepositoryCloner.cpp` lines 55–67) to read
  `m_cloneErrorKind == Auth`.
- Add unit tests for the new properties:
  - Per-kind classification: synthesize error results for each of
    `Auth`, `NotFoundOrAccess`, `HostUnreachable`, `Other` and verify
    `cloneErrorKind` is set correctly.
  - `cloneErrorFriendlyMessage` non-empty for `Auth`,
    `NotFoundOrAccess`, `HostUnreachable`; empty for `Other`.
  - `cloneErrorFriendlyMessage` for `NotFoundOrAccess` against a
    github.com URL contains both the friendly text and the
    `<a href="https://github.com/...">` link suffix.
  - `cloneErrorMessage` retains the raw libgit2 string verbatim across
    all cases.
- Add a **regression test for the auth-retry path** (the
  `setGitHubIntegration` lambda update):
  1. Drive the cloner into the `Auth` state with a fake
     `cwGitHubIntegration` that returns an empty token.
  2. Update the integration's `accessToken` to a non-empty value and
     fire `accessTokenChanged`.
  3. Verify the cloner re-issues a clone of the pending URL (e.g.
     check that `m_cloneWatcher->future()` is replaced).
  Without this test the auto-retry is just a couple of buried lines
  in a lambda; it's exactly the kind of behavior that regresses
  silently during a refactor.
- **Testable**: new and old QML APIs work; old tests pass; new tests
  cover the four kinds end-to-end at the C++ layer.

### Commit 5 — Switch QML to the new API; drop the compat shim

- Update `CloneArea.qml` per **Step 3** (drop `authErrorMessage`,
  render friendly + details, expose `cloneErrorKind`).
- Update `DeepLinkConfirmDialog.qml` per **Step 4** (`_showAuthFlow`
  reads the enum; remove `authErrorMessage` assignment).
- Remove `cloneFailedDueToAuthError` Q_PROPERTY / getter / signal from
  `cwRemoteRepositoryCloner` (the compat shim added in Commit 4).
- Update any QML/C++ tests that referenced the boolean or
  `authErrorMessage`.
- **Testable**: manual QA covers the full matrix; QML tests pass;
  C++ tests still pass after the boolean's removal.

### Bisect / rollback notes

- Commits 1–3 are purely additive — safe to land independently.
- Commit 4 is the largest behavioral change but is gated by the
  compat shim, so user-visible behavior is unchanged until commit 5.
- Commit 5 flips the user-visible UX. If anything regresses, this is
  the commit to revert.

## Verification

This is a UX/error-message change plus a structural refactor (boolean
→ enum, host knowledge → provider namespace). Manual end-to-end is
the primary gate; the table invites targeted unit tests.

1. **Build**:
   ```
   cmake --build build/<preset> --target CaveWhere cavewhere-test cavewhere-qml-test
   ```

2. **Manual scenarios** — Clone dialog:
   - **Public repo, have access** → succeeds, no error.
   - **Private GitHub repo, no access** → friendly *"…GitHub account…
     pending invitation."* + clickable "Open repository on GitHub";
     raw `unexpected http status code: 404` under "Details:". Auth
     GroupBox **not** shown.
   - **Nonexistent GitHub repo** → same friendly message.
   - **Private GitLab repo, no access** → *"…GitLab account… project
     invitations…"* with "Open project on GitLab" link.
   - **Bitbucket equivalent** → *"…Bitbucket account…"* wording.
   - **Unknown host 404** (e.g. fabricated codeberg.org URL) →
     generic *"…your account doesn't have access…"* wording; "Open
     repository in browser" link only when `repositoryWebUrl` could
     derive one.
   - **Bad credentials (401)** on a GitHub URL → *"Your GitHub
     credentials are invalid…"*; inline `GitHubAuthFlow` GroupBox
     visible in `DeepLinkConfirmDialog`; completing sign-in
     auto-retries the clone (**regression point** — auto-retry trigger
     moved to `m_cloneErrorKind == Auth`).
   - **DNS failure** (`https://nosuchhost.invalid/foo.git`) →
     *"Couldn't reach nosuchhost.invalid…"*; raw text under "Details:".
   - **Connection refused / timeout** → same `HostUnreachable`
     message.

3. **Narrow-tag suite** (per project guidance — do not run the full
   suite):
   ```
   ./build/<preset>/cavewhere-test "[remote]" 2>&1 | tee /tmp/remote.log
   ```
   Adjust tag based on what `grep -rln "cwRemoteRepositoryCloner\|cwGitHostingProvider" testcases/` reveals.

4. **QML tests** — `grep -rln "CloneArea\|DeepLinkConfirmDialog" test-qml/`.
   Any test referencing `cloneFailedDueToAuthError` or
   `authErrorMessage` needs updating to the enum-based API.

## Out of scope

- **Not** modifying the QQuickGit submodule.
- **Not** trying to distinguish "doesn't exist" from "no access" — the
  hosting service intentionally returns the same 404 for both.
- **Not** handling SSH-specific 404 equivalents in v1; they fall into
  `Other` and show the raw text.
- **Not** adding a retry button for `NotFoundOrAccess` — the embedded
  link is enough self-service.
- **Not** routing other libgit2 failures through their own friendly
  kinds — HTTP 403 (SSO/permissions), HTTP 5xx (host outage), SSL/TLS
  certificate errors, LFS hydration failures, "destination not empty",
  out-of-disk all fall into `Other`. Easy to graduate later: add a
  matcher in `handleCloneWatcherStateChanged` + a case in
  `applyCloneError`. If the message becomes provider-specific, add it
  to `cwGitHostingProviderInfo`.
- **Not** moving the host allowlist into `cwSettings`.
  `cwGitHostingProvider::knownHosts()` is the single source of truth.
- **Not** promoting the data table to a polymorphic interface. If a
  future host needs an algorithm that doesn't fit the
  `host + collaboratorPathSuffix` shape (e.g. GitLab subgroups,
  Bitbucket workspaces in the URL), promote then. ~30-minute mechanical
  refactor.
