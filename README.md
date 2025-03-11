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
pipx ensurepath
```

Restart your terminal or you'll get:

```bash
bash: conan: command not found
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
conan profile detect --force
conan install ../cavewhere -o "&:system_qt=False" --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True -of conan_deps
```
        
   Conan will try to use the local package manager to install compatible dependencies. If a dependency isn't compatible, it will download it from Conan Center or build it from source.

   Conan may also build Qt from source, which can take a long time and use a large amount of ram (16GB recommend for the build, 2GB VM will not cut it). However, this ensures you get the correct version of Qt that has been tested with Cavewhere. To use the system's Qt, set ```system_qt=False```.

   **To use the system Qt libraries (note that you might encounter build errors, as Qt is typically outdated on most Linux distributions 2025.2 will build and run on 6.8 or later):**

```
sudo apt install -y qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-shadertools-dev
 ```
    
4. **Configure the Project with CMake**

```bash
cmake -G Ninja --preset conan-release -DCMAKE_TOOLCHAIN_FILE=conan_deps/conan_toolchain.cmake -S ../cavewhere -B .
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



