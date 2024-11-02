# CaveWhere

![CaveWhere Logo](/cavewhereLib/icons/githubPage.png)

## Cave Mapping software

[CaveWhere](https://cavewhere.com) is a cave mapping software with an intuitive design that enables building and visualizing underground cave maps.
Using it’s advanced 3D engine, CaveWhere automatically morphs your 2D cave notes in 3D.
Loop closures re-morph effected scan scraps automatically so your map is always up-to-date.

## Download Binaries

[CaveWhere Downloads](https://cavewhere.com/downloads/)


## Building and Running CaveWhere on Ubuntu 23

This guide outlines the steps to build and run CaveWhere, a cave mapping software, on Ubuntu 23.

## Dependencies Installation

First, update your package list and install all necessary dependencies with the following command:

```bash
sudo apt update && sudo apt install -y ninja-build libx11-xcb-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 qt5-default qtdeclarative5-dev libqt5svg5-dev libgtk2.0-dev libxmu-dev libfontenc-dev libxaw7-dev libxkbfile-dev libxmuu-dev libxpm-dev libxres-dev libxss-dev libxtst-dev libxv-dev libxxf86vm-dev libwebkit2gtk-4.0-dev qml-module-qtquick-controls qml-module-qtquick-dialogs qml-module-qtquick-window2 qml-module-qt-labs-settings qml-module-qtquick-controls2 survex
```

## Conan Package Manager Installation

Conan is required for managing packages and dependencies. If you have Conan installed, skip this step. Otherwise, uninstall any existing Conan versions and install the specified version using `pipx`:

```bash
pipx install conan==1.63.0
```

## Building CaveWhere

1. **Clone the Repository and Prepare the Environment**

   Clone the CaveWhere repository and checkout the correct branch (assuming `WIP-cmake` in this case). Initialize and update the submodules:

   ```bash
   git clone https://github.com/Cavewhere/cavewhere.git
   cd cavewhere
   git checkout --track origin/WIP-cmake
   git submodule update --init --recursive
   cd ..
   ```

2. **Create a Build Directory**

   Create a separate directory for the build to keep it clean from the source code:

   ```bash
   mkdir cavewhere-build-release && cd cavewhere-build-release
   ```

3. **Install Dependencies with Conan**

   Use Conan to install the project dependencies:

   ```bash
   conan install ../cavewhere --build=missing
   ```

4. **Source the Conan Environment**

   Source the Conan environment script to set up necessary environment variables:

   ```bash
   source conanbuildenv-release-x86_64.sh
   ```

5. **Configure the Project with CMake**

   Use CMake to configure the project. Ensure the `CMAKE_BUILD_TYPE` is set to `Release`:

   ```bash
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DWITH_PDF=OFF -DCMAKE_MODULE_PATH=`pwd` ../cavewhere
   ```

6. **Build the Project**

   Now, build the project using CMake:

   ```bash
   cmake --build .
   ```

## Running CaveWhere

After a successful build, run CaveWhere directly from the build directory:

```bash
./CaveWhere
```



