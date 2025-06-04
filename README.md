Known working devices / digitizers

| OS                      | Device                   | Pen                    | Notes                                                                                                                                                      |
|-------------------------|--------------------------|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------|
| iOS                     | iPad Pro 10.5 (2017)     | Apple Pencil (1st gen) | **Works!**                                                                                                                                                  |
| Android                 | Samsung Note 10          | S Pen                  | **Works!**                                                                                                                                                  |
| Windows 11              | ThinkVision M14t (Gen 1) | Pen                    | **Works!**                                                                                                                                                  |
| macOS with Sidecar      | iPad Pro 10.5 (2017)     | Apple Pencil (1st gen) | Pressure sensitivity is supported. The Apple Pencil isn’t detected as a stylus in Qt, but works with `PointerDevice.Unknown`. [QTBUG-80072](https://bugreports.qt.io/browse/QTBUG-80072) |

__Setups that DON'T work__

| OS                                 | Device                   | Pen                    | Notes                                                                                                       |
|------------------------------------|--------------------------|------------------------|-------------------------------------------------------------------------------------------------------------|
| Android < 9.0                      | Samsung Note 4           | S Pen                  | Uses Android 6.0, which is too old and unsupported by Qt 6.8.3 (requires Android >= 9).                     |
| macOS                              | ThinkVision M14t (Gen 1) | Pen                    | Fails – pen tracks, but the pen tip doesn’t draw. The problem exists across all applications.             |
| Windows 11 (ARM) via Parallels on macOS | ThinkVision M14t (Gen 1) | Pen                    | Fails – pen tracks, but the pen tip doesn’t draw. The problem exists across all applications.             |

# Building CaveWhere Sketch in QtCreator 

## Conan Package Manager Installation

Conan > 2.0 is required for managing packages and dependencies. If you already have Conan installed, skip this step. 

```bash
pip install conan
pip ensurepath
```

Restart your terminal or you’ll get:

```bash
bash: conan: command not found
```

## Building CaveWhere in QtCreator

1. **Clone the Repository and Prepare the Environment**

   Clone the CaveWhere repository and check out the correct branch (assuming `master` in this case). Initialize and update the submodules:

   ```bash
   git clone git@github.com:Cavewhere/cavewhere-sketch.git
   cd cavewhere
   git submodule update --init --recursive
   ```

2. **Download and install Qt > 6.8.0**

   You’ll need a Qt account, but it’s free for open-source projects like CaveWhere Sketch:
   https://www.qt.io/download-qt-installer-oss 

   ![Add and remove](readme-resources/add-remove.png)

   Make sure you install for the platform you want to build for: Desktop, iOS, or Android. Also install QtCreator through the installer:

   ![Download the correct qt type](readme-resources/qt-types.png)

   CaveWhere Sketch uses some additional Qt libraries:
   - Qt Image Formats
   - Qt Shader Tools

   ![Download extra libs](readme-resources/extra-libs.png)

3. **Open the CaveWhere Sketch project in QtCreator**

   Go to File → Open File or Project → `CMakeLists.txt`.

   ![Open CMakeLists.txt](readme-resources/open-cmakelist.png)

4. **Configuring**

   Depending on the platform, CaveWhere Sketch might not configure correctly when you first open `CMakeLists.txt`. Go to the Projects pane and select the platform you want using the dropdown at the top.

   ![Configuration page](readme-resources/configuring-platform.png)

5. **Configuring for Desktop – macOS build and macOS run (host)**

   To make Conan run correctly, set up the following build environment:

   ```bash
   SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
   PATH=/usr/bin:/bin:/usr/sbin:/sbin:/Users/cave/Qtcom/Tools/Ninja:/Users/cave/Library/Python/3.9/bin
   ```

   ![Environment](readme-resources/env.png)

   ![Setting the Environment](readme-resources/env-settings.png)

   **Note**  
   It’s important that `/Users/cave/Library/Python/3.9/bin` is in the `PATH` so QtCreator can find `conan`.

   CaveWhere Sketch also needs the following CMake variables for initial configuration:

   ```text
   CMAKE_CXX_STANDARD:STRING=17
   CMAKE_OSX_DEPLOYMENT_TARGET:STRING=14.0
   ```

   ![Extra CMake settings for macOS](readme-resources/extra-cmake-macos.png)

6. **Configuring for Android – macOS build and Android run (host)**

   You need to install Java and the Android NDK on your system. Documentation is here:
   https://doc.qt.io/qt-6/android-getting-started.html

   To make Conan run correctly, set up the following build environment:

   ```bash
   SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
   PATH=/usr/bin:/bin:/usr/sbin:/sbin:/Users/cave/Qtcom/Tools/Ninja:/Users/cave/Library/Python/3.9/bin
   ```

   ![Android environment](readme-resources/env-android.png)

   **Note**  
   It’s important that `/Users/cave/Library/Python/3.9/bin` is in the `PATH` so QtCreator can find `conan`.

   CaveWhere Sketch also needs the following CMake variables for initial configuration:

   ```text
   CMAKE_CXX_STANDARD:STRING=17
   CMAKE_PROJECT_INCLUDE_BEFORE:FILEPATH=YOUR_PATH_TO/CaveWhereSketch/conan/auto-setup-cross.cmake
   ```

   ![Android CMake settings](readme-resources/extra-cmake-android.png)

   You’ll need to hit **Re-configure** after making changes.

7. **Configuring for iOS – macOS build and iOS run (host)**

   Qt iOS documentation is here:
   https://doc.qt.io/qt-6/ios.html

   To make Conan run correctly, set up the following build environment:

   ```bash
   PATH=/usr/bin:/bin:/usr/sbin:/sbin:/Users/cave/Qtcom/Tools/Ninja:/Users/cave/Library/Python/3.9/bin
   ```

   ![Environment](readme-resources/env.png)

   ![Setting the Environment](readme-resources/env-settings.png)

   **Note**  
   It’s important that `/Users/cave/Library/Python/3.9/bin` is in the `PATH` so QtCreator can find `conan`.

   CaveWhere Sketch also needs the following CMake variables for initial configuration:

   ```text
   CMAKE_CXX_STANDARD:STRING=17
   CMAKE_OSX_DEPLOYMENT_TARGET:STRING=16.0
   CMAKE_OSX_ARCHITECTURES:STRING=arm64
   CMAKE_PROJECT_INCLUDE_BEFORE:FILEPATH=YOUR_PATH_TO/CaveWhereSketch/conan/auto-setup-cross.cmake
   ```

   ![iOS CMake settings](readme-resources/extra-cmake-ios.png)

   You’ll need to hit **Re-configure** after making changes.

8. **Building CaveWhere Sketch**

   Press **⌘R** (Command-R) in QtCreator.

   ![Build and run CaveWhere Sketch](readme-resources/run.png)
