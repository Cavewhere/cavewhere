# CaveWhere

![CaveWhere Logo](cavewhere_about.png)

## Cave Mapping software

[CaveWhere](https://cavewhere.com) is a cave mapping software with an intuitive design that enables building and visualizing underground cave maps.
Using it’s advanced 3D engine, CaveWhere automatically morphs your 2D cave notes in 3D.
Loop closures re-morph effected scan scraps automatically so your map is always up-to-date.

## Download Binaries

[CaveWhere Downloads](https://cavewhere.com/downloads/)

## Building CaveWhere from source


GitHub Actions provide the most up-to-date instructions for building CaveWhere. 
You can check the latest workflow file here:  
[Linux Build Workflow](https://github.com/cavewhere/CaveWhere/blob/master/.github/workflows/build-linux.yml)

[Windows Build Workflow](https://github.com/cavewhere/CaveWhere/blob/master/.github/workflows/build-windows.yml)

## Building and Running CaveWhere on Ubuntu 23

This guide outlines the steps to build and run CaveWhere, a cave mapping software, on Ubuntu 23.

## Dependencies Installation

First, update your package list and install all necessary dependencies with the following command:

```bash
sudo apt update && sudo apt install -y build-essential cmake ninja-build pipx liblocale-po-perl git
```

## Conan Package Manager Installation

Conan is required for managing packages and dependencies. If you have Conan installed, skip this step. Otherwise, uninstall any existing Conan versions and install the specified version using `pipx`:

```bash
pipx install conan
```

## Building CaveWhere

1. **Clone the Repository and Prepare the Environment**

   Clone the CaveWhere repository and checkout the correct branch (assuming `master` in this case). Initialize and update the submodules:

   ```bash
   git clone https://github.com/Cavewhere/cavewhere.git
   cd cavewhere
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
   conan install ../cavewhere --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True 
   ```

4. **Configure the Project with CMake**

   Use CMake to configure the project. Ensure the `CMAKE_BUILD_TYPE` is set to `Release`:

   ```bash
   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DWITH_PDF=OFF -DCMAKE_MODULE_PATH=`pwd` ../cavewhere
   ```

5. **Build the Project**

   Now, build the project using CMake:

   ```bash
   cmake --build .
   ```

## Running CaveWhere

After a successful build, run CaveWhere directly from the build directory:

```bash
./CaveWhere
```



