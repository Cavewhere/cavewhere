# Building CaveWhere for Android

## Prerequisites

- macOS, Linux, or Windows host with Qt Creator installed
- Qt 6.11+ for Android (installed via Qt Online Installer — pick the ABIs you
  need: `android_arm64_v8a`, `android_armv7`, `android_x86_64`, `android_x86`)
- Qt 6.11+ for the host platform (required for `QT_HOST_PATH` — provides
  native `moc`, `rcc`, `qmlcachegen`, etc.)
- Android SDK + NDK (the Qt Online Installer can install these, or use the
  copies bundled with Android Studio)
- Conan 2.x
- Ninja (`brew install ninja` on macOS)
- CMake < 4.0 (pinned via the Conan profile)
- JDK 17 (required by Gradle for recent Android plugin versions)

## 1. Create the Android Conan Profile

Create `~/.conan2/profiles/android`:

```ini
[settings]
arch=armv8
build_type=Release
compiler=clang
compiler.cppstd=gnu20
compiler.libcxx=c++_static
compiler.version=18
os=Android
os.api_level=24

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.android:ndk_path=/Users/YOU/Library/Android/sdk/ndk/27.2.12479018

[tool_requires]
!cmake/*: cmake/[>=3 <4]
```

Update the fields for your environment:

- `arch` — pick the ABI you are building for: `armv8` (arm64-v8a, modern
  devices), `armv7` (armeabi-v7a), `x86_64` (emulators), `x86`. Create a
  separate profile per ABI if you need to build multiple.
- `compiler.version` — the major version of the NDK's bundled clang. Check with:
  ```bash
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/clang --version
  ```
  NDK 27.2 ships clang 18; NDK 26.1 ships clang 17.
- `os.api_level` — minimum Android API level. 24 (Android 7.0) is a
  reasonable baseline; raise it if a dependency requires newer.
- `tools.android:ndk_path` — absolute path to your NDK. The Qt Online
  Installer typically lands the NDK under
  `$HOME/Library/Android/sdk/ndk/<version>` (macOS) or the equivalent on
  other OSes.

## 2. Install Conan Dependencies

Conan deps must be built with the Ninja generator — the Xcode quirks that
motivated this on iOS don't apply, but Ninja is still the right default for a
command-line cross-compile. Install for both Debug and Release so multi-config
IDE tooling can find them:

```bash
conan install . -o "&:system_qt=True" -o "&:mobile=True" --build=missing \
    -pr:h android -pr:b default \
    -of build/Qt_6_11_0_for_Android_arm64_v8a/conan_deps \
    -s build_type=Release

conan install . -o "&:system_qt=True" -o "&:mobile=True" --build=missing \
    -pr:h android -pr:b default \
    -of build/Qt_6_11_0_for_Android_arm64_v8a/conan_deps \
    -s build_type=Debug
```

Adjust the output path (`-of`) to match the build directory Qt Creator
generates for your kit (e.g. `Qt_6_11_0_for_Android_arm64_v8a`,
`Qt_6_11_0_for_Android_x86_64`). You need a separate `conan_deps` directory
per ABI.

Key options:

- `system_qt=True` — use the Qt installation from Qt Online Installer, not
  Conan Qt.
- `mobile=True` — skips desktop-only dependencies (wxwidgets, gdal, survex,
  libtiff, etc.).
- `-pr:h android` — host profile targets the Android ABI from the profile.
- `-pr:b default` — build profile uses native host tools (for `protoc` etc.).

## 3. Configure with CMake (Qt Creator)

In Qt Creator, select the **Qt for Android** kit (matching the ABI you ran
`conan install` for) and add these CMake arguments under **Initial
Configuration**:

```
-DQT_CREATOR_ENABLE_PACKAGE_MANAGER_SETUP=OFF
-DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=/path/to/build/Qt_6_11_0_for_Android_arm64_v8a/conan_deps
```

- `QT_CREATOR_ENABLE_PACKAGE_MANAGER_SETUP=OFF` prevents Qt Creator's
  built-in Conan support from running its own install with the wrong
  generator/profile.
- `QT_ADDITIONAL_PACKAGES_PREFIX_PATH` points Qt's toolchain at the
  Conan-installed packages.

Qt Creator populates the Android SDK / NDK paths and `QT_HOST_PATH` from the
selected kit, so you don't need to pass them explicitly in the IDE.

