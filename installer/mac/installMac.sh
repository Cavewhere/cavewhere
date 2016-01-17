#!/bin/bash

echo "Source directory" $2

echo "Cavewhere Mac Installer"

echo "Installing name tool on plotSauce"
install_name_tool -change @rpath/QtXml.framework/Versions/5/QtXml @executable_path/../Frameworks/QtXml.framework/Versions/5/QtXml Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change @rpath/QtCore.framework/Versions/5/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/5/QtCore Cavewhere.app/Contents/MacOS/plotsauce
install_name_tool -change @rpath/QtGui.framework/Versions/5/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/5/QtGui Cavewhere.app/Contents/MacOS/plotsauce

echo "Deleting old DMG"
rm -rv "Cavewhere $1.dmg"

echo "Creating Cavewhere DMG"
$2/mac/create-dmg/create-dmg \
    --window-size 615 379 \
    --app-drop-link 440 175 \
    --background $2/mac/dmgBackground.png \
    --icon "Cavewhere.app" 170 175 \
    "Cavewhere $1.dmg" \
    "Cavewhere.app"
