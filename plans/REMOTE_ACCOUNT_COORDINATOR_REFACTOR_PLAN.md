# Remote Account Coordinator Refactor Plan

## Goal

Replace UI-driven account sync and single-token GitHub auth with a coordinator-driven, multi-account architecture that:

- Keeps account lifecycle in C++ (not QML page code).
- Supports multiple GitHub accounts.
- Resolves the correct token per remote/repository for LFS.
- Uses a clean, new API design (no legacy migration path required).

---

## Current Problems

- `RemoteRepositoryPage.qml` mutates `remoteAccountModel` directly based on view state.
- `cwGitHubIntegration` effectively represents one global GitHub token/session.
- `cwGitHubLfsAuthProvider` returns one cached GitHub header for all GitHub URLs.
- `cwRemoteAccountModel` stores only `(provider, username)` without auth/session metadata.

---

## Target Architecture

### New Coordinator (instead of `cwRootData` owning behavior)

Introduce a dedicated coordinator class, e.g. `cwRemoteAccountCoordinator`, responsible for:

- Bridging provider integrations (GitHub for now) with account model + credential store.
- Managing account lifecycle (authorize, activate, forget, revoke).
- Resolving account identity for repository operations.
- Exposing an API QML can call without embedding provider/model coupling in pages.

### Core Components

1. `cwRemoteAccountCoordinator` (new)
- Owns orchestration logic.
- No UI logic.

2. `cwRemoteAccountModel` (expanded)
- Becomes stateful metadata model (not just labels).

3. `cwRemoteCredentialStore` (new)
- Per-account credential persistence (QtKeychain).

4. `cwRemoteBindingStore` (new)
- Maps canonical remote identity -> `accountId`.

5. `cwGitHubIntegration` (narrowed)
- Provider API + login flow + provider API calls.
- No direct mutation of account model.

6. `cwGitHubLfsAuthProvider` (rewired)
- Resolves header by URL through binding + credential store.

---

## Data Model Changes

Extend `cwRemoteAccountModel::Entry` to include:

- `QString accountId` (stable UUID)
- `Provider provider`
- `QString username`
- `AuthState authState` (`Unknown`, `Authorized`, `SignedOut`, `Revoked`, `Error`)
- `bool active`
- `QDateTime lastUsedAt`
- optional: `QString displayName`

Add roles (in addition to existing roles):

- `AccountIdRole`
- `AuthStateRole`
- `ActiveRole`
- `LastUsedAtRole`

Add APIs:

- `QString upsertAccount(Provider provider, const QString& username)`
- `void setAuthState(const QString& accountId, AuthState state)`
- `void setActiveAccount(Provider provider, const QString& accountId)`
- `QString activeAccountId(Provider provider) const`
- `QVariantMap accountById(const QString& accountId) const`
- `void removeAccountById(const QString& accountId)`

---

## Credential & Binding Strategy

### Credential keys

Store tokens by account ID, e.g.:

- Service: `CaveWhere`
- Key: `RemoteAccount/<provider>/<accountId>/AccessToken`

### Binding keys

Canonical remote key example:

- `github.com/<owner>/<repo>`

Store:

- `remoteKey -> accountId`

When known:

- On clone via account X, write binding.
- On remote-repo actions, update `lastUsedAt` + active account.

---

## LFS Resolution Rules

`cwGitHubLfsAuthProvider::authorizationHeader(url)`:

1. Parse URL host/path.
2. If not GitHub host -> no header.
3. Resolve remote key from URL.
4. Look up bound `accountId`.
5. If bound account exists and authorized -> return that token header.
6. Else fallback to active GitHub account token.
7. Else return empty header.

---

## QML/UI Refactor

- Remove account sync logic from `RemoteRepositoryPage.qml`.
- QML should call coordinator intents only:
  - startAddAccount(provider)
  - forgetAccount(accountId)
  - setActiveAccount(accountId)
  - refreshProvider(provider/accountId)
- Coordinator updates model; page reacts via bindings.

---

## Phased Checklist (for multiple prompts)

### Phase 1: Coordinator Skeleton
- [x] Add `cwRemoteAccountCoordinator` class + headers/sources.
- [x] Instantiate and expose coordinator to QML (via RootData property only as access point).
- [x] Move existing account add/remove calls from page logic into coordinator methods.
- [x] Keep current behavior working with single account.

### Phase 2: Stateful Account Model
- [x] Extend `cwRemoteAccountModel` entry schema + roles.
- [x] Add accountId/authState/active/lastUsed persistence in settings.
- [x] Add model APIs for upsert/setActive/setAuthState/removeById.
- [x] Update `cwRemoteAccountSelectionModel` delegates/labels as needed (no UI role changes required in this phase).

### Phase 3: Credential Store
- [x] Add `cwRemoteCredentialStore` (QtKeychain wrapper) with async read/write/delete.
- [x] Replace direct single-key token usage with accountId-based storage.
- [x] Remove old token API surface and use only accountId-based credentials.

### Phase 4: GitHub Integration Decoupling
- [x] Remove direct account model mutation assumptions from `cwGitHubIntegration`.
- [x] Emit provider lifecycle signals consumed by coordinator.
- [x] Coordinator owns login completion -> account upsert -> token store update.
- [x] Coordinator owns logout/forget semantics for selected account.

### Phase 5: Remote Binding Store
- [x] Add `cwRemoteBindingStore` and canonical remote key parser.
- [x] Bind accountId on clone/remote selection flows.
- [x] Add tests for key normalization and lookup.

### Phase 6: LFS Provider Refactor
- [x] Rewire `cwGitHubLfsAuthProvider` to use coordinator/stores (not one cached integration header).
- [x] Resolve header per URL via binding, with explicit active-account fallback policy.
- [x] Add tests for multi-account LFS resolution.

### Phase 7: QML Cleanup
- [x] Remove `syncAuthorizedGitHubAccount` and GitHub `Connections` from `RemoteRepositoryPage.qml`.
- [x] Replace page account actions with coordinator API calls.
- [x] Ensure forget/remove flow updates model + token store correctly.

### Phase 8: Test Expansion
- [x] Update `tst_RemoteRepositoryPage.qml` for coordinator-driven flow.
- [x] Add assisted test for two GitHub accounts + switching active account.
- [ ] Add C++ tests for:
  - [x] account model persistence
  - [x] account-scoped credential storage lifecycle (no legacy migration on dev branch)
  - [x] binding resolution
  - [x] LFS header selection

### Phase 9: Hardening
- [ ] Define behavior for stale bindings (account removed/revoked).
- [ ] Add telemetry/log points for auth/binding resolution failures.
- [ ] Document operational semantics in `docs/`.
- [ ] Remove any deprecated/bridge APIs introduced during intermediate phases.

---

## Open Decisions

- How to represent provider-specific account identity beyond username (GitHub numeric user ID)?
- Should `active` be global per provider or per repository context?
- Should forgetting an account remove bindings or mark them dangling with repair UI?
- Coordinator ownership location: `cwRootData` access point vs separate singleton registration.

---

## Prompt-by-Prompt Execution Notes

Recommended prompt sequence:

1. Implement Phase 1 + Phase 2 scaffolding.
2. Implement credential store + migration (Phase 3).
3. Decouple GitHub integration and wire coordinator events (Phase 4).
4. Add binding store + LFS resolver changes (Phase 5 + 6).
5. Cleanup QML and finalize tests (Phase 7 + 8 + 9).
