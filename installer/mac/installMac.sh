#!/bin/bash

qtDir=~/Qt5.0.1/5.0.1/clang_64

echo "Cavewhere Mac Installer"

echo "Delete rm Cavewhere"
rm -r Cavewhere.app

echo "Copying bundle"
cp -r ../../Cavewhere.app .

echo "Running macdeployqt"
$qtDir/bin/macdeployqt Cavewhere.app

echo "Coping QML Plugins"
cp -r $qtDir/qml/QtDesktop Cavewhere.app/Contents/MacOS
cp -r $qtDir/qml/QtQuick.2 Cavewhere.app/Contents/MacOS

echo "Coping QML"
cp -r ../../qml Cavewhere.app/Contents/MacOS

echo "Coping Shaders"
cp -r ../../shaders Cavewhere.app/Contents/MacOS

echo "Installing name tool on plotSauce"
install_name_tool -change $qtDir/lib/QtXml.framework/Versions/5/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/5/QtXml Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/plotsauce

echo "Installing name tool on QtDesktop"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtDesktop/libstyleplugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtDesktop/libstyleplugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtDesktop/libstyleplugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtDesktop/libstyleplugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtDesktop/libstyleplugin.dylib
install_name_tool -change $qtDir/lib/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets Cavewhere.app/Contents/MacOS/QtDesktop/libstyleplugin.dylib

echo "Installing name tool on QtQuick.2"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib

