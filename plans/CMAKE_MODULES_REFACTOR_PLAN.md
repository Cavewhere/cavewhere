# CMake modules refactor

## Context

The root `CMakeLists.txt` is ~1164 lines. Most of that bulk is platform-specific
packaging logic that no one reads while editing engine/test/QML targets, but
that still has to be scrolled past every time. A brief detour into a
custom Android APK signing target (~67 lines, since removed in favor of Qt
Creator's native `--sign` support) made this worse and prompted the question.

A `cmake/` directory does not yet exist. This plan introduces one and extracts
the substantial, self-contained chunks into per-area module files, one phase at
a time. The root file becomes an orchestrator (~250 lines) that mostly just
`include()`s and calls functions defined in `cmake/*.cmake`.

The refactor is pure-restructure: no behavior change, no new features, no
flag/var renames visible to users.

## Conventions for `cmake/` modules

- One concern per file. File name = `Cavewhere<Area>.cmake`
  (`CavewhereAndroid.cmake`, `CavewhereAppleBundle.cmake`, etc.).
- Each module exposes one or more `cavewhere_*` functions that take the target
  name as the first argument. No top-level side effects when `include()`d —
  the root file decides when/whether to call.
- Cache variables that belong to a module are declared **inside** the function
  the first time it runs, not at file top-level, so `include()` is free.
- Use `function()` (not `macro()`) — local variable scope avoids leaking
  helper vars into the caller.
- Include via absolute path from `CMAKE_SOURCE_DIR`:
  `include(${CMAKE_SOURCE_DIR}/cmake/CavewhereAndroid.cmake)`.

## Extraction targets

Survey of substantial blocks in the current root file:

| Lines (pre-refactor) | Block | Target file | Phase |
|---:|---|---|---|
| 695-722 (~28) | Android OpenSSL setup (inside `if(MOBILE_BUILD)`) | `cmake/CavewhereAndroid.cmake` | 1 ✅ |
| 912-1154 (~243) | macOS bundle / Info.plist / install rules | `cmake/CavewhereAppleBundle.cmake` | 2 |
| 807-910 (~104) | Windows runtime DLL collection | `cmake/CavewhereWindowsRuntime.cmake` | 3 |
| 367-469 (~103) | Linux AppImage / linuxdeploy ExternalProject | `cmake/CavewhereLinuxDeploy.cmake` | 4 |
| 156-262 (~107) | `cw_prepend_conan_buildenv_path()` | `cmake/CavewhereConanBuildEnv.cmake` | 5 |
| 287-342 (~56)  | `cw_link_absl_static()` | `cmake/CavewhereAbseilLink.cmake` | 5 |
| 1-44 (~44)     | Git-derived marketing version | `cmake/CavewhereVersion.cmake` | 6 |

Estimated final root size: ~250 lines (orchestrator + cross-cutting setup).

## Phase 1 — Android OpenSSL ✅ DONE

**Validates the pattern.** Smallest blast radius (Android-only).

### File: `cmake/CavewhereAndroid.cmake`

One function:

```cmake
# Wires KDAB's prebuilt OpenSSL libs into the APK so Qt's TLS plugin can
# dlopen them at runtime. Sets ANDROID_OPENSSL_DIR cache var on first call.
function(cavewhere_setup_android_openssl target)
    # body from former lines 707-721
endfunction()
```

### Root file callsite

Inside the existing `if(MOBILE_BUILD) / if(ANDROID)` block, replace the
OpenSSL body with:

```cmake
include(${CMAKE_SOURCE_DIR}/cmake/CavewhereAndroid.cmake)
cavewhere_setup_android_openssl(CaveWhere)
```

### Historical note: APK signing target (removed)

Phase 1 originally also extracted a `cavewhere_setup_android_apk_signing()`
function that defined a `sign_apk` target wrapping `zipalign` + `apksigner`
with passwords read from the macOS keychain. After landing, we realized Qt
Creator already supports the same flow natively via **Projects → Build →
Build Android APK → Sign package**, which runs `androiddeployqt --sign`.
The custom target and its `scripts/sign-android-apk.sh` wrapper were
removed; the module file now contains only the OpenSSL function. The
signing flow lives in `docs/android-build.md` pointing at Qt Creator's
built-in controls.

### Verification (completed)

- `cmake .` succeeded in the existing Android build dir; root file shrank
  from 1164 → ~1078 lines.
- File-level guard works: `cmake/CavewhereAndroid.cmake` is only `include()`d
  inside `if(ANDROID)`, so desktop builds are unaffected.

**Done. Phase 2 next.**

## Phase 2 — Apple bundle

**Biggest single win** (~243 lines extracted). Highest risk too: the macOS
bundle has Info.plist substitution, icon resource paths, codesign rules, and
install() commands. Easy to break with a stray scope change.

### File: `cmake/CavewhereAppleBundle.cmake`

One function: `cavewhere_setup_apple_bundle(target)`. Body is the entire
`if(APPLE)` block from current lines 912-1154.

Audit before extracting:
- Any variables set inside the block that are **read outside** it? If so,
  `set(... PARENT_SCOPE)` or pull them out before the call.
- Any `set_target_properties` / `target_*` calls — those work fine from a
  function scope as long as the target name is passed in.
- `set_source_files_properties` on resource files — verify those still apply
  to the right targets when invoked from a different scope.
- `install()` commands — confirm they still resolve `${CMAKE_INSTALL_PREFIX}`
  correctly (they should; install is global).

### Root file callsite

Replace lines 912-1154 with:

```cmake
if(APPLE)
    include(${CMAKE_SOURCE_DIR}/cmake/CavewhereAppleBundle.cmake)
    cavewhere_setup_apple_bundle(CaveWhere)
endif()
```

### Verification

- Configure + build the macOS Debug preset; bundle structure unchanged
  (`find build/<preset>/CaveWhere.app -type f | sort` before/after diff = empty).
- `codesign -dv build/<preset>/CaveWhere.app` — same signing identity.
- Run the app; confirm icon, version string in About, file associations all
  match pre-refactor.

**Stop here. Wait for user review before starting Phase 3.**

## Phase 3 — Windows runtime DLLs

### File: `cmake/CavewhereWindowsRuntime.cmake`

`cavewhere_setup_windows_runtime(target)` containing current lines 807-910.
Watch for variables from the surrounding scope (protobuf/abseil var names);
those must be visible inside the function — they're set by Conan-generated
config files via `find_package`, so they should be in `PROJECT_SCOPE`/cache
and visible from anywhere.

### Verification

Requires a Windows machine. Plan boundary: extract and configure-test on macOS
(`if(WIN32)` skipped, no effect), then ask user to verify on Windows before
Phase 4.

**Stop here. Wait for user review before starting Phase 4.**

## Phase 4 — Linux AppImage

### File: `cmake/CavewhereLinuxDeploy.cmake`

`cavewhere_setup_linux_appimage(target)` containing current lines 367-469.
Uses `ExternalProject` — that's a module-level concern, so the
`include(ExternalProject)` belongs inside the new `.cmake` file.

### Verification

Requires a Linux machine (or `linux/amd64` Docker). Plan boundary: extract
and configure-test on macOS (`if(UNIX AND NOT APPLE)` skipped, no effect),
then user verifies on Linux.

**Stop here. Wait for user review before starting Phase 5.**

## Phase 5 — Build helpers (Conan buildenv, Abseil link)

### Files

- `cmake/CavewhereConanBuildEnv.cmake` — `cw_prepend_conan_buildenv_path()`
  (current lines 156-262). Function already self-contained.
- `cmake/CavewhereAbseilLink.cmake` — `cw_link_absl_static()`
  (current lines 287-342). Function already self-contained.

These are the easiest extractions (already functions, no surrounding state).
Could batch into one PR.

### Verification

- Re-configure each platform; `cmake .` succeeds.
- macOS Debug build still completes (smoke test).

**Stop here. Wait for user review before starting Phase 6.**

## Phase 6 — Git-derived version

### File: `cmake/CavewhereVersion.cmake`

Top of root file (lines 1-44) derives `PROJECT_VERSION` from `git describe`.
Wrap into `cavewhere_compute_marketing_version(out_var)` that writes to a
caller-provided variable name in `PARENT_SCOPE`.

Tricky bit: this code must run **before** `project()` (it sets variables
consumed by `project()`'s VERSION argument). Confirm that `include()` from
before `project()` is legal in CMake 3.x — it is, with the caveat that the
included file can't itself call `project()`.

### Verification

`cmake .` reports the same `CaveWhere marketing version: ...` line as before
on a clean tag and on a post-tag commit (build number increments).

## Out of scope

- Renaming any cache variables (`CAVEWHERE_*`, `ANDROID_OPENSSL_DIR`, etc.).
  Existing build dirs and CI configs keep working.
- Renaming any functions (`cw_*` → `cavewhere_*`). The new modules use the
  `cavewhere_setup_*` convention, but existing `cw_*` functions keep their
  current names when extracted — renaming is a separate cleanup.
- Touching `cavewherelib/CMakeLists.txt` or `testcases/CMakeLists.txt`.
- Build system unification (one preset to rule them all, etc.).

## Risk register

| Risk | Mitigation |
|---|---|
| Variable scope leak when wrapping in `function()` | Diff `cmake -LA .` output before/after each phase; any disappeared cache var means I missed a `PARENT_SCOPE`. |
| Generator-expression context changes (target properties evaluated in wrong scope) | Phase-by-phase verification on the actual target platform before moving on. |
| `include()` ordering breaks `project()` setup in Phase 6 | Phase 6 is last specifically because it touches pre-`project()` code; can be deferred indefinitely if it gets hairy. |
| Behavior drift no one notices until release | Each phase includes platform-specific smoke verification, not just "configure succeeds". |

## Order of work

1. Phase 1 (Android) — validates the pattern. **Pause.**
2. Phase 2 (Apple) — biggest reader-experience win. **Pause.**
3. Phases 3 + 4 (Windows + Linux packaging) — can be one PR if user has both
   environments handy, otherwise sequential. **Pause after each.**
4. Phase 5 (helpers) — small, batch as one. **Pause.**
5. Phase 6 (version) — only if it's worth the pre-`project()` care.

Each phase is independently revertable: the only file under hot diff at any
time is the root `CMakeLists.txt` plus one new `cmake/*.cmake`.
