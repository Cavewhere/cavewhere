#!/bin/bash

qtDir=~/Qt5.1.1/5.1.1/clang_64
buildDirectory=/Users/vpicaver/Documents/Projects/cavewhere/qtc_Desktop_Qt_5_1_1_clang_64bit-release

echo "Cavewhere Mac Installer"

echo "Delete rm Cavewhere"
rm -r Cavewhere.app

echo "Copying bundle from $buildDirectory/Cavewhere.app"
cp -r $buildDirectory/Cavewhere.app .

echo "Running macdeployqt"
$qtDir/bin/macdeployqt Cavewhere.app

echo "Coping QML Plugins"
mkdir -p Cavewhere.app/Contents/MacOS/QtQuick
cp -r $qtDir/qml/QtGraphicalEffects Cavewhere.app/Contents/MacOS
cp -r $qtDir/qml/QtQuick.2 Cavewhere.app/Contents/MacOS
cp -r $qtDir/qml/QtQuick/Dialogs Cavewhere.app/Contents/MacOS/QtQuick
cp -r $qtDir/qml/QtQuick/Layouts Cavewhere.app/Contents/MacOS/QtQuick
cp -r $qtDir/qml/QtQuick/Controls Cavewhere.app/Contents/MacOS/QtQuick
cp -r $qtDir/qml/QtQuick/Window.2 Cavewhere.app/Contents/MacOS/QtQuick

echo "Coping QML"
cp -r ../../qml Cavewhere.app/Contents/MacOS

echo "Coping Shaders"
cp -r ../../shaders Cavewhere.app/Contents/MacOS

echo "Installing name tool on libQMath3d"
cp $buildDirectory/libQMath3d.dylib Cavewhere.app/Contents/Frameworks
install_name_tool -change $buildDirectory/libQMath3d.dylib @executable_path/../Frameworks/libQMath3d.dylib Cavewhere.app/Contents/MacOS/Cavewhere
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/Frameworks/libQMath3d.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/Frameworks/libQMath3d.dylib

echo "Installing name tool on plotSauce"
install_name_tool -change $qtDir/lib/QtXml.framework/Versions/5/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/5/QtXml Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/plotsauce

echo "Installing name tool on QtQuick.Controls"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtQuick/Controls/libqtquickcontrolsplugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtQuick/Controls/libqtquickcontrolsplugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtQuick/Controls/libqtquickcontrolsplugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtQuick/Controls/libqtquickcontrolsplugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtQuick/Controls/libqtquickcontrolsplugin.dylib
install_name_tool -change $qtDir/lib/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets Cavewhere.app/Contents/MacOS/QtQuick/Controls/libqtquickcontrolsplugin.dylib

echo "Installing name tool on QtQuick.Controls.Private"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtQuick/Controls/Private/libqtquickcontrolsprivateplugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtQuick/Controls/Private/libqtquickcontrolsprivateplugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtQuick/Controls/Private/libqtquickcontrolsprivateplugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtQuick/Controls/Private/libqtquickcontrolsprivateplugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtQuick/Controls/Private/libqtquickcontrolsprivateplugin.dylib
install_name_tool -change $qtDir/lib/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets Cavewhere.app/Contents/MacOS/QtQuick/Controls/Private/libqtquickcontrolsprivateplugin.dylib

echo "Installing name tool on QQuick.Layouts"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtQuick/Layouts/libqquicklayoutsplugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtQuick/Layouts/libqquicklayoutsplugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtQuick/Layouts/libqquicklayoutsplugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtQuick/Layouts/libqquicklayoutsplugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtQuick/Layouts/libqquicklayoutsplugin.dylib
install_name_tool -change $qtDir/lib/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets Cavewhere.app/Contents/MacOS/QtQuick/Layouts/libqquicklayoutsplugin.dylib
install_name_tool -change $qtDir/lib/QtV8.framework/Versions/5/QtV8 @executable_path/../Frameworks/QtV8.framework/Versions/5/QtV8 Cavewhere.app/Contents/MacOS/QtQuick/Layouts/libqquicklayoutsplugin.dylib

echo "Installing name tool on QQuick.Dialogs"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtQuick/Dialogs/libdialogplugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtQuick/Dialogs/libdialogplugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtQuick/Dialogs/libdialogplugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtQuick/Dialogs/libdialogplugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtQuick/Dialogs/libdialogplugin.dylib
install_name_tool -change $qtDir/lib/QtWidgets.framework/Versions/5/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/5/QtWidgets Cavewhere.app/Contents/MacOS/QtQuick/Dialogs/libdialogplugin.dylib

echo "Installing name tool on QtQuick.2"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtQuick.2/libqtquick2plugin.dylib

echo "Installing name tool on QtQuick.Window.2"
install_name_tool -change $qtDir/lib/QtQuick.framework/Versions/5/QtQuick @executable_path/../Frameworks/QtQuick.framework/Versions/5/QtQuick Cavewhere.app/Contents/MacOS/QtQuick/Window.2/libwindowplugin.dylib
install_name_tool -change $qtDir/lib/QtQml.framework/Versions/5/QtQml @executable_path/../Frameworks/QtQml.framework/Versions/5/QtQml Cavewhere.app/Contents/MacOS/QtQuick/Window.2/libwindowplugin.dylib
install_name_tool -change $qtDir/lib/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/QtQuick/Window.2/libwindowplugin.dylib
install_name_tool -change $qtDir/lib/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/QtQuick/Window.2/libwindowplugin.dylib
install_name_tool -change $qtDir/lib/QtNetwork.framework/Versions/5/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/5/QtNetwork Cavewhere.app/Contents/MacOS/QtQuick/Window.2/libwindowplugin.dylib