### Configure from Command Line

```bash
cmake -S . -B build/Qt_6_11_0_for_Android_arm64_v8a \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/Qt/6.11.0/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
    -DQT_HOST_PATH=$HOME/Qt/6.11.0/macos \
    -DANDROID_SDK_ROOT=$HOME/Library/Android/sdk \
    -DANDROID_NDK_ROOT=$HOME/Library/Android/sdk/ndk/27.2.12479018 \
    -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=$(pwd)/build/Qt_6_11_0_for_Android_arm64_v8a/conan_deps \
    -DQT_CREATOR_ENABLE_PACKAGE_MANAGER_SETUP=OFF \
    -DCAVEWHERE_BUILD_NUMBER=1
```

- `QT_HOST_PATH` must point to a host-architecture Qt install of the same
  version as the target Android kit (for native build tools like
  `qmlcachegen`).
- Replace `macos` with `gcc_64` on Linux or `msvc2022_64` on Windows.
- `CAVEWHERE_BUILD_NUMBER` is required because the mobile version check
  rejects the `0.0.0` fallback (see [Versioning](#versioning) below).

## 4. Build and Deploy

Build from Qt Creator. The **Build Android APK** target generates the APK and
an Android App Bundle (AAB); the **Run** button installs and launches on the
attached device or emulator.

From the command line:

```bash
cmake --build build/Qt_6_11_0_for_Android_arm64_v8a --target apk
```

Deploying to a physical device requires USB debugging enabled and the device
authorised. Signing an APK/AAB for Play Store upload requires an Android
keystore — configure signing in Qt Creator's **Projects → Build → Build
Android APK → Sign package**, or via Gradle properties.

## How It Works

### Shared pattern with iOS

Android and iOS both set `CW_MOBILE ON` in the top-level `CMakeLists.txt`
(detected via `ANDROID`, `IOS`, or `CMAKE_OSX_SYSROOT MATCHES iphoneos`).
That flag drives:

- Skipping desktop installer scripts (`installer/installer.cmake`).
- Parsing `conanbuildenv-*.sh` to put the native host `protoc` on `PATH`
  for cross-compile configure steps.
- Setting `<package>_DIR` cache entries for every Conan-generated
  `*Config.cmake` so `find_package` resolves to the correct configs.
- Adding `conan_deps` to `CMAKE_MODULE_PATH` / `CMAKE_PREFIX_PATH`.
- Refusing to build with the marketing-version fallback `0.0.0` (see below).

### Static Linking

Conan dependencies are built as static libraries for Android (protobuf,
abseil, libgit2, libssh2, openssl, etc.) because Qt for Android loads the
app as a shared library and pulling another layer of runtime loading for
third-party shared libs is painful. `conanfile.py::configure()` forces
`openssl.shared=False`, `protobuf.shared=False`, and
`abseil.shared=False` when `os=Android`.

### Versioning

The top-level `CMakeLists.txt` wires the same `CAVEWHERE_BUILD_NUMBER` cache
variable used for iOS `CFBundleVersion` into the Android manifest via the
`QT_ANDROID_VERSION_CODE` / `QT_ANDROID_VERSION_NAME` target properties on
the `CaveWhere` target; `androiddeployqt` substitutes both into
`AndroidManifest.xml` at deploy time:

- `android:versionName` ← `PROJECT_VERSION` (the marketing version derived
  from the most recent reachable git tag, e.g. `2026.4.2`)
- `android:versionCode` ← `CAVEWHERE_BUILD_NUMBER`

Semantics differ from iOS — read carefully:

| | iOS `CFBundleVersion` | Android `versionCode` |
|---|---|---|
| Format | Dot-separated non-negative integers, ≤18 chars (`1`, `1.2.3`, `42`) | Single 32-bit non-negative integer |
| Play/Apple cap | n/a | 2,100,000,000 |
| Uniqueness scope | Strictly > previous uploads with the same `CFBundleShortVersionString` — resets per marketing version | Strictly > every previous upload for the package — does **not** reset per `versionName` |
| Relationship to marketing version | Paired | Independent |

Because of this, `CMakeLists.txt` applies two stricter rules on Android:

1. `CAVEWHERE_BUILD_NUMBER` must be a single non-negative integer
   ≤ 2,100,000,000. A value like `1.2.3` is accepted for iOS but rejected
   at configure time on Android.
2. The cache default `auto` (which resolves to commits-since-tag + 1 on
   iOS) is **rejected on Android**. That default works on iOS because
   CFBundleVersion resets per marketing version, but on Android it would
   silently collide across tag cycles and cause Play Store rejections. You
   must pass an explicit integer:
   ```bash
   cmake -DCAVEWHERE_BUILD_NUMBER=<N> ...
   ```
   For local dev builds, any integer (e.g. `1`) is fine; for Play Store
   uploads, pick the next unused integer greater than every previous
   upload. The value sticks in `CMakeCache.txt` across reconfigures.

## Troubleshooting

### `linuxdeploy not found` during configure

The top-level `CMakeLists.txt` gates `installer/installer.cmake` on
`NOT CW_MOBILE`. If you see this error on an Android configure, make sure
`ANDROID` or the Android-aware toolchain file is actually being picked up
(check the configure output for `Configuring 'CaveWhere' for the following
Android ABIs: …`).

### `QT_HOST_PATH` / `qmlcachegen` not found

Cross-compiling Qt requires a native host Qt install of the same version.
Install `macos` (or `gcc_64` on Linux, `msvc2022_64` on Windows) from the Qt
Online Installer at the exact version you chose for Android, and point
`QT_HOST_PATH` at it. Qt Creator configures this automatically when the host
Qt kit is registered.

### Stale `<pkg>_DIR` after switching ABIs

Each ABI needs its own `conan_deps` directory. If you accidentally reuse a
`conan_deps` from a different ABI, `find_package` will find the wrong static
libs and link will fail (or succeed with the wrong instruction set). Delete
the `conan_deps` folder and `CMakeCache.txt`, then reinstall Conan deps for
the correct ABI.

### `Could NOT find Protobuf (missing: Protobuf_INCLUDE_DIR)`

Delete `CMakeCache.txt` and reconfigure. Same as iOS — stale cache entries
from a prior configure can pin wrong package paths.

### App crashes on launch with `dlopen failed: library ... not found`

Usually means a Conan dep got built as a shared `.so` and didn't make it
into the APK. Confirm `conanfile.py::configure()` is forcing
`shared=False` for that package on Android. Rebuild Conan deps with
`--build=missing` after the fix.

### Multiple ABIs in a single APK

Qt for Android can bundle multiple ABIs. To do that, run `conan install`
once per ABI (with its own profile and `-of` directory), and set
`QT_ANDROID_ABIS=arm64-v8a;armeabi-v7a;…` plus the corresponding
`QT_ANDROID_ABIS_EXTRA_PACKAGES_PREFIX_PATH` entries at configure time.
Single-ABI builds are simpler; do multi-ABI only when you need it.

## Release / store workflow

### Cutting a release build

1. Tag the commit to ship:
   ```bash
   git tag 2026.4.2
   git push --tags
   ```
2. From a clean checkout at that tag, reconfigure. The configure log should
   include:
   ```
   -- CaveWhere marketing version: 2026.4.2 (build 1)
   ```
3. Pick a `versionCode` — the next unused integer greater than every
   previous Play Store upload for this package. Remember: `versionCode`
   does **not** reset when `versionName` changes. Pass it explicitly:
   ```bash
   cmake -B build/Qt_6_11_0_for_Android_arm64_v8a -DCAVEWHERE_BUILD_NUMBER=<N>
   ```
4. Build the AAB target in Qt Creator (**Build → Build Android App Bundle**)
   or from CLI:
   ```bash
   cmake --build build/Qt_6_11_0_for_Android_arm64_v8a --target aab
   ```
5. Sign with your release keystore and upload via the Play Console.

### Re-uploading to Play Store

Any upload — new tag or re-roll of the same commit — must use a strictly
greater `versionCode` than every previous upload. Since Android rejects
`auto` at configure time, this is already explicit; just bump the number:

```bash
cmake -DCAVEWHERE_BUILD_NUMBER=<next-unused-integer> ...
```

### Fallback behaviour

Source tarballs or checkouts with no reachable tag produce marketing
version `0.0.0`. The mobile configure step rejects this explicitly
(shared with iOS) so store builds fail loudly instead of shipping a bogus
version. Pass `-DCAVEWHERE_BUILD_NUMBER=<N>` and ensure a tag is reachable
before archiving.
