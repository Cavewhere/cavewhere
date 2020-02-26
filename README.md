# CaveWhere

[![Build Status](https://travis-ci.org/Cavewhere/cavewhere.svg?branch=master)](https://travis-ci.org/Cavewhere/cavewhere)

![CaveWhere Logo](/icons/githubPage.png)

## Cave Mapping software

[CaveWhere](https://cavewhere.com) is a cave mapping software with an intuitive design that enables building and visualizing underground cave maps.
Using it’s advanced 3D engine, CaveWhere automatically morphs your 2D cave notes in 3D.
Loop closures re-morph effected scan scraps automatically so your map is always up-to-date.

## Download Binaries

[CaveWhere Downloads](https://cavewhere.com/downloads/)

## Building from Source on Ubuntu 18.04

CaveWhere is built around Qt and QBS.

### Get all dependencies
```{sh}
sudo add-apt-repository -y ppa:beineri/opt-qt-5.12.7-bionic
sudo apt-get -y update
sudo apt-get -y install git build-essential qt512-meta-minimal qt512svg qt512quickcontrols qt512quickcontrols2 qt512graphicaleffects qt512script libgl1-mesa-glx libgl1-mesa-dev
```

### Make sure libGL.so exists.

If you're on a x86_64 linux box. Make sure `/usr/lib/x86_64-linux-gnu/libGL.so` exists. If
doesn't you simply need to create a symbolic link to it.

```{sh}
ls -l /usr/lib/x86_64-linux-gnu/libGL.so
sudo ln -s /usr/lib/x86_64-linux-gnu/libGL.so.1 /usr/lib/x86_64-linux-gnu/libGL.so
```

### Building QBS

Ubuntu 18.04 comes with QBS 1.10. This version doesn't build CaveWhere. CaveWhere is typically built with
QBS 1.14 or later. Ubuntu 19.04, does come with QBS 1.13, which will probably work. For Ubuntu 18.04,
building it from source is the way to go.

```{sh}
git clone https://github.com/qbs/qbs.git
cd qbs
git checkout v1.15.0
/opt/qt512/bin/qmake -r qbs.pro && make -j `nproc`
sudo make install
cd ..
qbs --version
```

### Building CaveWhere
Once Qt and QBS have been installed, you can build CaveWhere with the following:

```{sh}
qbs setup-toolchains --detect
qbs setup-qt /opt/qt512/bin/qmake qt5
qbs config profiles.qt5.baseProfile x86_64-linux-gnu-gcc-7
qbs config defaultProfile qt5
qbs resolve profile:qt5 config:release
qbs build --products profile:qt5 config:release
```

### Running CaveWhere

```{sh}
qbs run --products CaveWhere profile:qt5 config:release
```

### Running Unit Testcases

```{sh}
qbs run --products cavewhere-test profile:qt5 config:release
```

### Building CaveWhere and running in debug

```{sh}
git clone --branch=master https://github.com/Cavewhere/cavewhere.git cavewhere
cd cavewhere
git submodule update --init --recursive
qbs resolve profile:qt5 config:debug
qbs build --products profile:qt5 config:debug
qbs run --products CaveWhere profile:qt5 config:debug
```
By default in linux and macos, in debug mode, CaveWhere uses -faddress-santizer for
detecting memory bugs and memory leaks.  You might get the following message when
running CaveWhere in debug:

```{sh}
==48458==ASan runtime does not come first in initial library list; you should either link runtime to your application or manually preload it with LD_PRELOAD.
```

To resolve this issue on Ubuntu 18.04, run the following:

```{sh}
export LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/7/libasan.so
qbs run --products CaveWhere profile:qt5 config:debug
```



