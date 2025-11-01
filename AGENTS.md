# Repository Guidelines

## Project Structure & Module Organization
cavewhere is a Qt/C++ desktop app. Top-level `main.cpp` boots the QML UI. Core engine code lives in `cavewherelib/src`, with shared resources under `cavewherelib/qml`, `cavewherelib/shaders`, and `cavewherelib/fonts`. Primary tests are under `testcases/` (C++) and `test-qml/` (QML), with fixtures in `test-qml/datasets`. Auxiliary math and survey logic sit in `QMath3d`, `dewalls`, and other submodules—update them with `git submodule update --init --recursive`. Packaging scripts reside in `installer/`, while Conan configuration sits in `conan/` and `conanfile.py`.

## Build, Test, and Development Commands
From a clean checkout:
```bash
git submodule update --init --recursive
conan profile detect --force
conan install . -o "&:system_qt=False" --build=missing -of conan_deps
cmake --preset conan-release -DCMAKE_TOOLCHAIN_FILE=conan_deps/conan_toolchain.cmake
cmake --build build/Qt_6_8_3_for_macOS-Debug --target all
```
Run the app via `./build/Qt_6_8_3_for_macOS-Debug/CaveWhere`. Build the test runners with `cmake --build build/Qt_6_8_3_for_macOS-Debug --target cavewhere-test cavewhere-qml-test`. Execute C++ unit tests through `./build/Qt_6_8_3_for_macOS-Debug/cavewhere-test` and QML tests via `./build/Qt_6_8_3_for_macOS-Debug/cavewhere-qml-test --platform offscreen`.

## Coding Style & Naming Conventions
Follow Qt’s 4-space indentation and brace-on-next-line style seen in `cavewherelib/src`. Classes and QObjects use UpperCamelCase (`cwProject` retains the legacy `cw` prefix). Member variables take a trailing underscore; helpers use lowerCamelCase. QML components follow the `Thing.qml` pattern, expose lowerCamelCase properties, and are exported through `qt_add_qml_module`. Keep user-facing strings translatable and bundle assets through the existing `.qrc` files.

## Testing Guidelines
C++ coverage lives in `cavewherelib/src` and is exercised by `cavewhere-test`; add new Qt Test cases alongside existing suites. QML behaviors belong in `test-qml/tst_*.qml`; mirror the `tst_Feature.qml` structure and keep fixtures deterministic in `test-qml/datasets`. Always run `./build/cavewhere-test` and `./build/cavewhere-qml-test --platform offscreen` before pushing. Submodule test harnesses (e.g., `QMath3d/tests`) are optional unless you touch those modules.

## Commit & Pull Request Guidelines
Recent history favors short, capitalized summaries (`Refactored to use cwGeometry`). Use the imperative, skip trailing punctuation, and keep subjects under ~60 characters. In PRs, summarize the change, list build/test commands run, call out data migrations, and attach UI screenshots when QML views change. Link related issues and flag dependency or submodule updates.

## Dependencies & Environment Notes
The project relies on Conan-managed Qt 6 packages; edit the default profile to pin `cmake/<4.0` before installing dependencies. If using system Qt, ensure the distro ships Qt ≥6.8 or expect build errors. macOS builds use `entitlements.plist`, while Windows packaging depends on `installer/windows`. Rebuild after merges so `GitHash.cmake` refreshes the revision shown in the About dialog.
