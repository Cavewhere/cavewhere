# AGENTS.md

## Repository Guidelines

## Project Structure and Module Organization
cavewhere is a Qt and C++ desktop application. The `main.cpp` file launches the QML user interface. Core engine code is located in `cavewherelib/src`, with shared resources in `cavewherelib/qml`, `cavewherelib/shaders`, and `cavewherelib/fonts`. Tests are located in `testcases/` for C++ and `test-qml/` for QML, with supporting datasets in `test-qml/datasets`.

Additional math and survey logic resides in modules such as `QMath3d` and `dewalls`. Make sure to keep these updated using:
```
git submodule update --init --recursive
```

Packaging scripts are found in `installer/`. Conan configuration lives in `conan/` and `conanfile.py`.

## Build, Test, and Development Commands

From a fresh checkout:
```
git submodule update --init --recursive
conan profile detect --force
conan install . -o "&:system_qt=False" --build=missing -of conan_deps
cmake --preset conan-release -DCMAKE_TOOLCHAIN_FILE=conan_deps/conan_toolchain.cmake
cmake --build build/Qt_6_8_3_for_macOS-Debug --target all
```

Run the application with:
```
./build/Qt_6_8_3_for_macOS-Debug/CaveWhere
```

Tests:
```
cmake --build build/Qt_6_8_3_for_macOS-Debug --target cavewhere-test cavewhere-qml-test
./build/Qt_6_8_3_for_macOS-Debug/cavewhere-test
./build/Qt_6_8_3_for_macOS-Debug/cavewhere-qml-test --platform offscreen
```

## Coding Style and Naming Conventions

### C++
Follow Qt’s standard conventions:
- Use 4 spaces for indentation.
- Opening braces on the next line.
- Class names and QObject types use UpperCamelCase.
- Member variables use the `m_` prefix.
- Helper functions use lowerCamelCase.
- Use clear and complete variable names.
- Prefer `const` correctness.

### QML
Follow Qt Quick coding conventions:
- Use strict property types rather than `var`.
- Use lowerCamelCase for property names.
- Capitalize QML component filenames.
- Only use `opacity` when necessary.
- Keep bindings simple and avoid binding loops.
- Prefer `id` selectors instead of `objectName`.
- Group related properties together.
- Export modules using `qt_add_qml_module`.
- Keep QML files small and focused.
- Use JavaScript only for simple logic.

### Performance Guidelines for QML
Based on Qt Quick performance best practices:
- Minimize the number of QML items.
- Avoid unnecessary nesting.
- Prefer `Item` instead of `Rectangle` when no visual is needed.
- Avoid expensive operations in bindings.
- Cache values when possible instead of recalculating.
- Use `Loader` for deferred or conditional loading.
- Prefer `Image` with texture caching enabled.
- Use `implicitWidth` and `implicitHeight` consistently.
- Avoid anchors in tight loops; prefer layout items for dynamic sizing.
- Animate only properties that are GPU efficient, such as `opacity` and `scale`.
- Avoid animating layout changes.
- Use `Layer.enabled: true` carefully and only when it improves rendering.

## Testing Guidelines

C++ tests live under `cavewherelib/src` and are run through `cavewhere-test`. Add new test files following existing patterns.

QML tests go in `test-qml/tst_*.qml`. Use deterministic datasets stored in `test-qml/datasets`. Run tests with the offscreen platform.

Always run both C++ and QML test suites before pushing changes.

## Commit and Pull Request Guidelines
- Keep commit subjects short and capitalized.
- Use imperative phrasing.
- Avoid trailing periods.
- Describe what changed and why.
- List the build and test commands executed.
- Attach screenshots for QML UI changes.
- Link any related issues.
- Clearly note changes involving submodules or dependencies.

## Dependencies and Environment Notes
The project uses Conan for managing Qt 6 packages. Pin CMake to versions below 4.0 in the Conan profile. If choosing to use a system Qt installation, ensure Qt 6.8 or above.

macOS builds use `entitlements.plist`. Windows installers live under `installer/windows`.

`GitHash.cmake` is updated on build and displayed in the About dialog.

## QML Guidelines Summary
- Avoid unnecessary opacity usage.
- Avoid `property var` and favor typed properties.
- Keep bindings efficient.
- Avoid deep nesting.
- Follow Qt’s coding conventions.

